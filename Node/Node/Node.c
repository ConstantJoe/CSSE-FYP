/*
 * FullPro.c
 *
 * Created: 10/10/2014 13:02:54
 *  Author: se414010
 */ 

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
#include "Agg.h"
#include "hw_timer.h"
#include "app.h"
#include "string.h"

//
// Constants
//

//Node states
#define IDLE 1 //TODO: Ask Stephen is there a better way of doing this. Just code cleanliness really.
#define TX_START_ACK 2
#define WAIT_START_ACK_ACK 3
#define TX_START 4
#define WAIT_START_ACK 5
#define TX_START_ACK_ACK 6
#define WAIT_DATA 7
#define TX_DATA_ACK 8
#define WAIT_DATA_ACK_ACK 9
#define TX_DATA 10
#define WAIT_DATA_ACK 11
#define TX_DATA_ACK_ACK 12

//Retransmission policies
#define NONAGG 1 //TODO: Ask Stephen is there a better way of doing this. Just code cleanliness really.
#define AGGT 2
#define AGG2T 3
#define AGGAT 4
#define AGG2AT 5
#define AGG4AT 6
#define AGG8AT 7
#define DATACOLLECTION 8

//
// Global Variables
//
static timer timer1;
//static timer timer2;

// Buffer for transmitting radio packets
unsigned char tx_buffer[RADIO_MAX_LEN];
bool tx_buffer_inuse=false; // Check false and set to true before sending a message. Set to false in tx_done
unsigned int state = IDLE;

unsigned int children[256];
int numberOfChildren = 0;

struct AggDataMsg childrensData[256]; //TODO: Overkill?
int currentChild = 0;
int parentNode = 0;
uint16_t currentPolicy = 0;

int txCount = 0;
int t = 2; //Original transmit limit
int txLimit = 8; 
int txStaticLimit = 10; //For starts, startacks, startackacks. Since a doesn't exist yet really.
bool sendYourOwnData = false;

AggDataMsg currentPkt;
AggAckMsg currentAck;
AggAckAckMsg currentAckAck;

NonAggDataMsg currentPktNonA;
NonAggAckMsg currentAckNonA;
NonAggAckAckMsg currentAckAckNonA;
NonAggDataMsg currentChildDataNonA;

DataCollectionDataMsg currentPktDataCollection;
DataCollectionAckMsg currentAckDataCollection;
DataCollectionAckAckMsg currentAckAckDataCollection;
DataCollectionDataMsg currentChildDataDataCollection;


uint16_t packetsSent = 0;
uint16_t retransmissionsRequired = 0;
uint16_t packetsReceived = 0;

bool resend = false;


//method taken from ATmega128RFA1 docs.
//MUX selects the temperature sensor, SRA sets the clock frequency, ADMUX sets the reference voltage, SRC sets the startup time.
//Accuracy can be increased by averaging or oversampling.

//See page 429 for details.
//Move this to seperate c file? Needs to be cleaned up too.
uint16_t ADCmeasureTemp()
{
	ADCSRC = 10<<ADSUT0; //set startup time
	ADCSRB = 1<<MUX5; //set MUX5 first
	ADMUX = (3<<REFS0) + (9<<MUX0); //store new ADMUX, 1.6V AREF
	//switch ADC on, set prescalar, start conversion
	ADCSRA = (1<<ADEN) + (1<<ADSC) + (4<<ADPS0);
	do{
		
	}while((ADCSRA & (1<<ADSC))); //wait for conversion end
	ADCSRA = 0; //disable the ADC
	return(ADC);
}

//Results in values of ~27, 28. Is it accurate? Also, its unsigned. What if temperature is <0C?
uint16_t measureTemperatureCelcius()
{
	return(1.13*ADCmeasureTemp()-272.8);
}

