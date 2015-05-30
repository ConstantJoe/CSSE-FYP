/*
 * FullPro.c
 *
 * Created: 10/10/2014 13:02:54
 *  Author: se414010
 */ 

//TODO:
//change states and policies to enums.
//Put children list somewhere better.
//Set policy somewhere better
//Fix structs
//
//Non aggregated case
//Data collection phase
//Sleep mode

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
#include <string.h>
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
#include "Agg.h"
#include "hw_timer.h"
#include "app.h"

//
// Constants
//
//Node states
#define IDLE 1
#define TX_START 2
#define WAIT_START_ACK 3
#define TX_START_ACK_ACK 4
#define WAIT_DATA 5
#define TX_DATA_ACK 6
#define WAIT_DATA_ACK_ACK 7

//Retransmission policies
#define NONAGG 1
#define AGGT 2
#define AGG2T 3
#define AGGAT 4
#define AGG2AT 5
#define AGG4AT 6
#define AGG8AT 7

//
// Global Variables
//
static timer timer1;
static timer timer2;

// Buffer for transmitting radio packets
unsigned char tx_buffer[RADIO_MAX_LEN];
bool tx_buffer_inuse=false; // Check false and set to true before sending a message. Set to false in tx_done
unsigned int state = IDLE;
int txCount = 0;

int t = 2; //original transfer limit.
int txStaticLimit = 10; //For starts, startacks, startackacks. Since a doesn't exist yet
int txLimit = 2;

unsigned int children[256];
int numberOfChildren = 2;

int currentChild = 0;
struct AggDataMsg childrensData[256];
struct NonAggDataMsg childrensDataNonA[256];

bool readyToWakeUp = true;

AggAckMsg currentAck;
NonAggAckMsg currentAckNonA;
uint16_t currentPolicy = AGGAT;

void setRetransmissionPolicy(uint16_t policy, uint16_t a){
	if(policy == NONAGG)
	{
		txLimit = txStaticLimit;
	}
	else if(policy == AGGT)
	{
		txLimit = t;
	}
	else if(policy == AGG2T)
	{
		txLimit = 2*t;
	}
	else if(policy == AGGAT)
	{
		txLimit = a*t;
	}
	else if(policy == AGG2AT)
	{
		txLimit = 2*a*t;
	}
	else if(policy == AGG4AT)
	{
		txLimit = 4*a*t;
	}
	else if(policy == AGG8AT)
	{
		txLimit = 8*a*t;
	}
}

void application_start()
{
	leds_init();
	children[0] = 0x0002; //TODO: move this somewhere better.
	children[1] = 0x0005;
	
	radio_init(NODE_ID, false);
	radio_set_power(15);
	timer_init(&timer1, TIMER_MILLISECONDS, 1000, 15000); //Restarts a run - fires every 15 seconds
	timer_init(&timer2, TIMER_MILLISECONDS, 1000, 1000);
	timer_start(&timer1);
	timer_start(&timer2);
	radio_start();
	serial_init(9600);
}

