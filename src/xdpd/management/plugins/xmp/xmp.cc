/*
 * xmp.cc
 *
 *  Created on: 11.01.2014
 *      Author: andreas
 */

#include "xmp.h"

#include <rofl/common/csocket.h>

#include "../../switch_manager.h"
#include "../../system_manager.h"



using namespace rofl; 
using namespace xdpd::mgmt::protocol;



xmp::xmp() :
		socket(NULL),
		socket_type(rofl::csocket::SOCKET_TYPE_PLAIN),
		fragment(NULL),
		msg_bytes_read(0)
{
	socket_params = rofl::csocket::get_default_params(rofl::csocket::SOCKET_TYPE_PLAIN);

	socket_params.set_param(rofl::csocket::PARAM_KEY_LOCAL_HOSTNAME).set_string(MGMT_PORT_UDP_ADDR);
	socket_params.set_param(rofl::csocket::PARAM_KEY_DOMAIN).set_string("inet");
	socket_params.set_param(rofl::csocket::PARAM_KEY_TYPE).set_string("stream");
	socket_params.set_param(rofl::csocket::PARAM_KEY_PROTOCOL).set_string("tcp");
}

xmp::~xmp()
{
	if (socket) {
		socket->close();
		delete socket;
	}
	for (std::set<rofl::csocket*>::iterator
			it = workers.begin(); it != workers.end(); ++it) {
		(*(*it)).close();
		delete (*it);
	}
}

void
xmp::init()
{
	if (socket) {
		socket->close();
		delete socket;
		socket = NULL;
	}

	if (system_manager::is_option_set("xmp-port")) {
		socket_params.set_param(rofl::csocket::PARAM_KEY_LOCAL_PORT).set_string(system_manager::get_option_value("xmp-port"));
	} else {
		socket_params.set_param(rofl::csocket::PARAM_KEY_LOCAL_PORT).set_string(MGMT_PORT_UDP_PORT);
	}


	(socket = rofl::csocket::csocket_factory(socket_type, this))->listen(socket_params);
}

std::vector<rofl::coption>
xdpd::mgmt::protocol::xmp::get_options(void)
{
	std::vector<rofl::coption> options;

	//Add mandatory -c argument
	options.push_back(rofl::coption(true, REQUIRED_ARGUMENT, 'x', "xmp-port", "listen port for xmp plugin", MGMT_PORT_UDP_PORT));

	return options;
}


void
xmp::handle_listen(
		rofl::csocket& socket,
		int newsd)
{
	LOGGING_INFO << "[xdpd][plugin][xmp]" << __PRETTY_FUNCTION__ << std::endl;
	LOGGING_DEBUG << "socket: " << socket;

	rofl::csocket* worker;
	(worker = rofl::csocket::csocket_factory(socket_type, this))->accept(socket_params, newsd);

	LOGGING_DEBUG << "worker: " << (*worker);
}

void
xmp::handle_accepted(
		rofl::csocket& socket)
{
	LOGGING_INFO << "[xdpd][plugin][xmp]" << __PRETTY_FUNCTION__ << ": " << socket << std::endl;
	LOGGING_DEBUG << socket;

	rofl::csocket* worker = &socket;
	workers.insert(worker);
}

void
xmp::handle_accept_refused(
		rofl::csocket& socket)
{
	LOGGING_INFO << "[xdpd][plugin][xmp]" << __PRETTY_FUNCTION__ << std::endl;

	rofl::csocket* worker = &socket;
	delete worker;
}

void
xmp::handle_closed(
		rofl::csocket& socket)
{
	LOGGING_INFO << "[xdpd][plugin][xmp]" << __PRETTY_FUNCTION__ << std::endl;
	LOGGING_DEBUG << socket;
	rofl::csocket* worker = &socket;
	workers.erase(worker);
	delete worker;
}

void
xmp::handle_timeout(
		int opaque, void *data)
{
	LOGGING_INFO << "[xdpd][plugin][xmp]" << __PRETTY_FUNCTION__ << std::endl;
	switch (opaque) {
	default:
		;;
	}
}

