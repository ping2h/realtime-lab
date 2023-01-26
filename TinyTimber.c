// The Timber compiler <timber-lang.org>
// 
// Copyright 2008-2012 Johan Nordlander <nordland@csee.ltu.se>
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 
// 3. Neither the names of the copyright holder and any identified
//    contributors, nor the names of their affiliations, may be used to 
//    endorse or promote products derived from this software without 
//    specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS ``AS IS'' AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

/*
 * 
 * TinyTimber.c
 *
 */

#include "TinyTimber.h"
#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_usart.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"

void DUMPC(char);

void DUMP(char *s) {
  while (*s) {
	if (*s != '\r')
		DUMPC(*s++);
	else
		s++;
  }
}

char hex[] = "0123456789ABCDEF";

void DUMPH(unsigned int val) {
    char buf[8];
    int i = 0;
    do {
        buf[i++] = hex[val & 0x0F];
        val = val >> 4;
    } while (val);
    while (i)
        DUMPC(buf[--i]);
}

void DUMPD(int val) {
    char buf[100];
	
	int sign = val < 0 ? -1 : 1;
	
	val = val * sign;
		
    int i = 0;
    do {
        buf[i++] = hex[val % 10];
        val = val / 10;
    } while (val);
	if (sign < 0)
		DUMPC('-');
    while (i)
        DUMPC(buf[--i]);

}

// Cortex m4 dependencies

#define __CURRENT_PRIORITY ((__get_BASEPRI() >> (8 - __NVIC_PRIO_BITS)))

void sei( void)			{ __set_BASEPRI(__DISABLED_PRIORITY << (8 - __NVIC_PRIO_BITS)); }
void cli( void)			{ __set_BASEPRI(__ENABLED_PRIORITY << (8 - __NVIC_PRIO_BITS)); }

#define PROTECTED()		(__CURRENT_PRIORITY == __DISABLED_PRIORITY)	

#define __CURRENT_EXCEPTION (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk)

#define THREADMODE()	(__CURRENT_EXCEPTION == 0)

#define ENABLED()      	(!PROTECTED())
#define DISABLE()      	{ sei(); }
#define ENABLE(s)	    { if (s) cli(); }
#define SLEEP()         { __asm volatile ("     wfi\n"); }

#define RED_ALERT()     { GPIO_WriteBit(GPIOB, GPIO_Pin_1, (BitAction) 0); }  // Red LED On

#define PANIC(s)         { DUMP("PANIC!!! "); RED_ALERT(); DUMP(s); while (1) SLEEP(); }

#ifdef __USE_LOCAL_SBRK
//register char *__stack_ptr asm ("sp");

char *__heap_end;

void *_sbrk (int incr)
{
	extern char   end; /* Set by linker.  */
	char *        prev_heap_end;

	DUMP("NOTE: _sbrk in use!\n\r");

	if (__heap_end == 0)
     __heap_end = & end;

	prev_heap_end = __heap_end;
   
//	if (__heap_end + incr > __stack_ptr) // will fail if stacks[0]-stacks[3] is used
//		PANIC("_sbrk: Heap and stack collision\r\n");

	__heap_end += incr;

	return (void *) prev_heap_end;
}
#endif

#define NMSGS           30
#define NTHREADS        4

#define CONTEXTSIZE		(2+16+8+16+10)

#define CONTEXT_T uint32_t

#define STACKSIZE       1024

#define STACK_T long long

struct stack;

/*
 * Context:
 * 
 * FPSCR + fill	(2 words)	(OFFSET = 50-51)
 * S15-S0		(16 words)	(OFFSET = 34-49)
 * xPSR,					(OFFSET = 33) 
 * PC,						(OFFSET = 32)
 * LR, 						(OFFSET = 31)
 * R12,						(OFFSET = 30)
 * R3-R0		(8 words)	(OFFSET = 26-29)
 * S31-S16		(16 words)	(OFFSET = 10-25)
 * R11-R4,					(OFFSET = 2-9)
 * BASEPRI,					(OFFSET = 1)
 * EXC_RETURN	(10 words)	(OFFSET = 0)
 */

