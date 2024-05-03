#include <stdlib.h>
#include <string.h>
#include <epicsPrint.h>
#include <ellLib.h>
#include <epicsThread.h>

#include <evrTime.h>
#include <eventPattern.h>

#include "drvPau.h"
#include "drvPauReport.h"

int drvPau_pauReport(pau_ts *pPau)
{
    pauDebugInfo_ts *pPauDebugInfo = pPau->pPauDebugInfo;

    epicsPrintf("PAU  name                          : %s\n", pPau->name);
    epicsPrintf("     descripton                    : %s\n", pPau->description);
    epicsPrintf("     id                            : 0x%8.8x\n", pPau->id);
    epicsPrintf("     fiducial counter              : %d\n", pPau->fiducialCounter);
    epicsPrintf("     callback call counter         : %d\n", pPau->callbackCallCounter);
    epicsPrintf("     pipeline index                : %d\n", pPau->pipelineIdx);
    epicsPrintf("     activatePau (1:ON, 0:OFF)     : %d\n", pPau->activatePau);
    epicsPrintf("     activate user pull function   : %d\n", pPau->activateUsrPull);
    epicsPrintf("     activate user push function   : %d\n", pPau->activateUsrPush);
    epicsPrintf("     feedback mode (1:ON, 0:OFF)   : %d\n", pPau->fbckMode);
    epicsPrintf("     Timer resource                : %8.8p\n", pPau->pPauTimer);
    epicsPrintf("     delay (usec)                  : %.3lf\n", pPau->delay_in_usec);
    epicsPrintf("     number of muxes               : %d\n", ellCount(&pPau->muxList));
    epicsPrintf("     delay for timer isr     (usec): %.3lf\n", pPauDebugInfo->delay_timerIsr_usec);
    epicsPrintf("     delay for pull function (usec): %.3lf\n", pPauDebugInfo->delay_pullFunc_usec);
    epicsPrintf("     delay for push function (usec): %.3lf\n", pPauDebugInfo->delay_pullFunc_usec);
    epicsPrintf("     delay for Diag function (usec): %.3lf\n", pPauDebugInfo->delay_diagFunc_usec);
    epicsPrintf("     spin time in fiducial   (usec): %.3lf\n", pPauDebugInfo->spinTime_diagFunc_usec);
    epicsPrintf("     spin time for diag func (usec): %.3lf\n", pPauDebugInfo->spinTime_diagFunc_usec);
    epicsPrintf("     TimeStamp for current pattern matching: 0x%8.8x, 0x%8.8x\n", 
                pPau->current.timestamp.secPastEpoch, pPau->current.timestamp.nsec);
    epicsPrintf("                          matched data slot: %x\n", pPau->current.matchedDataSlot);
    epicsPrintf("                             pattern status: %8.8x\n", pPau->current.patternStatus);
    epicsPrintf("     TimeStamp for advance pattern matching: 0x%8.8x, 0x%8.8x\n", 
                pPau->advance.timestamp.secPastEpoch, pPau->advance.timestamp.nsec);
    epicsPrintf("                          matched data slot: %x\n", pPau->advance.matchedDataSlot);
    epicsPrintf("                             pattern status: %8.8x\n", pPau->advance.patternStatus);


    return 0;
}


int drvPau_pauPatternReport(pau_ts *pPau)
{
    int          i, j;
    pattern_ts   *patternData = pPau->patternData;

    epicsPrintf("    Inclusion mask and Exclusion mask:\n");
    for(i = 0; i < MAXNUM_DATASLOTS; i++) {
        printf("data slot %d\n", i);
        printf("    beam code: %d, time slot: %d\n", (patternData+i)->beamCode, (patternData+i)->timeSlot);
        printf("    inclusion mask:");
        for(j = MAX_EVR_MODIFIER-1; j >= 0; j--) 
            printf("  (%d) 0x%.8x", j, (patternData+i)->inclusion[j]);
        printf("\n    exclusion mask:");
        for(j = MAX_EVR_MODIFIER-1; j >=0; j--)
            printf("  (%d) 0x%.8x", j, (patternData+i)->exclusion[j]);
        printf("\n");
    }
    printf("    history of current pattern matching:\n");
    for(i=0;i<360;i++) printf("%4d", pPau->current.snapshot_history[i]);
    printf("\n");
    printf("    history of advance pattern matching:\n");
    for(i=0;i<360;i++) printf("%4d", pPau->advance.snapshot_history[i]);
    printf("\n"); 

    return 0;
}

