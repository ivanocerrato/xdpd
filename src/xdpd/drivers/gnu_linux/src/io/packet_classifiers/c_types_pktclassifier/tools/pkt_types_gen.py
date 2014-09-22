from collections import OrderedDict
import re

#
# Protocols
#
#protocols = OrderedDict = 
protocols = OrderedDict(
	(
	("ETHERNET", 14), 
	("8023", 22), 
	("MPLS", 4), 
	("VLAN", 4), 
	("ISID", 4), 
	("PPPOE", 6), 
	("PPP", 2), 
	("ARPV4", 28), 
	("IPV4", 20), 
	("ICMPV4", 4), 
	("IPV6", 40), 
	("ICMPV6", 4), 
	("ICMPV6_RTR_SOL", 4),
	("ICMPV6_RTR_SOL_OPTS_LLADR_SRC", 8),
	("ICMPV6_RTR_ADV", 12), 
	("ICMPV6_RTR_ADV_OPTS_LLADR_SRC", 8),
	("ICMPV6_NEIGH_SOL", 20),
	("ICMPV6_NEIGH_SOL_OPTS_LLADR_SRC", 8),
	("ICMPV6_NEIGH_ADV", 20), 
	("ICMPV6_NEIGH_ADV_OPTS_LLADR_TGT", 8),
	("ICMPV6_REDIRECT", 36),
	("ICMPV6_REDIRECT_OPTS_LLADR_TGT", 8),
	("ICMPV6_OPTS", 20), 
	("ICMPV6_OPTS_LLADR_SRC", 20), 
	("ICMPV6_OPTS_LLADR_TGT", 20), 
	("ICMPV6_OPTS_PREFIX_INFO", 20), 
	("TCP", 32), 
	("UDP", 8), 
	("SCTP", 12),
	("GTPU4", 20),
	("GTPU6", 20)
	)
)

unrolled_protocols = OrderedDict()

#
# Packet types (using a notation similar to scapy)
#
pkt_types = [ 
	"L2", #This is ETHERNET or 8023 or ETHERNET/VLAN or 8023/VLAN or ETHERNET/VLAN/VLAN or 8023/VLAN/VLAN and all the PBB stuff

	#MPLS - no parsing beyond 	
	"L2/MPLS",
	
	#PPPOE/PPP We don't parse beyond that (for the moment)
	"L2/PPPOE",
	"L2/PPPOE/PPP",

	#ARP	
	"L2/ARPV4",
	
	##########
	## IPv4	##
	##########
	"L2/IPV4",
	
	#ICMPs
	"L2/IPV4/ICMPV4",
	
	"L2/IPV4/TCP",
	"L2/IPV4/UDP",
	"L2/IPV4/UDP/GTPU4",
	"L2/IPV4/SCTP",

	##########
	## IPv6	##
	##########
	"L2/IPV6",

	#ICMPs
	"L2/IPV6/ICMPV6",
	"L2/IPV6/ICMPV6/ICMPV6_OPTS_LLADR_SRC",
	"L2/IPV6/ICMPV6/ICMPV6_OPTS_LLADR_TGT",
	"L2/IPV6/ICMPV6/ICMPV6_OPTS_PREFIX_INFO",
	"L2/IPV6/ICMPV6/ICMPV6_RTR_SOL",
	"L2/IPV6/ICMPV6/ICMPV6_RTR_ADV",
	"L2/IPV6/ICMPV6/ICMPV6_NEIGH_SOL",
	"L2/IPV6/ICMPV6/ICMPV6_NEIGH_ADV",
	"L2/IPV6/ICMPV6/ICMPV6_RTR_SOL/ICMPV6_RTR_SOL_OPTS_LLADR_SRC",
	"L2/IPV6/ICMPV6/ICMPV6_RTR_ADV/ICMPV6_RTR_ADV_OPTS_LLADR_SRC",
	"L2/IPV6/ICMPV6/ICMPV6_NEIGH_SOL/ICMPV6_NEIGH_SOL_OPTS_LLADR_SRC",
	"L2/IPV6/ICMPV6/ICMPV6_NEIGH_ADV/ICMPV6_NEIGH_ADV_OPTS_LLADR_TGT",

	"L2/IPV6/TCP",
	"L2/IPV6/UDP",
	"L2/IPV6/UDP/GTPU6",
	"L2/IPV6/SCTP",
]