void
xmp::handle_read(
		csocket& socket)
{
	LOGGING_INFO << "[xdpd][plugin][xmp]" << __PRETTY_FUNCTION__ << std::endl;
	LOGGING_DEBUG << socket;


	try {

		if (0 == fragment) {
			fragment = new rofl::cmemory(sizeof(struct xmp_header_t));
			msg_bytes_read = 0;
		}

		while (true) {
			uint16_t msg_len = 0;

			// how many bytes do we have to read?
			if (msg_bytes_read < sizeof(struct xmp_header_t)) {
				msg_len = sizeof(struct xmp_header_t);
			} else {
				struct xmp_header_t *header = (struct xmp_header_t*)fragment->somem();
				msg_len = be16toh(header->len);
			}

			// sanity check: 8 <= msg_len <= 2^16
			if (msg_len < sizeof(struct xmp_header_t)) {
				LOGGING_WARN << "[xdpd][plugin][xmp] received message with invalid length field, closing socket." << std::endl;
				socket.close();
				workers.erase(&socket);
				return;
			}

			// resize msg buffer, if necessary
			if (fragment->memlen() < msg_len) {
				fragment->resize(msg_len);
			}

			// read from socket more bytes, at most "msg_len - msg_bytes_read"
			int nbytes = socket.recv((void*)(fragment->somem() + msg_bytes_read), msg_len - msg_bytes_read);

			msg_bytes_read += nbytes;

			// minimum message length received, check completeness of message
			if (fragment->memlen() >= sizeof(struct xmp_header_t)) {
				struct xmp_header_t *header = (struct xmp_header_t*)fragment->somem();
				uint16_t msg_len = be16toh(header->len);

				// ok, message was received completely
				if (msg_len == msg_bytes_read) {
					rofl::cmemory *mem = fragment;
					fragment = NULL; // just in case, we get an exception from parse_message()
					msg_bytes_read = 0;

					cxmpmsg msg(mem->somem(), msg_len);

					switch (header->type) {
					case XMPT_REQUEST:
					{
						handle_request(socket, msg);
					}
						break;
					case XMPT_REPLY:
					case XMPT_NOTIFICATION:
					default:
					{
						LOGGING_ERROR
								<< "[xdpd][plugin][xmp] unknown message rcvd"
								<< std::endl;
					}
						;
					}

					delete mem;

					return;
				}
			}
		}

	} catch (rofl::eSocketRxAgain& e) {

		// more bytes are needed, keep pointer to msg in "fragment"
		LOGGING_DEBUG << "[xdpd][plugin][xmp] read again" << std::endl;

	} catch (rofl::eSysCall& e) {

		LOGGING_WARN << "[xdpd][plugin][xmp] closing socket: " << e << std::endl;

		if (fragment) {
			delete fragment;
			fragment = NULL;
		}

		// close socket, as it seems, we are out of sync
		socket.close();

	} catch (rofl::RoflException& e) {

		LOGGING_WARN << "[xdpd][plugin][xmp] dropping invalid message: " << e << std::endl;

		if (fragment) {
			delete fragment;
			fragment = NULL;
		}

		// close socket, as it seems, we are out of sync
		socket.close();
	}
}


void
xmp::handle_request(csocket& socket, cxmpmsg& msg)
{
	LOGGING_INFO<< "[xdpd][plugin][xmp] rcvd message:" << std::endl << msg;

	if (not msg.get_xmpies().has_ie_command()) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] rcvd xmp request without -COMMAND- IE, dropping message." << std::endl;
		return;
	}

	switch (msg.get_xmpies().get_ie_command().get_command()) {
	case XMPIEMCT_PORT_ATTACH: {
		handle_port_attach(socket, msg);
	} break;
	case XMPIEMCT_PORT_DETACH: {
		handle_port_detach(socket, msg);
	} break;
	case XMPIEMCT_PORT_ENABLE: {
		handle_port_enable(socket, msg);
	} break;
	case XMPIEMCT_PORT_DISABLE: {
		handle_port_disable(socket, msg);
	} break;
	case XMPIEMCT_PORT_LIST: {
		handle_port_list(socket, msg);
	} break;
	case XMPIEMCT_PORT_INFO: {
		handle_port_info(socket, msg);
	} break;
	case XMPIEMCT_LSI_LIST: {
		handle_lsi_list(socket, msg);
	} break;
	case XMPIEMCT_LSI_INFO: {
		handle_lsi_info(socket, msg);
	} break;
	case XMPIEMCT_LSI_CREATE: {
		handle_lsi_create(socket, msg);
	} break;
	case XMPIEMCT_LSI_DESTROY: {
		handle_lsi_destroy(socket, msg);
	} break;
	case XMPIEMCT_LSI_CONTROLLER_CONNECT: {
		handle_lsi_connect_to_controller(socket, msg);
	} break;
	case XMPIEMCT_LSI_XCONNNECT: {
		handle_lsi_cross_connect(socket, msg);
	} break;
	case XMPIEMCT_NONE:
	default: {
		LOGGING_ERROR << "[xdpd][plugin][xmp] rcvd xmp request with unknown command:"
				<< (int)msg.get_xmpies().get_ie_command().get_command() << ", dropping message." << std::endl;

		cxmpmsg *reply = new cxmpmsg(XMP_VERSION, XMPT_ERROR);
		reply->set_xid(msg.get_xid());

		socket.send(reply);
		return;
	};
	}
}


