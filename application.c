#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "semaphore.h"  
#include "sioTinyTimber.h"
#define TRUE 1
#define FALSE 0
#define HOLD 0
#define MOMENTARY 1
#define CONDUCTOER 1
#define MUSICIAN 2
#define OPCODE 0
#define RECEIVER 1




/* 
 * User Button: Tap tempo
 *
 * S: start/stop the playing of melody  
 * 
 * K: change key
 *
 * M: mute/unmute
 *
 * T: change tempo
 * 
 * up arrow: increase volume
 * 
 * down arrow: decrease volume
 * 
 * Integer: end with 'e'
 */


typedef struct {
    Object super;
    int index;
    char buffer[50];
    int mod;
    int numOfNodes;   // 
    int Id;
} App;

typedef struct {
    Object super;
    CallBlock callBlock;
    int period;
    bool lh;    // 1 or 0
    int volume;
    int mute;
    Time deadline;
    bool muted;
    int start;
} ToneGenerator;


typedef struct {
    Object super;
    int key;
    int tempo;
    int frequency_index;
    int start;
    
} MusicPlayer;


typedef struct {
    Object super;
    Timer timer;
    Time last;
    int count;
    int mod;
    Time smaples[3];
} Button;

typedef struct 
{   
    Object super;
    int tempo;
    int start;
} LED;




///////////////////////////////////////////////////////////
int* dac = (int *)0x4000741C;
int frequency_indices[32] = {0,2,4,0,0,2,4,0,4,5,7,4,5,7,7,9,7,5,4,0,7,9,7,5,4,0,0,-5,0,0,-5,0};
int periods[] = {2024,1911,1803,1702,1607,1516,1431,1351,1275,1203,1136,1072,1012,955,901,851,803,758,715,675,637,601,568,536,506};
char tempos[] = {'a','a','a','a','a','a','a','a','a','a',
                'b','a','a','b','c','c','c','c','a','a',
                'c','c','c','c','a','a','a','a','b','a',
                'a','b'};
bool keybool = false;  
bool tempobool = false;



///////////////////////////////////////////////////////////
void reader(App*, int);
void receiver(App*, int);
void newrec(App*, int);
// initally broadcast
void pitchSelfRPC(int);
int getId(App*, int);  //get the ID of the node

void press(Button*, int);
void check1Sec(Button*, int);
int  timeToBPM(Time);
bool checkComparable(Time, Time, Time);


void tick(ToneGenerator*, int);
void upVolume(ToneGenerator*, int);
void downVolume(ToneGenerator*, int);
void mute(ToneGenerator*, int);
void lockRequest(ToneGenerator*, int);
int  checkMuted(ToneGenerator*, int);
void muteGap(ToneGenerator*, int);
void unMuteGap(ToneGenerator*, int);
void changeTone(ToneGenerator*, int);
void startTG(ToneGenerator*, int);
void stopTG(ToneGenerator*, int);

void play(MusicPlayer*, int);
void changeKey(MusicPlayer*, int);
void changeTempo(MusicPlayer*, int);
int  checkStart(MusicPlayer*, int);
void start(MusicPlayer*, int);
void stop(MusicPlayer*, int);
void play1Note(MusicPlayer*, int);
void stopAndSend(MusicPlayer*, int);
void play1NoteRPC(int, int);
void startDS(MusicPlayer*, int);
void startRPC(int);

void blink(LED*, int);
void changeLed(LED*, int);
void startled(LED*, int);
void stopled(LED*, int);



///////////////////////////////////////////////////////////
App app = { initObject(), 0 , {}, CONDUCTOER, 1, 2}; // id 0, H priority
Serial sci0 = initSerial(SCI_PORT0, &app, reader);
Semaphore muteVolumeSem = initSemaphore(1);       // lock the tg when is muted
Can can0 = initCan(CAN_PORT0, &app, newrec); 
Button bt = {initObject(), initTimer(), 0, 0, MOMENTARY, {}};
SysIO button = initSysIO(SIO_PORT0, &bt, press);
ToneGenerator tg = {initObject(),initCallBlock(), 500, true, 5, FALSE, USEC(100), false,TRUE}; // 500 USEC 650USEC 931USEC
MusicPlayer mp = {initObject(), 0, 120, 0, TRUE};
LED led = {initObject(), 120, TRUE};