#define	CONTEXT_xPSR_OFF	33
#define	CONTEXT_PC_OFF		32
#define	CONTEXT_BASEPRI_OFF	1
#define	CONTEXT_EXC_OFF		0

#define HW32_REG(ADDRESS) (*((volatile unsigned long *)(ADDRESS)))
 
#define SETCONTEXT(c)	

void SETSTACK(CONTEXT_T *cp, struct stack *sp) {
	*cp = ((CONTEXT_T) sp) + STACKSIZE*sizeof(STACK_T) - CONTEXTSIZE*sizeof(CONTEXT_T);
	
	CONTEXT_T ci = *cp;
	int i;
	for (i=0; i<CONTEXTSIZE;i++)
		HW32_REG(ci + (i<<2)) = 0;
	HW32_REG(ci + (CONTEXT_EXC_OFF<<2)) = 0xFFFFFFE9;
	HW32_REG(ci + (CONTEXT_BASEPRI_OFF<<2)) = __ENABLED_PRIORITY;
	HW32_REG(ci + (CONTEXT_xPSR_OFF<<2)) = 0x01000000;
}

void SETPC(CONTEXT_T *cp, void (*fp)(void)) {
	CONTEXT_T pc_p = *cp + (CONTEXT_PC_OFF<<2);
	HW32_REG(pc_p) = (unsigned long) fp;
}

#define	PendSV_IRQ_VECTOR		(0x2001C000+0x38)
#define PendSV_Exception		void vect_PendSV( void ) 

PendSV_Exception;

#define	SVCall_IRQ_VECTOR		(0x2001C000+0x2C)
#define SVCall_Exception		void vect_SVCall( void ) 

SVCall_Exception;

#define	TIM5_IRQ_VECTOR			(0x2001C000+0x108)
#define TIMER_COMPARE_INTERRUPT void vect_TIM5( void ) 

TIMER_COMPARE_INTERRUPT;

void TIMER_INIT() {
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;	

/*
	Default values:
        (++) Autoreload value = 0xFFFFFFFF
        (++) Prescaler value = 0x0000
        (++) Counter mode = Up counting
        (++) Clock Division = TIM_CKD_DIV1
 */

	TIM_DeInit(TIM5);
	
	TIM_TimeBaseStructInit(&TIM_TimeBaseInitStructure);
	TIM_TimeBaseInitStructure.TIM_Prescaler = __TIMER_PRESCALE;
	TIM_TimeBaseInit(TIM5, &TIM_TimeBaseInitStructure);

	*((void (**)(void) ) PendSV_IRQ_VECTOR ) = vect_PendSV;

	NVIC_SetPriority(PendSV_IRQn, __IRQ_PRIORITY); // same priority as timer and USART1

	*((void (**)(void) ) SVCall_IRQ_VECTOR ) = vect_SVCall;

	NVIC_SetPriority(SVCall_IRQn, 0x00); // highest priority

	*((void (**)(void) ) TIM5_IRQ_VECTOR ) = vect_TIM5;

	NVIC_SetPriority( TIM5_IRQn, __IRQ_PRIORITY); 
	NVIC_EnableIRQ( TIM5_IRQn);

	TIM_SetCounter(TIM5, 0);	
	TIM_Cmd( TIM5, ENABLE);

	TIM_ITConfig( TIM5, TIM_IT_CC1, ENABLE);	
}

#define TIMER_CCLR()    { TIM_ClearITPendingBit(TIM5, TIM_IT_CC1); }  // Timer compare interrupt clear

#define TIMERGET(x)		(x = TIM_GetCounter(TIM5))

#define TIMERSET(x)		(TIM_SetCompare1(TIM5, x->baseline))

#define INFINITY        0x7fffffffL

void DUMPC(char c) {
   USART_SendData(USART1, c );
   while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);    
}

// End of target dependencies

typedef struct thread_block *Thread;

#define INSTALLED_TAG (Thread)1

