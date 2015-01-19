/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IOPORT_H
#define IOPORT_H

#include <string>
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>

#include <rofl.h>
#include <rofl/datapath/pipeline/common/datapacket.h>
#include <rofl/datapath/pipeline/switch_port.h>
#include "../../config.h"
#include "../../util/circular_queue.h"

/**
* @file ioport.h
* @author Marc Sune<marc.sune (at) bisdn.de>
*
* @brief Abstract class representing a network interface (port)
*
*/

namespace xdpd {
namespace gnu_linux {


#define ETHER_MAC_LEN 6

/**
* @brief Abstract class representing a network interface (port)
*
* @ingroup driver_gnu_linux_io_ports
*/
class ioport{

public:
	/**
	* @brief Constructor for the ioport
	* Constructs the ioport. The state of the port is explicitely undefined
	* (usually down) unless up/down is called AFTER the constructor.
	*/
	ioport(switch_port_t* of_ps, unsigned int q_num=IO_IFACE_NUM_QUEUES);

	/**
	* Destructor
	*/
	virtual ~ioport(void);

	/**
	* Retrieve the number of queues this port supports. 0 lowest priority.
	* This method is thread safe and shall be invoked, generally speaking,
	* by the threads in charge of processing data packets. This method shall
	* NOT be called by an I/O thread
	*/
	inline unsigned int get_num_of_queues(void){ return num_of_queues; }

	/**
	* Retrieve the size (in number of slots) of a queue
	*/
	inline unsigned int get_queue_size(unsigned int id){
		if(id < num_of_queues)
			return output_queues[id]->slots;

		return 0;
	}

	//Non-blocking read and write
	/**
	* @brief Read(RX) one (1) packet if available, return NULL if no packet
	* can be read immediately. The packet MUST be already classified.
	*
	* This method must be NON-BLOCKING
	*/
	virtual datapacket_t* read(void)=0;

	/**
	* Enque packet for transmission(this must be a non blocking call).
	*/
#ifdef COMPILE_IPV4_FRAG_FILTER_SUPPORT
	void enqueue_packet(datapacket_t* pkt, unsigned int q_id);
#else
	inline void enqueue_packet(datapacket_t* pkt, unsigned int q_id){
		enqueue_packet__(pkt, q_id);
	};
#endif

	/**
	* @brief Write in the wire up to up_to_buckets number of packets from queue q_id
	*
	* The packets shall be read from the output_queues[q_id]. If no more packets
	* are currently present on the queue, the method shall immediately return.
	*
	* This method must be NON-BLOCKING
	*
	* @return up_to_buckets - (number of buckets used)
	*/
	virtual unsigned int write(unsigned int q_id, unsigned int up_to_buckets)=0;

	//Get read&write fds. Return -1 if do not exist
	virtual int get_read_fd(void)=0;
	virtual int get_write_fd(void)=0;

#if 0
	//Get buffer status; generally used to create "smart" schedulers. TODO: evaluate if they should be
	//non-virtual (inline+virtual does not make a lot of sense here), and evaluate if they are necessary
	//at all.
	virtual inline circular_queue_state_t get_input_queue_state(void){
		return input_queue.get_queue_state();
	};
	virtual inline circular_queue_state_t get_output_queue_state(unsigned int q_id=0){
		if(q_id<num_of_queues)
			return output_queues[q_id].get_queue_state();
		else{
			assert(0);
			return RB_BUFFER_AVAILABLE;
		}
	};
#endif

	/**
	 * Sets the port administratively up. This MUST change the of_port_state appropiately
	 */
	virtual rofl_result_t up(void)=0;

	/**
	 * Sets the port administratively down. This MUST change the of_port_state appropiately
	 */
	virtual rofl_result_t down(void)=0;

	/**
	 * Sets the port receiving behaviour. This MUST change the of_port_state appropiately
	 * Inherited classes may override this method if they have specific things to do.
	 */
	virtual rofl_result_t set_drop_received_config(bool drop_received);

	/**
	 * Sets the port flood output behaviour. This MUST change the of_port_state appropiately
	 * Inherited classes may override this method if they have specific things to do.
	 */
	virtual rofl_result_t set_no_flood_config(bool no_flood);

	/**
	 * Sets the port output behaviour. This MUST change the of_port_state appropiately
	 * Inherited classes may override this method if they have specific things to do.
	 */
	virtual rofl_result_t set_forward_config(bool forward_packets);

	/**
	 * Sets the port Openflow specific behaviour for non matching packets (PACKET_IN). This MUST change the of_port_state appropiately
	 * Inherited classes may override this method if they have specific things to do.
	 */
	virtual rofl_result_t set_generate_packet_in_config(bool generate_pkt_in);

	/**
	 * Sets the port advertised features. This MUST change the of_port_state appropiately
	 * Inherited classes may override this method if they have specific things to do.
	 */
	virtual rofl_result_t set_advertise_config(uint32_t advertised);


	/**
	* Check if output queue q_id has packets
	*/
	inline bool output_queue_has_packets(unsigned int q_id){
		return output_queues[q_id]->is_empty() == false;
	}

	//Port state (rofl-pipeline port state reference)
	switch_port_t* of_port_state;

	inline void set_link_state(bool up){

		if(!up)
			of_port_state->state = (of_port_state->state | (PORT_STATE_LINK_DOWN));
		else
			of_port_state->state = (of_port_state->state & ~(PORT_STATE_LINK_DOWN));

	}

	static const unsigned int MAX_OUTPUT_QUEUES=IO_IFACE_NUM_QUEUES; /*!< Constant max output queues */
	unsigned int port_group;
	pthread_rwlock_t rwlock; //Serialize management actions

protected:

	/**
	* Wipes output and input queues
	*/
	virtual void drain_queues(void);

	//Output QoS queues
	unsigned int num_of_queues;

	//Max packet size
	unsigned int mps;

	//No MPS limit port
	static const unsigned int NO_MPS = 0xFFFFFFFF;

	//MAC address
	uint8_t mac[ETHER_MAC_LEN];

	/**
	* @brief Output (TX) queues (num_of_queues)
	*
	* The output queues is an array of queues (output_queues[num_of_queues])
	* for QoS purposes (set-queue). output_queues[0] is always the queue with
	* least priority (best effort)
	*/
	circular_queue<datapacket_t>* output_queues[IO_IFACE_NUM_QUEUES];

	/**
	* Input queue (intermediate-buffering).
	*
	* This queue might not always be used by the ioports, if they don't need a intermediate
	* buffering of packets. If it is not used, the port shall attempt to enqueue packets
	* to the appropiate LS processing queue.
	*
	*/
	circular_queue<datapacket_t>* input_queue;

	//Cache of filter status
	bool sw_ipv4_frag_filter_status;
	bool sw_ipv4_reas_filter_status;

	/**
	* Enque packet for transmission(this must be a non blocking call).
	* Function cannot assume that q_id is a valid id, so it MUST do the appropiate
	* checks before attempting to enqueue the packet.
	*/
	virtual void enqueue_packet__(datapacket_t* pkt, unsigned int q_id)=0;
};

}// namespace xdpd::gnu_linux
}// namespace xdpd

#endif /* IOPORT_H_ */