///////////////////////////////////////////////////////////
// app
void newrec(App *self, int unused) {
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
    char tempBuffer[50];

    sprintf(tempBuffer, "receive a message from 111111node: %d\n", msg.nodeId);
    SCI_WRITE(&sci0, tempBuffer);
    sprintf(tempBuffer, "receiver : %d\n", msg.buff[RECEIVER]);
    SCI_WRITE(&sci0, tempBuffer);
    sprintf(tempBuffer, "op code: %d\n", msg.buff[OPCODE]);
    SCI_WRITE(&sci0, tempBuffer);
    if (msg.buff[RECEIVER] != 0 && msg.buff[RECEIVER] != self->Id) {
        return;
    }
    
    int op = msg.buff[OPCODE];
    switch (op)
    {
    case 0:
        self->numOfNodes++;
        SCI_WRITE(&sci0, "nodes: 2\n");
        break;
    case 1:
        ASYNC(&mp, startDS, 0);
        SCI_WRITE(&sci0, "ready to start \n");
        break;
    case 4:
        ASYNC(&mp, play1Note, msg.buff[5]);
        sprintf(tempBuffer, "play1note index:%d\n", msg.buff[5]);
        SCI_WRITE(&sci0, tempBuffer);
        break;
    default:
        break;
    }
}
void receiver(App *self, int unused) {
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
    char tempBuffer[50];
    int msg_id = msg.msgId;
    int bufferValue = atoi(msg.buff);
    switch (msg_id) {
        case 1: // change key
            sprintf(tempBuffer, "Can received key value: %d\n", bufferValue);
            SCI_WRITE(&sci0, tempBuffer);
            if (self->mod == MUSICIAN) {
                ASYNC(&mp, changeKey, bufferValue);
            }
            break;
        case 2: // change tempo
            sprintf(tempBuffer, "Can received tempo value: %d\n", bufferValue);
            SCI_WRITE(&sci0, tempBuffer);
            if (self->mod == MUSICIAN) {
                ASYNC(&mp, changeTempo, bufferValue);
            }
            break;
        case 3: // mute
            SCI_WRITE(&sci0, "Can received mute/unmute signal.\n");
            if (self->mod == MUSICIAN) {
                if (SYNC(&tg, checkMuted, 0))        // sycn will return a value
                {
                    ASYNC(&tg, lockRequest, (int)mute);
                } else {
                    ASYNC(&tg, mute, 0);  
                }
            }
            break;
        case 4: // increase volume
            SCI_WRITE(&sci0, "Can received increase vol signal.\n");
            if (self->mod == MUSICIAN) {
                ASYNC(&tg, lockRequest, (int)upVolume);
            }
            break;
        case 5: // decrease volume
            SCI_WRITE(&sci0, "Can received decrease vol signal.\n");
            if (self->mod == MUSICIAN) {
               ASYNC(&tg, lockRequest, (int)downVolume);
            }
            
            break;
        case 6:
            SCI_WRITE(&sci0, "Can received start/stop signal.\n");
            if (self->mod == MUSICIAN) {
               if(SYNC(&mp, checkStart, 0)) {
                    ASYNC(&mp, stop, 0);
                } else {
                    ASYNC(&mp, start, 0);
                }
            }
        default: 
            break;
    }
}

