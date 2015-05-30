#ifndef Agg_H
#define Agg_H

#define TRUE 1
#define FALSE 2
enum {
	AM_Agg = 6,         // AM number
	Agg_ACK_TMO = 32,    // 32 mS local response timeout
	Agg_ACKACK_TMO = 32    // 32 mS local response timeout
};

enum Agg_Type {
	Agg_Data = 0,
	Agg_Ack,
	Agg_AckAck,
	Agg_Start,
	NonAgg_Start,
	NonAgg_Data,
	NonAgg_Ack,
	NonAgg_AckAck,
	Agg_TYPES
};

typedef struct AggHeader {
	uint16_t dl_dst;    // destination mac address (shouldn't need either of these
	// two - but there is no i/f to extract these from the MAC packet)
	uint16_t dl_src;    // sender mac address
	uint8_t type;       // Agg message type
} AggHeader;

typedef struct AggStartMsg {
	uint16_t dl_dst;
	uint16_t dl_src;
	uint8_t type;  // =Agg_Start
	uint16_t policy;
} AggStartMsg;

typedef struct AggAckMsg {
	uint16_t dl_dst;
	uint16_t dl_src;
	uint8_t type;  // =Agg_Ack
} AggAckMsg;

typedef struct AggDataMsg {
	uint16_t dl_dst;
	uint16_t dl_src;
	uint8_t type;  // =Agg_Data
	uint16_t nodesFeatured;
	uint16_t temp; // max temperature recorded
	uint8_t temp_src;
} AggDataMsg;

typedef struct AggAckAckMsg {
	uint16_t dl_dst;
	uint16_t dl_src;
	uint8_t type;  // =Agg_AckAck
} AggAckAckMsg;

typedef struct NonAggStartMsg {
	uint16_t dl_dst;
	uint16_t dl_src;
	uint8_t type; // =NonAgg_Start
} NonAggStartMsg;

typedef struct NonAggDataMsg {
	uint16_t dl_dst;
	uint16_t dl_src;
	uint8_t type;  // =NonAgg_Data
	uint16_t temp; // max temperature recorded
	uint8_t temp_src;
	uint16_t final;
} NonAggDataMsg;

typedef struct NonAggAckMsg {
	uint16_t dl_dst;
	uint16_t dl_src;
	uint8_t type;
} NonAggAckMsg;

typedef struct NonAggAckAckMsg {
	uint16_t dl_dst;
	uint16_t dl_src;
	uint8_t type;
} NonAggAckAckMsg;
#endif
