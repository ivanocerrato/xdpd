/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPC_IPV4_H_
#define _CPC_IPV4_H_

/**
* @file cpc_ipv4.h
* @author Victor Alvarez<victor.alvarez (at) bisdn.de>
*
* @brief Structure definitions and inline getters and setters for IPv4
*/

/* IPv4 constants and definitions */
enum ipv4_ip_proto_t {
	IPV4_IP_PROTO = 4,
};

// IPv4 header
typedef struct cpc_ipv4_hdr {
	uint8_t ihlvers;        // IP header length in 32bit words,
							// TODO: check for options and padding
	uint8_t tos;
	uint16_t length;
	uint16_t ident;
	uint8_t offset_flags[2];
	uint8_t ttl;
	uint8_t proto;
	uint16_t checksum;
	uint32_t src;
	uint32_t dst;
	uint8_t data[0];
} __attribute__((packed)) cpc_ipv4_hdr_t;

inline static
void ipv4_calc_checksum(void *hdr){
	int i;
	//initialize();

	size_t datalen = sizeof(cpc_ipv4_hdr_t);

	// force header checksum to 0x0000
	((cpc_ipv4_hdr_t*)hdr)->checksum = 0;

	// pointer on 16bit words
	uint16_t *word16 = (uint16_t*)hdr;
	// number of 16bit words
	int wnum = (datalen / (sizeof(uint16_t)));
	// sum
	uint32_t sum = 0;

	for (i = 0; i < wnum; i++)
	{
		uint32_t tmp = (uint32_t)(word16[i]);
		sum += tmp;
		//fprintf(stderr, "word16[%d]=0x%08x sum()=0x%08x\n", i, tmp, sum);
	}
	//fprintf(stderr, "   sum(1)=0x%x\n", sum);

	uint16_t res16 = (sum & 0x0000ffff) + ((sum & 0xffff0000) >> 16);

	//fprintf(stderr, " res16(1)=0x%x\n", res16);

	((cpc_ipv4_hdr_t*)hdr)->checksum = ~res16;

	//fprintf(stderr, "~res16(1)=0x%x\n", ipv4_hdr->checksum);
};

inline static
bool ipv4_is_fragment(void *hdr){
	return 	((cpc_ipv4_hdr_t*)hdr)->offset_flags[1] |
		(((cpc_ipv4_hdr_t*)hdr)->offset_flags[0]& ~OF1X_2MSBITS_MASK);
};

inline static
void set_ipv4_src(void *hdr, uint32_t src){
	((cpc_ipv4_hdr_t*)hdr)->src = src;
};

inline static
uint32_t* get_ipv4_src(void *hdr){
	return &((cpc_ipv4_hdr_t*)hdr)->src;
};

inline static
void set_ipv4_dst(void *hdr, uint32_t dst){
	((cpc_ipv4_hdr_t*)hdr)->dst = dst;
};

inline static
uint32_t* get_ipv4_dst(void *hdr){
	return &((cpc_ipv4_hdr_t*)hdr)->dst;
};

inline static
void set_ipv4_dscp(void *hdr, uint8_t dscp){
	((cpc_ipv4_hdr_t*)hdr)->tos = (dscp & OF1X_6MSBITS_MASK) | (((cpc_ipv4_hdr_t*)hdr)->tos & ~OF1X_6MSBITS_MASK);
};

inline static
uint8_t get_ipv4_dscp(void *hdr){
	//NOTE we need to pass this one as value because its alineation is differrent from the one in ipv6
	//return (((cpc_ipv4_hdr_t*)hdr)->tos & OF1X_6MSBITS_MASK);
	return ((cpc_ipv4_hdr_t*)hdr)->tos;
};

inline static
void set_ipv4_ecn(void *hdr, uint8_t ecn){
	((cpc_ipv4_hdr_t*)hdr)->tos = (((cpc_ipv4_hdr_t*)hdr)->tos & ~OF1X_2LSBITS_MASK) | (ecn & OF1X_2LSBITS_MASK);
};

