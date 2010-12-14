#include <stdlib.h>               /* for calloc */
#include <string.h>
#include <errlog.h>
#include <epicsPrint.h>
#include <epicsMessageQueue.h>
#include <ellLib.h>               /* for epics linked list */
#include <epicsRingBytes.h>
#include <epicsThread.h>
#include <epicsMutex.h>
#include <drvSup.h>               /* for DRVSUPFN */

#include <registryFunction.h>    /* for epicsExport */
#include <epicsExport.h>         /* for epicsRegisterFunction */

#include "evrTime.h"
#include "evrPattern.h"

#include "drvPau.h"
#include "drvPauReport.h"
#include "drvPauTimer.h"


#define  PAUQUEUE_SIZE   1024
#define  INIT_USRFUNC    1
#define  PROC_USRFUNC    0


static epicsThreadOnceId     pauOnceInitFlag = EPICS_THREAD_ONCE_INIT;
static epicsMutexId          pauGlobalMutex;
static epicsMessageQueueId   pauQueue;
static ELLLIST pauList_s;
static uint32_t              clock_ticks_insec = 133333333/4;  /* mv6100 default,
                                                                 it will be updated during driver init. */


static int pauReport();
struct drvet drvPau = {
    2,
    (DRVSUPFUN) pauReport,
    (DRVSUPFUN) pauInitialize
};

epicsExportAddress(drvet, drvPau);


static int getDevFromPV(char *pv, char *dev)
{
    size_t i;

    strcpy(dev, pv);
    i = strlen(dev);

    while(i) {
        if(dev[i--] == ':') { dev[i+1] = 0; break; }
    }

    return 0;
}



static int uhcbPau(void)
{

    pauQueueComp_ts pauQueueComp;  

    for(;;) {
        epicsMessageQueueReceive(pauQueue, (void *) &pauQueueComp, sizeof(pauQueueComp_ts));
        if(pauQueueComp.pfunc) (*pauQueueComp.pfunc)(pauQueueComp.parg);
    }

    return 0;
}

static void queueToUhcb(void *pVar)
{
    pauQueueComp_ts *pQueueComp = (pauQueueComp_ts *) pVar;
    pau_ts          *pPau       = pQueueComp->parg;
    pauDebugInfo_ts *pPauDebugInfo = pPau->pPauDebugInfo;

    CPU_Get_timebase_low(pPauDebugInfo->tick_cnt_timerIsr);

    epicsMessageQueueSend(pauQueue, pQueueComp, sizeof(pauQueueComp_ts));
}


static void pauDiag(void *pArg)
{
    pau_ts *pPau                   = (pau_ts *) pArg;
    mux_ts *pMux;
    pauDebugInfo_ts *pPauDebugInfo = pPau->pPauDebugInfo;
    muxDebugInfo_ts *pMuxDebugInfo;

    uint32_t     start, end;

    CPU_Get_timebase_low(start);
    pPauDebugInfo->tick_cnt_diagFunc = start;

    epicsMutexLock(pPau->lockPau);
    pPauDebugInfo->delay_timerIsr_usec
        = (double)(pPauDebugInfo->tick_cnt_timerIsr
           - pPauDebugInfo->tick_cnt_fiducial)/(double)(clock_ticks_insec)*1.E+6;
    pPauDebugInfo->delay_pullFunc_usec
        = (double)(pPauDebugInfo->tick_cnt_pullFunc 
           - pPauDebugInfo->tick_cnt_fiducial)/(double)(clock_ticks_insec)*1.E+6;
    pPauDebugInfo->delay_pushFunc_usec
        = (double)(pPauDebugInfo->tick_cnt_pushFunc
           - pPauDebugInfo->tick_cnt_fiducial)/(double)(clock_ticks_insec)*1.E+6;
    pPauDebugInfo->delay_diagFunc_usec
        = (double)(pPauDebugInfo->tick_cnt_diagFunc
           - pPauDebugInfo->tick_cnt_fiducial)/(double)(clock_ticks_insec)*1.E+6;
    epicsMutexUnlock(pPau->lockPau);




    pMux = (mux_ts *) ellFirst(&pPau->muxList);
    while(pMux) {
        pMuxDebugInfo = pMux->pMuxDebugInfo;

        epicsMutexLock(pMux->lockMux);
        pMuxDebugInfo->spinTime_pullFunc_usec
            = (double)(pMuxDebugInfo->tick_cnt_pullEnd
               - pMuxDebugInfo->tick_cnt_pullStart)/(double)(clock_ticks_insec)*1.E+6;
        pMuxDebugInfo->spinTime_pushFunc_usec
            = (double)(pMuxDebugInfo->tick_cnt_pushEnd
               - pMuxDebugInfo->tick_cnt_pushStart)/(double)(clock_ticks_insec)*1.E+6;
        epicsMutexUnlock(pMux->lockMux);
        pMux = (mux_ts *) ellNext(&pMux->node);
    }


    CPU_Get_timebase_low(end);

    epicsMutexLock(pPau->lockPau);
    pPauDebugInfo->spinTime_fiduFunc_usec
        = (double)(pPauDebugInfo->tick_cnt_fiducial
           - pPauDebugInfo->tick_cnt_begin)/(double)(clock_ticks_insec)*1.E+6;
    pPauDebugInfo->spinTime_diagFunc_usec = (double)(end-start)/(double)(clock_ticks_insec)*1.E+6;
    epicsMutexUnlock(pPau->lockPau);


    return;
}


