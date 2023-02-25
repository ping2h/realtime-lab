#ifndef SIO_TINYT_H
#define SIO_TINYT_H

#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_exti.h"

typedef struct {
    Object super;
    GPIO_TypeDef *port;
    Object *obj;
    Method meth;
} SysIO;

#define initSysIO(port, obj, meth) \
    { initObject(), port, (Object*)obj, (Method)meth }

#define SIO_PORT0   (GPIO_TypeDef *)(GPIOB)
#define	SIO_IRQ0	IRQ_EXTI9_5

void sio_init(SysIO *sio, int unused);
int sio_read(SysIO *sio, int unused);
void sio_write(SysIO *sio, int val);
void sio_toggle(SysIO *sio, int unused);
void sio_trig(SysIO *sio, int rise);

#define SIO_INIT(sio)       SYNC(sio, sio_init, 0)
#define SIO_READ(sio)       SYNC(sio, sio_read, 0)
#define SIO_WRITE(sio,val)  SYNC(sio, sio_write, val)
#define SIO_TOGGLE(sio)     SYNC(sio, sio_toggle, 0)
#define SIO_TRIG(sio,rise)  SYNC(sio, sio_trig, rise)

int sio_interrupt(SysIO *self, int unused);

#endif