#
# This is the unrolled version (options, mpls labels...) of the previous type
#
pkt_types_unrolled = [ ]

#Unroll parameters 
ipv4_max_options_words = 15 #15 Inclusive 15x4bytes=60bytes 
mpls_max_depth=32 #Maximum
vlan_max=2 #Maximum

##
## Helper Functions
##

def filter_pkt_type(pkt_type):
	#MPLS
	if "MPLS" in pkt_type:
		return pkt_type.split("_nlabels_")[0]
	
	if "IPV4" in pkt_type:
		return pkt_type.split("_noptions_")[0]
	
	return pkt_type

def unroll_pkt_types():
	for type__ in pkt_types:

		#Unrolled
		unrolled_types=[]

		#Add without optional
		tmp = type__.replace("L2","ETHERNET")
		unrolled_types.append(tmp)
		tmp = type__.replace("L2","ETHERNET/ISID/ETHERNET")
		unrolled_types.append(tmp)
		tmp = type__.replace("L2","ETHERNET/VLAN/ISID/ETHERNET")
		unrolled_types.append(tmp)
		#Add 802.3
		tmp = type__.replace("L2","8023")
		unrolled_types.append(tmp)
		
		#Add ETHERNET+VLAN 
		tmp = type__.replace("L2","ETHERNET/VLAN")
		unrolled_types.append(tmp)
		tmp = type__.replace("L2","ETHERNET/ISID/ETHERNET/VLAN")
		unrolled_types.append(tmp)
		tmp = type__.replace("L2","ETHERNET/VLAN/ISID/ETHERNET/VLAN")
		unrolled_types.append(tmp)
		#Add 802.3+VLAN
		tmp = type__.replace("L2","8023/VLAN")
		unrolled_types.append(tmp)
		
		#Add ETHERNET+VLAN+VLAN 
		tmp = type__.replace("L2","ETHERNET/VLAN/VLAN")
		unrolled_types.append(tmp)
		tmp = type__.replace("L2","ETHERNET/ISID/ETHERNET/VLAN/VLAN")
		unrolled_types.append(tmp)
		tmp = type__.replace("L2","ETHERNET/VLAN/ISID/ETHERNET/VLAN/VLAN")
		unrolled_types.append(tmp)

		#Add 802.3+VLAN+VLAN
		tmp = type__.replace("L2","8023/VLAN/VLAN")
		unrolled_types.append(tmp)
		

		#for t in unrolled_types:
		#	print t+"\n"
	
		#Loop over all "unrolled types
		for type_ in unrolled_types:
			if "IPV4" in type_:
				for i in range(0,ipv4_max_options_words+1):
					pkt_types_unrolled.append(type_.replace("IPV4","IPV4_noptions_"+str(i)))
				continue
			if "MPLS" in type_:
				for i in range(1,mpls_max_depth+1):
					pkt_types_unrolled.append(type_.replace("MPLS","MPLS_nlabels_"+str(i)))
				continue
			pkt_types_unrolled.append(type_)


def unroll_protocols():
	# Unroll protocols (only IPv4)
	# We need to transition based on IPv4 options
	
	for proto in protocols:
		if "IPV4" in proto:
			for i in range(0,ipv4_max_options_words+1):
				unrolled_protocols[proto.replace("IPV4","IPV4_noptions_"+str(i))] = 0
			continue
		unrolled_protocols[proto] = 0

def sanitize_pkt_type(pkt_type):
	pkt_type = pkt_type.replace("/","_")
	return pkt_type	

def calc_inner_len_pkt_type(curr_len, pkt_type):
	""" Inner len in the offset of the inner most header for stackable headers (MPLS, VLAN)"""	
	if "MPLS" in pkt_type:
		#Return inner-most
		n_mpls_labels = int(pkt_type.split("_nlabels_")[1])
		curr_len += protocols[filter_pkt_type(pkt_type)]*(n_mpls_labels-1)
	return curr_len