static void pauProcessing(void *pArg)
{
    pau_ts *pPau = (pau_ts *) pArg;
    pauDebugInfo_ts *pPauDebugInfo = pPau->pPauDebugInfo;
    mux_ts          *pMux;
    muxDebugInfo_ts *pMuxDebugInfo;

    CPU_Get_timebase_low(pPauDebugInfo->tick_cnt_pullFunc);

    pMux = (mux_ts *) ellFirst(&pPau->muxList);
    while(pMux) {
        pMuxDebugInfo = pMux->pMuxDebugInfo;

        CPU_Get_timebase_low(pMuxDebugInfo->tick_cnt_pullStart);
        if(pMux->pPullFunc) (*pMux->pPullFunc)((void *) pMux, PROC_USRFUNC);
        CPU_Get_timebase_low(pMuxDebugInfo->tick_cnt_pullEnd);

        pMux = (mux_ts *) ellNext(&pMux->node);
    }

    CPU_Get_timebase_low(pPauDebugInfo->tick_cnt_pushFunc);

    pMux = (mux_ts *) ellFirst(&pPau->muxList);
    while(pMux) {
        pMuxDebugInfo = pMux->pMuxDebugInfo;

        CPU_Get_timebase_low(pMuxDebugInfo->tick_cnt_pushStart);
        if(pMux->pPushFunc) (*pMux->pPushFunc)((void *) pMux, PROC_USRFUNC);
        CPU_Get_timebase_low(pMuxDebugInfo->tick_cnt_pushEnd);

        pMux = (mux_ts *) ellNext(&pMux->node);
    }

    pauDiag((void *) pPau);

    

    return; 
}



static void pauOnceInit(void)
{
    uint32_t time_start = 0;
    uint32_t time_end   = 0;

    ellInit(&pauList_s);
    pauGlobalMutex = epicsMutexCreate();
    pauQueue       = epicsMessageQueueCreate(PAUQUEUE_SIZE, sizeof(pauQueueComp_ts));


    epicsThreadCreate("uhcbPau", epicsThreadPriorityHigh,
                      epicsThreadGetStackSize(epicsThreadStackMedium),
                      (EPICSTHREADFUNC) uhcbPau, 0);

    while(time_start >= time_end) {
        CPU_Get_timebase_low(time_start);
        epicsThreadSleep(1.);
        CPU_Get_timebase_low(time_end);
    }
    clock_ticks_insec = time_end - time_start;
    
}


