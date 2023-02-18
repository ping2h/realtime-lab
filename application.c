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
    int index;
    char buffer[50];
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
    int key;
    int tempo;
    int frequency_index;
} MusicPlayer;


int* dac = (int *)0x4000741C;
int frequency_indices[32] = {0,2,4,0,0,2,4,0,4,5,7,4,5,7,7,9,7,5,4,0,7,9,7,5,4,0,0,-5,0,0,-5,0};
int periods[] = {2024,1911,1803,1702,1607,1516,1431,1351,1275,1203,1136,1072,1012,955,901,851,803,758,715,675,637,601,568,536,506};
bool keybool = false;  
bool tempobool = false;


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
void muteGap(ToneGenerator*, int);
void unMuteGap(ToneGenerator*, int);
void changeTone(ToneGenerator*, int);

void play(MusicPlayer*, int);
void changeKey(MusicPlayer*, int);
void changeTempo(MusicPlayer*, int);

///////////////////////////////////////////////////////////
App app = { initObject(), 0 };
Serial sci0 = initSerial(SCI_PORT0, &app, reader);
Semaphore muteVolumeSem = initSemaphore(1);       // lock the tg when is muted
Can can0 = initCan(CAN_PORT0, &app, receiver);
ToneGenerator tg = {initObject(),initCallBlock(), 500, true, 5, FALSE, USEC(100)}; // 500 USEC 650USEC 931USEC
MusicPlayer mp = {initObject(), 0, 120, 0};



///////////////////////////////////////////////////////////
// app
void receiver(App *self, int unused) {
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
    SCI_WRITE(&sci0, "Can msg received: ");
    SCI_WRITE(&sci0, msg.buff);
}

void reader(App *self, int c) {
    int bufferValue;
    char tempBuffer[50];
    SCI_WRITE(&sci0, "Rcv: \'");
    SCI_WRITECHAR(&sci0, c);
    SCI_WRITE(&sci0, "\'\n");
    switch (c)
    {
    case '0' ... '9':
    case '-':
        self->buffer[self->index++] = c;
        break;
    case 'e':
        self->buffer[self->index] = '\0';
        self->index = 0;
        bufferValue = atoi(self->buffer);
        sprintf(tempBuffer, "Entered integer: %d \n", bufferValue);
        SCI_WRITE(&sci0, tempBuffer);
        if (keybool) {
            if (bufferValue<-5 || bufferValue > 5)
            {
                SCI_WRITE(&sci0, " -5<=key<=5, try again!\n");
                break;
            }
            ASYNC(&mp, changeKey, bufferValue);
            break;
        }
        if (tempobool) {
            if (bufferValue< 60 || bufferValue > 240)
            {
                SCI_WRITE(&sci0, " 60<=tempo<=240, try again!\n");
                break;
            }
            ASYNC(&mp, changeTempo, bufferValue);
            break;
        }
        break;
    case 30:   //up
        ASYNC(&tg, lockRequest, (int)upVolume);
        break;
    case 31:  //down
        ASYNC(&tg, lockRequest, (int)downVolume);
        break;
    case 'K':  // change key
        SCI_WRITE(&sci0, "Please input the key(-5~5) you want:\n");
        keybool = true;                       // next input interger is saved as the key 
        break;
    case 'T':  // change Tempo
        SCI_WRITE(&sci0, "Please input the tempo(60~240) you want:\n");
        tempobool = true;                       // next input interger is saved as the tempo
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
        break;

    default:
        break;
    }
}

void startApp(App *self, int arg) {

    SCI_INIT(&sci0);  // ?
    SCI_WRITE(&sci0, "Hello, hello...\n");
    ASYNC(&tg, tick, 0);                   // follow correct paradigm   
    ASYNC(&mp, play, 0); 

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

void muteGap(ToneGenerator* self, int c) {
    self->mute = self->volume;
    self->volume = 0;
}

void unMuteGap(ToneGenerator* self, int c) {
    self->volume = self->mute;
    self->mute = FALSE;
}

void changeTone(ToneGenerator* self, int c) {
    self->period = c;
}





///////////////////////////////////////////////////////////
// music player
void play(MusicPlayer* self, int c) {
    int frequency_index;
    int period;
    if (self->frequency_index==32){
        self->frequency_index = 0;
    }
    SYNC(&tg, muteGap, 0);
    frequency_index = frequency_indices[self->frequency_index] + self->key;
    period = periods[frequency_index+10];
    self->frequency_index++;
    SYNC(&tg, changeTone, period);
    AFTER(MSEC(50), &tg, unMuteGap, 0);
    SEND(MSEC(60000 / self->tempo), USEC(100), self, play, 0);
}

void changeKey(MusicPlayer* self, int c) {
    keybool = false;
    self->key = c;
}

void changeTempo(MusicPlayer* self, int c) {
    tempobool = false;
    self->tempo = c;
}
///////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////
int main() {
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}
