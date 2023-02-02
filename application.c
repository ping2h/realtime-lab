#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct
{
    Object super;
    int count;
    int index;
    int index1;     //index for intHistory[]
    char buffer[50];
    int intHistory[3];
} App;

App app = {initObject(), 0, 0, 0, {}, {}};

void reader(App *, int);
void receiver(App *, int);
int median(int[], int);
int sum(int[], int);

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


    }
}

//
//
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

int sum(int a[], int len) {
    int sum = 0;
    for (size_t i = 0; i < len; i++)
    {
        sum += a[i];
    }
    return sum;
    
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