static void pauFiducial(void *pArg)
{
    pau_ts *pPau                   = (pau_ts*) pArg;
    pauDebugInfo_ts *pPauDebugInfo = pPau->pPauDebugInfo;

    int    matches_current = 0;
    int    matches_advance = 0;
    int    dataslot_current;
    int    dataslot_advance;
    evrTimeId_te       idx_advance = pPau->pipelineIdx;
    evrModifier_ta     modifier_current;
    evrModifier_ta     modifier_advance;
    epicsTimeStamp     timestamp_current;
    epicsTimeStamp     timestamp_advance;
    unsigned long      patternStatus_current;
    unsigned long      patternStatus_advance;


    if(!pPau->activatePau)  return;

  
    CPU_Get_timebase_low(pPauDebugInfo->tick_cnt_begin);
 
   
    evrTimeGetFromPipeline(&timestamp_current, evrTimeCurrent, modifier_current, &patternStatus_current,0,0,0); 

    for(dataslot_current=0; dataslot_current</*MAXNUM_DATASLOTS*/ 4; dataslot_current++) {
        matches_current = evrPatternCheck(((pPau->patternData)+dataslot_current)->beamCode,
                                          ((pPau->patternData)+dataslot_current)->timeSlot,
                                          ((pPau->patternData)+dataslot_current)->inclusion,
                                          ((pPau->patternData)+dataslot_current)->exclusion,
                                          modifier_current);
        if(matches_current) break;
    }

    evrTimeGetFromPipeline(&timestamp_advance, idx_advance, modifier_advance, &patternStatus_advance,0,0,0);

    for(dataslot_advance=0; dataslot_advance</*MAXNUM_DATASLOTS*/ 4; dataslot_advance++) {
        matches_advance = evrPatternCheck(((pPau->patternData)+dataslot_advance)->beamCode,
                                          ((pPau->patternData)+dataslot_advance)->timeSlot,
                                          ((pPau->patternData)+dataslot_advance)->inclusion,
                                          ((pPau->patternData)+dataslot_advance)->exclusion,
                                           modifier_advance);

        if(matches_advance) break;
    }


    epicsMutexLock(pPau->lockPau);
        pPau->fiducialCounter++;
        pPau->timestamp = timestamp_current;
        if(matches_current) {
            pPau->current.timestamp       = timestamp_current;
            pPau->current.prev_timestamp[dataslot_current] = pPau->current.timestamp;
            pPau->current.matchedDataSlot = dataslot_current;
            pPau->current.usedFlag        = 0;
            pPau->current.patternStatus   = patternStatus_current;
        } else dataslot_current = -1;

        if(matches_advance) {

            pPau->advance.prev_timestamp[dataslot_advance] = pPau->advance.timestamp;

            pPau->advance.timestamp       = timestamp_advance;
            pPau->advance.matchedDataSlot = dataslot_advance;
            pPau->advance.usedFlag        = 0;
            pPau->advance.patternStatus   = patternStatus_advance;

            pPau->callbackCallCounter++;
        } else dataslot_advance = -1;

        epicsRingBytesPut(pPau->current.id_history, (char*) &dataslot_current, sizeof(int));
        epicsRingBytesPut(pPau->advance.id_history, (char*) &dataslot_advance, sizeof(int));

        if(!(pPau->fiducialCounter % 360)) {
            epicsRingBytesGet(pPau->current.id_history, (char*) pPau->current.snapshot_history, 360 * sizeof(int));
            epicsRingBytesGet(pPau->advance.id_history, (char*) pPau->advance.snapshot_history, 360 * sizeof(int));
            scanIoRequest(pPau->ioscanpvt);
        }
    epicsMutexUnlock(pPau->lockPau);


    CPU_Get_timebase_low(pPauDebugInfo->tick_cnt_fiducial);

    if(matches_advance) {
        if(pPau->pPauTimer && (pPau->delay_in_usec > 0.))  pauTimerStart(pPau->pPauTimer, pPau->delay_in_usec);
        else                                               queueToUhcb((void *) pPau->pProcessQueueComp); 
    }


}