void
xmp::handle_port_attach(csocket& socket, cxmpmsg& msg)
{
	cxmpmsg *reply = new cxmpmsg(XMP_VERSION, XMPT_REPLY);
	reply->set_xid(msg.get_xid());

	std::string portname;
	uint64_t dpid = 0;

	try {
		if (not msg.get_xmpies().has_ie_portname()) {
			LOGGING_ERROR << "[xdpd][plugin][xmp] rcvd xmp Port-Attach request without -PORTNAME- IE, dropping message." << std::endl;
			return;
		}

		if (not msg.get_xmpies().has_ie_dpid()) {
			LOGGING_ERROR << "[xdpd][plugin][xmp] rcvd xmp Port-Attach request without -DPID- IE, dropping message." << std::endl;
			return;
		}

		portname = msg.get_xmpies().get_ie_portname().get_name();
		dpid = msg.get_xmpies().get_ie_dpid().get_dpid();

		unsigned int of_port_num = 0;
		port_manager::attach_port_to_switch(dpid, portname, &of_port_num);
		LOGGING_ERROR << "[xdpd][plugin][xmp] attached port:" << portname
				<< " to dpid:" << (unsigned long long)dpid << " "
				<< " port-no:" << of_port_num << std::endl;

		port_manager::bring_up(portname);

	} catch(eOfSmDoesNotExist& e) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] attaching port:" << portname
				<< " to dpid:" << (unsigned long long)dpid << " failed, LSI does not exist" << std::endl;
		reply->set_type(XMPT_ERROR);

	} catch(ePmInvalidPort& e) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] attaching port:" << portname
				<< " to dpid:" << (unsigned long long)dpid << " failed (ePmInvalidPort)" << std::endl;
		reply->set_type(XMPT_ERROR);

	} catch(ePmUnknownError& e) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] attaching port:" << portname
				<< " from dpid:" << (unsigned long long)dpid << " failed (ePmUnknownError)" << std::endl;
		reply->set_type(XMPT_ERROR);

	} catch(eOfSmGeneralError& e) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] attaching port:" << portname
				<< " to dpid:" << (unsigned long long)dpid << " failed (eOfSmGeneralError)" << std::endl;
		reply->set_type(XMPT_ERROR);

	} catch (...) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] attaching port:" << portname
				<< " to dpid:" << (unsigned long long)dpid << " failed" << std::endl;
		reply->set_type(XMPT_ERROR);
	}

	socket.send(reply);
}


void
xmp::handle_port_detach(csocket& socket, cxmpmsg& msg)
{
	cxmpmsg *reply = new cxmpmsg(XMP_VERSION, XMPT_REPLY);
	reply->set_xid(msg.get_xid());

	std::string portname;
	uint64_t dpid = 0;

	try {
		if (not msg.get_xmpies().has_ie_portname()) {
			LOGGING_ERROR << "[xdpd][plugin][xmp] rcvd xmp Port-Detach request without -PORTNAME- IE, dropping message." << std::endl;
			return;
		}

		if (not msg.get_xmpies().has_ie_dpid()) {
			LOGGING_ERROR << "[xdpd][plugin][xmp] rcvd xmp Port-Detach request without -DPID- IE, dropping message." << std::endl;
			return;
		}

		portname = msg.get_xmpies().get_ie_portname().get_name();
		dpid = msg.get_xmpies().get_ie_dpid().get_dpid();

		port_manager::detach_port_from_switch(dpid, portname);
		LOGGING_ERROR << "[xdpd][plugin][xmp] detached port:" << portname
				<< " from dpid:" << (unsigned long long)dpid << std::endl;

	} catch(eOfSmDoesNotExist& e) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] detaching port:" << portname
				<< " from dpid:" << (unsigned long long)dpid << " failed, LSI does not exist (eOfSmDoesNotExist)" << std::endl;
		reply->set_type(XMPT_ERROR);

	} catch(ePmInvalidPort& e) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] detaching port:" << portname
				<< " from dpid:" << (unsigned long long)dpid << " failed, port does not exist (ePmInvalidPort)" << std::endl;
		reply->set_type(XMPT_ERROR);

	} catch(ePmPortNotAttachedError& e) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] detaching port:" << portname
				<< " from dpid:" << (unsigned long long)dpid << " failed, port does not exist (ePmPortNotAttachedError)" << std::endl;
		reply->set_type(XMPT_ERROR);

	} catch(ePmUnknownError& e) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] detaching port:" << portname
				<< " from dpid:" << (unsigned long long)dpid << " failed, port does not exist (ePmUnknownError)" << std::endl;
		reply->set_type(XMPT_ERROR);

	} catch(eOfSmGeneralError& e) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] detaching port:" << portname
				<< " from dpid:" << (unsigned long long)dpid << " failed (eOfSmGeneralError)" << std::endl;
		reply->set_type(XMPT_ERROR);

	} catch (...) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] detaching port:" << portname
				<< " from dpid:" << (unsigned long long)dpid << " failed." << std::endl;
		reply->set_type(XMPT_ERROR);
	}
	socket.send(reply);
}


