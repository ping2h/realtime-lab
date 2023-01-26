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
 * TinyTimber.h v2.0
 *
 */

#ifndef _TINYTIMBER_
#define _TINYTIMBER_

#define TINYTIMBER_VERSION "v2.06a1 (2021-03-11)"

#include "stm32f4xx.h"

#define __USE_LOCAL_SBRK
//#define __USE_SAFE_TIMER
#define __USE_FUTURE_CHECK_TIMER

#define __ENABLED_PRIORITY	3
#define __DISABLED_PRIORITY	1
#define __IRQ_PRIORITY		2

//#define __TRACE_DISPATCH
//#define __TRACE_RUN
//#define __TRACE_TIMER
//#define __TRACE_SCHEDULE
//#define __TRACE_ASYNC

extern int doIRQSchedule;

//      Abstract type, used in the definition of Object.
struct thread_block;

//      Base class of reactive objects. Every reactive object in a TinyTimber 
//      system must be of a class that inherits this class.
typedef struct {
    struct thread_block *ownedBy, *wantedBy;
} Object;

//      Initialization macro for class Object. 
#define initObject() \
        { NULL, NULL }

//  int SYNC( T* obj, int (*meth)(T*, A), A arg );
//      Synchronously invoke method meth on object obj with argument arg. Type T 
//      must be a struct type that inherits from Object, while A can be any 
//      int-sized type. If completion of the call would result in deadlock, 
//      -1 is returned; otherwise the result is the value returned by m.
#define SYNC(obj, meth, arg) \
        sync((Object*)obj, (Method)meth, (int)arg)

//      Abstract type, used in the definition of Msg
struct msg_block;

//      Type that identifies asynchronous messages
typedef struct msg_block *Msg;

//      Base type for methods. Every method in a TinyTimber system should take 
//      a first argument that is a reference to a subclass of class Object.
typedef int (*Method)(Object*, int);

//      Unit pointer value.
#ifndef NULL
#define NULL 0
#endif

//  Msg ASYNC(T *obj, int (*meth)(T*, A), A arg);
//      Asynchronously invoke method meth on object obj with argument arg. 
//      Identical to SEND(0, 0, obj, meth, arg).
#define ASYNC(obj, meth, arg) \
        async((Time)0, (Time)0, (Object*)obj, (Method)meth, (int)arg)

//  Msg AFTER(Time bl, T *obj, int (*meth)(T*, A), A arg);
//      Asynchronously invoke method meth on object obj with argument arg and
//      baseline offset bl. 
//      Identical to SEND(bl, 0, obj, meth, arg).
#define AFTER(bl, obj, meth, arg) \
        async(bl, (Time)0, (Object*)obj, (Method)meth, (int)arg)

//  Msg BEFORE(Time dl, T *obj, int (*meth)(T*, A), A arg);
//      Asynchronously invoke method meth on object obj with argument arg and
//      relative deadline dl. 
//      Identical to SEND(0, dl, obj, meth, arg). 
#define BEFORE(dl, obj, meth, arg) \
        async((Time)0, dl, (Object*)obj, (Method)meth, (int)arg)

//  Msg SEND(Time bl, Time dl, T *obj, int (*meth)(T*, A), A arg);
//      Asynchronously invoke method meth on object obj with argument arg, baseline 
//      offset bl, and relative deadline dl. Type T must be a struct type that 
//      inherits from Object, while A can be any int-sized type.
//      Offsets bl and dl allow a new execution window for the asynchronous 
//      call to be defined on basis of the current one:
//         new baseline = current baseline + bl
//         new deadline = new baseline + dl,    dl > 0
//                      = new baseline + rel,   otherwise
//      where rel = current deadline - current baseline.
//      During interrupts, current baseline = time of interrupt and current
//      deadline = infinity.
#define SEND(bl, dl, obj, meth, arg) \
        async(bl, dl, (Object*)obj, (Method)meth, (int)arg)


// Cortex m4 dependencies

//      Type of time values (with platform-dependent resolution).
typedef int32_t Time;

#define __TIMER_PRESCALE    (840-1) // 10us tick @ 84 MHz, (See table 51 in F407 - Datasheet.pdf)

//      Construct a Time value from an argument given in microseconds.
#define USEC(x) \
        ((Time)((x) / 10))
//      Construct a Time value from an argument given in milliseconds.
#define MSEC(x) \
        ((Time)((x) * (Time)100))
//      Construct a Time value from an argument given in seconds.
#define SEC(x) \
        ((Time)((x) * (Time)100000))
//      Extract the microsecond fraction of a Time value
#define USEC_OF(t) \
        (long)((t) % ((Time)100000) * 10)
//      Extract the millisecond fraction of a Time value
#define MSEC_OF(t) \
        (int)((t) % ((Time)100000) / 100)
//      Extract the while second basis of a Time value
#define SEC_OF(t) \
        (int)((t) / ((Time)100000))

enum Vector { 
        IRQ_USART1, 
        IRQ_CAN1,
        IRQ_EXTI9_5,

        N_VECTORS
};

// End of target dependencies

//      Prematurely aborts pending asynchronous message m.  Does nothing if m 
//      has already begun executing. 
void ABORT(Msg m);

// void INSTALL (T* obj, int (*meth)(T*, enum Vector), enum Vector i )
//      Install method meth on object obj as an interrupt-handler for
//      interrupt source i. Type T must be a struct type that inherits
//      from Object. When an interrupt on i occurs, meth will be
//      invoked on obj with i as its argument.
#define INSTALL(obj,meth,i) install((Object*)obj, (Method)meth, i)

//  int TINYTIMBER ( T* obj, int (*meth)(T*, A), A arg )
//      Start up the TinyTimber system by invoking method meth on obj with
//      argument arg; then handle all subsequent interrupts and timed
//      events as they occur. Type T must be a struct type that inherits
//      from Object, while A can be any int-sized type. This function never
//      returns.
#define TINYTIMBER(obj,meth,arg) tinytimber((Object*)obj, (Method)meth, (int)arg)

//      Built-in type of primitive baseline timers
typedef struct {
    Time accum;
} Timer;

//      Initialization macro for Timer objects
#define initTimer() { 0 }

//      Reset timer t to the value of of current baseline
void T_RESET(Timer *t);

//      Return difference between current baseline and timer t
Time T_SAMPLE(Timer *t);

//      Return current time measured from current baseline
Time CURRENT_OFFSET(void);


// -------------------------------------------------------------------
// No externally significant information below this line
// -------------------------------------------------------------------

Msg async(Time bl, Time dl, Object *to, Method m, int arg); 
int sync(Object *to, Method m, int arg);
void install(Object *obj, Method m, enum Vector index);
int tinytimber(Object *obj, Method startup, int arg);

#endif