void reader(App *self, int c) {
    int bufferValue;
    char tempBuffer[50];
    CANMsg msg;
    SCI_WRITE(&sci0, "Rcv: \'");
    SCI_WRITECHAR(&sci0, c);
    SCI_WRITE(&sci0, "\'\n");
    msg.nodeId = 1;
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
        // change key 
        if (keybool) {
            if (bufferValue<-5 || bufferValue > 5)
            {
                SCI_WRITE(&sci0, " -5<=key<=5, try again!\n");
                break;
            }
            if (self->mod == CONDUCTOER) {
                ASYNC(&mp, changeKey, bufferValue);
            }
            keybool = false;
            msg.msgId = 1;
        }
        // change tempo
        if (tempobool) {
            if (bufferValue< 60 || bufferValue > 240)
            {
                SCI_WRITE(&sci0, " 60<=tempo<=240, try again!\n");
                break;
            }
            if (self->mod == CONDUCTOER) {
                ASYNC(&mp, changeTempo, bufferValue);
            }
            tempobool = false;
            msg.msgId = 2;
        }
        CAN_SEND(&can0, &msg);
        break;
    case 30:   //up
        msg.length = 0;
        msg.msgId = 4;
        if (self->mod == CONDUCTOER) {
            ASYNC(&tg, lockRequest, (int)upVolume);
        }
        CAN_SEND(&can0, &msg);
        break;
    case 31:  //down
        msg.length = 0;
        msg.msgId = 5;
        if (self->mod == CONDUCTOER) {
            ASYNC(&tg, lockRequest, (int)downVolume);
        }
        CAN_SEND(&can0, &msg);
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
        msg.length = 0;
        msg.msgId = 3;
        if (self->mod == CONDUCTOER) {
            if (SYNC(&tg, checkMuted, 0))        // sycn will return a value
            {
                ASYNC(&tg, lockRequest, (int)mute);
            } else {
                ASYNC(&tg, mute, 0);  
            }
        }
            
        CAN_SEND(&can0, &msg);
        break;
    case 'S':
        msg.length = 0;
        msg.msgId = 6;
        if (self->mod == CONDUCTOER) {
            if(SYNC(&mp, checkStart, 0)) {
                ASYNC(&mp, stop, 0);
            } else {
                ASYNC(&mp, start, 0);
            }
        }
        CAN_SEND(&can0, &msg);  
        break;
    case 'C':
        if (self->mod == CONDUCTOER) {
            self->mod = MUSICIAN;
            SCI_WRITE(&sci0, "change to musician mod\n");
        } else {
            self->mod = CONDUCTOER;
            SCI_WRITE(&sci0, "Change to conductoer mod\n");
        }
        break;
    case 'B':   
        pitchSelfRPC(self->Id);
        SCI_WRITE(&sci0, "pitch myself\n");
        break;
    case 'P': // conductor plays the first note and send message to other nodes.
        SYNC(&mp, startDS, 0);
        startRPC(self->Id);
        ASYNC(&mp, play1Note, 0);
        break;
    default:
        break;
    }
   
}

void startRPC(int nodeId) {
    CANMsg msg;
    msg.nodeId = nodeId;
    msg.length = 7;
    msg.buff[OPCODE] = 1; 
    msg.buff[RECEIVER] = 0; // broadcast

    CAN_SEND(&can0, &msg);
    return;
}
void pitchSelfRPC(int nodeId) {
    CANMsg msg;
    msg.nodeId = nodeId;
    msg.length = 7;
    msg.buff[OPCODE] = 0x00; // claim myself
    msg.buff[RECEIVER] = 0x00;  /////
    msg.buff[2] = 0; ///
    CAN_SEND(&can0, &msg);
    return;

}
int getId(App* self, int c) {
    return self->Id;
}
void startApp(App *self, int arg) {
    
    SCI_INIT(&sci0); 
    CAN_INIT(&can0);
    SIO_INIT(&button); 
    SCI_WRITE(&sci0, "Hello, hello...\n");
    ASYNC(&tg, tick, 0);                   
    ASYNC(&mp, play, 0); 
    ASYNC(&mp, stop, 0);
    SIO_TOGGLE(&button);
    
    
    

}




///////////////////////////////////////////////////////////
// tone generator
void tick(ToneGenerator *self, int c) {
    if (!self->start) {
        return;
    }
    if (self->lh)   
    {
        *dac = self->volume;
        self->lh = false;
    } else {
        *dac = 0;
        self->lh = true;
    }
    SEND(USEC(self->period), self->deadline, self, tick, c);   
    
    
}

void upVolume(ToneGenerator *self, int c) {
    char tempBuffer[50];
    if (self->volume < 50)
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
    if(!self->muted) {
        self->mute = self->volume;
        self->volume = 0;
        self->muted = true;
        SCI_WRITE(&sci0, "muted\n");
    } else {
        if (self->mute == 0)
        {
            self->volume = 20;
        } else {      
            self->volume = self->mute;
        }
        self->mute = FALSE;
        self->muted = false;
        SCI_WRITE(&sci0, "unmuted\n");

        ASYNC(&muteVolumeSem, Signal, 0);    //  realse lock
    }
    
}


void lockRequest(ToneGenerator* self, int c) {
    self->callBlock.obj = self;
    self->callBlock.meth = (Method)c;
    ASYNC(&muteVolumeSem, Wait, (int)&self->callBlock);
}


int checkMuted(ToneGenerator* self, int c) {
    if (!self->muted)
    {
        return TRUE;
    } else {
        return FALSE;
    }
}

void muteGap(ToneGenerator* self, int c) {
    if(!self->muted) {
        self->mute = self->volume;
        self->volume = 0;
    }
}
    