def calc_len_pkt_type(pkt_type):
	""" Calculates the total len including options and other variable length headers depending on the unrolled pkt types"""	
	if "IPV4" in pkt_type:
		n_options = int(pkt_type.split("_noptions_")[1])
		return protocols[filter_pkt_type(pkt_type)] + (n_options*4) #1 option = 4 octets
	
	return protocols[filter_pkt_type(pkt_type)]	
##
## Main functions
##

def license(f):
	f.write("/* This Source Code Form is subject to the terms of the Mozilla Public\n * License, v. 2.0. If a copy of the MPL was not distributed with this\n * file, You can obtain one at http://mozilla.org/MPL/2.0/. */\n\n")

def init_guard(f):
	f.write("#ifndef PKT_TYPES_H\n#define PKT_TYPES_H\n\n")
def end_guard(f):
	f.write("#endif //PKT_TYPES_H\n")

def comments(f):
	f.write("//This file has been autogenerated. DO NOT modify it\n\n")	
def protocols_enum(f):
	f.write("typedef enum protocol_types{\n")
	for proto in protocols:
		f.write("\tPT_PROTO_"+proto+",\n")
		
	f.write("\tPT_PROTO_MAX__\n}protocol_types_t;\n\n")

def packet_types_enum(f):
	
	f.write("typedef enum pkt_types{\n")
	f.write("\tPT_INVALID = -1,\n")
	for type_ in pkt_types_unrolled:
		f.write("\tPT_"+sanitize_pkt_type(type_)+",\n")
		
	f.write("\tPT_MAX__\n}pkt_types_t;\n\n")


def packet_offsets_fwd(f):
	
	f.write("//Matrix of header offsets from the first byte of the buffer.\n")
	f.write("extern const int protocol_offsets_bt[PT_MAX__][PT_PROTO_MAX__];\n\n")


def packet_offsets(f):
	
	f.write("//Matrix of header offsets from the first byte of the buffer.\n")
	f.write("const int protocol_offsets_bt[PT_MAX__][PT_PROTO_MAX__] = {")

	first_type = True 
	
	#Comment to help identifying the protocols
	f.write("\n\t/* {")
	for type_ in protocols:
		f.write(type_+",")
	f.write("}*/ \n")
	
	#Real rows
	for type_ in pkt_types_unrolled:
		
		if not first_type:
			f.write(",")
		first_type = False

		len=0
		row=[]
		for proto in protocols:
			row.append(-1)

		parsed_isid=False	
		for proto in type_.split("/"):
			if row[ protocols.keys().index(filter_pkt_type(proto))] == -1:
				row[ protocols.keys().index(filter_pkt_type(proto))] = len #calc_inner_len_pkt_type(len, proto) 
			#row[ protocols.keys().index(filter_pkt_type(proto))] = calc_inner_len_pkt_type(len, proto) 
			len += calc_len_pkt_type(proto)

		f.write("\n\t/* "+type_+" */ {")

		first_proto = True
		for proto_len in row: 
			if not first_proto:
				f.write(",")
			first_proto = False
			f.write(str(proto_len))

		f.write("}")
		
	f.write("\n};\n\n")
	
def eth_type_offsets_fwd(f):
	
	f.write("//Array of eth_type offsets from the first byte of the buffer.\n")
	f.write("extern const int eth_type_offsets[PT_MAX__];\n\n")


