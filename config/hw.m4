#############################
# CHECKING Hardware support #
#############################
AC_MSG_CHECKING(for hardware support to be compiled)

#With
AC_ARG_WITH(hw-support, AS_HELP_STRING([--with-hw-support="driver-name"],[Compile with hardware support for platform 'driver-name' [default=gnu-linux]])
			  [Available platforms:]
				[gnu-linux: compile GNU/Linux hardware support]
				[gnu-linux-dpdk: compile GNU/Linux Intel DPDK support]
				[bcm: compile Broadcom hardware support]
				[octeon: compile OCTEON hardware support]
				[ofdpa: compile OF-DPA Broadcom hardware support]
				[netfpga10g: compile NetFPGA 10G hardware code (HW code not included)]
				[example: compile the example forwarding module code]
,
 [ 
	#They provide a platform 
	HW=$withval
 ],[
	#Use default
	HW="gnu-linux"
])

#Add subpackages conditionally
if ( test "$HW" = "gnu-linux");then
	msg="$msg GNU/Linux"
	AC_DEFINE(HW_GNU_LINUX)
	PLATFORM=gnu_linux
	AC_CONFIG_SUBDIRS([src/xdpd/drivers/gnu_linux])
fi
if ( test "$HW" = "gnu-linux-dpdk");then
	msg="$msg GNU/Linux Intel DPDK"
	AC_DEFINE(HW_GNU_LINUX_DPDK)
	PLATFORM=gnu_linux_dpdk
	AC_CONFIG_SUBDIRS([src/xdpd/drivers/gnu_linux_dpdk])
fi
if( test "$HW" = "bcm");then
	msg="$msg Broadcom"
	AC_DEFINE(HW_BCM)
	PLATFORM=bcm
	AC_CONFIG_SUBDIRS([src/xdpd/drivers/bcm])
fi	
if( test "$HW" = "octeon");then
	msg="$msg OCTEON"
	AC_DEFINE(HW_OCTEON)
	PLATFORM=octeon
	AC_CONFIG_SUBDIRS([src/xdpd/drivers/octeon])
fi
if( test "$HW" = "ofdpa");then
	msg="$msg OF-DPA"
	AC_DEFINE(HW_OFDPA)
	PLATFORM=ofdpa
	AC_CONFIG_SUBDIRS([src/xdpd/drivers/ofdpa])
fi
if( test "$HW" = "netfpga10g");then
	msg="$msg NetFPGA-10G"
	AC_DEFINE(HW_NETFPGA10G)
	PLATFORM=netfpga10g
	AC_CONFIG_SUBDIRS([src/xdpd/drivers/netfpga10g])
fi
if( test "$HW" = "example");then
	msg="$msg Example platform"
	AC_DEFINE(HW_EXAMPLE)
	PLATFORM=example
	AC_CONFIG_SUBDIRS([src/xdpd/drivers/example])
fi
#[+]Add your platform here...

if test -z $PLATFORM; then
	AC_MSG_RESULT(error)
  	AC_ERROR(Unknown platform: $HW)
fi

#Print fancy message
AC_MSG_RESULT($msg)
AC_SUBST(PLATFORM)