void unMuteGap(ToneGenerator* self, int c) {
    if(!self->muted) {
        self->volume = self->mute;
        self->mute = FALSE;
    }
    
}

void changeTone(ToneGenerator* self, int c) {
    self->period = c;
}

void startTG(ToneGenerator* self, int c) {
    self->start = TRUE;

}

void stopTG(ToneGenerator* self, int c) {
    self->start = FALSE;

}




///////////////////////////////////////////////////////////
// music player
void play(MusicPlayer* self, int c) {
    int frequency_index;
    int period;
    double tempoFactor;
    if (!self->start) {
        return;
    }
    if (self->frequency_index==32){
        self->frequency_index = 0;
    }
    if (tempos[self->frequency_index] == 'b') {
        tempoFactor = 2.0;
    } else if (tempos[self->frequency_index] == 'c'){
        tempoFactor = 0.5;
    } else {
        tempoFactor = 1.0;
    }
    SYNC(&tg, muteGap, 0);
    frequency_index = frequency_indices[self->frequency_index] + self->key;
    period = periods[frequency_index+10];
    self->frequency_index++;
    SYNC(&tg, changeTone, period);
    AFTER(MSEC(50), &tg, unMuteGap, 0);
    // SIO_TOGGLE(&button);
    // AFTER(MSEC((int)30000 / self->tempo ), &button, sio_toggle, 0);
    SEND(MSEC((int)60000 / self->tempo * tempoFactor), USEC(100), self, play, 0);
}

/* */
void play1Note(MusicPlayer* self, int noteIndex){
    int frequency_index;
    int period;
    double tempoFactor;
    SCI_WRITE(&sci0, "want to \n");
    if (!self->start) {
        return;
    }
    SCI_WRITE(&sci0, "played");
    if (tempos[noteIndex] == 'b') {
        tempoFactor = 2.0;
    } else if (tempos[noteIndex] == 'c'){
        tempoFactor = 0.5;
    } else {
        tempoFactor = 1.0;
    }
    SYNC(&tg, muteGap, 0);
    frequency_index = frequency_indices[noteIndex] + self->key;
    period = periods[frequency_index+10];
    SYNC(&tg, changeTone, period);
    ASYNC(&tg, startTG, 0);
    ASYNC(&tg, tick, 0);
    AFTER(MSEC(50), &tg, unMuteGap, 0);
    SEND(MSEC((int)60000 / self->tempo * tempoFactor), USEC(100), self, stopAndSend, noteIndex);

}
void changeKey(MusicPlayer* self, int c) {
    keybool = false;
    self->key = c;
}

void changeTempo(MusicPlayer* self, int c) {
    tempobool = false;
    self->tempo = c;
    ASYNC(&led, changeLed, c);
}

int checkStart(MusicPlayer* self, int c) {
    return self->start;
}

void start(MusicPlayer* self, int c) {
    self->start = TRUE;
    ASYNC(&led, startled, 0);
    ASYNC(&tg, startTG, 0);
    ASYNC(&tg, tick, 0);                
    ASYNC(&mp, play, 0);
    ASYNC(&led, blink, 0); 
    SCI_WRITE(&sci0, "startMp\n");
  
}

void stop(MusicPlayer* self, int c) {
    self->frequency_index = 0;
    self->start = FALSE;
    ASYNC(&tg, stopTG, 0);
    ASYNC(&led, stopled, 0);
    SCI_WRITE(&sci0, "stopMp\n");
    

}

void startDS(MusicPlayer* self, int c) {
    self->start = TRUE;
    ASYNC(&led, startled, 0);
    ASYNC(&led, blink, 0); 
    

    return;

}
void stopAndSend(MusicPlayer* self, int noteIndex) {
    int receiverID;
    ASYNC(&tg, stopTG, 0);
    receiverID = (SYNC(&app, getId, 0) + 1) % 2 + 1;   // test 2  /// 3
    play1NoteRPC(noteIndex+1, receiverID);
    SCI_WRITE(&sci0, "stopMp and sent instruction to next processor \n");

}

void play1NoteRPC(int noteIndex, int receiverID) {
    CANMsg msg;
    if (noteIndex == 32){
        noteIndex = 0;
    }
    
    msg.nodeId = SYNC(&app, getId, 0);
    msg.length = 7;
    msg.buff[OPCODE] = 0x04; // claim myself///
    msg.buff[RECEIVER] = receiverID;
    msg.buff[2] = 0;
    msg.buff[3] = 0;
    msg.buff[4] = 0;
    msg.buff[5] = noteIndex;
    CAN_SEND(&can0, &msg);
    SCI_WRITE(&sci0, "rpc\n");
    return;
}