def eth_type_offsets(f):
	
	f.write("//Array of eth_type offsets from the first byte of the buffer.\n")
	f.write("const int eth_type_offsets[PT_MAX__] = {\n")

	first_type = True 
	
	
	#Real rows
	for type_ in pkt_types_unrolled:
	
		#Comment to help identifying the protocols
		f.write("\t/* {"+str(type_)+"} */ ")	
		
		offset=0
		
		#Calculate the ethertype
		if "ISID" in type_:
			if "VLAN/ISID" in type_:
				offset = protocols["ETHERNET"]+protocols["VLAN"]-2 
			else:
				offset = protocols["ETHERNET"]-2
		elif "VLAN/VLAN" in type_:
			if "ETHERNET/VLAN" in type_:
				offset = protocols["ETHERNET"]+(protocols["VLAN"]*2)-2 
			else:
				offset = protocols["8023"]+(protocols["VLAN"]*2)-2 
		elif "VLAN" in type_:
			if "ETHERNET/VLAN" in type_:
				offset = protocols["ETHERNET"]+protocols["VLAN"]-2
			else:
				offset = protocols["8023"]+protocols["VLAN"]-2
		else:
			if "ETHERNET" in type_:
				offset = protocols["ETHERNET"]-2
			else:
				offset = protocols["8023"]-2 
				
		f.write(str(offset)+",\n")
		
	f.write("\n};\n\n")

def parse_transitions_fwd(f):

	##
	## Unrolled protocols enum
	##
	f.write("//Only used in transitions (inside the MACRO )\n")
	f.write("typedef enum __unrolled_protocol_types{\n")
	for proto in unrolled_protocols:
		f.write("\t__UNROLLED_PT_PROTO_"+proto+",\n")
		
	f.write("\t__UNROLLED_PT_PROTO_MAX__\n}__unrolled_protocol_types_t;\n\n")


	##
	## Parse transitions
	##
	f.write("//Matrix of incremental classification steps. The only purpose of this matrix is to simplify parsing code, it is not essentially necessary, although _very_ desirable.\n")
	f.write("extern const int parse_transitions [PT_MAX__][__UNROLLED_PT_PROTO_MAX__];\n\n")


def parse_transitions(f):
	
	##
	## Parse transitions
	##
	f.write("//Matrix of incremental classification steps. The only purpose of this matrix is to simplify parsing code, it is not essentially necessary, although _very_ desirable.\n")
	f.write("const int parse_transitions [PT_MAX__][__UNROLLED_PT_PROTO_MAX__] = {")

	#Comment to help identifying the protocols
	f.write("\n\t/* {")
	for type_ in unrolled_protocols:
		f.write(type_+",")
	f.write("}*/ \n")

	#Real rows
	first_type = True 
	for type_ in pkt_types_unrolled:
		
		if not first_type:
			f.write(",")
		first_type = False

		len=0
		row=[]
		for proto in unrolled_protocols:
			new_type=""
			if "MPLS" in proto:
				#Special cases: MPLS
				if type_.split("MPLS_nlabels_").__len__() == 1:
					new_type=type_+"/"+proto+"_nlabels_1"	
				elif type_.split("MPLS_nlabels_").__len__() == 2:
					n_labels = int(type_.split("MPLS_nlabels_")[1])+1
					new_type=type_.split("MPLS_nlabels_")[0]+"MPLS_nlabels_"+str(n_labels)
				else:
					#No type
					new_type=type_+"/"+proto
			else:
				new_type=type_+"/"+proto

			if new_type in pkt_types_unrolled:
				row.append(new_type)
			else:
				row.append("-1")
			#row.append("PT_"+sanitize_pkt_type(type_))
		f.write("\n\t/* "+type_+" */ {")

		first_proto = True
		for next_proto in row: 
			if not first_proto:
				f.write(",")
			first_proto = False
			if next_proto != "-1":
				f.write("PT_"+sanitize_pkt_type(next_proto))
			else:
				f.write(next_proto)

		f.write("}")
		
	f.write("\n};\n\n")

def push_transitions_fwd(f):
	
	##
	## Push operations
	##
	f.write("//Matrix for pushing protocol headers. \n")
	f.write("extern const int push_transitions [PT_MAX__][__UNROLLED_PT_PROTO_MAX__];\n\n")