static int func_createPau(char *pauName, unsigned pipelineIdx, char *description)
{
    int    i;
    pau_ts *pPau = (pau_ts *) malloc(sizeof(pau_ts));
    if(!pPau) {
        errlogPrintf("createPau(%s): memory allocation fail.\n", pauName);
        return -1;
     }

    strcpy(pPau->name, pauName); strcpy(pPau->description, description);

    pPau->fiducialCounter     = 0;
    pPau->callbackCallCounter = 0;
    pPau->pipelineIdx         = pipelineIdx;
    pPau->activatePau         = 1;
    pPau->activateUsrPull     = 1;
    pPau->activateUsrPush     = 1;
    pPau->activateDiag        = 1;
    pPau->fbckMode            = 0;
    pPau->pFiducial           = pauFiducial;
    pPau->lockPau             = epicsMutexCreate();
    scanIoInit(&pPau->ioscanpvt);
    pPau->delay_in_usec       = 500.;
    pPau->pProcessQueueComp   = (pauQueueComp_ts *) malloc(sizeof(pauQueueComp_ts));
    if(!(pPau->pProcessQueueComp)) {
        errlogPrintf("createPau(%s): memory allocation fail.\n", pauName);
         return -1;
    }
    pPau->pPauDebugInfo       = (pauDebugInfo_ts *) malloc(sizeof(pauDebugInfo_ts));
    if(!(pPau->pPauDebugInfo)) {
        errlogPrintf("createPau(%s): memory allocation fail.\n", pauName);
        return -1;
    }

    pPau->pProcessQueueComp->pfunc = (PAUQUEUEFUNC) pauProcessing;
    pPau->pProcessQueueComp->parg  = (void *) pPau;

    pPau->current.usedFlag         = 1;
    pPau->advance.usedFlag         = 1;
    pPau->current.missingCounter   = 0;
    pPau->advance.missingCounter   = 0;
    
    pPau->current.id_history       = epicsRingBytesCreate(2 * 360 * sizeof(int));
    pPau->advance.id_history       = epicsRingBytesCreate(2 * 360 * sizeof(int));

    pPau->pPauTimer = pauTimerSetup(queueToUhcb, (void *) pPau->pProcessQueueComp);
    epicsPrintf("timer: %p\n", pPau->pPauTimer);

    for(i=0; i<MAXNUM_DATASLOTS; i++) {
        (pPau->patternData+i)->beamCode = 0;
        (pPau->patternData+i)->timeSlot = 0;
        memset((pPau->patternData+i)->inclusion, 0, sizeof(evrModifier_ta));
        memset((pPau->patternData+i)->exclusion, 0, sizeof(evrModifier_ta));
    }
 
    ellInit(&pPau->muxList); 

    epicsMutexLock(pauGlobalMutex);
    pPau->id = ellCount(&pauList_s);
    pPau->id <<= 16;
    ellAdd(&pauList_s, &pPau->node);
    epicsMutexUnlock(pauGlobalMutex);


    return 0;
}



static int func_createMux(char *pauName, char *attribute, char *pushFuncName, char *pullFuncName, char *description)
{
    pau_ts *pPau;
    mux_ts *pMux;
    int i;

    if(!(pPau=findPau(pauName))) {
        errlogPrintf("createMux: there is not PAU which has name %s\n", pauName);
        return -1;
    }

    if(!(pMux = (mux_ts *) malloc(sizeof(mux_ts)))) {
        errlogPrintf("createMux(%s): memory allocation fail.\n", attribute);
        return -1;
    }
    pMux->pMuxDebugInfo = (muxDebugInfo_ts *)malloc(sizeof(muxDebugInfo_ts));
    if(!(pMux->pMuxDebugInfo)) {
        errlogPrintf("createMux(%s): memory allocation fail.\n", attribute);
        return -1;
    }
    pMux->pMuxDebugInfo->usrPullCounter = 0;
    pMux->pMuxDebugInfo->usrPushCounter = 0;

    strcpy(pMux->name, attribute); 
    strcpy(pMux->description, description);
    strcpy(pMux->pushFuncName, pushFuncName);
    strcpy(pMux->pullFuncName, pullFuncName);

    getDevFromPV(pMux->name, pMux->device);

    pMux->pPullUsr = (void *) NULL;
    pMux->pPushUsr = (void *) NULL;

    pMux->pCbOnFunc  = (PAUCBFUNC) NULL;
    pMux->pCbOffFunc = (PAUCBFUNC) NULL;
    pMux->pCbOnUsr   = (void *) NULL;
    pMux->pCbOffUsr  = (void *) NULL;

    pMux->lockMux = epicsMutexCreate();
    pMux->pPau    = pPau;
    pMux->pAssociation = (void *) NULL;
    pMux->fbckMode = 0;

    for(i=0;i<MAXNUM_DATASLOTS; i++) memset(pMux->dataSlot+i, 0, sizeof(dataSlot_ts));


    scanIoInit(&pMux->ioscanpvt);
    

    if(!(pMux->pPushFunc = (PAUUSRFUNC) registryFunctionFind(pMux->pushFuncName))) {
        errlogPrintf("createMux(%s): could not find user function %s.\n", attribute, pMux->pushFuncName);
        /* return -1; */
    }

    if(!(pMux->pPullFunc = (PAUUSRFUNC) registryFunctionFind(pMux->pullFuncName))) {
        errlogPrintf("createMux(%s): could not find user function %s.\n", attribute, pMux->pullFuncName);
        /* return -1; */
    }

    epicsMutexLock(pPau->lockPau);
        pMux->id = (pPau->id | ellCount(&pPau->muxList));
        ellAdd(&pPau->muxList, &pMux->node);
    epicsMutexUnlock(pPau->lockPau);

    return 0;
}