void application_timer_tick(timer *t)
{
	if(t == &timer1)
	{
		readyToWakeUp = true;
	}
	else if(t == &timer2)
	{
		//on cycle start
		if(state == IDLE && readyToWakeUp == true)
		{
			state = TX_START;
			//send start, change state to waitACK
			if(tx_buffer_inuse == false)
			{
				tx_buffer_inuse = true;
				
				if(currentPolicy == NONAGG)
				{
					NonAggStartMsg startpktNonA;
					startpktNonA.dl_dst = children[currentChild];
					startpktNonA.dl_src = NODE_ID;
					startpktNonA.type = NonAgg_Start;
					
					memcpy(&tx_buffer, &startpktNonA, sizeof(NonAggStartMsg));
					
					radio_send(tx_buffer, sizeof(NonAggStartMsg), startpktNonA.dl_dst);
				}
				else
				{
					AggStartMsg startpkt;
					startpkt.dl_dst = children[currentChild];
					startpkt.dl_src = NODE_ID;
					startpkt.type = Agg_Ack;
					startpkt.policy = currentPolicy;
					
					memcpy(&tx_buffer, &startpkt, sizeof(AggStartMsg));
					
					radio_send(tx_buffer, sizeof(AggStartMsg), startpkt.dl_dst);
				}
			}
			
		}
		else if(state == WAIT_START_ACK)
		{
			if(txCount < txStaticLimit) //hasn't given up yet
			{
				txCount++;
				
				//resend start packet
				
				state = TX_START;
			
				if(tx_buffer_inuse == false)
				{
					tx_buffer_inuse = true;
					
					if(currentPolicy == NONAGG)
					{
						NonAggStartMsg startpktNonA;
						startpktNonA.dl_dst = children[currentChild];
						startpktNonA.dl_src = NODE_ID;
						startpktNonA.type = NonAgg_Start;
						
						memcpy(&tx_buffer, &startpktNonA, sizeof(NonAggStartMsg));
						
						radio_send(tx_buffer, sizeof(NonAggStartMsg), startpktNonA.dl_dst);
					}
					else
					{
						AggStartMsg startpkt;
						startpkt.dl_dst = children[currentChild];
						startpkt.dl_src = NODE_ID;
						startpkt.type = Agg_Ack;
						startpkt.policy = currentPolicy;
						
						memcpy(&tx_buffer, &startpkt, sizeof(AggStartMsg));
						
						radio_send(tx_buffer, sizeof(AggStartMsg), startpkt.dl_dst);
					}
					
				}
			}
			else //tried too many times, just give up
			{
				state = IDLE;
				txCount = 0;
			}
		}
		else if(state == WAIT_DATA)
		{
			//don't think there should be anything here. Node simply waits for data to arrive.
		}
		else if(state == WAIT_DATA_ACK_ACK)
		{
			if(txCount < txLimit)
			{
				txCount++;
				
				//if too much time has passed, retransmit dataack
				
				state = TX_DATA_ACK;
				
				if(tx_buffer_inuse == false)
				{
					tx_buffer_inuse = true;
				
					if(currentPolicy == NONAGG)
					{
						memcpy(&tx_buffer, &currentAckNonA, sizeof(NonAggAckMsg));
						
						radio_send(tx_buffer, sizeof(NonAggAckMsg), currentAckNonA.dl_dst);
					}
					else
					{
						memcpy(&tx_buffer, &currentAck, sizeof(AggAckMsg));
						
						radio_send(tx_buffer, sizeof(AggAckMsg), currentAck.dl_dst);
					}
					
				}
			}
			else
			{
				state = IDLE; //Does making it idle again make sense?
				txCount = 0;
			}
			
		}
	}
}