def push_transitions(f):
	
	##
	## Push operations
	##
	f.write("//Matrix for pushing protocol headers. \n")
	f.write("const int push_transitions [PT_MAX__][__UNROLLED_PT_PROTO_MAX__] = {")

	#Comment to help identifying the protocols
	f.write("\n\t/* {")
	for type_ in unrolled_protocols:
		f.write(type_+",")
	f.write("}*/ \n")

	#Real rows
	first_type = True 
	for type_ in pkt_types_unrolled:
		
		if not first_type:
			f.write(",")
		first_type = False

		len=0
		row=[]
		for proto in unrolled_protocols:
			new_type=""
			if "ISID" in proto or "ISID" in  type_:
				if "ISID" in proto and "ISID" not in type_: 
					row.append("ETHERNET/ISID/ETHERNET") #type_.replace("ETHERNET","ETHERNET/ISID/ETHERNET"))
				else:
					row.append("-1")
			elif "MPLS" in proto:
				if "MPLS" in type_:
					#If there is already an MPLS add to the end
					n_labels = int(type_.split("MPLS_nlabels_")[1])+1
					if(n_labels < mpls_max_depth):
						new_type=type_.split("MPLS_nlabels_")[0]+"MPLS_nlabels_"+str(n_labels)
					else:
						new_type +="-1"
				else:
					if "VLAN/VLAN" in type_:
						new_type=type_.split("VLAN/VLAN")[0] + "VLAN/VLAN/MPLS_nlabels_1"
					elif "VLAN" in type_:
						new_type=type_.split("VLAN")[0] +"VLAN/MPLS_nlabels_1"
					elif "ETHERNET" in type_:
						new_type=type_.split("ETHERNET")[0] +"ETHERNET/MPLS_nlabels_1"
					elif "8023" in type_:
						new_type=type_.split("8023")[0] +"8023/MPLS_nlabels_1"
					else:
						new_type +="-1"
				row.append(new_type)
			elif "VLAN" in proto:
				if "ISID" in type_:
					if "ISID/VLAN" not in type_:
						new_type=type_.replace("ISID", "ISID/VLAN")
					else:
						new_type ="-1"
				elif "VLAN/VLAN" in type_:
					new_type="-1"
				elif "VLAN" in type_:
					new_type=type_.replace("VLAN", "VLAN/VLAN")
				elif "ETHERNET" in type_:
					new_type=type_.replace("ETHERNET", "ETHERNET/VLAN")
				elif "8023" in type_:
					new_type=type_.replace("8023", "8023/VLAN")
				else:
					new_type ="-1"

				row.append(new_type)
			elif "PPPOE" in proto:
				if "PPPOE" in type_ or "MPLS" in type_:
					new_type +="-1"
				elif "VLAN/VLAN" in type_:
					new_type=type_.replace("VLAN/VLAN", "VLAN/VLAN/PPPOE")
				elif "VLAN" in type_:
					new_type=type_.replace("VLAN", "VLAN/PPPOE")
				elif "ETHERNET" in type_:
					new_type=type_.replace("ETHERNET", "ETHERNET/PPPOE")
				elif "8023" in type_:
					new_type=type_.replace("8023", "8023/PPPOE")
				else:
					new_type +="-1"
					
				#We don't parse beyond PPPOE
				if new_type != "-1":
					new_type=new_type.split("PPPOE")[0]+"PPPOE"
				
				row.append(new_type)
			elif "PPP" in proto:
				if "PPPOE" in type_ and not "PPPOE/PPP" in type_:
					row.append(type_.replace("PPPOE", "PPPOE/PPP"))
				else:
					row.append("-1")
			elif "GTPU" in proto and ("IPV4_noptions_0" in type_ or "IPV6" in type_) and "MPLS" not in type_ and "PPPOE" not in type_ and "GTPU" not in type_ and "VLAN" not in type_:
				new_type = type_.split("IP")[0]+proto # take everything before the IPV4/IPV6 payload				
				if "GTPU4" in proto:
					row.append(new_type.replace("GTPU4", "IPV4_noptions_0/UDP/GTPU4"))
				elif "GTPU6" in proto:
					row.append(new_type.replace("GTPU6", "IPV6/UDP/GTPU6"))
				else:
					row.append("-1")
			else:
				row.append("-1")
		
		f.write("\n\t/* "+type_+" */ {")

		first_proto = True
		for next_proto in row:
			if not first_proto:
				f.write(",")
			first_proto = False
			if next_proto != "-1":
				f.write("PT_"+sanitize_pkt_type(next_proto))
				#if "GTPU" in type_ or "GTPU" in next_proto:
				if "GTPU" not in type_ and "GTPU" in next_proto:
					print "PT_"+sanitize_pkt_type(type_)+" => "+"PT_"+sanitize_pkt_type(next_proto)
			else:
				f.write(next_proto)
		f.write("}")
		
	f.write("\n};\n\n")


