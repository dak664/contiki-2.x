/* This file has been prepared for Doxygen automatic documentation generation.*/
/*! \file cdc_task.c **********************************************************
 *
 * \brief
 *      Manages the CDC-ACM Virtual Serial Port Dataclass for the USB Device
 *
 * \addtogroup usbstick
 *
 * \author 
 *        Colin O'Flynn <coflynn@newae.com>
 *
 ******************************************************************************/
/* Copyright (c) 2008  ATMEL Corporation
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
   * Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.
   * Neither the name of the copyright holders nor the names of
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
*/
/**
 \ingroup usbstick
 \defgroup cdctask CDC Task
 @{
 */

//_____  I N C L U D E S ___________________________________________________


#include "contiki.h"
#include "usb_drv.h"
#include "usb_descriptors.h"
#include "usb_specific_request.h"
#include "cdc_task.h"
#include "serial/uart_usb_lib.h"
#include "rndis/rndis_protocol.h"
#include "rndis/rndis_task.h"
#include "sicslow_ethernet.h"

#include <stdio.h>
#include <stdlib.h>
#include "dev/watchdog.h"
#include "rng.h"

#if UIP_CONF_IPV6_RPL
#include "net/uip-ds6.h"
#include "rpl.h"
extern uip_ds6_nbr_t uip_ds6_nbr_cache[];
extern uip_ds6_route_t uip_ds6_routing_table[];
extern uip_ds6_netif_t uip_ds6_if;
#endif

#include "bootloader.h"

#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <util/delay.h>

#include "rf230bb.h"

#if JACKDAW_CONF_USE_SETTINGS
#include "settings.h"
#endif

#define BUF ((struct uip_eth_hdr *)&uip_buf[0])
#define PRINTF printf
#define PRINTF_P printf_P

//_____ M A C R O S ________________________________________________________


#define bzero(ptr,size)	memset(ptr,0,size)

//_____ D E F I N I T I O N S ______________________________________________


#define IAD_TIMEOUT_DETACH 300
#define IAD_TIMEOUT_ATTACH 600

//_____ D E C L A R A T I O N S ____________________________________________


void menu_print(void);
void menu_process(char c);

extern char usb_busy;

//! Counter for USB Serial port
extern U8    tx_counter;

//! previous configuration
static uint8_t previous_uart_usb_control_line_state = 0;


static uint8_t timer = 0;
static struct etimer et;

PROCESS(cdc_process, "Debug Menu");

#define CONVERTTXPOWER 1
#if CONVERTTXPOWER  //adds 92 bytes to program flash size
const char txonesdigit[16]   PROGMEM = {'3','2','2','1','1','0','0','1','2','3','4','5','7','9','2','7'};
const char txtenthsdigit[16] PROGMEM = {'0','6','1','6','1','5','2','2','2','2','2','2','2','2','2','2'};
#endif

/**
 * \brief Communication Data Class (CDC) Process
 *
 *   This is the link between USB and the "good stuff". In this routine data
 *   is received and processed by CDC-ACM Class
 */
PROCESS_THREAD(cdc_process, ev, data_proc)
{
	PROCESS_BEGIN();

	while(1) {
		if(usb_mode == mass_storage) {
			etimer_set(&et, CLOCK_SECOND);
			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
			continue;
		}

 		if(Is_device_enumerated()) {
			// If the configuration is different than the last time we checked...
			if((uart_usb_get_control_line_state()&1)!=previous_uart_usb_control_line_state) {
				previous_uart_usb_control_line_state = uart_usb_get_control_line_state()&1;
				static FILE* previous_stdout;
				
				if(previous_uart_usb_control_line_state&1) {
					previous_stdout = stdout;
					uart_usb_set_stdout();
					menu_print();
				} else {
					stdout = previous_stdout;
				}
			}

			//Flush buffer if timeout
	        if(timer >= 4 && tx_counter!=0 ){
	            timer = 0;
	            uart_usb_flush();
	        } else {
				timer++;
			}

			while (uart_usb_test_hit()){
  		  	   menu_process(uart_usb_getchar());   // See what they want
            }
		}//if (Is_device_enumerated())



		if (USB_CONFIG_HAS_DEBUG_PORT(usb_configuration_nb)) {
			etimer_set(&et, CLOCK_SECOND/80);
		} else {
			etimer_set(&et, CLOCK_SECOND);
		}

		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));	
		
	} // while(1)

	PROCESS_END();
}