struct msg_block {
    Msg next;                // for use in linked lists
    Time baseline;           // event time reference point
    Time deadline;           // absolute deadline (=priority)
    Object *to;              // receiving object
    Method method;           // code to run
    int arg;                 // argument to the above
};

struct thread_block {
	CONTEXT_T context;     	 // machine state */
	int thread_no;
    Thread next;             // for use in linked lists
    Msg msg;                 // message under execution
    Object *waitsFor;        // deadlock detection link
};

struct stack {
    STACK_T stack[STACKSIZE];
};

struct msg_block    messages[NMSGS];
struct thread_block threads[NTHREADS];
struct stack        stacks[NTHREADS];

struct thread_block thread0;

Msg msgPool         = messages;
Msg msgQ            = NULL;
Msg timerQ          = NULL;
int runAsHardware	= 0;
int doIRQSchedule	= 0;
Time timestamp      = 0;
int overflows       = 0;

Thread threadPool   = threads;
Thread activeStack  = &thread0;
Thread current      = &thread0;
Thread upcoming;

Method  mtable[N_VECTORS];
Object *otable[N_VECTORS];

static void dispatch( Thread);
static void schedule( void);

// Cortex m4 dependencies

#define	    USART1_IRQ_VECTOR		(0x2001C000+0xD4)
#define	    CAN1_IRQ_VECTOR			(0x2001C000+0x90)
#define	    EXTI9_5_IRQ_VECTOR		(0x2001C000+0x9C)

#ifdef	__TRACE_SCHEDULE
#define IRQ(n,v) void v (void) { \
        DISABLE(); TIMERGET(timestamp); runAsHardware = 1; doIRQSchedule = 0; \
        if (mtable[n]) mtable[n](otable[n],n); \
		DUMP("schedule() in IRQ()"); DUMP("\n\r"); \
        runAsHardware = 0; if (doIRQSchedule) schedule(); doIRQSchedule = 0; ENABLE(1); \
}
#else
#define IRQ(n,v) void v (void) { \
        TIMERGET(timestamp); runAsHardware = 1; doIRQSchedule = 0; \
        if (mtable[n]) mtable[n](otable[n],n); \
		runAsHardware = 0; if (doIRQSchedule) schedule(); doIRQSchedule = 0; \
}
#endif

IRQ(IRQ_USART1,		vect_USART1);
IRQ(IRQ_CAN1,		vect_CAN1);
IRQ(IRQ_EXTI9_5,	vect_EXTI9_5);

// End of target dependencies

/* queue manager */
void enqueueByDeadline(Msg p, Msg *queue) {
    Msg prev = NULL, q = *queue;
    while (q && (q->deadline <= p->deadline)) {
        prev = q;
        q = q->next;
    }
    p->next = q;
    if (prev == NULL)
        *queue = p;
    else
        prev->next = p;
}

void enqueueByBaseline(Msg p, Msg *queue) {
    Msg prev = NULL, q = *queue;
    while (q && (q->baseline <= p->baseline )) {
        prev = q;
        q = q->next;
    }
    p->next = q;
    if (prev == NULL)
        *queue = p;
    else
        prev->next = p;
}

Msg dequeue(Msg *queue) {
    Msg m = *queue;
    if (m)
        *queue = m->next;
    else
        PANIC("Empty queue");  // Empty queue, kernel panic!!!
    return m;
}

Msg dequeue_pool(Msg *queue) {
    Msg m = *queue;
    if (m)
        *queue = m->next;
    else
        PANIC("Empty pool");  // Empty pool, kernel panic!!!
    return m;
}

void insert(Msg m, Msg *queue) {
    m->next = *queue;
    *queue = m;
}

void push(Thread t, Thread *stack) {
    t->next = *stack;
    *stack = t;
}

Thread pop(Thread *stack) {
    Thread t = *stack;
    *stack = t->next;
    return t;
}

static int remove(Msg m, Msg *queue) {
    Msg prev = NULL, q = *queue;
    while (q && (q != m)) {
        prev = q;
        q = q->next;
    }
    if (q) {
        if (prev)
            prev->next = q->next;
        else
            *queue = q->next;
        return 1;
    }
    return 0;
}

