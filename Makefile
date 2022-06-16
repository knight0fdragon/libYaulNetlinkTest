THIS_ROOT:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

ifeq ($(strip $(YAUL_INSTALL_ROOT)),)
  $(error Undefined YAUL_INSTALL_ROOT (install root directory))
endif

SH_SPECS:= $(THIS_ROOT)/yaul.specs

include $(YAUL_INSTALL_ROOT)/share/pre.common.mk

IMAGE_1ST_READ_BIN:=GAME/0.BIN
SH_PROGRAM:= vdp1-balls
SH_SRCS:= 	$(wildcard source/*.* source/**/*.*)
ROMDISK_SYMBOLS= root
ROMDISK_DIRS= romdisk

SH_LIBRARIES:=
SH_CFLAGS+= -I. -I source/perf -Wno-error=unused-variable -Wno-error -Os -save-temps=obj -g

IP_VERSION:= V1.000
IP_RELEASE_DATE:= 20160101
IP_AREAS:= JTUE
IP_PERIPHERALS:= JAMKST
IP_TITLE:= VDP1 balls test
IP_MASTER_STACK_ADDR:= 0x060FFC00
IP_SLAVE_STACK_ADDR:= 0x06001E00
IP_1ST_READ_ADDR:= 0x06006000
M68K_PROGRAM:=
M68K_OBJECTS:=

include post.common2.mk