/**
 \brief Print debug menu
 */
void menu_print(void)
{
		PRINTF_P(PSTR("\n\r*********** Jackdaw Menu **********\n\r"));
		PRINTF_P(PSTR("*                                 *\n\r"));
		PRINTF_P(PSTR("*  m        Print current mode    *\n\r"));
		PRINTF_P(PSTR("*  s        Set to sniffer mode   *\n\r"));
		PRINTF_P(PSTR("*  n        Set to network mode   *\n\r"));
		PRINTF_P(PSTR("*  c        Set RF channel        *\n\r"));
		PRINTF_P(PSTR("*  6        Toggle 6lowpan        *\n\r"));
		PRINTF_P(PSTR("*  r        Toggle raw mode       *\n\r"));
#if USB_CONF_RS232
		PRINTF_P(PSTR("*  d        Toggle RS232 output   *\n\r"));
#endif
#if UIP_CONF_IPV6_RPL
		PRINTF_P(PSTR("*  N        RPL Neighbors         *\n\r"));
		PRINTF_P(PSTR("*  G        RPL Global Repair     *\n\r"));
#endif
		PRINTF_P(PSTR("*  e        Energy Scan           *\n\r"));
#if USB_CONF_STORAGE
		PRINTF_P(PSTR("*  u        Switch to mass-storage*\n\r"));
#endif
		if(bootloader_is_present())
		PRINTF_P(PSTR("*  D        Switch to DFU mode    *\n\r"));
		PRINTF_P(PSTR("*  R        Reset (via WDT)       *\n\r"));
        PRINTF_P(PSTR("*  h,?      Print this menu       *\n\r"));
		PRINTF_P(PSTR("*                                 *\n\r"));
		PRINTF_P(PSTR("* Make selection at any time by   *\n\r"));
		PRINTF_P(PSTR("* pressing your choice on keyboard*\n\r"));
		PRINTF_P(PSTR("***********************************\n\r"));
		PRINTF_P(PSTR("[Built "__DATE__"]\n\r"));
}

#if UIP_CONF_IPV6_RPL
static void
ipaddr_add(const uip_ipaddr_t *addr)
{
  uint16_t a;
  int8_t i, f;
  for(i = 0, f = 0; i < sizeof(uip_ipaddr_t); i += 2) {
    a = (addr->u8[i] << 8) + addr->u8[i + 1];
    if(a == 0 && f >= 0) {
      if(f++ == 0) PRINTF_P(PSTR("::"));
    } else {
      if(f > 0) {
        f = -1;
      } else if(i > 0) {
	    PRINTF_P(PSTR(":"));
      }
	  PRINTF_P(PSTR("%x"),a);
    }
  }
}
#endif

/**
 \brief Process incomming char on debug port
 */
