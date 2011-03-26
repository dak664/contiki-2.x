CONTIKI_PROJECT = smcp-demo


all: $(CONTIKI_PROJECT)

UIP_CONF_IPV6 = 1

SMCP_SOURCEFILES = btree.c smcp.c smcp_node.c smcp_pairing.c url-helpers.c smcp-task.c coap.c smcp_timer.c smcp_timer_node.c smcp_variable_node.c
CFLAGS += -ISMCP/src
CONTIKI_SOURCEFILES += $(SMCP_SOURCEFILES)
APPDIRS += SMCP/src/smcp
CLEAN += *.hex *.elf symbols.c symbols.h
MCU = m1284p
APPS += raven-webserver
#APPS += webserver
APPS += raven-lcd-interface

#CFLAGS += -g -O0

CFLAGS += -DUIP_UDP_SEND_UNREACH_NOPORT=1
#CFLAGS += -DUIP_CONF_IPV6_QUEUE_PKT=0
#CFLAGS += -DUIP_CONF_IPV6_RPL=0
#CFLAGS += -DRESOLV_CONF_SUPPORTS_MDNS=0
#CFLAGS += -DRESOLV_CONF_MDNS_RESPONDER=0

#CFLAGS += -DUIP_CONF_LOOPBACK_INTERFACE=1

ifdef VERBOSE_DEBUG
CFLAGS += -DDEBUG=1 -D_DEBUG=1 -DVERBOSE_DEBUG=1
endif

CONTIKI = ../..
include $(CONTIKI)/Makefile.include

AVRDUDE_OPTIONS=-V
AVRDUDE_OPTIONS+=-B 4
AVRDUDE_PROGRAMMER=dragon_isp
#AVRDUDE_PROGRAMMER=dragon_jtag
#AVRDUDE_PROGRAMMER=usbtiny
AVRDUDE_PORT=usb

burn: $(CONTIKI_PROJECT).u

burn-eeprom: $(CONTIKI_PROJECT).u-eep