int drvPau_muxReport(mux_ts *pMux)
{
    pau_ts          *pPau          = pMux->pPau;
    muxDebugInfo_ts *pMuxDebugInfo = pMux->pMuxDebugInfo;
    mux_ts          *pAssociation  = (mux_ts *) pMux->pAssociation;

    epicsPrintf("  MUX name (attribute)                : %s\n", pMux->name);
    epicsPrintf("    description                       : %s\n", pMux->description);
    epicsPrintf("    device name                       : %s\n", pMux->device);
    epicsPrintf("    pau information                   : %s (%s)\n", pPau->name, pPau->description);
    epicsPrintf("    data pull function                : %s (%.8p)\n", pMux->pullFuncName, pMux->pPullFunc);
    epicsPrintf("    data push function                : %s (%.8p)\n", pMux->pushFuncName, pMux->pPushFunc);
    epicsPrintf("    associated mux                    : %.8p", pMux->pAssociation);
    if(pAssociation) epicsPrintf("  mux(%s)\n", pAssociation->name);
    else epicsPrintf("\n");
    epicsPrintf("    Callback ON function              : %.8p\n", pMux->pCbOnFunc);
    epicsPrintf("    Callback OFF function             : %.8p\n", pMux->pCbOffFunc);
    epicsPrintf("    Callback ON Usr data              : %.8p\n", pMux->pCbOnUsr);
    epicsPrintf("    Callback OFF Usr data             : %.8p\n", pMux->pCbOffUsr);
    epicsPrintf("    spin time for pull function (usec): %.3lf\n", pMuxDebugInfo->spinTime_pullFunc_usec);
    epicsPrintf("    spin time for push function (usec): %.3lf\n", pMuxDebugInfo->spinTime_pushFunc_usec);
    epicsPrintf("    exec. counter for user pull func  : %d\n", pMuxDebugInfo->usrPullCounter);
    epicsPrintf("    exec. counter for user push func  : %d\n", pMuxDebugInfo->usrPushCounter);
    epicsPrintf("    feedback mode for mux (1:ON/0:OFF): %d\n", pMux->fbckMode);
    epicsPrintf("    data pointer for user pull func.  : %8.8p\n", pMux->pPullUsr);
    epicsPrintf("    data pointer for user push func.  : %8.8p\n", pMux->pPushUsr);
    

    return 0;
}

int drvPau_muxDataReport(mux_ts *pMux)
{
    int i;

    dataSlot_ts *dataSlot = pMux->dataSlot;

    for(i=0; i<MAXNUM_DATASLOTS; i++)
        epicsPrintf("Data slot %1X : pUsr1 (%8.8p), pUsr2 (%8.8p), pUsr3 (%8.8p), fcomData(%.3lf), staticData(%.3lf)\n",
                    i, (dataSlot+i)->pUsr1, (dataSlot+i)->pUsr2, (dataSlot+i)->pUsr3,
                    (dataSlot+i)->fcomData, (dataSlot+i)->staticData);

    return 0;
    
}


/*
 *    Command Line Interface for CEXP in RTEMS
 *
 */

int drvPau_show_list(void)
{
    pau_ts *pPau = findFirstPau();
    mux_ts *pMux;

    if(!pPau) {
        epicsPrintf("drvPau_show_list: could not find pau.\n");
        return -1;
    }

    while(pPau) {
        epicsPrintf("PAU: %s (%s)\n", pPau->name, pPau->description);
 
            pMux = (mux_ts *) ellFirst(&pPau->muxList);
            while(pMux) {
                epicsPrintf("\tMUX: %s (%s)\n", pMux->name, pMux->description);
                pMux = (mux_ts *) ellNext(&pMux->node);
            }
    
        pPau = (pau_ts *) ellNext(&pPau->node);
    }

    return 0;
}

int drvPau_show_pauReport(char *pauName)
{
    pau_ts *pPau = findPau(pauName);

    if(pPau) {
        epicsPrintf("drvPau_show_pauReport: find a matched pau name (%s).\n", pauName);
        drvPau_pauReport(pPau);
        return 0;
    }
    else {
        epicsPrintf("drvPau_show_pauReport: could not find matched pau name (%s).\n", pauName);
        return -1;
    }
}

int drvPau_show_pauPatternReport(char *pauName)
{
    pau_ts *pPau = findPau(pauName);

    if(pPau) {
        epicsPrintf("drvPau_show_pauPatternReport: find a matched pau name (%s).\n", pauName);
        drvPau_pauPatternReport(pPau);
        return 0;
    }
    else {
        epicsPrintf("drvPau_show_pauPatternReport: could not find a matched pau name (%s).\n", pauName);
        return -1;
    }
}

int drvPau_show_muxReport(char *muxName)
{
    mux_ts    *pMux = findMuxwithOnlyName(muxName);

    if(pMux) {
        epicsPrintf("drvPau_show_muxReport: find the matched mux name (%s)\n", muxName);
        drvPau_muxReport(pMux);
        return 0;
    }
    else {
        epicsPrintf("drvPau_show_muxReport: could not fine the mux (%s)\n", muxName);
        return -1;
    }
}

int drvPau_show_muxDataReport(char *muxName)
{
    mux_ts    *pMux = findMuxwithOnlyName(muxName);

    if(pMux) {
        epicsPrintf("drvPau_show_muxDataReport: find a matched mux name (%s)\n", muxName);
        drvPau_muxDataReport(pMux);
        return 0;
    }
    else {
       epicsPrintf("drvPau_show_muxDataReport: could not find a matched mux (%s)\n", muxName);
       return -1;
    }
}