void
xmp::handle_port_enable(csocket& socket, cxmpmsg& msg)
{
	cxmpmsg *reply = new cxmpmsg(XMP_VERSION, XMPT_REPLY);
	reply->set_xid(msg.get_xid());

	std::string portname;

	try {
		if (not msg.get_xmpies().has_ie_portname()) {
			LOGGING_ERROR << "[xdpd][plugin][xmp] rcvd xmp Port-Bring-Up request without -PORTNAME- IE, dropping message." << std::endl;
			return;
		}

		portname = msg.get_xmpies().get_ie_portname().get_name();

		port_manager::bring_up(portname);
		LOGGING_ERROR << "[xdpd][plugin][xmp] brought port:" << portname <<" up"<< std::endl;

	} catch(ePmInvalidPort& e) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] bringing port:" << portname << " up failed (ePmInvalidPort)" << std::endl;
		reply->set_type(XMPT_ERROR);

	} catch(ePmUnknownError& e) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] bringing port:" << portname << " up failed (ePmUnknownError)" << std::endl;
		reply->set_type(XMPT_ERROR);

	} catch(eOfSmGeneralError& e) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] bringing port:" << portname << " up failed (eOfSmGeneralError)" << std::endl;
		reply->set_type(XMPT_ERROR);

	} catch (...) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] bringing port:" << portname << " up failed" << std::endl;
		reply->set_type(XMPT_ERROR);
	}
	socket.send(reply);
}


void
xmp::handle_port_disable(csocket& socket, cxmpmsg& msg)
{
	cxmpmsg *reply = new cxmpmsg(XMP_VERSION, XMPT_REPLY);
	reply->set_xid(msg.get_xid());

	std::string portname;

	try {
		if (not msg.get_xmpies().has_ie_portname()) {
			LOGGING_ERROR << "[xdpd][plugin][xmp] rcvd xmp Port-Bring-Down request without -PORTNAME- IE, dropping message." << std::endl;
			return;
		}

		portname = msg.get_xmpies().get_ie_portname().get_name();

		port_manager::bring_down(portname);
		LOGGING_ERROR << "[xdpd][plugin][xmp] brought port:" << portname <<" down"<< std::endl;

	} catch(ePmInvalidPort& e) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] bringing port:" << portname << " down failed (ePmInvalidPort)" << std::endl;
		reply->set_type(XMPT_ERROR);

	} catch(ePmUnknownError& e) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] bringing port:" << portname << " down failed (ePmUnknownError)" << std::endl;
		reply->set_type(XMPT_ERROR);

	} catch(eOfSmGeneralError& e) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] bringing port:" << portname << " down failed (eOfSmGeneralError)" << std::endl;
		reply->set_type(XMPT_ERROR);

	} catch (...) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] bringing port:" << portname << " down failed" << std::endl;
		reply->set_type(XMPT_ERROR);
	}

	socket.send(reply);
}


void
xmp::handle_port_list(csocket& socket, cxmpmsg& msg)
{
	LOGGING_TRACE<< "[xdpd][plugin][xmp] " << __PRETTY_FUNCTION__  << ": socket=" << socket << std::endl;

	bool query_all_dp = true;
	uint64_t dpid = 0;

	if (msg.get_xmpies().has_ie_dpid()) {
		query_all_dp = false;
		dpid = msg.get_xmpies().get_ie_dpid().get_dpid();
		LOGGING_DEBUG << "[xdpd][plugin][xmp] only ports of dpid=" << dpid << std::endl;
	}

	cxmpmsg *reply = new cxmpmsg(XMP_VERSION, XMPT_REPLY);
	reply->set_xid(msg.get_xid());

	// get all ports
	std::set<std::string> all_ports = port_manager::list_available_port_names();

	for (std::set<std::string>::const_iterator iter = all_ports.begin(); iter != all_ports.end(); ++iter) {

		if (not query_all_dp) {
			LOGGING_TRACE << "[xdpd][plugin][xmp] check port " << *iter << std::endl;
			port_snapshot snapshot;
			try {
				port_manager::get_port_info((*iter), snapshot);
			} catch(ePmInvalidPort &e) {
				// skip if port was removed in this short time
				LOGGING_ERROR << "[xdpd][plugin][xmp] failed to retrieve snapshot of port" << *iter << std::endl;
				continue;
			}

			// only ports attached to dpid?
			if (not query_all_dp && snapshot.attached_sw_dpid != dpid) {
				LOGGING_DEBUG << "[xdpd][plugin][xmp] skip port " << *iter << std::endl;
				continue;
			}
		}

		reply->get_xmpies().set_ie_multipart().push_back(new cxmpie_name(XMPIET_PORTNAME, *iter));
	}

	LOGGING_DEBUG << "[xdpd][plugin][xmp] sending: " << reply;

	reply->resize(reply->length());
	reply->pack(reply->somem(), reply->memlen());
	socket.send(reply);
}