void application_radio_rx_msg(unsigned short dst, unsigned short src, int len, unsigned char *msgdata)
{	
	AggHeader msg;
	msg.dl_dst = msgdata[0];
	msg.dl_src = msgdata[2];
	msg.type = msgdata[4];
	
	if(msg.type == Agg_Ack) //receives the start ack
	{
		if(state == WAIT_START_ACK)
		{
			leds_on(LED_GREEN);
			//change state (stop resending the start packet, reset counter)
			state = TX_START_ACK_ACK;
			
			if(tx_buffer_inuse == false)
			{
				tx_buffer_inuse = true;
				
				txCount = 0;
				
				//send startAckAck, change state to waitData
				AggAckAckMsg startAckAckPkt;
				startAckAckPkt.dl_dst = msg.dl_src;
				startAckAckPkt.dl_src = NODE_ID;
				startAckAckPkt.type = Agg_AckAck;
				
				memcpy(&tx_buffer, &startAckAckPkt, sizeof(AggAckAckMsg));
				
				radio_send(tx_buffer, sizeof(AggAckAckMsg), startAckAckPkt.dl_dst);
			}
		}
	}
	else if(msg.type == Agg_Data) //receives data
	{
		if(state == WAIT_DATA)
		{
			leds_on(LED_ORANGE);
			
			//change state to waitDataAckAck, send dataAck
			state = TX_DATA_ACK;
			if(tx_buffer_inuse == false)
			{
				tx_buffer_inuse = true;
				
				childrensData[currentChild].dl_dst =  msgdata[0];
				childrensData[currentChild].dl_src = msgdata[2];
				childrensData[currentChild].type = msgdata[4];
				childrensData[currentChild].nodesFeatured = msgdata[5];
				childrensData[currentChild].temp = msgdata[7];
				childrensData[currentChild].temp_src = msgdata[9];
				
				setRetransmissionPolicy(currentPolicy, childrensData[currentChild].nodesFeatured);
			
				currentAck.dl_dst = msg.dl_src;
				currentAck.dl_src = NODE_ID;
				currentAck.type = Agg_Ack;
			
				memcpy(&tx_buffer, &currentAck, sizeof(AggAckMsg));
				
				radio_send(tx_buffer, sizeof(AggAckMsg), currentAck.dl_dst);
			}
		}
	}
	else if(msg.type == Agg_AckAck) //receives dataAckAck
	{
		leds_on(LED_RED);
		//stop resending acks, reset counter
		txCount = 0;
		
		if(currentChild < numberOfChildren-1)
		{
			//change current child
			currentChild++;
			//change state so that system will repeat
			state = IDLE;
		}
		else
		{
			//aggregation
			uint16_t max_temp = 0;
			uint8_t max_temp_src = 0;
			uint16_t totalNodesFeatured = 0;
			for(int i=0; i<numberOfChildren; i++)
			{
				if(childrensData[i].temp > max_temp)
				{
					max_temp = childrensData[i].temp;
					max_temp_src = childrensData[i].temp_src;
				}
				totalNodesFeatured += childrensData[i].nodesFeatured; //calculate total number of nodes featured so far.
			}
			
			//TODO: should I measure temperature on this node too?
			
			//send results down serial line
			printf("Maximum temperature: %d\r\n", max_temp);
			printf("Max temperature source: %d\r\n", max_temp_src);
			printf("Number of nodes data taken from: %d\r\n", totalNodesFeatured);
		
			currentChild = 0;
			
			//stop sending new starts until long timer fires again.
			readyToWakeUp = false;
			
			//TODO: go to sleep
			leds_off(LED_GREEN);
			leds_off(LED_RED);
			leds_off(LED_ORANGE);
		}
	}
	//unaggregated case
	else if(msg.type == NonAgg_Ack)
	{
		if(state == WAIT_START_ACK)
		{
			leds_on(LED_GREEN);
			//change state (stop resending the start packet, reset counter)
			state = TX_START_ACK_ACK;
			
			if(tx_buffer_inuse == false)
			{
				tx_buffer_inuse = true;
				
				txCount = 0;
				
				//send startAckAck, change state to waitData
				NonAggAckAckMsg startAckAckPktNotA;
				startAckAckPktNotA.dl_dst = msg.dl_src;
				startAckAckPktNotA.dl_src = NODE_ID;
				startAckAckPktNotA.type = NonAgg_AckAck;
				
				memcpy(&tx_buffer, &startAckAckPktNotA, sizeof(NonAggAckAckMsg));
				
				radio_send(tx_buffer, sizeof(NonAggAckAckMsg), startAckAckPktNotA.dl_dst);
			}
		}
	}
	else if(msg.type == NonAgg_Data)
	{
		if(state == WAIT_DATA)
		{
			leds_on(LED_ORANGE);
			
			//change state to waitDataAckAck, send dataAck
			state = TX_DATA_ACK;
			if(tx_buffer_inuse == false)
			{
				tx_buffer_inuse = true;
				
				childrensDataNonA[currentChild].dl_dst =  msgdata[0];
				childrensDataNonA[currentChild].dl_src = msgdata[2];
				childrensDataNonA[currentChild].type = msgdata[4];
				childrensDataNonA[currentChild].temp = msgdata[5];
				childrensDataNonA[currentChild].temp_src = msgdata[7];
				childrensDataNonA[currentChild].final = msgdata[8];
				
				setRetransmissionPolicy(currentPolicy, 1);
				
				currentAckNonA.dl_dst = msg.dl_src;
				currentAckNonA.dl_src = NODE_ID;
				currentAckNonA.type = Agg_Ack;
				
				memcpy(&tx_buffer, &currentAck, sizeof(AggAckMsg));
				
				radio_send(tx_buffer, sizeof(AggAckMsg), currentAck.dl_dst);
			}
		}
	}
	else if(msg.type == NonAgg_AckAck)
	{
		if(state == WAIT_DATA_ACK_ACK)
		{
			if(childrensDataNonA[currentChild].final == TRUE)
			{
				currentChild++;
			}
			if(currentChild != numberOfChildren)
			{
				//send another start
				state = TX_START;
				
				NonAggStartMsg startpktNonA;
				startpktNonA.dl_dst = children[currentChild];
				startpktNonA.dl_src = NODE_ID;
				startpktNonA.type = NonAgg_Start;
				
				memcpy(&tx_buffer, &startpktNonA, sizeof(NonAggStartMsg));
				
				radio_send(tx_buffer, sizeof(NonAggStartMsg), startpktNonA.dl_dst);
			}
			else
			{
				printf("Print out all the data");
			}
		}
	}
	
}

void application_radio_tx_done()
{
	tx_buffer_inuse = false;
	
	//empty buffer after data has been sent
	for(int j=0; j<RADIO_MAX_LEN; j++)
	{
		tx_buffer[j] = 0x00;
	}
	
	if(state == TX_START)
	{
		state = WAIT_START_ACK;
	}
	else if(state == TX_START_ACK_ACK)
	{
		state = WAIT_DATA;
	}
	else if(state == TX_DATA_ACK)
	{
		state = WAIT_DATA_ACK_ACK;
	}
}

void application_button_pressed(){}
	
void application_button_released(){}