#include <stdlib.h>

#ifdef __rtems__
#include <bsp/gt_timer.h>
#endif

#include <epicsPrint.h>
#include <epicsThread.h>
#include <epicsMutex.h>
#include <ellLib.h>

#include <drvPauTimer.h>


static epicsThreadOnceId pauTimerInitFlag  =  EPICS_THREAD_ONCE_INIT;
static epicsMutexId      pauTimerMutex;     
static ELLLIST           pauTimerList_s;

static void pauTimerInit(void)
{
    ellInit(&pauTimerList_s);
    pauTimerMutex = epicsMutexCreate();
}


pauTimer_ts* pauTimerSetup(void (*isr)(void *arg), void *arg)
{
#ifdef __rtems__

    pauTimer_ts           *pPauTimer;

    epicsThreadOnce(&pauTimerInitFlag, (void *)pauTimerInit, (void*) NULL);

    pPauTimer = (pauTimer_ts*) malloc(sizeof(pauTimer_ts));
    if(!pPauTimer) return pPauTimer;

    epicsMutexLock(pauTimerMutex);

    pPauTimer->id  = ellCount(&pauTimerList_s);
    if(pPauTimer->id >= BSP_timer_instances()) {
        free((void*) pPauTimer);
        return NULL;
    }

    pPauTimer->clock_freq = BSP_timer_clock_get(pPauTimer->id);
    pPauTimer->isr        = isr;
    pPauTimer->arg        = arg;

    BSP_timer_setup(pPauTimer->id, pPauTimer->isr, pPauTimer->arg, DO_NOT_RELOAD);

    ellAdd(&pauTimerList_s, &pPauTimer->node);

    epicsMutexUnlock(pauTimerMutex);

    return pPauTimer;

#endif
    return NULL;
}


int pauTimerStart(pauTimer_ts* pPauTimer, double period_in_usec)
{
#ifdef __rtems__
  
    return BSP_timer_start(pPauTimer->id,
                           (unsigned int) (period_in_usec * (double)(pPauTimer->clock_freq) * 1.E-6));


#endif
    return -1;
}



int pauTimerReport(void)
{
    pauTimer_ts   *pPauTimer; 

    epicsThreadOnce(&pauTimerInitFlag, (void*) pauTimerInit, (void*) NULL);
    epicsPrintf("drvPauTimer: %d timers have been allocated.\n", ellCount(&pauTimerList_s));

    pPauTimer = (pauTimer_ts *) ellFirst(&pauTimerList_s);
    while(pPauTimer) {
        epicsPrintf("Timer (%p)\n", pPauTimer);
        epicsPrintf("    id        : %d\n", pPauTimer->id);
        epicsPrintf("    clock_freq: %d\n", pPauTimer->clock_freq);
        epicsPrintf("    isr       : %p\n", pPauTimer->isr);
        epicsPrintf("    arg of isr: %p\n", pPauTimer->arg);

        pPauTimer = (pauTimer_ts *) ellNext(&pPauTimer->node);
    }

    return 0;
    
}