void
xmp::handle_port_info(csocket& socket, cxmpmsg& msg)
{
	LOGGING_TRACE<< "[xdpd][plugin][xmp] " << __PRETTY_FUNCTION__  << ": socket=" << socket << std::endl;

	cxmpmsg *reply = new cxmpmsg(XMP_VERSION, XMPT_REPLY);
	reply->set_xid(msg.get_xid());

	// get all ports
	std::set<std::string> all_ports = port_manager::list_available_port_names();

	for (std::set<std::string>::const_iterator iter = all_ports.begin(); iter != all_ports.end(); ++iter) {
		LOGGING_TRACE << "[xdpd][plugin][xmp] check port " << *iter << std::endl;
		port_snapshot snapshot;
		try {
			port_manager::get_port_info((*iter), snapshot);
		} catch(ePmInvalidPort &e) {
			// skip if port was removed in this short time
			LOGGING_ERROR << "[xdpd][plugin][xmp] failed to retrieve snapshot of port" << *iter << std::endl;
			continue;
		}


		reply->get_xmpies().set_ie_multipart().push_back(new cxmpie_portinfo(snapshot));
	}

	LOGGING_DEBUG << "[xdpd][plugin][xmp] sending: " << reply;

	reply->resize(reply->length());
	reply->pack(reply->somem(), reply->memlen());
	socket.send(reply);
}

void
xmp::handle_lsi_list(rofl::csocket& socket, cxmpmsg& msg)
{
	LOGGING_TRACE<< "[xdpd][plugin][xmp] " << __PRETTY_FUNCTION__  << ": socket=" << socket << std::endl;

	cxmpmsg *reply = new cxmpmsg(XMP_VERSION, XMPT_REPLY);
	reply->set_xid(msg.get_xid());

	// get all ports
	std::list<std::string> all_lsi = switch_manager::list_sw_names();

	for (std::list<std::string>::const_iterator iter = all_lsi.begin(); iter != all_lsi.end(); ++iter) {
		LOGGING_TRACE << "[xdpd][plugin][xmp] got lsi " << *iter << std::endl;
		reply->get_xmpies().set_ie_multipart().push_back(new cxmpie_name(XMPIET_LSINAME, *iter));
	}

	LOGGING_DEBUG << "[xdpd][plugin][xmp] sending: " << reply;

	reply->resize(reply->length());
	reply->pack(reply->somem(), reply->memlen());
	socket.send(reply);
}

void
xmp::handle_lsi_info(rofl::csocket& socket, cxmpmsg& msg)
{
	LOGGING_TRACE<< "[xdpd][plugin][xmp] " << __PRETTY_FUNCTION__  << ": socket=" << socket << std::endl;

	cxmpmsg *reply = new cxmpmsg(XMP_VERSION, XMPT_REPLY);
	reply->set_xid(msg.get_xid());

	// get all ports
	std::list<std::string> all_lsi = switch_manager::list_sw_names();

	for (std::list<std::string>::const_iterator iter = all_lsi.begin(); iter != all_lsi.end(); ++iter) {

		LOGGING_TRACE << "[xdpd][plugin][xmp] got lsi " << *iter << std::endl;

		xdpd::openflow_switch_snapshot snapshot;
		try {
			switch_manager::get_switch_info(switch_manager::get_switch_dpid((*iter)), snapshot);
		} catch (eOfSmDoesNotExist &e) {
			// skip if lsi was removed in this short time
			LOGGING_ERROR << "[xdpd][plugin][xmp] failed to retrieve snapshot of lsi " << *iter << std::endl;
			continue;
		}
		reply->get_xmpies().set_ie_multipart().push_back(new cxmpie_lsiinfo(snapshot));
	}

	LOGGING_DEBUG << "[xdpd][plugin][xmp] sending: " << reply;

	reply->resize(reply->length());
	reply->pack(reply->somem(), reply->memlen());
	socket.send(reply);
}