TIMER_COMPARE_INTERRUPT {
    Time now;
 
 	TIMER_CCLR();
#ifdef	__USE_SAFE_TIMER
	TIM_Cmd( TIM5, DISABLE);
#endif
    TIMERGET(now);
	
#ifdef	__TRACE_TIMER1
	DUMP("In TIMER_COMPARE_INTERRUPT()!");
 	DUMP(" Counter = ");
    DUMPD(TIM_GetCounter(TIM5));
    DUMP(", Exception = ");
    DUMPD(__CURRENT_EXCEPTION);
    DUMP(", timerQ = ");
    DUMPH((unsigned int) timerQ);
	DUMP("\n\r");
#endif

    while (timerQ && (timerQ->baseline - now <= 0))
        enqueueByDeadline( dequeue(&timerQ), &msgQ );
    if (timerQ) {
#ifdef	__USE_FUTURE_CHECK_TIMER
		Time timcount = TIM_GetCounter(TIM5);
		if (timerQ->baseline < timcount)
			RED_ALERT();    // Next event is in the past!
#endif
		TIMERSET(timerQ);		
	}
#ifdef	__TRACE_SCHEDULE
	DUMP("schedule() in TIMER_COMPARE_INTERRUPT()");
	DUMP("\n\r");
#endif
#ifdef	__USE_SAFE_TIMER
	TIM_Cmd( TIM5, ENABLE);
#endif
	
    schedule();
}

/* context switching */

__attribute__((naked)) 
void __svc_dispatch( Thread next ) {
	upcoming = next;
	asm volatile(
		"svc 0x10\n"
		"mov pc, lr\n"
	);
}

__attribute__((naked)) 
void __pendSV_dispatch( Thread next ) {
	upcoming = next;
	SCB->ICSR |= (1<<28); // SCB_ICSR_PENDSVSET_Msk
	asm volatile(
		"mov pc, lr\n"
	);
}

void dispatch( Thread next ) {
#ifdef	__TRACE_DISPATCH
		DUMP("Entered dispatch(): ");
		DUMP("thread #");
		DUMPD(next->thread_no);
		DUMP(", mode = ");
		DUMPD(__CURRENT_EXCEPTION);
		DUMP("\n\r");
#endif
	
	if (THREADMODE()) {
		__svc_dispatch( next);	
	}
	else {
		__pendSV_dispatch( next);
	}

	ENABLE(1);
}

static void run(void) {
    while (1) {
#ifdef	__TRACE_RUN
		DUMP("Entered run(): ");
		DUMP("thread #");
		DUMPD(current->thread_no);
		DUMP(", Exception = ");
		DUMPD(__CURRENT_EXCEPTION);
		DUMP("\n\r");
#endif

        Msg this = current->msg = dequeue(&msgQ); // Get first pending message
        Msg oldMsg;
        
#ifdef	__TRACE_RUN
		DUMP("Dequeue in run() done:");
		DUMP("thread #");
		DUMPD(current->thread_no);
		DUMP(", method = ");
		DUMPH((unsigned int) this->method);
		DUMP("\n\r");
#endif

        ENABLE(1);
        SYNC(this->to, this->method, this->arg);
        DISABLE();

        insert(this, &msgPool);
       
        oldMsg = activeStack->next->msg;
        if (!msgQ || (oldMsg && (msgQ->deadline - oldMsg->deadline > 0))) {
            Thread t;
            push(pop(&activeStack), &threadPool);
            t = activeStack;  // can't be NULL, may be &thread0
            while (t->waitsFor) 
	            t = t->waitsFor->ownedBy;
#ifdef	__TRACE_DISPATCH
			DUMP("dispatch() in run()");
			DUMP("\n\r");
#endif
            dispatch(t);
        }
	}
}

static void idle(void) {
#ifdef	__TRACE_SCHEDULE
	DUMP("schedule() in idle()");
	DUMP("\n\r");
#endif
    schedule();
    while (1) {
		ENABLE(1);
		SLEEP();
    }
}