static int func_makeAssociation(char *attribute1, char *attribute2)
{
    mux_ts *pMux1, *pMux2;

    if(!(pMux1 = findMuxwithOnlyName(attribute1))) {
        errlogPrintf("makeAssociation: could not find the mux for the attribute (%s)\n", attribute1);
        return -1; 
    }
    if(!(pMux2 = findMuxwithOnlyName(attribute2))) {
        errlogPrintf("makeAssociation: could not find the mux for the attribute (%s)\n", attribute2);
        return -1;
    }

    if(pMux1->pPau != pMux2->pPau) {
        errlogPrintf("makeAssociation: two muxes have to be in same PAU\n");
        return -1;
    }


    pMux1->pAssociation = (void *) pMux2;
    pMux2->pAssociation = (void *) pMux1;

    return 0;
}



/* API section */

pau_ts* findFirstPau(void)
{
    pau_ts *pPau = (pau_ts *) ellFirst(&pauList_s);
    return pPau;
}

pau_ts* findPau(char *pauName)
{
    pau_ts *pPau = (pau_ts *) ellFirst(&pauList_s);
    
    while(pPau) {
        if(!strcmp(pPau->name, pauName)) break;
        pPau = (pau_ts *) ellNext(&pPau->node);
    }

    return pPau;
}

pau_ts* findPaubyId(unsigned id)
{
    pau_ts *pPau = (pau_ts *) ellFirst(&pauList_s);

    while(pPau) {
        if(pPau->id == id) break;
        pPau = (pau_ts *) ellNext(&pPau->node);
    }

    return pPau;
}


mux_ts* findMux(char *pauName, char *attribute)
{
    mux_ts *pMux = NULL;
    pau_ts *pPau = findPau(pauName);

    if(!pPau) return pMux;

    pMux = (mux_ts *) ellFirst(&pPau->muxList);
    while(pMux) {
        if(!strcmp(pMux->name, attribute)) break;
        pMux = (mux_ts *) ellNext(&pMux->node);
    }

    return pMux;
   
}

mux_ts* findMuxbyId(unsigned pauId, unsigned muxId)
{
    mux_ts *pMux = NULL;
    pau_ts *pPau = findPaubyId(pauId);

    if(!pPau) return pMux;

    pMux = (mux_ts *) ellFirst(&pPau->muxList);
    while(pMux) {
        if(pMux->id == muxId) break;
        pMux = (mux_ts *) ellNext(&pMux->node);
    }

    return pMux;
}

mux_ts* findMuxwithOnlyName(char *attribute)
{
    mux_ts *pMux = NULL;
    pau_ts *pPau = (pau_ts *) ellFirst(&pauList_s);

    while(pPau) {
        pMux = (mux_ts *) ellFirst(&pPau->muxList);

        while(pMux) {
            if(!strcmp(pMux->name, attribute)) return pMux;
            pMux = (mux_ts *) ellNext(&pMux->node);
        }

        pPau = (pau_ts *) ellNext(&pPau->node);
    }

    return pMux;
}