int
xmp::controller_connect(uint64_t dpid, std::deque<cxmpie*>::const_iterator iter, std::deque<cxmpie*>::const_iterator end)
{
	for (; iter != end; ++iter) {

		cxmpie_controller* controller = dynamic_cast<cxmpie_controller*>(*iter);
		assert(controller);

		enum rofl::csocket::socket_type_t socket_type = csocket::SOCKET_TYPE_PLAIN;
		if (0 == controller->get_proto().compare(std::string("tcp"))) {
			// keep socket type plain
#ifdef ROFL_HAVE_OPENSSL
		} else if (0 == controller->get_proto().compare(std::string("tls"))) {
			socket_type = csocket::SOCKET_TYPE_OPENSSL;
#endif
		} else {

			LOGGING_ERROR << "[xdpd][plugin][xmp] rcvd xmp Lsi-Create request without -CONTROLLER- IE, dropping message." << std::endl;

			return -1;
		}

		cparams socket_params(csocket::get_default_params(socket_type));

		if (AF_INET == controller->get_ip_domain()) {
			socket_params.set_param(csocket::PARAM_KEY_DOMAIN) = std::string("inet");
			caddress tmp(controller->get_ip_address());
			socket_params.set_param(csocket::PARAM_KEY_REMOTE_HOSTNAME) = static_cast<const caddress_in4&>(tmp).str();
		} else if (AF_INET6 == controller->get_ip_domain()) {
			socket_params.set_param(csocket::PARAM_KEY_DOMAIN) = std::string("inet6");
			caddress tmp(controller->get_ip_address());
			socket_params.set_param(csocket::PARAM_KEY_REMOTE_HOSTNAME) = static_cast<const caddress_in6&>(tmp).str();
		} else {
			assert(0);
		}

		// todo move to c++11 and use to_string
		socket_params.set_param(csocket::PARAM_KEY_REMOTE_PORT) = dynamic_cast< std::ostringstream & >( ( std::ostringstream() << std::dec << controller->get_port() ) ).str();

		switch_manager::rpc_connect_to_ctl(dpid, socket_type, socket_params);
	}

	return 0;
}

void
xmp::handle_lsi_create(rofl::csocket& socket, cxmpmsg& msg)
{
	LOGGING_TRACE<< "[xdpd][plugin][xmp] " << __PRETTY_FUNCTION__  << ": socket=" << socket << std::endl;

	cxmpmsg *reply = new cxmpmsg(XMP_VERSION, XMPT_REPLY);
	reply->set_xid(msg.get_xid());

	if (not msg.get_xmpies().has_ie_dpid()) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] rcvd xmp Lsi-Create request without -DPID- IE, dropping message." << std::endl;
		reply->set_type(XMPT_ERROR);
	} else if (not msg.get_xmpies().has_ie_lsiname()) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] rcvd xmp Lsi-Create request without -LSINAME- IE, dropping message." << std::endl;
		reply->set_type(XMPT_ERROR);

	} else if (not msg.get_xmpies().has_ie_multipart() ||
			not msg.get_xmpies().get_ie_multipart().get_ies().size() ||
			XMPIET_CONTROLLER != msg.get_xmpies().get_ie_multipart().get_ies().front()->get_type() ) {

		LOGGING_ERROR << "[xdpd][plugin][xmp] rcvd xmp Lsi-Create request without -CONTROLLER- IE, dropping message." << std::endl;
		reply->set_type(XMPT_ERROR);
	} else {

		of_version_t version = OF_VERSION_13;
		uint64_t dpid = msg.get_xmpies().get_ie_dpid().get_dpid();
		std::string dpname(msg.get_xmpies().get_ie_lsiname().get_name());
		unsigned int num_of_tables = 8;
		int ma_list[OF1X_MAX_FLOWTABLES] = { 0 }; /* todo currently it is only first ma for all tables */
		int reconnect_start_timeout = 5;


		const std::deque<cxmpie*> & ies = msg.get_xmpies().get_ie_multipart().get_ies();

		cxmpie_controller* controller = dynamic_cast<cxmpie_controller*>(ies.front());

		enum rofl::csocket::socket_type_t socket_type = csocket::SOCKET_TYPE_PLAIN;
		if (0 == controller->get_proto().compare(std::string("tcp"))) {
			// keep socket type plain
#ifdef ROFL_HAVE_OPENSSL
		} else if (0 == controller->get_proto().compare(std::string("tls"))) {
			socket_type = csocket::SOCKET_TYPE_OPENSSL;
#endif
		} else {

			LOGGING_ERROR << "[xdpd][plugin][xmp] rcvd xmp Lsi-Create request without -CONTROLLER- IE, dropping message." << std::endl;
			reply->set_type(XMPT_ERROR);

			socket.send(reply);
			return;
		}

		cparams socket_params(csocket::get_default_params(socket_type));

		if (AF_INET == controller->get_ip_domain()) {
			socket_params.set_param(csocket::PARAM_KEY_DOMAIN) = std::string("inet");
			caddress tmp(controller->get_ip_address());
			socket_params.set_param(csocket::PARAM_KEY_REMOTE_HOSTNAME) = static_cast<const caddress_in4&>(tmp).str();
		} else if (AF_INET6 == controller->get_ip_domain()) {
			socket_params.set_param(csocket::PARAM_KEY_DOMAIN) = std::string("inet6");
			caddress tmp(controller->get_ip_address());
			socket_params.set_param(csocket::PARAM_KEY_REMOTE_HOSTNAME) = static_cast<const caddress_in6&>(tmp).str();
		} else {
			assert(0);
		}

		// todo move to c++11 and use to_string
		socket_params.set_param(csocket::PARAM_KEY_REMOTE_PORT) = dynamic_cast< std::ostringstream & >( ( std::ostringstream() << std::dec << controller->get_port() ) ).str();

		try {
			switch_manager::create_switch(version, dpid, dpname, num_of_tables, ma_list, reconnect_start_timeout, socket_type, socket_params);
		} catch (xdpd::eOfSmVersionNotSupported &e) {
			LOGGING_ERROR << "[xdpd][plugin][xmp] caught error eOfSmVersionNotSupported: " << e << std::endl;
			reply->set_type(XMPT_ERROR);
		} catch (eOfSmExists &e) {
			LOGGING_ERROR << "[xdpd][plugin][xmp] caught error eOfSmExists: " << e << std::endl;
			reply->set_type(XMPT_ERROR);
		} catch (eOfSmErrorOnCreation &e) {
			LOGGING_ERROR << "[xdpd][plugin][xmp] caught error eOfSmErrorOnCreation: " << e << std::endl;
			reply->set_type(XMPT_ERROR);
		} catch (...) {
			LOGGING_ERROR << "[xdpd][plugin][xmp] caught unknown error." << std::endl;
			reply->set_type(XMPT_ERROR);
		}

		// attach other controllers
		if (1 < ies.size()) {
			std::deque<cxmpie*>::const_iterator iter = ++ies.begin();
			int rv = controller_connect(dpid, iter, ies.end());
			if (0 != rv) {
				reply->set_type(XMPT_ERROR);
			}
		}
	}

	LOGGING_DEBUG << "[xdpd][plugin][xmp] sending: " << reply;

	socket.send(reply);
}

