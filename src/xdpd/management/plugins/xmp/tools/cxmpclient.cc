/*
 * cxmpclient.cc
 *
 *  Created on: 12.01.2014
 *      Author: andreas
 */

#include "cxmpclient.h"

using namespace xdpd::mgmt::protocol;

cxmpclient::cxmpclient() :
		socket(NULL),
		dest(AF_INET, "127.0.0.1", 8444),
		mem(NULL),
		fragment(NULL),
		msg_bytes_read(0),
		observer(NULL)
{
	socket = rofl::csocket::csocket_factory(rofl::csocket::SOCKET_TYPE_PLAIN, this);

	socket_params = rofl::csocket::get_default_params(rofl::csocket::SOCKET_TYPE_PLAIN);

	socket_params.set_param(rofl::csocket::PARAM_KEY_REMOTE_HOSTNAME).set_string("127.0.0.1");
	socket_params.set_param(rofl::csocket::PARAM_KEY_REMOTE_PORT).set_string("8444");
	socket_params.set_param(rofl::csocket::PARAM_KEY_DOMAIN).set_string("inet");
	socket_params.set_param(rofl::csocket::PARAM_KEY_TYPE).set_string("stream");
	socket_params.set_param(rofl::csocket::PARAM_KEY_PROTOCOL).set_string("tcp");

	socket->connect(socket_params);

}


cxmpclient::~cxmpclient()
{

}

void
cxmpclient::handle_connected(rofl::csocket& socket)
{
	rofl::logging::debug << __PRETTY_FUNCTION__ << std::endl;
	assert(this->socket == &socket);
	assert(mem);

	socket.send(mem, dest);
	register_timer(TIMER_XMPCLNT_EXIT, 1);
}

void
cxmpclient::handle_connect_refused(rofl::csocket& socket)
{
	rofl::logging::debug << __PRETTY_FUNCTION__ << std::endl;
}

void
cxmpclient::handle_connect_failed(rofl::csocket& socket)
{
	rofl::logging::debug << __PRETTY_FUNCTION__ << std::endl;
}

void
cxmpclient::handle_read(rofl::csocket& socket)
{
	rofl::logging::info << "[xmpclient]" << __PRETTY_FUNCTION__ << std::endl;

	try {

		if (NULL == fragment) {
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
				rofl::logging::warn << "[xmpclient] received message with invalid length field, closing socket." << std::endl;
				socket.close();
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
					case XMPT_REPLY:
					{
						handle_reply(msg);
					} break;
					case XMPT_NOTIFICATION:
					case XMPT_REQUEST:
					default:
					{
						rofl::logging::error
								<< "[xmpclient] unexpected message rcvd"
								<< std::endl;
					}
						;
					}

					return;
				}
			}
		}

	} catch (rofl::eSocketRxAgain& e) {

		// more bytes are needed, keep pointer to msg in "fragment"
		rofl::logging::debug << "[xmpclient] read again" << std::endl;

	} catch (rofl::eSysCall& e) {

		rofl::logging::warn << "[xmpclient] closing socket: " << e << std::endl;

		if (fragment) {
			delete fragment; fragment = (rofl::cmemory*)0;
		}

		// close socket, as it seems, we are out of sync
		socket.close();

	} catch (rofl::RoflException& e) {

		rofl::logging::warn << "[xmpclient] dropping invalid message: " << e << std::endl;

		if (fragment) {
			delete fragment; fragment = (rofl::cmemory*)0;
		}

		// close socket, as it seems, we are out of sync
		socket.close();
	}
}

void
cxmpclient::handle_closed(rofl::csocket& socket)
{
	rofl::logging::debug << __PRETTY_FUNCTION__ << std::endl;
}

void
cxmpclient::handle_timeout(
		int opaque, void *data)
{
	rofl::logging::debug << __PRETTY_FUNCTION__ << std::endl;

	switch (opaque) {
	case TIMER_XMPCLNT_EXIT: {
		// exit(0);
	} break;
	default: {

	};
	}
}

void
cxmpclient::handle_reply(cxmpmsg& msg)
{
	rofl::logging::info << "[xdpd][plugin][xmp] rcvd message:" << std::endl << msg;

	if (NULL != observer) {
		observer->notify(msg);
	}
}

void
cxmpclient::register_observer(cxmpobserver *observer)
{
	assert(observer);
	if (NULL != this->observer)
		this->observer = observer;
}

void
cxmpclient::port_list()
{
	cxmpmsg msg(XMP_VERSION, XMPT_REQUEST);
	msg.get_xmpies().add_ie_command().set_command(XMPIEMCT_PORT_LIST);

	std::cerr << "[xmpclient] sending Port-List request:" << std::endl << msg;
	mem = new rofl::cmemory(msg.length());
	msg.pack(mem->somem(), mem->memlen());
}

void
cxmpclient::port_list(uint64_t dpid)
{
	cxmpmsg msg(XMP_VERSION, XMPT_REQUEST);
	msg.get_xmpies().add_ie_command().set_command(XMPIEMCT_PORT_LIST);
	msg.get_xmpies().add_ie_dpid().set_dpid(dpid);

	std::cerr << "[xmpclient] sending Port-Attach request:" << std::endl << msg;
	mem = new rofl::cmemory(msg.length());
	msg.pack(mem->somem(), mem->memlen());
}

void
cxmpclient::port_attach(
		uint64_t dpid, std::string const& portname)
{
	cxmpmsg msg(XMP_VERSION, XMPT_REQUEST);

	msg.get_xmpies().add_ie_command().set_command(XMPIEMCT_PORT_ATTACH);
	msg.get_xmpies().add_ie_portname().set_portname(portname);
	msg.get_xmpies().add_ie_dpid().set_dpid(dpid);

	std::cerr << "[xmpclient] sending Port-Attach request:" << std::endl << msg;

	mem = new rofl::cmemory(msg.length());
	msg.pack(mem->somem(), mem->memlen());
}


void
cxmpclient::port_detach(
		uint64_t dpid, std::string const& portname)
{
	cxmpmsg msg(XMP_VERSION, XMPT_REQUEST);

	msg.get_xmpies().add_ie_command().set_command(XMPIEMCT_PORT_DETACH);
	msg.get_xmpies().add_ie_portname().set_portname(portname);
	msg.get_xmpies().add_ie_dpid().set_dpid(dpid);

	std::cerr << "[xmpclient] sending Port-Detach request:" << std::endl << msg;

	mem = new rofl::cmemory(msg.length());
	msg.pack(mem->somem(), mem->memlen());
}


void
cxmpclient::port_enable(
		std::string const& portname)
{
	cxmpmsg msg(XMP_VERSION, XMPT_REQUEST);

	msg.get_xmpies().add_ie_command().set_command(XMPIEMCT_PORT_ENABLE);
	msg.get_xmpies().add_ie_portname().set_portname(portname);

	std::cerr << "[xmpclient] sending Port-Enable request:" << std::endl << msg;

	mem = new rofl::cmemory(msg.length());
	msg.pack(mem->somem(), mem->memlen());
}


void
cxmpclient::port_disable(
		std::string const& portname)
{
	cxmpmsg msg(XMP_VERSION, XMPT_REQUEST);

	msg.get_xmpies().add_ie_command().set_command(XMPIEMCT_PORT_DISABLE);
	msg.get_xmpies().add_ie_portname().set_portname(portname);

	std::cerr << "[xmpclient] sending Port-Disable request:" << std::endl << msg;

	mem = new rofl::cmemory(msg.length());
	msg.pack(mem->somem(), mem->memlen());
}