mux_ts* findMuxwithOnlyId(unsigned muxId)
{
    mux_ts *pMux = NULL;
    pau_ts *pPau = (pau_ts *) ellFirst(&pauList_s);

    while(pPau) {
        pMux = (mux_ts *) ellFirst(&pPau->muxList);
    
        while(pMux) {
            if(pMux->id == muxId) return pMux;
            pMux = (mux_ts *) ellNext(&pMux->node);
        }

        pPau = (pau_ts *) ellNext(&pPau->node);
    }

    return pMux;
}


int putInclusionMask(pau_ts *pPau, unsigned slot, unsigned idx, unsigned mask)
{
    pattern_ts *patternData = pPau->patternData;

    if(slot>MAXNUM_DATASLOTS || idx>MAX_EVR_MODIFIER) return -1;

    epicsMutexLock(pPau->lockPau);
        (patternData+slot)->inclusion[idx] = mask;
    epicsMutexUnlock(pPau->lockPau);


    return 0;    
}

int getInclusionMask(pau_ts *pPau, unsigned slot, unsigned idx, unsigned *mask)
{
    pattern_ts *patternData = pPau->patternData;

    if(slot>MAXNUM_DATASLOTS || idx>MAX_EVR_MODIFIER) return -1;

    epicsMutexLock(pPau->lockPau);
        *mask = (patternData+slot)->inclusion[idx];
    epicsMutexUnlock(pPau->lockPau);

    return 0;
}


int putExclusionMask(pau_ts *pPau, unsigned slot, unsigned idx, unsigned mask)
{

    pattern_ts *patternData = pPau->patternData;

    if(slot>MAXNUM_DATASLOTS || idx>MAX_EVR_MODIFIER) return -1;

    epicsMutexLock(pPau->lockPau);
        (patternData+slot)->exclusion[idx] = mask;
    epicsMutexUnlock(pPau->lockPau);

    return 0;
}

int getExclusionMask(pau_ts *pPau, unsigned slot, unsigned idx, unsigned *mask)
{
    pattern_ts *patternData = pPau->patternData;

    if(slot>MAXNUM_DATASLOTS || idx>MAX_EVR_MODIFIER) return -1;

    epicsMutexLock(pPau->lockPau);
        *mask = (patternData+slot)->exclusion[idx];
    epicsMutexUnlock(pPau->lockPau);

    return 0;
}


int putInclusionModifier(pau_ts *pPau, unsigned slot, evrModifier_ta modifier)
{
    int idx;
    pattern_ts *patternData = pPau->patternData;

    if(slot>MAXNUM_DATASLOTS) return -1;

    epicsMutexLock(pPau->lockPau);
        for(idx=0; idx<MAX_EVR_MODIFIER; idx++) (patternData+slot)->inclusion[idx] = modifier[idx];
    epicsMutexUnlock(pPau->lockPau);

    return 0;
}

int getInclusionModifier(pau_ts *pPau, unsigned slot, evrModifier_ta modifier)
{
    int idx;
    pattern_ts *patternData = pPau->patternData;

    if(slot>MAXNUM_DATASLOTS) return -1;

    epicsMutexLock(pPau->lockPau);
        for(idx=0; idx<MAX_EVR_MODIFIER; idx++) modifier[idx] = (patternData+slot)->inclusion[idx];
    epicsMutexUnlock(pPau->lockPau);

    return 0;
}

int putExclusionModifier(pau_ts *pPau, unsigned slot, evrModifier_ta modifier)
{
    int idx;
    pattern_ts *patternData = pPau->patternData;

    if(slot>MAXNUM_DATASLOTS) return -1;

    epicsMutexLock(pPau->lockPau);
        for(idx=0; idx<MAX_EVR_MODIFIER; idx++) (patternData+slot)->exclusion[idx] = modifier[idx];
    epicsMutexUnlock(pPau->lockPau);

    return 0;
}