void
xdpd::mgmt::protocol::xmp::handle_lsi_destroy(rofl::csocket& socket, cxmpmsg& msg)
{
	LOGGING_TRACE << "[xdpd][plugin][xmp] " << __PRETTY_FUNCTION__ << ": socket=" << socket << std::endl;

	cxmpmsg *reply = new cxmpmsg(XMP_VERSION, XMPT_REPLY);
	reply->set_xid(msg.get_xid());

	if (not msg.get_xmpies().has_ie_dpid()) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] rcvd xmp Lsi-Destroy request without -DPID- IE, dropping message." << std::endl;
		reply->set_type(XMPT_ERROR);
	} else {

		uint64_t dpid = msg.get_xmpies().get_ie_dpid().get_dpid();

		try {
			switch_manager::destroy_switch(dpid);
		} catch (xdpd::eOfSmDoesNotExist &e) {
			LOGGING_ERROR << "[xdpd][plugin][xmp] caught error eOfSmDoesNotExist: " << e << std::endl;
			reply->set_type(XMPT_ERROR);
		} catch (xdpd::eOfSmGeneralError &e) {
			LOGGING_ERROR << "[xdpd][plugin][xmp] caught error eOfSmGeneralError: " << e << std::endl;
			reply->set_type(XMPT_ERROR);
		} catch (xdpd::ePmUnknownError &e) {
			LOGGING_ERROR << "[xdpd][plugin][xmp] caught error ePmUnknownError: " << e << std::endl;
			reply->set_type(XMPT_ERROR);
		} catch (...) {
			LOGGING_ERROR << "[xdpd][plugin][xmp] caught unknown error." << std::endl;
			reply->set_type(XMPT_ERROR);
		}
	}

	LOGGING_DEBUG << "[xdpd][plugin][xmp] sending: " << reply;

	socket.send(reply);
}

