#ifndef CAN_TINYT_H
#define CAN_TINYT_H

#include "stm32f4xx.h"
#include "stm32f4xx_can.h"

typedef unsigned char uchar;

typedef struct {
	uchar msgId;  //Valid values: 0-127
	uchar nodeId; //Valid values: 0-15
	uchar length;
	uchar buff[8];
} CANMsg;

#define CAN_BUFSIZE 8

typedef struct {
	Object super; 
	CAN_TypeDef* port;
	Object *obj;
	Method meth;
	int head;
	int tail;
	int count;
	CANMsg iBuff[CAN_BUFSIZE];
} Can;

#define initCan(port, obj, meth)  { initObject(), port, (Object*)obj, (Method)meth, 0, 0, 0}

#define CAN_PORT0   (CAN_TypeDef *)(CAN1)
#define	CAN_IRQ0	IRQ_CAN1

void can_init(Can *obj, int unused);
int can_receive(Can *obj, CANMsg *msg);
int can_send(Can *obj, CANMsg *msg);

#define CAN_INIT(can)               SYNC(can, can_init, 0)
#define CAN_SEND(can, msgptr)       SYNC(can, can_send, msgptr)
#define CAN_RECEIVE(can, msgptr)    SYNC(can, can_receive, msgptr)

void can_interrupt(Can *self, int unused);

#endif