def pop_transitions_fwd(f):
	
	##
	## Pop operations
	##
	f.write("//Matrix for popping protocol headers. \n")
	f.write("extern const int pop_transitions [PT_MAX__][__UNROLLED_PT_PROTO_MAX__];\n\n")

def pop_transitions(f):
	
	##
	## Pop operations
	##
	f.write("//Matrix for popping protocol headers. \n")
	f.write("const int pop_transitions [PT_MAX__][__UNROLLED_PT_PROTO_MAX__] = {")

	#Comment to help identifying the protocols
	f.write("\n\t/* {")
	for type_ in unrolled_protocols:
		f.write(type_+",")
	f.write("}*/ \n")

	#Real rows
	first_type = True 
	for type_ in pkt_types_unrolled:
		
		if not first_type:
			f.write(",")
		first_type = False

		len=0
		row=[]
		for proto in unrolled_protocols:
			new_type=""
			if "ISID" in proto or "ISID" in  type_:
				if "ISID" in proto and "ISID" in type_ and "VLAN/ISID" not in type_: 
					row.append(type_.replace("ETHERNET/ISID/", ""))
				else:
					row.append("-1")
			elif "MPLS" in proto:
				if "MPLS" in type_:
					#If there is already an MPLS add to the end
					n_labels = int(type_.split("MPLS_nlabels_")[1])
					if(n_labels > 1):
						new_type=type_.replace("MPLS_nlabels_"+str(n_labels), "MPLS_nlabels_"+str(n_labels-1))
					else:
						new_type=type_.replace("/MPLS_nlabels_1","")
				else:
					new_type +="-1"
				row.append(new_type)
			elif "VLAN" in proto:
				if "/ISID/VLAN" in type_:
					new_type=type_.replace("/ISID/VLAN", "/ISID")
				elif "/VLAN/VLAN" in type_:
					new_type=type_.replace("/VLAN/VLAN", "/VLAN")
				elif "/VLAN" in type_:
					new_type=type_.replace("/VLAN", "")
				else:
					new_type +="-1"

				row.append(new_type)
			elif "PPPOE" in proto:
				if "PPPOE/PPP" in type_:
					new_type +="-1"
				else:
					new_type=type_.replace("/PPPOE", "")
				row.append(new_type)
			elif "PPP" in proto:
				if "/PPPOE/PPP" in type_:
					row.append(type_.replace("/PPPOE/PPP", "/PPPOE"))
				else:
					row.append("-1")
			elif "GTPU" in proto:
				if "GTPU4" in type_:
					row.append(type_.replace("/IPV4_noptions_0/UDP/GTPU4", ""))
				elif "GTPU6" in type_:
					row.append(type_.replace("/IPV6/UDP/GTPU6", ""))
				else:
					row.append("-1")
			else:
				row.append("-1")
		f.write("\n\t/* "+type_+" */ {")

		first_proto = True
		for next_proto in row: 
			if not first_proto:
				f.write(",")
			first_proto = False
			if next_proto != "-1":
				f.write("PT_"+sanitize_pkt_type(next_proto))
			else:
				f.write(next_proto)
		f.write("}")
		
	f.write("\n};\n\n")

def get_hdr_macro(f):
	f.write("\n#define PT_GET_HDR(tmp, state, proto)\\\n\tdo{\\\n\t\ttmp = state->base + protocol_offsets_bt[ state->type ][ proto ];\\\n\t\tif(tmp < state->base )\\\n\t\t\ttmp = NULL;\\\n\t}while(0)\n\n")

#
# MPLS stuff
#

def mpls_tables_fwd(f):
	"""
	MPLS forward decls.
	"""

	f.write("//Num of headers. \n")
	f.write("extern const int mpls_num_of_labels[PT_MAX__];\n\n")

