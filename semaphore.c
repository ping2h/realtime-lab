#include "TinyTimber.h"
#include "semaphore.h"

void c_enqueue(Caller c, Caller *queue) {
	 Caller prev = NULL, q = *queue;
    while (q) { // find last element in queue
        prev = q;
        q = q->next;
    }
    if (prev == NULL)
        *queue = c;	// empty queue: put ‘c’ first
    else
        prev->next = c; // non-empty queue: put ‘c’ last
    c->next = NULL;
}

Caller c_dequeue(Caller *queue) {
    Caller c = *queue;
    if (c)
        *queue = c->next; // remove first element in queue
    return c;
}

void Wait(Semaphore *self, int c) {
    Caller wakeup = (Caller) c;  // type-cast back from ‘int’
    if (self->value > 0) {
        self->value--;
        ASYNC(wakeup->obj, wakeup->meth, 0);		
    }
    else
        c_enqueue(wakeup, &self->queue);
}

void Signal(Semaphore *self, int unused) {
    if (self->queue) {
        Caller wakeup = c_dequeue(&self->queue);
        ASYNC(wakeup->obj, wakeup->meth, 0);
    }
    else
        self->value++;
}
