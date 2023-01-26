#ifndef SCI_TINYT_H
#define SCI_TINYT_H

#include "stm32f4xx.h"
#include "stm32f4xx_usart.h"

#define SCI_BUFSIZE  1024

typedef struct {
    Object super;
    USART_TypeDef *port;
    Object *obj;
    Method meth;
    int head;
    int tail;
    int count;
    char buf[SCI_BUFSIZE];
} Serial;

#define initSerial(port, obj, meth) \
    { initObject(), port, (Object*)obj, (Method)meth, 0, 0, 0 }

#define SCI_PORT0   (USART_TypeDef *)(USART1)
#define	SCI_IRQ0	IRQ_USART1

void sci_init(Serial *sci, int unused);
void sci_write(Serial *sci, char *buf);
void sci_writechar(Serial *sci, int ch);

#define SCI_INIT(sci)           SYNC(sci, sci_init, 0)
#define SCI_WRITE(sci,buf)      SYNC(sci, sci_write, buf)
#define SCI_WRITECHAR(sci,ch)   SYNC(sci, sci_writechar, ch)

int sci_interrupt(Serial *self, int unused);

#endif
