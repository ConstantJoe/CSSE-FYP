//
// AVR C library
//
#include <avr/io.h>
//
// Standard C include files
//
#include <stdbool.h>
#include <stdio.h>
#include <stdio.h>
//
// You MUST include app.h and implement every function declared
//
#include "app.h"
//
// Include the header files for the various required libraries
//
#include "simple_os.h"
#include "button.h"
#include "leds.h"
#include "radio.h"
#include "serial.h"

//
// Constants
//

//
// Global Variables
//
static timer timer1;

// Buffer for transmitting radio packets
unsigned char tx_buffer[RADIO_MAX_LEN];
bool tx_buffer_inuse=false; // Chack false and set to true before sending a message. Set to false in tx_done

//
// App init function
//
void application_start()
{
	// initialise required services
	button_init();
	leds_init();
	leds_on(LED_GREEN);
	serial_init(9600);
	radio_init(NODE_ID, false);
	timer_init(&timer1, TIMER_MILLISECONDS, 1000, 250);
	// say hello
	printf("Program XXX started on node %04x\n\r", NODE_ID);
	// start required services
}
//
// Timer tick handler
//
void application_timer_tick(timer *t)
{
}

//
// This function is called whenever a radio message is received
// You must copy any data you need out of the packet - as 'msgdata' will be overwritten by the next message
//
void application_radio_rx_msg(unsigned short dst, unsigned short src, int len, unsigned char *msgdata)
{
	printf("Rx %d bytes: [%04x][%04x]\n", len, dst, src);
}

//
// This function is called whenever a radio message has been transmitted
// You need to free up the transmit buffer here
//
void application_radio_tx_done()
{
	tx_buffer_inuse = false;
}

void application_button_pressed()
{
}

void application_button_released()
{
}
