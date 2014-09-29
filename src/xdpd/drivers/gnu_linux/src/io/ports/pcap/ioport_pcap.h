/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IOPORT_PCAP_H
#define IOPORT_PCAP_H 


#include <linux/sockios.h>
 
#include <string>

#include <rofl.h>
#include <rofl/datapath/pipeline/common/datapacket.h>
#include <rofl/datapath/pipeline/switch_port.h>
//#include <rofl/common/cmacaddr.h>

#include "../ioport.h"
#include "../../datapacketx86.h"

#include <pcap.h>

 namespace xdpd {
 namespace gnu_linux {

/**
* @file ioport_pcap.h
* @author Marc Sune<marc.sune (at) bisdn.de>
*
* @brief GNU/Linux interface access via pcap
*/

#define PORT_ETHER_LENGTH 18
#define PORT_DEFAULT_PKT_SIZE 1518

class ioport_pcap : public ioport{


public:
	//ioport_pcap
	ioport_pcap(switch_port_t* of_ps, unsigned int num_queues = IO_IFACE_NUM_QUEUES);

	virtual
	~ioport_pcap();

	//Enque packet for transmission(blocking)
	virtual void enqueue_packet(datapacket_t* pkt, unsigned int q_id);


	//Non-blocking read and write
	virtual datapacket_t* read(void);

	virtual unsigned int write(unsigned int q_id, unsigned int num_of_buckets);

	// Get read fds. Return -1 if do not exist
	inline virtual int
	get_read_fd(void){
		
		if (descr == NULL)
			return -1;

		return pcap_get_selectable_fd(descr);
		
	};

	// Get write fds. Return -1 if do not exist
	inline virtual int get_write_fd(void){
		//FIXME
		return -1;
	};

	unsigned int get_port_no() {
		if(of_port_state)
			return of_port_state->of_port_num;
		else
			return 0;
	}


	/**
	 * Sets the port administratively up. This MUST change the of_port_state appropiately
	 */
	 virtual rofl_result_t up(void);

	/**
	 * Sets the port administratively down. This MUST change the of_port_state appropiately
	 */
	 virtual rofl_result_t down(void);

	protected:

	private:

		pcap_t* descr;
		//char *dev;

	};

}// namespace xdpd::gnu_linux 
}// namespace xdpd

#endif /* IOPORT_PCAP_H_ */