inline static
uint8_t get_ipv4_ecn(void *hdr){
	//NOTE we need to pass this one as value because its alineation is differrent from the one in ipv6
	//return (((cpc_ipv4_hdr_t*)hdr)->tos & OF1X_2LSBITS_MASK);
	return ((cpc_ipv4_hdr_t*)hdr)->tos;
};

inline static
void set_ipv4_ttl(void *hdr, uint8_t ttl){
	((cpc_ipv4_hdr_t*)hdr)->ttl = ttl;
};

inline static
uint8_t* get_ipv4_ttl(void *hdr){
	return &((cpc_ipv4_hdr_t*)hdr)->ttl;
};

inline static
void dec_ipv4_ttl(void *hdr){
	((cpc_ipv4_hdr_t*)hdr)->ttl--;
};

inline static
void set_ipv4_proto(void *hdr, uint8_t proto){
	((cpc_ipv4_hdr_t*)hdr)->proto = proto;
};

inline static
uint8_t* get_ipv4_proto(void *hdr){
	return &((cpc_ipv4_hdr_t*)hdr)->proto;
};

inline static
void set_ipv4_ihl(void *hdr, uint8_t ihl){
	((cpc_ipv4_hdr_t*)hdr)->ihlvers = (((cpc_ipv4_hdr_t*)hdr)->ihlvers & ~OF1X_4LSBITS_MASK) + (ihl & OF1X_4LSBITS_MASK);
};

inline static
uint8_t* get_ipv4_ihl(void *hdr){
	return &((cpc_ipv4_hdr_t*)hdr)->ihlvers;
};

inline static
void set_ipv4_version(void *hdr, uint8_t version){
	((cpc_ipv4_hdr_t*)hdr)->ihlvers = (((cpc_ipv4_hdr_t*)hdr)->ihlvers & ~OF1X_4MSBITS_MASK) | (version & OF1X_4MSBITS_MASK);
};

inline static
uint8_t* get_ipv4_version(void *hdr){
	return &((cpc_ipv4_hdr_t*)hdr)->ihlvers;
};

inline static
void set_ipv4_length(void *hdr, uint16_t length){
	((cpc_ipv4_hdr_t*)hdr)->length = length;
};

inline static
uint16_t* get_ipv4_length(void *hdr){
	return &((cpc_ipv4_hdr_t*)hdr)->length;
};

inline static
void set_ipv4_ident(void *hdr, uint16_t ident){
	((cpc_ipv4_hdr_t*)hdr)->ident = ident;
};

inline static
uint16_t* get_ipv4_ident(void *hdr){
	return &((cpc_ipv4_hdr_t*)hdr)->ident;
};

inline static
void set_ipv4_DF_bit(void *hdr){
	((cpc_ipv4_hdr_t*)hdr)->offset_flags[0] =  ((cpc_ipv4_hdr_t*)hdr)->offset_flags[0] | OF1X_BIT6_MASK;
};

inline static
bool has_ipv4_DF_bit_set(void *hdr){
	return (bool)(((cpc_ipv4_hdr_t*)hdr)->offset_flags[0] & OF1X_BIT6_MASK);
};

inline static
void clear_ipv4_DF_bit(void *hdr){
	((cpc_ipv4_hdr_t*)hdr)->offset_flags[0] = ((cpc_ipv4_hdr_t*)hdr)->offset_flags[0] & ~OF1X_BIT6_MASK;
};

inline static
void set_ipv4_MF_bit(void *hdr){
	((cpc_ipv4_hdr_t*)hdr)->offset_flags[0] = ((cpc_ipv4_hdr_t*)hdr)->offset_flags[0] | OF1X_BIT5_MASK;
};

inline static
bool has_ipv4_MF_bit_set(void *hdr){
	return (bool)(((cpc_ipv4_hdr_t*)hdr)->offset_flags[0] & OF1X_BIT5_MASK);
};

inline static
void clear_ipv4_MF_bit(void *hdr){
	((cpc_ipv4_hdr_t*)hdr)->offset_flags[0] = ((cpc_ipv4_hdr_t*)hdr)->offset_flags[0] & ~OF1X_BIT5_MASK;
};

#endif //_CPC_IPV4_H_
