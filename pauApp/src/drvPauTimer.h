#ifndef DRV_PAUTIMER_H
#define DRV_PAUTIMER_H

#include <epicsThread.h>
#include <ellLib.h>

typedef struct {
    ELLNODE       node;
    unsigned int  clock_freq;
    unsigned int  id;
    void          (*isr)(void*);
    void          *arg;
} pauTimer_ts;

#define DO_NOT_RELOAD 0


pauTimer_ts* pauTimerSetup(void (*isr)(void *arg), void *arg);

int pauTimerStart(pauTimer_ts* pPauTimer, double period_in_usec);
int pauTimerReport(void);

#endif