void
xmp::handle_lsi_connect_to_controller(rofl::csocket& socket, cxmpmsg& msg)
{
	LOGGING_TRACE<< "[xdpd][plugin][xmp] " << __PRETTY_FUNCTION__  << ": socket=" << socket << std::endl;

	cxmpmsg *reply = new cxmpmsg(XMP_VERSION, XMPT_REPLY);
	reply->set_xid(msg.get_xid());

	if (not msg.get_xmpies().has_ie_dpid()) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] rcvd xmp Lsi-Connect-To-Controller request without -DPID- IE, dropping message." << std::endl;
		reply->set_type(XMPT_ERROR);

	} else if (not msg.get_xmpies().has_ie_multipart() ||
			not msg.get_xmpies().get_ie_multipart().get_ies().size() ||
			XMPIET_CONTROLLER != msg.get_xmpies().get_ie_multipart().get_ies().front()->get_type() ) {

		LOGGING_ERROR << "[xdpd][plugin][xmp] rcvd xmp Lsi-Connect-To-Controller request without -CONTROLLER- IE, dropping message." << std::endl;
		reply->set_type(XMPT_ERROR);
	} else {

		try {
			uint64_t dpid = msg.get_xmpies().get_ie_dpid().get_dpid();
			const std::deque<cxmpie*> & ies = msg.get_xmpies().get_ie_multipart().get_ies();

			int rv = controller_connect(dpid, ies.begin(), ies.end());
			if (0 != rv) {
				reply->set_type(XMPT_ERROR);
			}

		} catch (xdpd::eOfSmVersionNotSupported &e) {
			LOGGING_ERROR << "[xdpd][plugin][xmp] caught error eOfSmVersionNotSupported: " << e << std::endl;
			reply->set_type(XMPT_ERROR);
		} catch (eOfSmExists &e) {
			LOGGING_ERROR << "[xdpd][plugin][xmp] caught error eOfSmExists: " << e << std::endl;
			reply->set_type(XMPT_ERROR);
		} catch (eOfSmErrorOnCreation &e) {
			LOGGING_ERROR << "[xdpd][plugin][xmp] caught error eOfSmErrorOnCreation: " << e << std::endl;
			reply->set_type(XMPT_ERROR);
		} catch (...) {
			LOGGING_ERROR << "[xdpd][plugin][xmp] caught unknown error." << std::endl;
			reply->set_type(XMPT_ERROR);
		}
	}

	LOGGING_DEBUG << "[xdpd][plugin][xmp] sending: " << reply;

	socket.send(reply);
}

void
xmp::handle_lsi_cross_connect(rofl::csocket& socket, cxmpmsg& msg)
{
	LOGGING_TRACE<< "[xdpd][plugin][xmp] " << __PRETTY_FUNCTION__  << ": socket=" << socket << std::endl;

	cxmpmsg *reply = new cxmpmsg(XMP_VERSION, XMPT_REPLY);
	reply->set_xid(msg.get_xid());

	if (not msg.get_xmpies().has_ie_lsixlsi()) {
		LOGGING_ERROR << "[xdpd][plugin][xmp] rcvd xmp Cross-Connect request without two -LSIXLSI- IE, dropping message." << std::endl;
		reply->set_type(XMPT_ERROR);
	} else {

		try {
			uint64_t dpid1 = msg.get_xmpies().get_ie_lsixlsi().get_dpid1();
			uint32_t portno1 = msg.get_xmpies().get_ie_lsixlsi().get_portno1();
			uint64_t dpid2 = msg.get_xmpies().get_ie_lsixlsi().get_dpid2();
			uint32_t portno2 = msg.get_xmpies().get_ie_lsixlsi().get_portno2();

			LOGGING_INFO << "[xdpd][plugin][xmp] cross-connect dpid1=" << dpid1 << " as portno " << portno1 << " and dpid2=" << dpid2 << " as portno2 " << portno2 << std::endl;

			std::string port1;
			std::string port2;
			port_manager::connect_switches(dpid1, &portno1, port1, dpid2, &portno2, port2);

			LOGGING_INFO << "[xdpd][plugin][xmp] cross-link-ports: port1=" << port1 << " port2=" << port2 << std::endl;

			port_manager::bring_up(port1);
			port_manager::bring_up(port2);

		} catch (xdpd::eOfSmDoesNotExist &e) {
			LOGGING_ERROR << "[xdpd][plugin][xmp] caught error eOfSmDoesNotExist: " << e << std::endl;
			reply->set_type(XMPT_ERROR);
		} catch (ePmUnknownError &e) {
			LOGGING_ERROR << "[xdpd][plugin][xmp] caught error ePmUnknownError: " << e << std::endl;
			reply->set_type(XMPT_ERROR);
		} catch (...) {
			LOGGING_ERROR << "[xdpd][plugin][xmp] caught unknown error." << std::endl;
			reply->set_type(XMPT_ERROR);
		}
	}

	LOGGING_DEBUG << "[xdpd][plugin][xmp] sending: " << reply;

	socket.send(reply);
}