// Button
///////////////////////////////////////////////////////////
void press(Button* self, int c) {
    char tempBuffer[50];
    
    if (self->mod == MOMENTARY) {
        SCI_WRITE(&sci0, "button pressed\n");
        if(self->count == 0) {
            T_RESET(&self->timer);
            self->count += 1;
            AFTER(SEC(1), &bt, check1Sec, self->count);
            return;
        }
        
        Time now = T_SAMPLE(&self->timer);
        Time sinceLast = now - self->last;
        if (sinceLast < MSEC(100)) {
            SCI_WRITE(&sci0, "Ignoring contact bounce\n");
            return;
        }
        self->last = now;

        sprintf(tempBuffer, "msec: %ld\n", sinceLast/100);
        SCI_WRITE(&sci0, tempBuffer);

        self->smaples[self->count - 1] = sinceLast/100;
        self->count++;
        if (self->count == 4) {
            self->count = 0;
            self->last = 0;
            Time average = 0;
            snprintf(tempBuffer, 100, " %ld. %ld.%ld. \n", self->smaples[0], self->smaples[1], self->smaples[2]);
            SCI_WRITE(&sci0, tempBuffer);
            if (checkComparable(self->smaples[0], self->smaples[1], self->smaples[2])) {
                for (size_t i = 0; i < 3; i++)
                {
                    average += self->smaples[i];
                }
                average /= 3;
                int bpm = timeToBPM(average);
                if (bpm >= 30 && bpm <= 300) {
                    snprintf(tempBuffer, 100, "Nice beat. Setting BPM to %d.\n", bpm);
                    SYNC(&mp, changeTempo, bpm);
                    SCI_WRITE(&sci0, tempBuffer);
                } else {
                    snprintf(tempBuffer, 100, "BPM: %d out of range [30..300]\n", bpm);
                    SCI_WRITE(&sci0, tempBuffer);
                }
            } else {
                SCI_WRITE(&sci0, "not comparable length \n");
                return;
            }

        }
        AFTER(SEC(1), &bt, check1Sec, self->count);
    } else {
        SCI_WRITE(&sci0, "button released\n");

        Time now = T_SAMPLE(&self->timer);
        Time sinceLast = now - self->last;
        self->last = now;
        if (sinceLast/100000 < 2) {
            sprintf(tempBuffer, "sec: %ld < 2, hold button 2 sec to reset\n", sinceLast/100000);
            SCI_WRITE(&sci0, tempBuffer);
        } else {
            SCI_WRITE(&sci0, "reset tempo \n");
            sprintf(tempBuffer, "sec: %ld\n", sinceLast/100000);
            SCI_WRITE(&sci0, tempBuffer);
            self->count = 0;
            self->last = 0;
            ASYNC(&mp, changeTempo, 120);
        }
        
        SIO_TRIG(&button, 0);
        self->mod = MOMENTARY;

    }
    
}

void check1Sec(Button* self, int c) {
    if (c == self->count && SIO_READ(&button)==0) {
        SCI_WRITE(&sci0, "holding more than 1 sec, switch to PRESS-AND-HOLD\n");
        self->mod = HOLD;
        SIO_TRIG(&button, 1);
    }

}

int timeToBPM(Time time) {
    return (60.0 / time) * 1000;
}

bool checkComparable(Time a, Time b, Time c) {
    int sum = abs(a-b) + abs(a-c) + abs(b-c);
    if (sum<300)
    {
        return true;
    } else {
        return false;
    }
    
}

///////////////////////////////////////////////////////////

void blink(LED* self, int c) {
    if (!self->start) {
        SIO_WRITE(&button, 1);
        return;
    }
    SIO_TOGGLE(&button);
    AFTER(MSEC((int)30000 / self->tempo ), &button, sio_toggle, 0);
    SEND(MSEC((int)60000 / self->tempo ), USEC(100), self, blink, 0);
}

void changeLed(LED* self, int c) {
    self->tempo = c;
}

void startled(LED* self, int c) {
    self->start = TRUE;


}

void stopled(LED* self, int c) {
    self->start = FALSE;
    
}

///////////////////////////////////////////////////////////
int main() {
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
    INSTALL(&can0, can_interrupt, CAN_IRQ0);
    INSTALL(&button, sio_interrupt, SIO_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}
