#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "semaphore.h"  // semaphore


typedef struct {
    Object super;
    int count;
    char c;
} App;

typedef struct {
    Object super;
    CallBlock callBlock;
    int period;
    bool lh;    // 1 or 0
    int volume;
    int mute;
} ToneGenerator;

int* dac = (int *)0x4000741C;



void reader(App*, int);
void receiver(App*, int);
void tick(ToneGenerator*, int);
void upVolume(ToneGenerator*, int);
void downVolume(ToneGenerator*, int);
void mute(ToneGenerator*, int);

App app = { initObject(), 0, 'X' };
Serial sci0 = initSerial(SCI_PORT0, &app, reader);
Semaphore muteVolumeSem = initSemaphore(1);       // lock the tg when is muted
Can can0 = initCan(CAN_PORT0, &app, receiver);
ToneGenerator tg = {initObject(),initCallBlock(), 500, true, 5, 0}; // 500 USEC

// app
void receiver(App *self, int unused) {
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
    SCI_WRITE(&sci0, "Can msg received: ");
    SCI_WRITE(&sci0, msg.buff);
}

void reader(App *self, int c) {
    
    SCI_WRITE(&sci0, "Rcv: \'");
    SCI_WRITECHAR(&sci0, c);
    SCI_WRITE(&sci0, "\'\n");
    switch (c)
    {
    case 30:   //up
        tg.callBlock.obj = &tg;
        tg.callBlock.meth = upVolume;
        ASYNC(&muteVolumeSem, Wait, (int)&tg.callBlock);
        break;
    case 31:  //down
        tg.callBlock.obj = &tg;
        tg.callBlock.meth = downVolume;
        ASYNC(&muteVolumeSem, Wait, (int)&tg.callBlock);
        
        break;
    case 'M': //  mute/unmute
        if (!tg.mute)                                               // mute need go with lock
        {
            tg.callBlock.obj = &tg;
            tg.callBlock.meth = mute;
            ASYNC(&muteVolumeSem, Wait, (int)&tg.callBlock);
            break; 
        } 
        ASYNC(&tg, mute, 0);   // ummute with no lock
        break;
    default:
        break;
    }
}

void startApp(App *self, int arg) {

    SCI_INIT(&sci0);  // ?
    SCI_WRITE(&sci0, "Hello, hello...\n");
    tick(&tg, 0);

    
}

// tone generator
void tick(ToneGenerator *self, int c) {
    if (self->lh)   
    {
        *dac = self->volume;
        self->lh = false;
    } else {
        *dac = 0;
        self->lh = true;
    }
    AFTER(USEC(self->period/2), self, tick, c);
    
}

void upVolume(ToneGenerator *self, int c) {
    char tempBuffer[50];
    if (self->volume < 25)
    {
        self->volume++;
    }
    sprintf(tempBuffer, "volume: %d\n", self->volume);
    SCI_WRITE(&sci0, tempBuffer);
    ASYNC(&muteVolumeSem, Signal, 0);
}

void downVolume(ToneGenerator *self, int c) {
    char tempBuffer[50];
    if (self->volume > 1)
    {
        self->volume--;
    }
    sprintf(tempBuffer, "volume: %d\n", self->volume);
    SCI_WRITE(&sci0, tempBuffer);
    ASYNC(&muteVolumeSem, Signal, 0);
}

void mute(ToneGenerator *self, int c) {  
    if(!self->mute) {
        self->mute = self->volume;
        self->volume = 0;
        SCI_WRITE(&sci0, "muted\n");
    } else {
        self->volume = self->mute;
        self->mute = 0;
        SCI_WRITE(&sci0, "unmuted\n");
        ASYNC(&muteVolumeSem, Signal, 0);    //  realse lock
    }
    
}


int main() {
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
	
    TINYTIMBER(&app, startApp, 0);
    return 0;
}
