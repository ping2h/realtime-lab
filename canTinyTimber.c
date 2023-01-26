#include "TinyTimber.h"
#include "canTinyTimber.h"

//#define	__CAN_LOOPBACK	// Note: requires physical loopback between CAN 1 and 2 jacks

void DUMP(char *s);

//
// Initialize CAN controller
//
void can_init(Can *self, int unused) {
	CAN_InitTypeDef CAN_InitStructure;

    self->count = self->head = self->tail = 0;

#ifdef __CAN_LOOPBACK
	DUMP("NOTE: CAN running in loopback mode!\n\r");
#endif

//#define	__CAN_TxAck // if defined: single transmission, can_send() will wait for message error or acknowledgement
                    //  default: automatic retransmission, can_send() will not wait for message error or acknowledgement
                        
	CAN_StructInit(&CAN_InitStructure);

	CAN_InitStructure.CAN_TTCM = DISABLE;   // time-triggered communication mode = DISABLED
	CAN_InitStructure.CAN_ABOM = DISABLE;   // automatic bus-off management mode = DISABLED
	CAN_InitStructure.CAN_AWUM = DISABLE;   // automatic wake-up mode = DISABLED
#ifdef __CAN_TxAck
	CAN_InitStructure.CAN_NART = ENABLE;    // non-automatic retransmission mode = ENABLED (single transmission)
	DUMP("NOTE: CAN running with Tx Ack!\n\r");
#else
	CAN_InitStructure.CAN_NART = DISABLE;    // non-automatic retransmission mode = DISABLED (retransmit until error or ack)
#endif
	CAN_InitStructure.CAN_RFLM = DISABLE;   // receive FIFO locked mode = DISABLED
	CAN_InitStructure.CAN_TXFP = DISABLE;   // transmit FIFO priority = DISABLED
	CAN_InitStructure.CAN_Mode = CAN_Mode_Normal; // normal CAN mode
	//
	// 42 MHz clock on APB1
	// Prescaler = 7 => time quanta tq = 1/6 us
	// Bit time = tq*(SJW + BS1 + BS2)
	// See figure 346 in F407 - Reference Manual.pdf
	// 
	CAN_InitStructure.CAN_SJW = CAN_SJW_1tq; // synchronization jump width = 1
	CAN_InitStructure.CAN_BS1 = CAN_BS1_3tq; 
	CAN_InitStructure.CAN_BS2 = CAN_BS2_4tq; 
	CAN_InitStructure.CAN_Prescaler = 7; // baudrate 750kbps

	if (CAN_Init(CAN1, &CAN_InitStructure) == CAN_InitStatus_Failed)
		DUMP("\n\rCAN #1 Init failed!\n\r");
//	else
//		DUMP("\n\rCAN #1 successful!\n\r");

	if (CAN_Init(CAN2, &CAN_InitStructure) == CAN_InitStatus_Failed)
		DUMP("CAN #2 Init failed!\n\r");
//	else
//		DUMP("CAN #2 successful!\n\r");

	NVIC_SetPriority( CAN1_RX0_IRQn, __IRQ_PRIORITY);
	NVIC_EnableIRQ( CAN1_RX0_IRQn);
	CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);
}

//
// When a message is received on the can bus, store it in a software
// buffer, notify the listener and clear the receive interrupt.
//
void can_interrupt(Can *self, int unused) {
    uchar index;

    if (CAN_GetFlagStatus( self->port, CAN_FLAG_FMP0) == SET) {           // Data received in FIFO0
//		DUMP("\n\rYay: A CAN #1 FIFO0 IRQ!\n\r");
	}
	else
		DUMP("\n\rStrange: Not a CAN #1 FIFO0 IRQ!\n\r");
	
	if (self->count < CAN_BUFSIZE) {
		CanRxMsg RxMessage;

		CAN_Receive(self->port, CAN_FIFO0, &RxMessage);
		
        self->iBuff[self->head].msgId = (RxMessage.StdId >> 4) & 0x7F;
        self->iBuff[self->head].nodeId = RxMessage.StdId & 0x0F;

        self->iBuff[self->head].length = (RxMessage.DLC & 0x0F);

        for (index = 0; index < self->iBuff[self->head].length; index++) {
            // Get received data
            self->iBuff[self->head].buff[index] = RxMessage.Data[index];
        }
    
        if (self->obj) {
			ASYNC(self->obj, self->meth, (self->iBuff[self->head].msgId<<4) + self->iBuff[self->head].nodeId);
			doIRQSchedule = 1;
		}
        
        self->head = (self->head + 1) % CAN_BUFSIZE;
        self->count++;
    } else {
 		DUMP("\n\rStrange: CAN #1 FIFO0 Full!\n\r");
		// Handle full buffers
        // Now just discards message
    }
}

//
// Copy the first message from the software buffer to the supplied
// message data structure.
//
int can_receive(Can *self, CANMsg *msg){
    uchar index;

    if (self->count > 0) {
        msg->msgId = self->iBuff[self->tail].msgId;
        msg->nodeId = self->iBuff[self->tail].nodeId;
        msg->length = self->iBuff[self->tail].length;

        // Get received data
        for (index = 0; index < msg->length; index++){
            msg->buff[index] = self->iBuff[self->tail].buff[index];
        }

        self->tail = (self->tail + 1) % CAN_BUFSIZE;
        self->count--;
        return 0;
    }
    return 1;
}

//
// Copy the given message to a transmit buffer and send the message
//
int can_send(Can *self, CANMsg *msg){
    uchar index;
	CAN_TypeDef* canport = self->port;
	CanTxMsg TxMessage;
	uint8_t TransmitMailbox = 0;

#ifdef __CAN_LOOPBACK
	canport = CAN2;
#endif
	
	//set the transmit ID, standard identifiers are used, combine IDs
	TxMessage.StdId = (msg->msgId<<4) + msg->nodeId;
	TxMessage.RTR = CAN_RTR_Data;
    TxMessage.IDE = CAN_Id_Standard;
	if (msg->length > 8) 
		msg->length = 8; 
    TxMessage.DLC = msg->length; // set number of bytes to send
	
	for (index = 0; index < msg->length; index++) {
		TxMessage.Data[index] = msg->buff[index]; //copy data to buffer
	}

	TransmitMailbox = CAN_Transmit(canport, &TxMessage);

	if (TransmitMailbox == CAN_TxStatus_NoMailBox) {
		DUMP("CAN TxBuf full!\n\r");
		return 1;
	}
	
#ifdef __CAN_TxAck
	while (CAN_TransmitStatus(canport, TransmitMailbox) == CAN_TxStatus_Pending) ;
	
	if (CAN_TransmitStatus(canport, TransmitMailbox) == CAN_TxStatus_Failed) {
		DUMP("CAN Tx fail!\n\r");
		return 1;
	}
#endif
	
	return 0;
}