void setRetransmissionPolicy(uint16_t policy, uint16_t a){
	if(policy == NONAGG)
	{
		//TODO: Figure out what to do here.
		txLimit = 10;
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
	else if(policy == DATACOLLECTION)
	{
		txLimit = 40;
	}
}

//
// App init function
//
void application_start()
{
	//children[0] = 0x0006; //hardcoded for now -> TODO: Find a better way of doing this. 
	//children[1] = 0x0007; //ditto
	
	leds_init();
	serial_init(9600);
	radio_init(NODE_ID, false);
	radio_set_power(15);
	timer_init(&timer1, TIMER_MILLISECONDS, 1000, 1000); //fires every second -> keeping it slow for now just tp be able to watch it propagate through the network.
	timer_start(&timer1);
	radio_start();
}
//
// Timer tick handler
//
void application_timer_tick(timer *t)
{
	if(state == WAIT_START_ACK_ACK)
	{
		if(txCount < txStaticLimit)
		{
			//if waiting too long, resend start_ack
			state = TX_START_ACK;
			
			if(tx_buffer_inuse == false)
			{
				tx_buffer_inuse = true;
				
				if(currentPolicy == NONAGG)
				{
					printf("resend nonagg startack\r\n");
					memcpy(&tx_buffer, &currentAckNonA, sizeof(NonAggAckMsg));
					
					retransmissionsRequired++;
					radio_send(tx_buffer, sizeof(NonAggAckMsg), currentAckNonA.dl_dst);
				}
				else if(currentPolicy == DATACOLLECTION)
				{
					printf("resend data collection startack\r\n");
					memcpy(&tx_buffer, &currentAckDataCollection, sizeof(DataCollectionAckMsg));
					
					radio_send(tx_buffer, sizeof(DataCollectionAckMsg), currentAckDataCollection.dl_dst);
				}
				else
				{
					printf("resend agg startack\r\n");
					memcpy(&tx_buffer, &currentAck, sizeof(AggAckMsg));
					
					retransmissionsRequired++;
					radio_send(tx_buffer, sizeof(AggAckMsg), currentAck.dl_dst);
				}
				
				txCount++;
			}		
		}
		else
		{
			printf("give up\r\n");
			//give up
			leds_on(LED_RED);
			state = IDLE;
			txCount = 0;
		}
		
	}
	else if(state == WAIT_DATA && resend == true)
	{
		if(txCount < txStaticLimit)
		{
			// if waiting too long, resend start
			state = TX_START;
			if(tx_buffer_inuse == false)
			{
				tx_buffer_inuse = true;
				
				if(currentPolicy == NONAGG)
				{
					printf("resend nonagg start\r\n");
					NonAggStartMsg startpktNonA;
					startpktNonA.dl_dst = children[currentChild];
					startpktNonA.dl_src = NODE_ID;
					startpktNonA.type = NonAgg_Start;
					
					memcpy(&tx_buffer, &startpktNonA, sizeof(NonAggStartMsg));
					
					retransmissionsRequired++;
					radio_send(tx_buffer, sizeof(NonAggStartMsg), startpktNonA.dl_dst);
				}
				else if(currentPolicy == DATACOLLECTION)
				{
					printf("resend data collection start\r\n");
					DataCollectionStartMsg startpktDataCollection;
					startpktDataCollection.dl_dst = children[currentChild];
					startpktDataCollection.dl_src = NODE_ID;
					startpktDataCollection.type = DataCollection_Start;
					
					memcpy(&tx_buffer, &startpktDataCollection, sizeof(DataCollectionStartMsg));
					
					radio_send(tx_buffer, sizeof(DataCollectionStartMsg), startpktDataCollection.dl_dst);
				}
				else
				{
					printf("resend start\r\n");
					//send start, change state to waitACK
					AggStartMsg startpkt;
					startpkt.dl_dst = children[currentChild];
					startpkt.dl_src = NODE_ID;
					startpkt.type = Agg_Ack;
					startpkt.policy = currentPolicy;
					
					memcpy(&tx_buffer, &startpkt, sizeof(AggStartMsg));
					
					retransmissionsRequired++;
					radio_send(tx_buffer, sizeof(AggStartMsg), startpkt.dl_dst);
				}
				txCount++;
			}
		}
		else
		{
			printf("give up\r\n");
			//give up
			leds_on(LED_RED);
			state = IDLE;
			txCount = 0;
			
			//TODO: Node should move on to next child instead of giving up entirely. 
		}
	}
	else if(state == WAIT_DATA)
	{
		//don't think there's anything to do here. Just keep waiting till data comes in.
	}
	else if(state == WAIT_DATA_ACK_ACK)
	{
		if(txCount < txLimit)
		{
			//if waiting too long, resend data_ack
			state = TX_DATA_ACK;
			
			if(tx_buffer_inuse == false)
			{
				tx_buffer_inuse = true;
				
				if(currentPolicy == NONAGG)
				{
					printf("resend nonagg dataack\r\n");
					memcpy(&tx_buffer, &currentAckNonA, sizeof(NonAggAckMsg));
					
					retransmissionsRequired++;
					radio_send(tx_buffer, sizeof(NonAggAckMsg), currentAckNonA.dl_dst);
				}
				else if(currentPolicy == DATACOLLECTION)
				{
					printf("resend datacollection dataack\r\n");
					memcpy(&tx_buffer, &currentAckDataCollection, sizeof(DataCollectionAckMsg));
					
					radio_send(tx_buffer, sizeof(DataCollectionAckMsg), currentAckDataCollection.dl_dst);
				}
				else
				{
					printf("resend dataack\r\n");
					memcpy(&tx_buffer, &currentAckNonA, sizeof(NonAggAckMsg));
					
					retransmissionsRequired++;
					radio_send(tx_buffer, sizeof(NonAggAckMsg), currentAckNonA.dl_dst);
				}
				txCount++;
			}	
		}
		else
		{
			printf("give up\r\n");
			//give up
			leds_on(LED_RED);
			state = IDLE;
			txCount = 0;
		}
		
	}
	else if(state == WAIT_DATA_ACK)
	{
		if(txCount < txLimit)
		{
			state = TX_DATA;
			
			if(tx_buffer_inuse == false)
			{
				tx_buffer_inuse = true;
				
				if(currentPolicy == NONAGG)
				{
					printf("resend nonagg data\r\n");
					memcpy(&tx_buffer, &currentPktNonA, sizeof(NonAggDataMsg));
					
					retransmissionsRequired++;
					radio_send(tx_buffer, sizeof(NonAggDataMsg),currentPktNonA.dl_dst);
				}
				else if(currentPolicy == DATACOLLECTION)
				{
					printf("resend data collection data\r\n");
					memcpy(&tx_buffer, &currentPktDataCollection, sizeof(DataCollectionDataMsg));
					
					radio_send(tx_buffer, sizeof(DataCollectionDataMsg),currentPktDataCollection.dl_dst);
				}
				else
				{
					printf("resend data\r\n");
					memcpy(&tx_buffer, &currentPkt, sizeof(AggDataMsg));
					
					retransmissionsRequired++;
					radio_send(tx_buffer, sizeof(AggDataMsg),currentPkt.dl_dst);
				}
				
				txCount++;
			}
		}
		else
		{
			printf("give up\r\n");
			//give up
			leds_on(LED_RED);
			state = IDLE;
			txCount = 0;
		}
		
	}
}

void application_radio_rx_msg(unsigned short dst, unsigned short src, int len, unsigned char *msgdata)
{	
	packetsReceived++;
	
	AggHeader msg;
	msg.dl_dst = msgdata[0];
	msg.dl_src = msgdata[2];
	msg.type = msgdata[4];
	
	if(msg.type == Agg_Ack) //TODO: create new "start" type
	{
		if(state == IDLE)
		{
			if(numberOfChildren != 0)
			{
				printf("Send a startack\r\n");
				//send startack, change state to waitstartackack
				state = TX_START_ACK;
			
				leds_on(LED_GREEN);
				
				if(tx_buffer_inuse == false)
				{
					tx_buffer_inuse = true;
					
					currentAck.dl_dst = msg.dl_src;
					currentAck.dl_src = NODE_ID;
					currentAck.type = Agg_Ack;
					
					parentNode = msg.dl_src;
					
					memcpy(&tx_buffer, &currentAck, sizeof(AggAckMsg));
					
					txCount = 0;
					packetsSent++;
					radio_send(tx_buffer, sizeof(AggAckMsg), currentAck.dl_dst);
				}
			}
			else
			{
				printf("Leaf - send data\r\n");
				AggStartMsg start;
				start.dl_dst = msgdata[0];
				start.dl_src = msgdata[2];
				start.type = msgdata[4];
				start.policy = msgdata[5];
				currentPolicy = start.policy;
				//its a leaf
				//send data to parent, change state to waitdataack
				setRetransmissionPolicy(currentPolicy, 1); //a is 1 since this is a leaf
				
				state = TX_DATA;
				
				leds_on(LED_GREEN);
				
				if(tx_buffer_inuse == false)
				{
					tx_buffer_inuse = true;
					
					currentPkt.dl_dst = msg.dl_src;
					currentPkt.dl_src = NODE_ID;
					currentPkt.type = Agg_Data;
					currentPkt.nodesFeatured = 1; //coming from a leaf
					currentPkt.temp = measureTemperatureCelcius();
					currentPkt.temp_src = NODE_ID;
					
					memcpy(&tx_buffer, &currentPkt, sizeof(AggDataMsg));
					
					txCount = 0;
					packetsSent++;
					radio_send(tx_buffer, sizeof(AggDataMsg),currentPkt.dl_dst);
				}
			}
			
		}
		else if(state == WAIT_DATA)
		{
			state = TX_START_ACK_ACK;
			resend = false;
			
			if(tx_buffer_inuse == false)
			{
				printf("send startackack\r\n");
				tx_buffer_inuse = true;
				
				//send startackack back to its child, change state to waitdata
				AggAckAckMsg startAckAckPkt;
				startAckAckPkt.dl_dst = msg.dl_src;
				startAckAckPkt.dl_src = NODE_ID;
				startAckAckPkt.type = Agg_AckAck;
				
				memcpy(&tx_buffer, &startAckAckPkt, sizeof(AggAckAckMsg));
				
				txCount = 0;
				packetsSent++;
				radio_send(tx_buffer, sizeof(AggAckAckMsg), startAckAckPkt.dl_dst);
			}
		}
		else if(state == WAIT_DATA_ACK) //could use a type for this too
		{
			printf("send dataackack\r\n");
			//send dataackack, change state to idle. Sleep now? Go over diagram.
			
			state = TX_DATA_ACK_ACK;
		
			leds_on(LED_ORANGE);
			
			if(tx_buffer_inuse == false)
			{
				tx_buffer_inuse = true;
				
				//send dataackack back to its child, then go to sleep. TODO: Can you go straight to sleep? Check notes
				AggAckAckMsg dataAckAckPkt;
				dataAckAckPkt.dl_dst = msg.dl_src;
				dataAckAckPkt.dl_src = NODE_ID;
				dataAckAckPkt.type = Agg_AckAck;
				
				memcpy(&tx_buffer, &dataAckAckPkt, sizeof(AggAckAckMsg));
				
				txCount = 0;
				packetsSent++;
				radio_send(tx_buffer, sizeof(AggAckAckMsg), dataAckAckPkt.dl_dst);
			}
		}
	}
	else if(msg.type == Agg_Data) //the data arrives in
	{
		if(state == WAIT_DATA)
		{
			printf("Send dataack\r\n");
			//send dataack, change state to waitdataackack
			state = TX_DATA_ACK;
			
			
			
			childrensData[currentChild].dl_dst = msgdata[0];
			childrensData[currentChild].dl_src =  msgdata[2];
			childrensData[currentChild].type =  msgdata[4];
			childrensData[currentChild].nodesFeatured =  msgdata[5];
			childrensData[currentChild].temp = msgdata[7];
			childrensData[currentChild].temp_src = msgdata[9];
			
			setRetransmissionPolicy(currentPolicy, childrensData[currentChild].nodesFeatured);
			
			currentAck.dl_dst = childrensData[currentChild].dl_src;
			currentAck.dl_src = NODE_ID;
			currentAck.type = Agg_Ack;
			
			if(tx_buffer_inuse == false)
			{
				tx_buffer_inuse = true;
				
				memcpy(&tx_buffer, &currentAck, sizeof(AggAckMsg));
				
				txCount = 0;
				packetsSent++;
				radio_send(tx_buffer, sizeof(AggAckMsg), currentAck.dl_dst);
			}
		}
	}
	else if(msg.type == Agg_AckAck)
	{
		if(state == WAIT_START_ACK_ACK)
		{
			printf("send start to a child\r\n");
			//send start to one of its children, change state to waitstartack
			state = TX_START;
			resend = true;
			
			if(tx_buffer_inuse == false)
			{
				tx_buffer_inuse = true;
				
				AggStartMsg startpkt;
				startpkt.dl_dst = children[currentChild];
				startpkt.dl_src = NODE_ID;
				startpkt.type = Agg_Ack;
				startpkt.policy = currentPolicy;
				
				memcpy(&tx_buffer, &startpkt, sizeof(AggStartMsg));
				
				txCount = 0;
				packetsSent++;
				radio_send(tx_buffer, sizeof(AggStartMsg), startpkt.dl_dst);
			}
		}
		else if(state == WAIT_DATA_ACK_ACK) //could use a new state for this too
		{
			if(currentChild < numberOfChildren-1)
			{
				printf("send start to next child\r\n");
				//if there are more children, increment child number and send a start to the next child
				currentChild++;
				
				state = TX_START;
				resend = true;
				
				if(tx_buffer_inuse == false)
				{
					tx_buffer_inuse = true;
					
					AggStartMsg startpkt;
					startpkt.dl_dst = children[currentChild];
					startpkt.dl_src = NODE_ID;
					startpkt.type = Agg_Ack;
					startpkt.policy = currentPolicy;
					
					memcpy(&tx_buffer, &startpkt, sizeof(AggStartMsg));
					
					txCount = 0;
					packetsSent++;
					radio_send(tx_buffer, sizeof(AggStartMsg), startpkt.dl_dst);
				}
			}
			else
			{
				printf("send data up tree\r\n");
				//if there are no more children, aggregate received data with your own data, send to parent, and change state to waitdataack
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
				
				uint16_t tempOnNode = measureTemperatureCelcius();
				if(tempOnNode > max_temp)
				{
					max_temp = tempOnNode; //this is the maximum temperature in the subtree.
					max_temp_src = NODE_ID;
				}
				totalNodesFeatured += 1; //adds in this node too.
				
				setRetransmissionPolicy(currentPolicy, totalNodesFeatured);
				
				state = TX_DATA;
				
				currentPkt.dl_dst = parentNode;
				currentPkt.dl_src = NODE_ID;
				currentPkt.type = Agg_Data;
				currentPkt.nodesFeatured = totalNodesFeatured;
				currentPkt.temp = max_temp;
				currentPkt.temp_src = max_temp_src;
				
				leds_on(LED_ORANGE);
				
				if(tx_buffer_inuse == false)
				{
					tx_buffer_inuse = true;
					
					memcpy(&tx_buffer, &currentPkt, sizeof(AggDataMsg));
					
					txCount = 0;
					packetsSent++;
					radio_send(tx_buffer, sizeof(AggDataMsg), currentPkt.dl_dst);
				}
			}
		}
	}
	
	//unaggregated case
	else if(msg.type == NonAgg_Start)
	{
		currentPolicy = NONAGG;
		printf("Receive non-agg start\r\n");
		if(state == IDLE)
		{
			if(sendYourOwnData == true)
			{
				printf("Send own non-agg data\r\n");
				currentPolicy = NONAGG;
				//its a leaf
				//send data to parent, change state to waitdataack
				setRetransmissionPolicy(currentPolicy, 1); //a is 1 since this is a leaf
				
				state = TX_DATA;
				
				leds_on(LED_GREEN);
				
				if(tx_buffer_inuse == false)
				{
					tx_buffer_inuse = true;
					
					currentPktNonA.dl_dst = msg.dl_src;
					currentPktNonA.dl_src = NODE_ID;
					currentPktNonA.type = NonAgg_Data;
					currentPktNonA.temp = measureTemperatureCelcius();
					currentPktNonA.temp_src = NODE_ID;
					currentPktNonA.final = TRUE;
					
					memcpy(&tx_buffer, &currentPktNonA, sizeof(NonAggDataMsg));
					
					txCount = 0;
					packetsSent++;
					radio_send(tx_buffer, sizeof(NonAggDataMsg),currentPktNonA.dl_dst);
				}
			}
			else if(numberOfChildren != 0)
			{
				//send startack, change state to waitstartackack
				printf("Send startack\r\n");
				state = TX_START_ACK;
				
				leds_on(LED_GREEN);
				
				if(tx_buffer_inuse == false)
				{
					tx_buffer_inuse = true;
					
					currentAckNonA.dl_dst = msg.dl_src;
					currentAckNonA.dl_src = NODE_ID;
					currentAckNonA.type = NonAgg_Ack;
					
					parentNode = msg.dl_src;
					
					memcpy(&tx_buffer, &currentAckNonA, sizeof(NonAggAckMsg));
					
					txCount = 0;
					packetsSent++;
					radio_send(tx_buffer, sizeof(NonAggAckMsg), currentAckNonA.dl_dst);
				}
			}
			else
			{
				printf("Leaf - send non-agg data up tree\r\n");
				currentPolicy = NONAGG;
				//its a leaf
				//send data to parent, change state to waitdataack
				setRetransmissionPolicy(currentPolicy, 1); //a is 1 since this is a leaf
				
				state = TX_DATA;
				
				leds_on(LED_GREEN);
				
				if(tx_buffer_inuse == false)
				{
					tx_buffer_inuse = true;
					
					currentPktNonA.dl_dst = msg.dl_src;
					currentPktNonA.dl_src = NODE_ID;
					currentPktNonA.type = NonAgg_Data;
					currentPktNonA.temp = measureTemperatureCelcius();
					currentPktNonA.temp_src = NODE_ID;
					currentPktNonA.final = TRUE;
					
					memcpy(&tx_buffer, &currentPktNonA, sizeof(NonAggDataMsg));
					
					txCount = 0;
					packetsSent++;
					radio_send(tx_buffer, sizeof(NonAggDataMsg),currentPktNonA.dl_dst);
				}
			}
		}
	}
	else if(msg.type == NonAgg_Ack)
	{
		if(state == WAIT_DATA)
		{
			printf("Send non-agg startackack\r\n");
			state = TX_START_ACK_ACK;
			
			if(tx_buffer_inuse == false)
			{
				tx_buffer_inuse = true;
				
				//send startackack back to its child, change state to waitdata
				NonAggAckAckMsg startAckAckPktNonA;
				startAckAckPktNonA.dl_dst = msg.dl_src;
				startAckAckPktNonA.dl_src = NODE_ID;
				startAckAckPktNonA.type = NonAgg_AckAck;
				
				memcpy(&tx_buffer, &startAckAckPktNonA, sizeof(NonAggAckAckMsg));
				
				txCount = 0;
				packetsSent++;
				radio_send(tx_buffer, sizeof(NonAggAckAckMsg), startAckAckPktNonA.dl_dst);
			}
		}
		else if(state == WAIT_DATA_ACK)
		{
			//send dataackack
			printf("Send non-agg dataackack\r\n");
			state = TX_DATA_ACK_ACK;
			
			leds_on(LED_ORANGE);
			
			if(tx_buffer_inuse == false)
			{
				tx_buffer_inuse = true;
				
				//send dataackack back to its child, then go to sleep. TODO: Can you go straight to sleep? Check notes
				NonAggAckAckMsg dataAckAckPktNotA;
				dataAckAckPktNotA.dl_dst = msg.dl_src;
				dataAckAckPktNotA.dl_src = NODE_ID;
				dataAckAckPktNotA.type = NonAgg_AckAck;
				
				memcpy(&tx_buffer, &dataAckAckPktNotA, sizeof(NonAggAckAckMsg));
				
				txCount = 0;
				packetsSent++;
				radio_send(tx_buffer, sizeof(NonAggAckAckMsg), dataAckAckPktNotA.dl_dst);
			}
		}
	}
	else if(msg.type == NonAgg_AckAck)
	{
		if(state == WAIT_START_ACK_ACK)
		{
			printf("Send nonagg start to a child.\r\n");
			//send start to one of its children, change state to waitstartack
			state = TX_START;
			resend = true;
			
			if(tx_buffer_inuse == false)
			{
				tx_buffer_inuse = true;
				
				NonAggStartMsg startpktNonA;
				startpktNonA.dl_dst = children[currentChild];
				startpktNonA.dl_src = NODE_ID;
				startpktNonA.type = NonAgg_Ack;
				printf("Sending to %d\r\n", startpktNonA.dl_dst);
				printf("Current policy: %d\r\n", currentPolicy);
				memcpy(&tx_buffer, &startpktNonA, sizeof(NonAggStartMsg));
				
				txCount = 0;
				packetsSent++;
				radio_send(tx_buffer, sizeof(NonAggStartMsg), startpktNonA.dl_dst);
			}
		}
		else if(state == WAIT_DATA_ACK_ACK) //could use a new state for this too
		{
			// if done = true && currentChild < number of children
			//increment current child
			//if done == true && currentChild == number of children
			
			//if done == false, just send on data
			
			state = TX_DATA;
			printf("Send up received nonagg data\r\n");
			if(currentChildDataNonA.final == TRUE)
			{
				if(currentChild == numberOfChildren)
				{
					//set up so with next received start send your own data.
					sendYourOwnData = true;
				}
				if(currentChild < numberOfChildren)
				{
					currentChild++;
				}
			}
			currentChildDataNonA.dl_dst = parentNode;
			
			if(tx_buffer_inuse == false)
			{
				tx_buffer_inuse = true;
				
				memcpy(&tx_buffer, &currentChildDataNonA, sizeof(NonAggDataMsg));
				
				txCount = 0;
				packetsSent++;
				radio_send(tx_buffer, sizeof(NonAggDataMsg), currentChildDataNonA.dl_dst);
			}
			
			
		}
	}
	else if(msg.type == NonAgg_Data)
	{
		if(state == WAIT_DATA)
		{
			printf("Send nonagg dataack\r\n");
			//save the data somewhere and send a dataack
			state = TX_DATA_ACK;
			
			currentChildDataNonA.dl_dst = msgdata[0];
			currentChildDataNonA.dl_src = msgdata[2];
			currentChildDataNonA.type = msgdata[4];
			currentChildDataNonA.temp = msgdata[5];
			currentChildDataNonA.temp_src = msgdata[7];
			currentChildDataNonA.final = msgdata[8];
			
			setRetransmissionPolicy(currentPolicy, 1);
			
			currentAckNonA.dl_dst = currentChildDataNonA.dl_src;
			currentAckNonA.dl_src = NODE_ID;
			currentAckNonA.type = NonAgg_Ack;
			
			if(tx_buffer_inuse == false)
			{
				tx_buffer_inuse = true;
				
				memcpy(&tx_buffer, &currentAckNonA, sizeof(NonAggAckMsg));
				
				txCount = 0;
				packetsSent++;
				radio_send(tx_buffer, sizeof(NonAggAckMsg), currentAckNonA.dl_dst);
			}
		}
	} 
	
	//data collection
	else if(msg.type == DataCollection_Start)
	{
		if(state == IDLE)
		{
			if(sendYourOwnData == true)
			{
				printf("Send own collected data\r\n");
				currentPolicy = DATACOLLECTION;
				//its a leaf
				//send data to parent, change state to waitdataack
				setRetransmissionPolicy(currentPolicy, 1); //a is 1 since this is a leaf
				
				state = TX_DATA;
				
				leds_on(LED_GREEN);
				
				if(tx_buffer_inuse == false)
				{
					tx_buffer_inuse = true;
					
					currentPktDataCollection.dl_dst = msg.dl_src;
					currentPktDataCollection.dl_src = NODE_ID;
					currentPktDataCollection.src = NODE_ID;
					currentPktDataCollection.sent = packetsSent;
					currentPktDataCollection.received = packetsReceived;
					currentPktDataCollection.retransmissions = retransmissionsRequired;
					currentPktDataCollection.final = TRUE;
					
					memcpy(&tx_buffer, &currentPktDataCollection, sizeof(DataCollectionDataMsg));
					
					txCount = 0;
					radio_send(tx_buffer, sizeof(DataCollectionDataMsg),currentPktDataCollection.dl_dst);
				}
			}
			else if(numberOfChildren != 0)
			{
				printf("send data collection startack\r\n");
				//send startack, change state to waitstartackack
				state = TX_START_ACK;
				
				leds_on(LED_GREEN);
				
				if(tx_buffer_inuse == false)
				{
					tx_buffer_inuse = true;
					
					currentAckDataCollection.dl_dst = msg.dl_src;
					currentAckDataCollection.dl_src = NODE_ID;
					currentAckDataCollection.type = DataCollection_Ack;
					
					parentNode = msg.dl_src;
					
					memcpy(&tx_buffer, &currentAckDataCollection, sizeof(DataCollectionAckMsg));
					
					txCount = 0;
					radio_send(tx_buffer, sizeof(DataCollectionAckMsg), currentAckDataCollection.dl_dst);
				}
			}
			else
			{
				printf("Leaf - send up own collected data\r\n");
				currentPolicy = DATACOLLECTION;
				//its a leaf
				//send data to parent, change state to waitdataack
				setRetransmissionPolicy(currentPolicy, 1); //a is 1 since this is a leaf
				
				state = TX_DATA;
				
				leds_on(LED_GREEN);
				
				if(tx_buffer_inuse == false)
				{
					tx_buffer_inuse = true;
					
					currentPktDataCollection.dl_dst = msg.dl_src;
					currentPktDataCollection.dl_src = NODE_ID;
					currentPktDataCollection.src = NODE_ID;
					currentPktDataCollection.sent = packetsSent;
					currentPktDataCollection.received = packetsReceived;
					currentPktDataCollection.retransmissions = retransmissionsRequired;
					currentPktDataCollection.final = TRUE;
					
					memcpy(&tx_buffer, &currentPktDataCollection, sizeof(DataCollectionDataMsg));
					
					txCount = 0;
					radio_send(tx_buffer, sizeof(DataCollectionDataMsg),currentPktDataCollection.dl_dst);
				}
			}
		}
	}
	else if(msg.type == DataCollection_Ack)
	{
		if(state == WAIT_DATA)
		{
			state = TX_START_ACK_ACK;
			printf("Send data collection start ackack");
			if(tx_buffer_inuse == false)
			{
				tx_buffer_inuse = true;
				
				//send startackack back to its child, change state to waitdata
				DataCollectionAckAckMsg startAckAckPktDataCollection;
				startAckAckPktDataCollection.dl_dst = msg.dl_src;
				startAckAckPktDataCollection.dl_src = NODE_ID;
				startAckAckPktDataCollection.type = DataCollection_AckAck;
				
				memcpy(&tx_buffer, &startAckAckPktDataCollection, sizeof(DataCollectionAckAckMsg));
				
				txCount = 0;
				radio_send(tx_buffer, sizeof(DataCollectionAckAckMsg), startAckAckPktDataCollection.dl_dst);
			}
		}
		else if(state == WAIT_DATA_ACK)
		{
			//send dataackack
			
			state = TX_DATA_ACK_ACK;
			
			leds_on(LED_ORANGE);
			printf("Send data collection ackack\r\n");
			if(tx_buffer_inuse == false)
			{
				tx_buffer_inuse = true;
				
				//send dataackack back to its child, then go to sleep. TODO: Can you go straight to sleep? Check notes
				DataCollectionAckAckMsg dataAckAckPktDataCollection;
				dataAckAckPktDataCollection.dl_dst = msg.dl_src;
				dataAckAckPktDataCollection.dl_src = NODE_ID;
				dataAckAckPktDataCollection.type = DataCollection_AckAck;
				
				memcpy(&tx_buffer, &dataAckAckPktDataCollection, sizeof(DataCollectionAckAckMsg));
				
				txCount = 0;
				radio_send(tx_buffer, sizeof(DataCollectionAckAckMsg), dataAckAckPktDataCollection.dl_dst);
			}
		}
	}
	else if(msg.type == DataCollection_AckAck)
	{
		if(state == WAIT_START_ACK_ACK)
		{
			//send start to one of its children, change state to waitstartack
			state = TX_START;
			resend = true;
			
			printf("Send data collection start to a child");
			if(tx_buffer_inuse == false)
			{
				tx_buffer_inuse = true;
				
				DataCollectionStartMsg startpktDataCollection;
				startpktDataCollection.dl_dst = children[currentChild];
				startpktDataCollection.dl_src = NODE_ID;
				startpktDataCollection.type = DataCollection_Ack;
				
				memcpy(&tx_buffer, &startpktDataCollection, sizeof(DataCollectionStartMsg));
				
				txCount = 0;
				radio_send(tx_buffer, sizeof(DataCollectionStartMsg), startpktDataCollection.dl_dst);
			}
		}
		else if(state == WAIT_DATA_ACK_ACK) //could use a new state for this too
		{
			// if done = true && currentChild < number of children
			//increment current child
			//if done == true && currentChild == number of children
			
			//if done == false, just send on data
			
			state = TX_DATA;
			printf("Send up collected data\r\n");
			if(currentChildDataDataCollection.final == TRUE)
			{
				if(currentChild == numberOfChildren)
				{
					//set up so with next received start send your own data.
					sendYourOwnData = true;
				}
				if(currentChild < numberOfChildren)
				{
					currentChild++;
				}
			}
			currentChildDataDataCollection.dl_dst = parentNode;
			
			if(tx_buffer_inuse == false)
			{
				tx_buffer_inuse = true;
				
				memcpy(&tx_buffer, &currentChildDataDataCollection, sizeof(DataCollectionDataMsg));
				
				txCount = 0;
				radio_send(tx_buffer, sizeof(DataCollectionDataMsg), currentChildDataDataCollection.dl_dst);
			}
			
			
		}
	}
	else if(msg.type == DataCollection_Data)
	{
		printf("Send data collection ack\r\n");
		if(state == WAIT_DATA)
		{
			//save the data somewhere and send a dataack
			state = TX_DATA_ACK;
			
			currentChildDataDataCollection.dl_dst = msgdata[0];
			currentChildDataDataCollection.dl_src = msgdata[2];
			currentChildDataDataCollection.src = msgdata[4];
			currentChildDataDataCollection.sent = msgdata[6];
			currentChildDataDataCollection.received = msgdata[8];
			currentChildDataDataCollection.retransmissions = msgdata[10];
			currentChildDataDataCollection.final = msgdata[12];
			
			setRetransmissionPolicy(currentPolicy, 1);
			
			currentAckDataCollection.dl_dst = currentChildDataDataCollection.dl_src;
			currentAckDataCollection.dl_src = NODE_ID;
			currentAckDataCollection.type = DataCollection_Ack;
			
			if(tx_buffer_inuse == false)
			{
				tx_buffer_inuse = true;
				
				memcpy(&tx_buffer, &currentAckDataCollection, sizeof(DataCollectionAckMsg));
				
				txCount = 0;
				radio_send(tx_buffer, sizeof(DataCollectionAckMsg), currentAckDataCollection.dl_dst);
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
	
	if(state == TX_START_ACK)
	{
		state = WAIT_START_ACK_ACK;
	}
	else if(state == TX_START)
	{
		state = WAIT_DATA; //could receive data or a start ack, depending on if the node is leaf or not. So if the message type is ack and we're in receive data mode, then we know that message is a start_ack.
	}
	else if(state == TX_START_ACK_ACK)
	{
		state = WAIT_DATA;
	}
	else if(state == TX_DATA_ACK)
	{
		state = WAIT_DATA_ACK_ACK;
	}
	else if(state == TX_DATA_ACK_ACK)
	{
		state = IDLE;
		//TODO: go to sleep for a calculated amount of time.
	}
	else if(state == TX_DATA)
	{
		state = WAIT_DATA_ACK;
	}
}

void application_button_pressed(){}
	
void application_button_released(){}