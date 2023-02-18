#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "semaphore.h"  // semaphore
#define TRUE 1
#define FALSE 0

/* 
 * D : enable/disable deadline
 *
 * M: mute/unmute
 *
 * up arrow: increase volume
 * 
 * down arrow: decrease volume
 * 
 * left arrow: decrease background load
 *
 * right arrow: increase background load
 */

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
    Time deadline;
} ToneGenerator;

typedef struct {                    
    Object super;
    int background_loop_range;
    Time deadline;
} Loader;

// typedef struct {
//     Time start;
//     Time totalMeasuredTime;
//     Time largestMeasuredTime;
//     int timeSamples;
// } WCETSampler;


int* dac = (int *)0x4000741C;

///////////////////////////////////////////////////////////
void reader(App*, int);
void receiver(App*, int);

void tick(ToneGenerator*, int);
void upVolume(ToneGenerator*, int);
void downVolume(ToneGenerator*, int);
void mute(ToneGenerator*, int);
void enableDeadlineTG(ToneGenerator*, int);
void lockRequest(ToneGenerator*, int);
int  checkMuted(ToneGenerator*, int);

void backgroundLoop(Loader*, int);
void increaseLoad(Loader*, int);
void decreaseLoad(Loader*, int);
void enableDeadlineLD(Loader*, int);

void wcetBegin(WCETSampler*);
void wcetEnd(WCETSampler*);

///////////////////////////////////////////////////////////
App app = { initObject(), 0, 'X' };
Serial sci0 = initSerial(SCI_PORT0, &app, reader);
Semaphore muteVolumeSem = initSemaphore(1);       // lock the tg when is muted
Can can0 = initCan(CAN_PORT0, &app, receiver);
ToneGenerator tg = {initObject(),initCallBlock(), 500, true, 5, FALSE, USEC(100)}; // 500 USEC 650USEC 931USEC
Loader ld = {initObject(), 1000, USEC(1300)};
WCETSampler wcettg = {0,0,0,0};
WCETSampler wcetld = {0,0,0,0};

///////////////////////////////////////////////////////////
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
        ASYNC(&tg, lockRequest, (int)upVolume);
        break;
    case 31:  //down
        ASYNC(&tg, lockRequest, (int)downVolume);
        break;
    case 28:  // decrease process load
        ASYNC(&ld, decreaseLoad, 0);
        break;
    case 29:  // increase process load
        ASYNC(&ld, increaseLoad, 0);
        break;
    case 'M': //  mute/unmute
        if (SYNC(&tg, checkMuted, 0))        // sycn will return a value
        {
            ASYNC(&tg, lockRequest, (int)mute);
        } else {
            ASYNC(&tg, mute, 0);  
        }
         
        break;
    case 'D': // deadline enable/disable
        ASYNC(&tg, enableDeadlineTG, 0);
        ASYNC(&ld, enableDeadlineLD, 0);
        break;

    default:
        break;
    }
}

void startApp(App *self, int arg) {

    SCI_INIT(&sci0);  // ?
    SCI_WRITE(&sci0, "Hello, hello...\n");
    ASYNC(&tg, tick, 0);                   // follow correct paradigm
    ASYNC(&ld, backgroundLoop, 0);        

}

///////////////////////////////////////////////////////////
// tone generator
void tick(ToneGenerator *self, int c) {
    for (size_t i = 0; i < 100; i++)
    {
         if (self->lh)   
    {
        *dac = self->volume;
        self->lh = false;
    } else {
        *dac = 0;
        self->lh = true;
    }
    }
    SEND(USEC(self->period), self->deadline, self, tick, c);   // step 2
    
    
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
        self->mute = FALSE;
        SCI_WRITE(&sci0, "unmuted\n");
        ASYNC(&muteVolumeSem, Signal, 0);    //  realse lock
    }
    
}

void enableDeadlineTG(ToneGenerator *self, int c) {
    if (self->deadline == 0)
    {
        self->deadline = USEC(100);
        SCI_WRITE(&sci0, "enable deadline\n");
    } else {
        self->deadline = 0;
        SCI_WRITE(&sci0, "disable deadline\n");
    }
    
}

void lockRequest(ToneGenerator* self, int c) {
    self->callBlock.obj = self;
    self->callBlock.meth = (Method)c;
    ASYNC(&muteVolumeSem, Wait, (int)&self->callBlock);
}


int checkMuted(ToneGenerator* self, int c) {
    if (!self->mute)
    {
        return TRUE;
    } else {
        return FALSE;
    }
}

///////////////////////////////////////////////////////////
// loader
void backgroundLoop(Loader* self, int c) {
    for (size_t i = 0; i < self->background_loop_range; i++)
    {
        
    }
    SEND(USEC(1300),self->deadline, self, backgroundLoop, c);  // step 2 
}

void increaseLoad(Loader* self, int c) {
    char tempBuffer[50];
    self->background_loop_range += 500;
    sprintf(tempBuffer, "background loop range: %d\n", self->background_loop_range);
    SCI_WRITE(&sci0, tempBuffer);
}

void decreaseLoad(Loader* self, int c) {
    char tempBuffer[50];
    if (self->background_loop_range > 0)
    {
        self->background_loop_range -= 500;
    }
    sprintf(tempBuffer, "background loop range: %d\n", self->background_loop_range);
    SCI_WRITE(&sci0, tempBuffer);
    
}

void enableDeadlineLD(Loader *self, int c) {
    if (self->deadline == 0)
    {
        self->deadline = USEC(1300);
    } else {
        self->deadline = 0;
    }
}

///////////////////////////////////////////////////////////
// void wcetBegin(WCETSampler* self) {
//     self->start = CURRENT_OFFSET();
// }

// void wcetEnd(WCETSampler* self) {
//     Time end = CURRENT_OFFSET();
//     if(self->timeSamples < 500) {
//         self->timeSamples += 1;

//         Time elapsed = end - self->start;
//         self->totalMeasuredTime += elapsed;
//         if(elapsed > self->largestMeasuredTime) {
//             self->largestMeasuredTime = elapsed;
//         }
//     } else {
//         if (self->timeSamples == 500) {
//             const int STRLEN = 100;
//             char s[STRLEN];
            
//             long worst = USEC_OF(self->largestMeasuredTime);
//             long average = USEC_OF(self->totalMeasuredTime) / self->timeSamples;

//             snprintf(s, STRLEN, "Worst: %ld\nAverage: %ld\nSamples: %d",  worst, average, self->timeSamples);
//             SCI_WRITE(&sci0, s);

//             self->timeSamples += 1;
//         }
//     }
// }

///////////////////////////////////////////////////////////
int main() {
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}
