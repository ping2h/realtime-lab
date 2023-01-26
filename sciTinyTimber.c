#include "TinyTimber.h"
#include "sciTinyTimber.h"

void sci_init(Serial *self, int unused) {
    self->count = self->head = self->tail = 0;

	USART_ITConfig( USART1, USART_IT_RXNE, ENABLE);
	USART_ITConfig( USART1, USART_IT_TXE, DISABLE);
	NVIC_SetPriority( USART1_IRQn, __IRQ_PRIORITY);
	NVIC_EnableIRQ( USART1_IRQn);
  
}

static void outc(Serial *self, char c){
    if (self->count < SCI_BUFSIZE) {
        self->buf[self->head] = c;
        self->head = (self->head + 1) % SCI_BUFSIZE;
	    self->count++;
    }
//	else
//		Should handle overflow;
}

void sci_write(Serial *self, char *p) {
	if (self->count == 0)
        USART_ITConfig( self->port, USART_IT_TXE, ENABLE);

    while (*p != '\0') {
        if (*p == '\n')
            outc(self, '\r');
        outc(self, *p++);
    }
}

void sci_writechar(Serial *self, int c) {
    if (self->count == 0)
        USART_ITConfig( self->port, USART_IT_TXE, ENABLE);
    outc(self, c);
}

int sci_interrupt(Serial *self, int unused) {
    if (USART_GetFlagStatus( self->port, USART_FLAG_RXNE) == SET) {     // Data received
		int c;
		
		c = USART_ReceiveData( self->port);
		
        if (self->obj) {
			ASYNC(self->obj, self->meth, c);
			doIRQSchedule = 1;
		}
    } 
    
    if (USART_GetFlagStatus(self->port, USART_FLAG_TXE) == SET) {       // Transmit buffer empty
        if (self->count > 0) {
            USART_SendData( self->port, self->buf[self->tail]);
            self->tail = (self->tail + 1) % SCI_BUFSIZE;
            self->count--;
            if (self->count == 0)
				USART_ITConfig( self->port, USART_IT_TXE, DISABLE);
        } else {
            USART_ITConfig( self->port, USART_IT_TXE, DISABLE);  
        }
    }
	return 0;
}