int getExclusionModifier(pau_ts *pPau, unsigned slot, evrModifier_ta modifier)
{
    int idx;
    pattern_ts *patternData = pPau->patternData;

    if(slot>MAXNUM_DATASLOTS) return -1;

    epicsMutexLock(pPau->lockPau);
        for(idx=0; idx<MAX_EVR_MODIFIER; idx++) modifier[idx] = (patternData+slot)->exclusion[idx];
    epicsMutexUnlock(pPau->lockPau);

    return 0;

}


char  *getPVNameFromMux(mux_ts *pMux)
{
    return pMux->name;
}

char *getDevNameFromMux(mux_ts *pMux)
{
    return pMux->device;
}


double getDataFromDataSlot(mux_ts *pMux)
{
    pau_ts       *pPau           = pMux->pPau;
    unsigned int matchedDataSlot = pPau->advance.matchedDataSlot;
    dataSlot_ts  *pdataSlot      = pMux->dataSlot + matchedDataSlot;
    double       data;

    epicsMutexLock(pMux->lockMux);

    if(pPau->fbckMode) data = pdataSlot->fcomData;
    else               data = pdataSlot->staticData;

    epicsMutexUnlock(pMux->lockMux);

    return data;

}

double getDataFromDataSlot_vMux(mux_ts *pMux)
{
    pau_ts       *pPau           = pMux->pPau;
    unsigned     matchedDataSlot = pPau->advance.matchedDataSlot;
    dataSlot_ts  *pdataSlot      = pMux->dataSlot + matchedDataSlot;
    double       data;

    epicsMutexLock(pMux->lockMux);

    if(pMux->fbckMode) data = pdataSlot->fcomData;
    else               data = pdataSlot->staticData;

    epicsMutexUnlock(pMux->lockMux);

    return data;
}

int setDataToFcomDataSlot(mux_ts *pMux, double data)
{
    pau_ts       *pPau           = pMux->pPau;
    unsigned int matchedDataSlot = pPau->current.matchedDataSlot;
    dataSlot_ts  *pdataSlot      = pMux->dataSlot + matchedDataSlot;

    epicsMutexLock(pMux->lockMux);

    pdataSlot->fcomData = data;

    epicsMutexUnlock(pMux->lockMux);

    return 0;
}


void updateFcomDataSlotFromStaticDataSlot(mux_ts *pMux, unsigned mutex_protection)
{
    pau_ts       *pPau = pMux->pPau;
    unsigned int idx;
    dataSlot_ts  *pdataSlot;

    if(mutex_protection) epicsMutexLock(pMux->lockMux);

    for(idx=0; idx < MAXNUM_DATASLOTS; idx++) {
        pdataSlot           = pMux->dataSlot + idx;
        pdataSlot->fcomData = pdataSlot->staticData;
    }

    if(mutex_protection) epicsMutexUnlock(pMux->lockMux);
}


void updateStaticDataSlotFromFcomDataSlot(mux_ts *pMux, unsigned mutex_protection)
{
    pau_ts       *pPau = pMux->pPau;
    unsigned int idx;
    dataSlot_ts  *pdataSlot;

    if(mutex_protection) epicsMutexLock(pMux->lockMux);

    for(idx=0; idx < MAXNUM_DATASLOTS; idx++) {
        pdataSlot             = pMux->dataSlot + idx;
        pdataSlot->staticData = pdataSlot->fcomData;
    }

    if(mutex_protection) epicsMutexUnlock(pMux->lockMux);
}




int pauVerifyPulseIdFcomBlob(FcomBlob *pBlob, mux_ts *pMux)
{
    pau_ts         *pPau          = pMux->pPau;
    unsigned int   dataSlot       = pPau->current.matchedDataSlot;
    epicsTimeStamp prev_timestamp = pPau->current.prev_timestamp[dataSlot];
    epicsTimeStamp sent_timestamp;
    unsigned int   sentPulseId, matchedPulseId;

    if(pBlob) {

       if((pBlob->hdr.vers != FCOM_PROTO_VERSION) ||
          (pBlob->hdr.type != FCOM_EL_DOUBLE)) return PAU_FCOM_MISMATCH;  /* version/type mismatch */

        sent_timestamp.secPastEpoch = pBlob->fc_tsHi;
        sent_timestamp.nsec         = pBlob->fc_tsLo;

        matchedPulseId              = PULSEID(prev_timestamp);
        sentPulseId                 = PULSEID(sent_timestamp);

        if(matchedPulseId != sentPulseId) return PAU_FCOM_WRONGTIMMING;   /* Not healthy */
        else                              return PAU_FCOM_OK;  /* OK */
    }


    return PAU_FCOM_COMMERR;   /* communication error */
}