def mpls_tables(f):
	"""
	MPLS forward decls.
	"""
	f.write("//Num of MPLS labels. \n")
	f.write("const int mpls_num_of_labels[PT_MAX__] = {\n")

	#Real rows
	first_type = True 
	for type_ in pkt_types_unrolled:
		
		if not first_type:
			f.write(",")
		first_type = False

		n_labels = 0
		if "MPLS" in type_:
			#If there is already an MPLS add to the end
			n_labels = int(type_.split("MPLS_nlabels_")[1])
		
		f.write("\n\t/* "+type_+" */"+str(n_labels))
	f.write("\n};\n\n")

	
def mpls_macros(f):
	#none for the moment
	pass

def add_class_type_macro(f):
	#Add main Macro
	f.write("\n#define PT_CLASS_ADD_PROTO(state, PROTO_TYPE) do{\\\n")
	f.write("\tpkt_types_t next_header = (pkt_types_t)parse_transitions[state->type][ __UNROLLED_PT_PROTO_##PROTO_TYPE ];\\\n")
	f.write("\tif( unlikely(next_header == PT_INVALID) ){ assert(0); return; }else{ state->type = next_header;  }\\\n")
	f.write("}while(0)\n")
	
	#Add IPv4 options macro
	f.write("\n#define PT_CLASS_ADD_IPV4_OPTIONS(state, NUM_OPTIONS) do{\\\n")
	f.write("\tswitch(NUM_OPTIONS){\\\n")
	for i in range(0,ipv4_max_options_words+1):
		f.write("\t\tcase "+str(i)+": PT_CLASS_ADD_PROTO(state, IPV4_noptions_"+str(i)+");break;\\\n")
	f.write("\t\tdefault: assert(0);\\\n")
	f.write("}}while(0)\n")
	
def push_macro(f):
	f.write("\n#define PT_PUSH_PROTO(state, PROTO_TYPE)\\\n")
	f.write("\t(pkt_types_t)push_transitions[state->type][ __UNROLLED_PT_PROTO_##PROTO_TYPE ]\n")
	
	#Nice usage
	f.write("\n#define PBB__ ISID\n\n");

def pop_macro(f):
	f.write("\n#define PT_POP_PROTO(state, PROTO_TYPE)\\\n")
	f.write("\t(pkt_types_t)pop_transitions[state->type][ __UNROLLED_PT_PROTO_##PROTO_TYPE ]\n")



##
## Main function
##

FILE_NAME_NOEXT="autogen_pkt_types"

def main():

	unroll_pkt_types()
	unroll_protocols()

	#Header file Open file
	with open("../"+FILE_NAME_NOEXT+'.h', 'w') as header:	
		#License
		license(header)

		#Init guard
		init_guard(header)
		
		#Header comment
		comments(header)
	
		#Generate protocol enum
		protocols_enum(header)

		#Generate pkt types enum
		packet_types_enum(header)
	
		#Offsets in bytes
		packet_offsets_fwd(header)

		#Offsets in bytes
		eth_type_offsets_fwd(header)

		#Transitions
		parse_transitions_fwd(header)

		#Push
		push_transitions_fwd(header)

		#Pop
		pop_transitions_fwd(header)

		#MPLS
		mpls_tables_fwd(header)

		#Macros
		get_hdr_macro(header)
		add_class_type_macro(header)	
		push_macro(header)	
		pop_macro(header)	
		mpls_macros(header)

		#End of guards
		end_guard(header)

	with open("../"+FILE_NAME_NOEXT+'.c', 'w') as c_file:	

		#Header inclusion
		c_file.write("#include \""+FILE_NAME_NOEXT+".h\"\n\n\n")

		#Offsets in bytes
		packet_offsets(c_file)

		#Offsets in bytes
		eth_type_offsets(c_file)


		#Transitions
		parse_transitions(c_file)

		#Push
		push_transitions(c_file)

		#Pop
		pop_transitions(c_file)

		#MPLS
		mpls_tables(c_file)

if __name__ == "__main__":
	main()