static void schedule(void) {
    Msg topMsg = activeStack->msg;

#ifdef	__TRACE_SCHEDULE
		DUMP("Entered schedule(): ");
		DUMP("msgQ = ");
		DUMPH((unsigned int) msgQ);
		DUMP(", topMsg = ");
		DUMPH((unsigned int) topMsg);
		DUMP("\n\r");
#endif
 
    if (msgQ && threadPool && ((!topMsg) || (msgQ->deadline - topMsg->deadline < 0))) {
        push(pop(&threadPool), &activeStack);

#ifdef	__TRACE_DISPATCH
		DUMP("dispatch() in schedule()");
		DUMP("\n\r");
#endif
        dispatch(activeStack);
    }
}

/* communication primitives */
Msg async(Time bl, Time dl, Object *to, Method meth, int arg) {
    Msg m;
    Time now;
    char wasEnabled = ENABLED();
    DISABLE();
    m = dequeue_pool(&msgPool); // Get new message template
    m->to = to; 
    m->method = meth; 
    m->arg = arg;
	m->baseline = (runAsHardware ? timestamp : current->msg->baseline) + bl;
    m->deadline = m->baseline + (dl > 0 ? dl : INFINITY);
    
#ifdef	__USE_SAFE_TIMER
	TIM_Cmd( TIM5, DISABLE);
#endif
    TIMERGET(now);

/*	DUMP("Entered async(): ");
	DUMP("bl = ");
	DUMPD(m->baseline);
	DUMP(", dl = ");
	DUMPD(m->deadline);
	DUMP(", now = ");
	DUMPD(now);
	DUMP(", wasEnabled = ");
	DUMPD(wasEnabled);
	DUMP(", runAsHardware = ");
	DUMPD(runAsHardware);
	DUMP("\n\r"); */

    if (m->baseline - now > 0) {        // baseline has not yet passed
#ifdef	__TRACE_ASYNC1
		DUMP("enqueueByBaseline() in async()");
		DUMP("\n\r");
#endif
        enqueueByBaseline(m, &timerQ);
#ifdef	__USE_FUTURE_CHECK_TIMER
		if (timerQ->baseline < now)
			RED_ALERT();    // Next event is in the past!
#endif			
        TIMERSET(timerQ);

#ifdef	__USE_SAFE_TIMER
		TIM_Cmd( TIM5, ENABLE);
#endif
    } else {                            // m is immediately schedulable
#ifdef	__TRACE_ASYNC1
 		DUMP("enqueueByDeadline() in async()");
		DUMP("\n\r");
#endif
#ifdef	__USE_SAFE_TIMER
		TIM_Cmd( TIM5, ENABLE);
#endif
        enqueueByDeadline(m, &msgQ);
        if (wasEnabled && threadPool && (msgQ->deadline - activeStack->msg->deadline < 0)) {
            push(pop(&threadPool), &activeStack);
#ifdef	__TRACE_DISPATCH
			DUMP("dispatch() in async()");
			DUMP("\n\r");
#endif
            dispatch(activeStack);
        }
    }
    
    ENABLE(wasEnabled);
    return m;
}

int sync(Object *to, Method meth, int arg) {
    Thread t;
    int result;
    char wasEnabled = ENABLED();
    
    DISABLE();

//	DUMP("Entered sync(): ");
//	DUMP("\n\r"); 

    t = to->ownedBy;
    if (t) {                            // to is already locked
        while (t->waitsFor) 
            t = t->waitsFor->ownedBy;
        if (t == current || !wasEnabled) {  // deadlock!
            ENABLE(wasEnabled);
            return -1;
        }
        if (to->wantedBy)               // must be a lower priority thread
            to->wantedBy->waitsFor = NULL;
        to->wantedBy = current;
        current->waitsFor = to;
#ifdef	__TRACE_DISPATCH
		DUMP("dispatch() in sync() - already locked");
		DUMP("\n\r");
#endif
        dispatch(t);
        if (current->msg == NULL) {     // message was aborted (when called from run)
            ENABLE(wasEnabled);
            return 0;
        }
    }
    to->ownedBy = current;
    ENABLE(wasEnabled && (to->wantedBy != INSTALLED_TAG)); // don't enable interrupts if running as handler
    result = meth(to, arg);
    DISABLE();
    to->ownedBy = NULL; 
    t = to->wantedBy;
    if (t && (t != INSTALLED_TAG)) {      // we have run on someone's behalf
        to->wantedBy = NULL; 
        t->waitsFor = NULL;
#ifdef	__TRACE_DISPATCH
		DUMP("dispatch() in sync() - run on someone's behalf");
		DUMP("\n\r");
#endif
        dispatch(t);
    }
    ENABLE(wasEnabled);
    return result;
}

