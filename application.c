#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct
{
    Object super;
    int count;
    int index;
    int index1;     //index for intHistory[]
    char buffer[50];
    int intHistory[3];
} App;

bool keybool = false;           // Step 6 
int frequency_indices[32] = {0,2,4,0,0,2,4,0,4,5,7,4,5,7,7,9,7,5,4,0,7,9,7,5,4,0,0,-5,0,0,-5,0};
int periods[] = {2024,1911,1803,1702,1607,1516,1431,1351,1275,1203,1136,1072,1012,955,901,851,803,758,715,675,637,601,568,536,506};

void reader(App *, int);
void receiver(App *, int);
int median(int[], int);
int sum(int[], int);
void period_lookup(int);

App app = {initObject(), 0, 0, 0, {}, {}};
Serial sci0 = initSerial(SCI_PORT0, &app, reader);

void receiver(App *self, int unused)
{
}

void reader(App *self, int c)
{
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
        if (keybool)
        {
            // procedure for step6
            if (bufferValue<-5 || bufferValue > 5)
            {
                SCI_WRITE(&sci0, " -5<=key<=5, try again!\n");
                break;
            }
            
            period_lookup(bufferValue);
            break;
        }
        
        if (self->count+1 > 3)
        {
            //eliminate the oldest one and add the int to that position
            self->index1 = (self->index1 + 1) % 3;
            self->intHistory[self->index1] = bufferValue;
        }
        // <=3
        self->intHistory[self->index1] = bufferValue;
        self->index1++;
        self->count++;
        sprintf(tempBuffer, "Entered integer: %d, sum = %d, median = %d\n", bufferValue, sum(self->intHistory, self->count), median(self->intHistory, self->count));
        SCI_WRITE(&sci0, tempBuffer);
        break;
    case 'F':
        self->count = 0;
        self->index1 = 0;
        SCI_WRITE(&sci0, "The 3-history has been erased\n");
        break;
    case 'K':
        SCI_WRITE(&sci0, "Please input the key(-5~5) you want:\n");
        keybool = true;                       // next interger is saved as the key and not saved in history buffer
        break;


    }
}

/* 
 * Bubble sort
 * helper function for median()
 */ 
void sort(int *history)
{
    int i, j, temp, m;
    m = 3;
    for (i = 0; i < 3; i++)
    {
        int exchange = 0;
        for (j = 0; j < m - 1; j++)
        {
            if (history[j] > history[j + 1])
            {
                temp = history[j];
                history[j] = history[j + 1];
                history[j + 1] = temp;
                exchange = 1;
            }
        }
        m--;
        if (!exchange)
            break;
    }
}

/*
 * function that returns the median of numbers
 */
int median(int history[], int count)
{
    if (count == 1)
    {
        return history[0];
    }
    else if (count == 2)
    {
        return (history[0] + history[1]) / 2;
    }
    else
    {
        sort(history);
        return history[1];
    }
}

/* 
 * sum the integers in the queue 
 * 
 * parameters: a[] is the history buffer, len is length of the buffer 
 * 
 * return the sum 
 * 
 */
int sum(int a[], int len) {
    int sum = 0;
    for (size_t i = 0; i < len; i++)
    {
        sum += a[i];
    }
    return sum;
    
}

/* 
 * takes the key as input from the keyboard (in the form of an integer number) and prints the 
 * periods corresponding to the 32 frequency indices of the Brother John melody
 * for the input key.
 * 
 * */
void period_lookup(int key){        //step 6
    keybool = false;
    char tempBuffer[50];
    sprintf(tempBuffer, "Key: %d\n", key);
    SCI_WRITE(&sci0, tempBuffer);
    int frequency_index;
    int period;
    for (size_t i = 0; i < 32; i++)
    {
        frequency_index = frequency_indices[i];
        sprintf(tempBuffer, "%d ", frequency_index);
        SCI_WRITE(&sci0, tempBuffer);
    }
    SCI_WRITE(&sci0, "\n");
    for (size_t i = 0; i < 32; i++)
    {
        frequency_index = frequency_indices[i] + key;
        period = periods[frequency_index+10];
        sprintf(tempBuffer, "%d ", period);
        SCI_WRITE(&sci0, tempBuffer);
    }
    SCI_WRITE(&sci0, "\n");
    
    
}

void startApp(App *self, int arg)
{

    SCI_INIT(&sci0);
    SCI_WRITE(&sci0, "Hello, hello...\n");
}

int main()
{
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}