void menu_process(char c)
{

	static enum menustate_enum            /* Defines an enumeration type    */
	{
		normal,
		channel
	} menustate = normal;
	
	static char channel_string[3];
	static uint8_t channel_string_i = 0;
	
	int tempchannel;

	if (menustate == channel) {

		switch(c) {
			case '\r':
			case '\n':		
								
				if (channel_string_i)  {
					channel_string[channel_string_i] = 0;
					tempchannel = atoi(channel_string);

					if ((tempchannel < 11) || (tempchannel > 26))  {
						PRINTF_P(PSTR("\n\rInvalid input\n\r"));
					} else {
						rf230_set_channel(tempchannel);
#if JACKDAW_CONF_USE_SETTINGS
						if(settings_set_uint8(SETTINGS_KEY_CHANNEL, tempchannel)!=SETTINGS_STATUS_OK) {
							PRINTF_P(PSTR("\n\rChannel changed to %d, but unable to store in EEPROM!\n\r"),tempchannel);
						} else
#else						
						AVR_ENTER_CRITICAL_REGION();
						eeprom_write_byte((uint8_t *) 9, tempchannel);   //Write channel
						eeprom_write_byte((uint8_t *)10, ~tempchannel); //Bit inverse as check
						AVR_LEAVE_CRITICAL_REGION();
#endif
						PRINTF_P(PSTR("\n\rChannel changed to %d and stored in EEPROM.\n\r"),tempchannel);
					}
				} else {
					PRINTF_P(PSTR("\n\rChannel unchanged.\n\r"));
				}

				menustate = normal;
				break;
		
			case '\b':
			
				if (channel_string_i) {
					channel_string_i--;
					PRINTF_P(PSTR("\b \b"));
				}
				break;
					
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if (channel_string_i > 1) {
					// This time the user has gone too far.
					// Beep at them.
					putc('\a', stdout);
					//uart_usb_putchar('\a');
					break;
				}
				putc(c, stdout);
				//uart_usb_putchar(c);
				
				channel_string[channel_string_i] = c;
				channel_string_i++;
				break;

			default:
				break;
		}


	} else {

		uint8_t i;
		switch(c) {
			case '\r':
			case '\n':
				break;

			case 'h':
			case '?':
				menu_print();
				break;
			case '-':
				PRINTF_P(PSTR("Bringing interface down\n\r"));
				usb_eth_set_active(0);
				break;
			case '=':
			case '+':
				PRINTF_P(PSTR("Bringing interface up\n\r"));
				usb_eth_set_active(1);
				break;

			case 't':
				// Test "strong" random number generator of R Quattlebaum
				PRINTF_P(PSTR("RNG Output: "));
				{
					uint8_t value = rng_get_uint8();
					uint8_t i;
					for(i=0;i<8;i++) {
						uart_usb_putchar(((value>>(7-i))&1)?'1':'0');
					}
					PRINTF_P(PSTR("\n\r"));
					uart_usb_flush();
					watchdog_periodic();
				}
				break;

			case 's':
				PRINTF_P(PSTR("Jackdaw now in sniffer mode\n\r"));
				usbstick_mode.sendToRf = 0;
				usbstick_mode.translate = 0;
                rf230_listen_channel(rf230_get_channel());
				break;

			case 'n':
				PRINTF_P(PSTR("Jackdaw now in network mode\n\r"));
				usbstick_mode.sendToRf = 1;
				usbstick_mode.translate = 1;
                rf230_set_channel(rf230_get_channel());
				break;

			case '6':
				if (usbstick_mode.sicslowpan) {
					PRINTF_P(PSTR("Jackdaw does not perform 6lowpan translation\n\r"));
					usbstick_mode.sicslowpan = 0;
				} else {
					PRINTF_P(PSTR("Jackdaw now performs 6lowpan translations\n\r"));
					usbstick_mode.sicslowpan = 1;
				}	
				
				break;

			case 'r':
				if (usbstick_mode.raw) {
					PRINTF_P(PSTR("Jackdaw does not capture raw frames\n\r"));
					usbstick_mode.raw = 0;
				} else {
					PRINTF_P(PSTR("Jackdaw now captures raw frames\n\r"));
					usbstick_mode.raw = 1;
				}	
				break;
#if USB_CONF_RS232
			case 'd':
				if (usbstick_mode.debugOn) {
					PRINTF_P(PSTR("Jackdaw does not output debug strings\n\r"));
					usbstick_mode.debugOn = 0;
				} else {
					PRINTF_P(PSTR("Jackdaw now outputs debug strings\n\r"));
					usbstick_mode.debugOn = 1;
				}	
				break;
#endif


			case 'c':
				PRINTF_P(PSTR("Select 802.15.4 Channel in range 11-26 [%d]: "), rf230_get_channel());
				menustate = channel;
				channel_string_i = 0;
				break;

#if JACKDAW_CONF_USE_CONFIGURABLE_RDC
extern void jackdaw_choose_rdc_driver(uint8_t i);
			case '1':
				jackdaw_choose_rdc_driver(0);
				PRINTF_P(PSTR("RDC Driver Changed To: %s\n"), NETSTACK_CONF_RDC.name);
				break;
			case '2':
				jackdaw_choose_rdc_driver(1);
				PRINTF_P(PSTR("RDC Driver Changed To: %s\n"), NETSTACK_CONF_RDC.name);
				break;
			case '3':
				jackdaw_choose_rdc_driver(2);
				PRINTF_P(PSTR("RDC Driver Changed To: %s\n"), NETSTACK_CONF_RDC.name);
				break;
			case '4':
				jackdaw_choose_rdc_driver(3);
				PRINTF_P(PSTR("RDC Driver Changed To: %s\n"), NETSTACK_CONF_RDC.name);
				break;
#endif

#if UIP_CONF_IPV6_RPL
			case 'N':
			{	uint8_t i,j;
                PRINTF_P(PSTR("\n\rAddresses [%u max]\n\r"),UIP_DS6_ADDR_NB);
                for (i=0;i<UIP_DS6_ADDR_NB;i++) {
                    if (uip_ds6_if.addr_list[i].isused) {	  
                        ipaddr_add(&uip_ds6_if.addr_list[i].ipaddr);
                        PRINTF_P(PSTR("\n\r"));
                    }
                }
				PRINTF_P(PSTR("\n\rNeighbors [%u max]\n\r"),UIP_DS6_NBR_NB);
				for(i = 0,j=1; i < UIP_DS6_NBR_NB; i++) {
					if(uip_ds6_nbr_cache[i].isused) {
						ipaddr_add(&uip_ds6_nbr_cache[i].ipaddr);
						PRINTF_P(PSTR("\n\r"));
						j=0;
					}
				}
				if (j) PRINTF_P(PSTR("  <none>"));
				PRINTF_P(PSTR("\n\rRoutes [%u max]\n\r"),UIP_DS6_ROUTE_NB);
				for(i = 0,j=1; i < UIP_DS6_ROUTE_NB; i++) {
					if(uip_ds6_routing_table[i].isused) {
						ipaddr_add(&uip_ds6_routing_table[i].ipaddr);
						PRINTF_P(PSTR("/%u (via "), uip_ds6_routing_table[i].length);
						ipaddr_add(&uip_ds6_routing_table[i].nexthop);
						if(uip_ds6_routing_table[i].state.lifetime < 600) {
							PRINTF_P(PSTR(") %lus\n\r"), uip_ds6_routing_table[i].state.lifetime);
						} else {
							PRINTF_P(PSTR(")\n\r"));
						}
						j=0;
					}
				}
				if (j) PRINTF_P(PSTR("  <none>"));
				PRINTF_P(PSTR("\n\r---------\n\r"));
				break;
			}
			
			case 'G':
				PRINTF_P(PSTR("Global repair returns %d\n\r"),rpl_repair_dag(rpl_get_dag(RPL_ANY_INSTANCE))); 
				break;
            
            case 'L':
                rpl_local_repair(rpl_get_dag(RPL_ANY_INSTANCE));
                 PRINTF_P(PSTR("Local repair initiated\n\r")); 
                 break;
 
            case 'Z':     //zap the routing table           
            {   uint8_t i; 
				for (i = 0; i < UIP_DS6_ROUTE_NB; i++) {
					uip_ds6_routing_table[i].isused=0;
                }
                PRINTF_P(PSTR("Routing table cleared!\n\r")); 
                break;
            }
#endif				
			
			case 'm':
				PRINTF_P(PSTR("Currently Jackdaw:\n\r  * Will "));
				if (usbstick_mode.sendToRf == 0) { PRINTF_P(PSTR("not "));}
				PRINTF_P(PSTR("send data over RF\n\r  * Will "));
				if (usbstick_mode.translate == 0) { PRINTF_P(PSTR("not "));}
				PRINTF_P(PSTR("change link-local addresses inside IP messages\n\r  * Will "));
				if (usbstick_mode.sicslowpan == 0) { PRINTF_P(PSTR("not "));}
				PRINTF_P(PSTR("decompress 6lowpan headers\n\r  * Will "));
				if (usbstick_mode.raw == 0) { PRINTF_P(PSTR("not "));}

				PRINTF_P(PSTR("Output raw 802.15.4 frames\n\r  * Will "));
				if (usbstick_mode.debugOn == 0) { PRINTF_P(PSTR("not "));}
				PRINTF_P(PSTR("Output RS232 debug strings\n\r"));
				PRINTF_P(PSTR("  * USB Ethernet MAC: %02x:%02x:%02x:%02x:%02x:%02x\n"),
					((uint8_t *)&usb_ethernet_addr)[0],
					((uint8_t *)&usb_ethernet_addr)[1],
					((uint8_t *)&usb_ethernet_addr)[2],
					((uint8_t *)&usb_ethernet_addr)[3],
					((uint8_t *)&usb_ethernet_addr)[4],
					((uint8_t *)&usb_ethernet_addr)[5]
				);
				extern uint64_t macLongAddr;
				PRINTF_P(PSTR("  * 802.15.4 EUI-64: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n"),
					((uint8_t *)&macLongAddr)[0],
					((uint8_t *)&macLongAddr)[1],
					((uint8_t *)&macLongAddr)[2],
					((uint8_t *)&macLongAddr)[3],
					((uint8_t *)&macLongAddr)[4],
					((uint8_t *)&macLongAddr)[5],
					((uint8_t *)&macLongAddr)[6],
					((uint8_t *)&macLongAddr)[7]
				);
#if UIP_CONF_IPV6_RPL
				PRINTF_P(PSTR("  * Suports RPL mesh routing\n\r"));
#endif
#if CONVERTTXPOWER
                {
                uint8_t power=rf230_get_txpower()&0xf;
                char sign=(power<6?'+':'-');
                char tens=(power>14?'1':'0');
                char ones=pgm_read_byte(&txonesdigit[power]);
                char tenths=pgm_read_byte(&txtenthsdigit[power]);
                PRINTF_P(PSTR("  * Operates on channel %d with TX power %c%c%c.%cdBm\n\r"), rf230_get_channel(),sign,tens,ones,tenths);
                }
#else  //just show the raw value          
				PRINTF_P(PSTR("  * Operates on channel %d\n\r"), rf230_get_channel());
				PRINTF_P(PSTR("  * TX Power Level: 0x%02X\n\r"), rf230_get_txpower());
#endif
				PRINTF_P(PSTR("  * Current RSSI: %ddB\n\r"), -91+3*(rf230_rssi()-1));
				PRINTF_P(PSTR("  * Last RSSI: %ddB\n\r"), -91+3*(rf230_last_rssi-1));

				PRINTF_P(PSTR("  * RDC Driver: %s\n\r"), NETSTACK_CONF_RDC.name);

#if SICSLOW_ETHERNET_CONF_UPDATE_USB_ETH_STATS
				PRINTF_P(PSTR("  * usb_eth_stat.txok: %lu\n"), (unsigned long)usb_eth_stat.txok);
				PRINTF_P(PSTR("  * usb_eth_stat.rxok: %lu\n"), (unsigned long)usb_eth_stat.rxok);
				PRINTF_P(PSTR("  * usb_eth_stat.txbad: %lu\n"), (unsigned long)usb_eth_stat.txbad);
				PRINTF_P(PSTR("  * usb_eth_stat.rxbad: %lu\n"), (unsigned long)usb_eth_stat.rxbad);
#endif
        
#if RADIOSTATS
				extern uint16_t RF230_sendpackets,RF230_receivepackets,RF230_sendfail,RF230_receivefail;
				PRINTF_P(PSTR("  * RF230_sendpackets: %u\n"), (uint16_t)RF230_sendpackets);
				PRINTF_P(PSTR("  * RF230_receivepackets: %u\n"), (uint16_t)RF230_receivepackets);
				PRINTF_P(PSTR("  * RF230_sendfail: %u\n"), (uint16_t)RF230_sendfail);
				PRINTF_P(PSTR("  * RF230_receivefail: %u\n"), (uint16_t)RF230_receivefail);
#endif

				PRINTF_P(PSTR("  * Configuration: %d, USB<->ETH is "), usb_configuration_nb);
                if (usb_eth_is_active == 0) PRINTF_P(PSTR("not "));
                PRINTF_P(PSTR("active\n\r"));

#if CONFIG_STACK_MONITOR
/* See contiki-raven-main.c for initialization of the magic numbers */
{
extern uint16_t __bss_end;
uint16_t p=(uint16_t)&__bss_end;
    do {
      if (*(uint16_t *)p != 0x4242) {
        printf_P(PSTR("  * Never-used stack > %d bytes\n\r"),p-(uint16_t)&__bss_end);
        break;
      }
      p+=100;
    } while (p<RAMEND-100);
}
#endif
             
				break;

			case 'e':
				PRINTF_P(PSTR("Energy Scan:\n"));
				uart_usb_flush();
				{
					uint8_t i;
					uint16_t j;
					uint8_t previous_channel = rf230_get_channel();
					int8_t RSSI, maxRSSI[17];
					uint16_t accRSSI[17];
					
					bzero((void*)accRSSI,sizeof(accRSSI));
					bzero((void*)maxRSSI,sizeof(maxRSSI));
					
					for(j=0;j<(1<<12);j++) {
						for(i=11;i<=26;i++) {
							rf230_listen_channel(i);
							_delay_us(3*10);
							RSSI = rf230_rssi();
							maxRSSI[i-11]=Max(maxRSSI[i-11],RSSI);
							accRSSI[i-11]+=RSSI;
						}
						if(j&(1<<7)) {
							LedVCP_on();
							if(!(j&((1<<7)-1))) {
								PRINTF_P(PSTR("."));
								uart_usb_flush();
							}
						}
						else
							LedVCP_off();
						watchdog_periodic();
					}
					rf230_set_channel(previous_channel);
					PRINTF_P(PSTR("\n"));
					for(i=11;i<=26;i++) {
						uint8_t activity=Min(maxRSSI[i-11],accRSSI[i-11]/(1<<7));
						PRINTF_P(PSTR(" %d: %02ddB "),i, -91+(maxRSSI[i-11]-1));
						for(;activity--;maxRSSI[i-11]--) {
							PRINTF_P(PSTR("#"));
						}
						for(;maxRSSI[i-11]>0;maxRSSI[i-11]--) {
							PRINTF_P(PSTR(":"));
						}
						PRINTF_P(PSTR("\n"));
						uart_usb_flush();
					}

				}
				PRINTF_P(PSTR("Done.\n"));
				uart_usb_flush();
				
				break;


			case 'D':
				{
					PRINTF_P(PSTR("Entering DFU Mode...\n\r"));
					uart_usb_flush();
					Leds_on();
					_delay_ms(100);
					Usb_detach();
					for(i = 0; i < 10; i++)_delay_ms(100);
					Leds_off();
					Jump_To_Bootloader();
				}
				break;
			case 'R':
				{
					PRINTF_P(PSTR("Resetting...\n\r"));
					uart_usb_flush();
					Leds_on();
					for(i = 0; i < 10; i++)_delay_ms(100);
					Usb_detach();
					for(i = 0; i < 20; i++)_delay_ms(100);
					watchdog_reboot();
				}
				break;
			
			case 'W':
				{
					PRINTF_P(PSTR("Switching to windows mode...\n\r"));
					uart_usb_flush();
					usb_eth_switch_to_windows_mode();
				}
				break;
#if USB_CONF_STORAGE
			case 'u':

				//Mass storage mode
				usb_mode = mass_storage;

				//No more serial port
				stdout = NULL;

				//Deatch USB
				Usb_detach();

				//RNDIS is over
				rndis_state = 	rndis_uninitialized;

				// Reset the USB configuration
				usb_configuration_nb = 0;

				Leds_off();

				//Wait a few seconds
				for(i = 0; i < 5; i++) {
					Led0_on();
					_delay_ms(100);
					Led0_off();
					Led1_on();
					_delay_ms(100);
					Led1_off();
					Led2_on();
					_delay_ms(100);
					Led2_off();
					Led3_on();
					_delay_ms(100);
					Led3_off();
					watchdog_periodic();
				}

				Leds_off();

				//Attach USB
				Usb_attach();


				break;
#endif

			default:
				PRINTF_P(PSTR("%c is not a valid option! h for menu\n\r"), c);
				break;
		}


	}

	return;

}

/** @}  */
