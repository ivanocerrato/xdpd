/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IOPORT_VLINK_H
#define IOPORT_VLINK_H 1

#include <unistd.h>
#include <rofl.h>
#include <rofl/datapath/pipeline/common/datapacket.h>
#include <rofl/datapath/pipeline/switch_port.h>
#include "../ioport.h"
#include "../../datapacketx86.h"


/**
* @file ioport_vlink.h
* @author Marc Sune<marc.sune (at) bisdn.de>
*
* @brief Contains the implementation of a virtual link among two LSIs
*/

namespace xdpd {
namespace gnu_linux {

/**
* @brief Virtual port used in a virtual link, conceptually similar to
* a veth in GNU/Linux.
*
* ioport_vlink ports shall always be created in pairs and connected each other.
*/
class ioport_vlink : public ioport{

public:
	//ioport_vlink
	ioport_vlink(switch_port_t* of_ps, unsigned int num_queues= IO_IFACE_NUM_QUEUES);
	virtual ~ioport_vlink();

	/**
	* Set other vlink edge (ioport)
	*/
	virtual void set_connected_port(ioport_vlink* connected_port);

	//Write up to num_of_buckets in the iface
	virtual unsigned int write(unsigned int q_id, unsigned int num_of_buckets);

	//Get read&write fds. Return -1 if do not exist
	inline virtual int get_read_fd(void){return rx_notify_pipe[READ];};
	inline virtual int get_write_fd(void){return tx_notify_pipe[READ];};

	virtual rofl_result_t down();

	virtual rofl_result_t up();

	/**
	* Emulate transmission of the packet
	*/
	rofl_result_t tx_pkt(datapacket_t* pkt);

	/**
	* Reference to the other edge (connected ioport)
	*/
	ioport_vlink* connected_port;

protected:
	//fds
	int rx_notify_pipe[2];
	int tx_notify_pipe[2];
	int deferred_drain_rx;
	int deferred_drain_tx;

	//Used to drain the pipe
	char draining_buffer[IO_IFACE_RING_SLOTS];

	//Pipe extremes
	static const unsigned int READ=0;
	static const unsigned int WRITE=1;

	static const unsigned int MIN_PKT_LEN=14;

	void empty_pipe(int* pipe, int* deferred_drain);

	//Non-blocking read and enqueue
	virtual datapacket_t* read__(void);
	virtual void enqueue_packet__(datapacket_t* pkt, unsigned int q_id);
};

}// namespace xdpd::gnu_linux
}// namespace xdpd


#endif /* IOPORT_VLINK_H_ */