int  makeFcomPVNamewithSlotNumber(const char *muxName, const char *fcomPVName, int slot_number)
{
   return  sprintf(fcomPVName, "%s:GETFCOM_%d", muxName, slot_number);

}




/* ========================================================================

    Name: pauReport

    ABS: Driver support registered function for reporting system info

============================================================================ */
static int pauReport(int interest)
{
    pau_ts *pPau;
    mux_ts *pMux;

    epicsThreadOnce(&pauOnceInitFlag, (void*) pauOnceInit, (void*) NULL);

    epicsPrintf("drvPau Report\n");
    epicsPrintf("PowerPC high-resolution tick counter: %d ticks/sec\n", clock_ticks_insec);

    pPau = (pau_ts *) ellFirst(&pauList_s);
    while(pPau) {
        drvPau_pauReport(pPau);
        if(interest) drvPau_pauPatternReport(pPau);


         pMux = (mux_ts *) ellFirst(&pPau->muxList);
         while(pMux) {
             drvPau_muxReport(pMux);
             if(interest) drvPau_muxDataReport(pMux);
             pMux = (mux_ts *) ellNext(&pMux->node);
         }


        pPau = (pau_ts *) ellNext(&pPau->node);
    }     

    epicsPrintf("drvPau "); epicsMessageQueueShow(pauQueue, 10);
    pauTimerReport();
    
    return 0;
}

/* ==========================================================================

    Name: pauInitialize

    Abs: Driver Initialization.

    Rem: Called during iocInit to initialize fiducial and data processing
         Can also be called before iocInit.

============================================================================= */

int pauInitialize(void)
{
    pau_ts *pPau;
    mux_ts *pMux;


    epicsThreadOnce(&pauOnceInitFlag, (void*) pauOnceInit, (void*) NULL);

    if(!ellCount(&pauList_s)) {
        errlogPrintf("pauIntialize: there are no registered PAU\n");
        return -1;
    }

    pPau = (pau_ts*) ellFirst(&pauList_s);              /* find first PAU */
    while(pPau) {                                       /* PAU loop */
        pMux = (mux_ts *) ellFirst(&pPau->muxList);     /* find first Mux */

        while(pMux) {                                    /* Mux Loop */
            if(pMux->pPullFunc) (*pMux->pPullFunc)((void *) pMux, INIT_USRFUNC);    /* Initialization for user pull function */
            if(pMux->pPushFunc) (*pMux->pPushFunc)((void *) pMux, INIT_USRFUNC);    /* Initialization for user push function */
            pMux = (mux_ts *) ellNext(&pMux->node);
        }

        pPau = (pau_ts*) ellNext(&pPau->node);
    }

    pPau = (pau_ts*) ellFirst(&pauList_s);
    while(pPau) {
        evrTimeRegister((FIDUCIALFUNCTION) pPau->pFiducial, (void*) pPau);
        pPau = (pau_ts*) ellNext(&pPau->node);
    } 


    return 0;
}




int createPau(char *pauName, unsigned pipelineIndex, char *description)
{

    epicsThreadOnce(&pauOnceInitFlag, (void*) pauOnceInit, (void*) NULL);

    return func_createPau(pauName, pipelineIndex, description);
}



int createMux(char *pauName, char *attribute, char *pushFuncName, char *pullFuncName, char *description)
{
    epicsThreadOnce(&pauOnceInitFlag, (void*) pauOnceInit, (void*) NULL);

    return func_createMux(pauName, attribute, pushFuncName, pullFuncName, description);
}


int makeAssociation(char *attribute1, char *attribute2)
{
    epicsThreadOnce(&pauOnceInitFlag, (void*) pauOnceInit, (void*) NULL);

    return func_makeAssociation(attribute1, attribute2);
}


