#include "TinyTimber.h"
#include "sioTinyTimber.h"

void sio_init(SysIO *self, int unused) {

    GPIO_WriteBit(GPIOB, GPIO_Pin_0, (BitAction) 0); // Green LED On

	NVIC_SetPriority( EXTI9_5_IRQn, __IRQ_PRIORITY);
	NVIC_EnableIRQ( EXTI9_5_IRQn );
}

int sio_read(SysIO *self, int unused) {
    return GPIO_ReadInputDataBit(self->port, GPIO_Pin_7);
}

void sio_write(SysIO *self, int val) {
    GPIO_WriteBit(self->port, GPIO_Pin_0, (BitAction) val);
}

void sio_toggle(SysIO *self, int unused) {
    GPIO_ToggleBits(self->port, GPIO_Pin_0);
}

void sio_trig(SysIO *self, int rise) {
	EXTI_InitTypeDef EXTI_InitStructure;

    EXTI_StructInit( &EXTI_InitStructure);
    EXTI_InitStructure.EXTI_Line = EXTI_Line7;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = rise ? EXTI_Trigger_Rising : EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init( &EXTI_InitStructure);
}

void DUMP(char *s);

int sio_interrupt(SysIO *self, int unused) {
    if (EXTI_GetITStatus(EXTI_Line7) == SET) {
//        DUMP("\n\rYey! A GPIOB bit7 IRQ!\n\r");

        EXTI_ClearITPendingBit(EXTI_Line7); // remove interrupt request
    
        if (self->obj) {
            ASYNC(self->obj, self->meth, 0);
            doIRQSchedule = 1;
        }
    } else {
        DUMP("\n\rStrange: Not a GPIOB bit7 IRQ!\n\r");
    }

	return 0;
}