void ABORT(Msg m) {
    char wasEnabled = ENABLED();
    DISABLE();

    if (remove(m, &timerQ) || remove(m, &msgQ))
        insert(m, &msgPool);
    else {
        Thread t = activeStack;
        while (t) {
            if ((t != current) && (t->msg == m) && (t->waitsFor == m->to)) {
	            t->msg = NULL;
	            insert(m, &msgPool);
	            break;
            }
            t = t->next;
        }
    }
    ENABLE(wasEnabled);
}

void T_RESET(Timer *t) {
    t->accum = ENABLED() ? current->msg->baseline : timestamp;
}

Time T_SAMPLE(Timer *t) {
    return (ENABLED() ? current->msg->baseline : timestamp) - t->accum;
}

Time CURRENT_OFFSET(void) {
    Time now;
    char wasEnabled = ENABLED();
    DISABLE();
    TIMERGET(now);
    ENABLE(wasEnabled);
	return now - (wasEnabled ? current->msg->baseline : timestamp);
}

/* initialization */
static void initialize(void) {
    int i;
    
    for (i=0; i<NMSGS-1; i++)
        messages[i].next = &messages[i+1];
    messages[NMSGS-1].next = NULL;
    
    for (i=0; i<NTHREADS-1; i++)
        threads[i].next = &threads[i+1];
    threads[NTHREADS-1].next = NULL;
    
    for (i=0; i<NTHREADS; i++) {
		threads[i].thread_no = i;
        SETCONTEXT( threads[i].context );
        SETSTACK( &threads[i].context, &stacks[i] );
        SETPC( &threads[i].context, run );
        threads[i].waitsFor = NULL;
    }

    thread0.thread_no = -1;
	thread0.next = NULL;
    thread0.waitsFor = NULL;
    thread0.msg = NULL;
    
    DUMP("\n\r");
    DUMP("TinyTimber ");
    DUMP(TINYTIMBER_VERSION);
    DUMP("\n\r");
    DUMP("\n\r");
 	
    TIMER_INIT();
}

void install(Object *obj, Method m, enum Vector i) {
    if (i >= 0 && i < N_VECTORS) {
        char wasEnabled = ENABLED();
        DISABLE();
		switch (i) {
		  case IRQ_USART1:
			*((void (**)(void) ) USART1_IRQ_VECTOR ) = vect_USART1;
			break;
		  
		  case IRQ_CAN1:
			*((void (**)(void) ) CAN1_IRQ_VECTOR ) = vect_CAN1;
			break;

		  case IRQ_EXTI9_5:
			*((void (**)(void) ) EXTI9_5_IRQ_VECTOR ) = vect_EXTI9_5;
			break;

		  default:
			PANIC("Device IRQ not supported ...");
		}
        otable[i] = obj;
        mtable[i] = m;
        obj->wantedBy = INSTALLED_TAG;  // Mark object as subject to synchronization by interrupt disabling
        ENABLE(wasEnabled);
    }
}

int tinytimber(Object *obj, Method meth, int arg) {
    DISABLE();
    initialize();
    if (meth != NULL) {
		TIMERGET(timestamp);
		runAsHardware = 1;
        ASYNC(obj, meth, arg);
		runAsHardware = 0;
#ifdef	__TRACE_SCHEDULE
		DUMP("schedule() in tinytimber()");
		DUMP("\n\r");
#endif
		schedule();
	}
    idle();
    return 0;
}
