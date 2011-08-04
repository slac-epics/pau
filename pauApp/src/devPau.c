#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errlog.h>
#include <math.h>

#include <alarm.h>
#include <callback.h>
#include <cvtTable.h>
#include <dbDefs.h>
#include <dbAccess.h>
#include <recGbl.h>
#include <recSup.h>
#include <devSup.h>
#include <link.h>
#include <dbCommon.h>

#include <aiRecord.h>
#include <aoRecord.h>
#include <boRecord.h>
#include <longoutRecord.h>
#include <epicsExport.h>

#include <drvPau.h>


typedef struct {
    char    attribute[80];
    int     commandIndex;
    pau_ts  *pPau;
    mux_ts  *pMux;
    int      dataslot_index;
    int      mask_index;
} recPrv_ts;

typedef enum {
    /* for the PAU, analog input record */
    fidCntType,     /* to get fiducial counter */  
    cbCntType,      /* to get callback call counter */
    dlyIsrType,     /* to get delay for timer isr (usec) */
    dlyPullType,    /* to get delay for pull function (usec) */
    dlyPushType,    /* to get delay for push function (usec) */
    dlyDiagType,    /* to get delay for Diag. function (usec) */
    spnFidType,     /* to get spin time for fiducial (usec) */
    spnDiagType,    /* to get spin time for diag. (usec) */
    /* for the MUX, analong input record */
    usrPullCntType, /* to get user pull function execution counter */
    usrPushCntType, /* to get user push function execution counter */
    spnUsrPullType, /* to get spin time for the user pull function (usec) */
    spnUsrPushType, /* to get spin time for the user push function (usec) */
    /* get data from data slot */
    fcomDataType,   /* to get data which are from fcom data */
    staticDataType, /* to get data which are from PV */
    hisCurrType,    /* to get the history of the current pattern matching */
    hisAdvType,     /* to get the history of the advanced pattern mathcing */
} type_getInfo;

typedef enum {
    activatePauType,      /* to activate PAU,  default is activated */
    activateUsrPullType,  /* to activate usr pull functions, default is activated */
    activateUsrPushType,  /* to activate usr push functions, default is activated */
    activateDiagType,     /* to activate diag function, default is activated */
    fbckModeType,       /* to turn ON/OFF the global feedback mode for the pau*/ 
    fbckModeMuxType,    /* to turn ON/OFF the global feedback mode for each mux */
    pipIdxType,     /* to put pipeline index */
    delayType,      /* to put timer delay */
    setDataType,    /* to put static data into data slot */
    beamCodeType,   /* to put beam code for each data slot */
    timeSlotType,   /* to put timeslot for each data slot */
    inclType,       /* to put inclusion mask for each data slot */
    exclType,       /* to put exclusion mask for each data slot */
} type_putInfo;


typedef struct {
    char *string;
    int  index;
} stringIndex_ts;

static stringIndex_ts idx_getInfo[] = { {"getFidCnt",     fidCntType},    /* AI record (for pau): @getFidCnt   pau_name */
                                        {"getCbCnt",      cbCntType},     /* AI record (for pau): @getCbCnt    pau_name */
                                        {"getDlyIsr",     dlyIsrType},    /* AI record (for pau): @getDlyIsr   pau_name */
                                        {"getDlyPull",    dlyPullType},   /* AI record (for pau): @getDlyPull  pau_name */
                                        {"getDlyPush",    dlyPushType},   /* AI record (for pau): @getDlyPush  pau_name */
                                        {"getDlyDiag",    dlyDiagType},   /* AI record (for pau): @getDlyDiag  pau_name */
                                        {"getSpnFid",     spnFidType},    /* AI record (for pau): @getSpnFid   pau_name */
                                        {"getSpnDiag",    spnDiagType},   /* AI record (for pau): @getSpnDiag  pau_name */
                                        {"getUsrPullCnt", usrPullCntType}, /* AI record (for mux): @getUsrPullCnt */
                                        {"getUsrPushCnt", usrPushCntType}, /* AI record (for mux): @getUsrPushCnt */
                                        {"getSpnUsrPull", spnUsrPullType}, /* AI record (for mux): @getSpnUsrPull */
                                        {"getSpnUsrPush", spnUsrPushType}, /* AI record (for mux): @getSpnUsrPush */
                                        {"getfcomData",   fcomDataType},   /* AI record (for data slot): @getfcomData  ds[0-F]*/
                                        {"getstaticData", staticDataType}, /* AI record (for data slot): @getstaticData ds[0-F]*/
                                        {"getHistoryCurr", hisCurrType},   /* WF record (for history of current pattern matching):  @getHistoryCurr pau_name */
                                        {"getHistoryAdv",  hisAdvType},    /* WF record (for history of advanced pattern matching): @getHistoryAdv pau_name */
                                        {(char *) NULL, -1} };

static stringIndex_ts idx_setInfo[] = { {"setActivatePau",     activatePauType},      /* BO record (for pau): @setActivatePau      pau_name */
                                        {"setActivateUsrPull", activateUsrPullType},  /* BO record (for pau): @setActivateUsrPull  pau_name */
                                        {"setActivateUsrPush", activateUsrPushType},  /* BO record (for pau): @setActivateUsrPush  pau_name */
                                        {"setActivateDiag",    activateDiagType},     /* BO record (for pau): @setActiateDiag      pau_name */
                                        {"setFeedbackMode",    fbckModeType},         /* BO record (for pau): @setFeedbackMode     pau_name */
                                        {"setFeedbackModeMux", fbckModeMuxType},      /* BO record (for mux): @setFeedbackModeMux */
                                        {"setPipIdx",          pipIdxType},           /* AO record (for pau): @setPipInx           pau_name */
                                        {"setDelay",           delayType},            /* AO record (for pau): @setDelay            pau_name */
                                        {"setstaticData",      setDataType},          /* AO record (for data slot): @setstaticData ds[0-F] */
                                        {"setBeamCode",        beamCodeType},         /* LO record (for pau data slot): @setBeamCode pau_name ds[0_F] */
                                        {"setTimeSlot",        timeSlotType},         /* LO record (for pau data slot): @setTimeSlot pau_name ds[0_F] */
                                        {"setInclMask",        inclType},             /* LO record (for pau data slot and mask index): @setInclMask pau_name ds[0-F] incl[0-5] */
                                        {"setExclMask",        exclType},             /* LO record (for pau data slot and mask index): @setExclMask pau_name ds[0-F] excl[0-5] */
                                        {(char*) NULL, -1} };



static stringIndex_ts idx_dataSlot[]    = { {"ds0", 0},  {"ds1", 1},  {"ds2", 2},  {"ds3", 3},
                                            {"ds4", 4},  {"ds5", 5},  {"ds6", 6},  {"ds7", 7},
                                            {"ds8", 8},  {"ds9", 9},  {"dsA", 10}, {"dsB", 11},
                                            {"dsC", 12}, {"dsD", 13}, {"dsE", 14}, {"dsF", 15},
                                            {(char*) NULL, -1} };

static stringIndex_ts idx_pattern[]     = { {"incl0", 0}, {"excl0", 0},
                                            {"incl1", 1}, {"excl1", 1},
                                            {"incl2", 2}, {"excl2", 2},
                                            {"incl3", 3}, {"excl3", 3},
                                            {"incl4", 4}, {"excl4", 4},
                                            {"incl5", 5}, {"excl5", 5},
                                            {(char *) NULL, -1} };


static int findIndex(stringIndex_ts *idx, char *string)
{
    int index =0;

    while((idx+index)->string) {
        if(!strcmp((idx+index)->string, string)) break;
        index++;
    }

    return (idx+index)->index;
   
}

static int getAttributeFromName(char *name, char *buffer)
{
    size_t i;

    strcpy(buffer, name);
    i = strlen(buffer);

    while(i) {
        if(buffer[i--] == ':') { buffer[i+1] = 0; break; }
    }

    return 0;
}





static long devAiPau_init_record(aiRecord *precord);
static long devAiPau_read_ai(aiRecord *precord);

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  read_ai;
    DEVSUPFUN  special_linconv;
} devAiPau = {
    6,
    NULL,
    NULL,
    devAiPau_init_record,
    NULL,
    devAiPau_read_ai,
    NULL
};

epicsExportAddress(dset, devAiPau);


static long devAoPau_init_record(aoRecord *precord);
static long devAoPau_write_ao(aoRecord *precord);

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write_ao;
    DEVSUPFUN  special_linconv;
} devAoPau = {
    6,
    NULL,
    NULL,
    devAoPau_init_record,
    NULL,
    devAoPau_write_ao,
    NULL
};

epicsExportAddress(dset, devAoPau);


static long devBoPau_init_record(boRecord *precord);
static long devBoPau_write_bo(boRecord *precord);

struct {
    long        number;
    DEVSUPFUN   report;
    DEVSUPFUN   init;
    DEVSUPFUN   init_record;
    DEVSUPFUN   get_ioint_info;
    DEVSUPFUN   write_bo;
    DEVSUPFUN   special_linconv;
} devBoPau = {
    6,
    NULL,
    NULL,
    devBoPau_init_record,
    NULL,
    devBoPau_write_bo,
    NULL
};

epicsExportAddress(dset, devBoPau);



static long devLongoutPau_init_record(longoutRecord *precord);
static long devLongoutPau_write_lo(longoutRecord *precord);

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write_lo;
    DEVSUPFUN  special_linconv;
} devLongoutPau = {
    6,
    NULL,
    NULL,
    devLongoutPau_init_record,
    NULL,
    devLongoutPau_write_lo,
    NULL
};

epicsExportAddress(dset, devLongoutPau);



static long devAiPau_init_record(aiRecord *precord)
{
    int       i;
    char      option[4][32];
    recPrv_ts *precPrv = (recPrv_ts *) malloc(sizeof(recPrv_ts));
    memset(precPrv, 0, sizeof(recPrv_ts));
    switch(precord->inp.type) {
        case INST_IO:
            i = sscanf(precord->inp.value.instio.string, "%s %s %s %s", option[0], option[1], option[2], option[3]);
            precPrv->commandIndex = findIndex(idx_getInfo, option[0]);
            if(precPrv->commandIndex <0 ) {
                 errlogPrintf("Rec(%s) has an improper param (%s)\n", precord->name, option[0]);
                 goto err1;
            }
            break;
        default:
            err1:
            recGblRecordError(S_db_badField, (void*) precord,
                              "devAiPau (init_record) Illeagal INP filed");
            free(precPrv);
            precord->udf = TRUE;
            precord->dpvt = NULL;
            return S_db_badField;
    }


    switch(precPrv->commandIndex) {

        /* to get the PAU information */
        case fidCntType ... spnDiagType:   /*syntax:  @command  pau_name */
            if(i<2) {
                errlogPrintf("Rec(%s) parm (%s) requires PAU name\n", precord->name, option[0]);
                goto err2; 
            }
            precPrv->pPau = findPau(option[1]);
            if(!precPrv->pPau) {
                errlogPrintf("Rec(%s) could not find PAU (%s)\n", precord->name, option[1]);
                goto err2;
            }
            break;

        /* to get the MUX information */
        case usrPullCntType ... spnUsrPushType: /* syntax: @command */
            getAttributeFromName(precord->name, precPrv->attribute);
            precPrv->pMux = findMuxwithOnlyName(precPrv->attribute);
            if(!precPrv->pMux) {
                errlogPrintf("Rec(%s) counld not find Mux/attribute (%s)\n", precord->name, precPrv->attribute);
                goto err2;
            }
            precPrv->pPau = precPrv->pMux->pPau;
            break;

        /* to get data slot information */
        case fcomDataType ... staticDataType:  /* syntax: @command ds[0-F] */
            getAttributeFromName(precord->name, precPrv->attribute);
            precPrv->pMux = findMuxwithOnlyName(precPrv->attribute);
            if(!precPrv->pMux) {
                errlogPrintf("Rec(%s) could not find Mux/attribute (%s)\n", precord->name, precPrv->attribute);
                goto err2;
            }
            precPrv->pPau = precPrv->pMux->pPau;
            if(i<2) {
                errlogPrintf("Rec(%s) parm (%s) requires the data slot information as like ds[0-F]\n",
                             precord->name, option[0]);
                goto err2;
            }
            precPrv->dataslot_index = findIndex(idx_dataSlot, option[1]);
            if(precPrv->dataslot_index < 0) {
                errlogPrintf("Rec(%s) parm (%s, %s) gave an incorrect data slot name\n",
                             precord->name, option[0], option[1]);
                goto err2;
            }
            break;
            

        default:
            err2:
            recGblRecordError(S_db_badField, (void*) precord,
                              "devAiPau (init_record) Illegal INP filed");
            free(precPrv);
            precord->udf  = TRUE;
            precord->dpvt = NULL;
            return S_db_badField;
    }



    precord->udf  = FALSE;
    precord->dpvt = (void *) precPrv;   
    return 2;
}

static long devAiPau_read_ai(aiRecord *precord)
{
    recPrv_ts *precPrv = (recPrv_ts *) precord->dpvt;
    pau_ts    *pPau    = precPrv->pPau;
    mux_ts    *pMux;

    switch(precPrv->commandIndex) {
                                     /* get information for PAU */
        case fidCntType:
            epicsMutexLock(pPau->lockPau);
                precord->val = pPau->fiducialCounter;
            epicsMutexUnlock(pPau->lockPau);
            break;

        case cbCntType:
            epicsMutexLock(pPau->lockPau);
                precord->val = pPau->callbackCallCounter;
            epicsMutexUnlock(pPau->lockPau);
            break;

        case dlyIsrType:
            epicsMutexLock(pPau->lockPau);
                precord->val = pPau->pPauDebugInfo->delay_timerIsr_usec;
            epicsMutexUnlock(pPau->lockPau);
            break;

        case dlyPullType:
            epicsMutexLock(pPau->lockPau);
                precord->val = pPau->pPauDebugInfo->delay_pullFunc_usec;
            epicsMutexUnlock(pPau->lockPau);
            break;

        case dlyPushType:
            epicsMutexLock(pPau->lockPau); 
                precord->val = pPau->pPauDebugInfo->delay_pushFunc_usec;
            epicsMutexUnlock(pPau->lockPau);
            break;

        case dlyDiagType:
            epicsMutexLock(pPau->lockPau);
                precord->val = pPau->pPauDebugInfo->delay_diagFunc_usec;
            epicsMutexUnlock(pPau->lockPau);
            break;

        case spnFidType:
            epicsMutexLock(pPau->lockPau);
                precord->val = pPau->pPauDebugInfo->spinTime_fiduFunc_usec;
            epicsMutexUnlock(pPau->lockPau);
            break;

        case spnDiagType:
            epicsMutexLock(pPau->lockPau);
                precord->val = pPau->pPauDebugInfo->spinTime_diagFunc_usec;
            epicsMutexUnlock(pPau->lockPau);
            break;

                                     /* get information for the MUX */
        case usrPullCntType:
            pMux = precPrv->pMux;
            epicsMutexLock(pMux->lockMux);
                precord->val = pMux->pMuxDebugInfo->usrPullCounter;
            epicsMutexUnlock(pMux->lockMux);
            break;

        case usrPushCntType:
            pMux = precPrv->pMux;
            epicsMutexLock(pMux->lockMux);
                precord->val = pMux->pMuxDebugInfo->usrPushCounter;
            epicsMutexUnlock(pMux->lockMux);
            break;

        case spnUsrPullType:
            pMux = precPrv->pMux;
            epicsMutexLock(pMux->lockMux);
                precord->val = pMux->pMuxDebugInfo->spinTime_pullFunc_usec;
            epicsMutexUnlock(pMux->lockMux);
            break;

        case spnUsrPushType:
            pMux = precPrv->pMux;
            epicsMutexLock(pMux->lockMux);
                precord->val = pMux->pMuxDebugInfo->spinTime_pushFunc_usec;
            epicsMutexUnlock(pMux->lockMux);
            break;

                                    /* get data slot information */
        case fcomDataType:
            pMux = precPrv->pMux;
            epicsMutexLock(pMux->lockMux);
                precord->val = (pMux->dataSlot+precPrv->dataslot_index)->fcomData;
            epicsMutexUnlock(pMux->lockMux);
            break;

        case staticDataType:
            pMux = precPrv->pMux;
            epicsMutexLock(pMux->lockMux);
                precord->val = (pMux->dataSlot+precPrv->dataslot_index)->staticData;
            epicsMutexUnlock(pMux->lockMux);
            break;
       

    }
    return 2;    /* no conversion */
}

static long devAoPau_init_record(aoRecord *precord)
{
    int       i;
    char      option[4][32];
    recPrv_ts *precPrv = (recPrv_ts *) malloc(sizeof(recPrv_ts));
    memset(precPrv, 0, sizeof(recPrv_ts));

    switch(precord->out.type) {
        case INST_IO:
            i = sscanf(precord->out.value.instio.string, "%s %s %s %s", option[0], option[1], option[2], option[3]);
            precPrv->commandIndex = findIndex(idx_setInfo, option[0]);
            if(precPrv->commandIndex <0) {
                errlogPrintf("Rec(%s) has improper parm (%s)\n", precord->name, option[0]);
                goto err1;
            }
            break;

        default:
            err1:
            recGblRecordError(S_db_badField, (void*) precord,
                              "devAoPau (init_record) Illegal OUT field");
            free(precPrv);
            precord->udf = TRUE;
            precord->dpvt = NULL;
            return S_db_badField;
    }

    switch(precPrv->commandIndex) {
        case pipIdxType ... delayType: /* syntax: @setPipIdx pau_name */
                                       /* syntax: @setDelay  pau_name */
            if(i<2) {
                errlogPrintf("Rec(%s) parm(%s) requires PAU name\n", precord->name, option[0]);
                goto err2;
            }
            precPrv->pPau = findPau(option[1]);
            if(!precPrv->pPau) {
                errlogPrintf("Rec(%s) could not find PAU (%s)\n", precord->name, option[1]);
                goto err2;
            }
            break;
        case setDataType:      /* syntax: @setstaticData ds[0-F] */
            getAttributeFromName(precord->name, precPrv->attribute);
            precPrv->pMux = findMuxwithOnlyName(precPrv->attribute);
            if(!precPrv->pMux) {
                errlogPrintf("Rec(%s) could not find Mux/attribute (%s)\n", precord->name, precPrv->attribute);
                goto err2;
            }
            precPrv->pPau = precPrv->pMux->pPau;
            if(i<2) {
                errlogPrintf("Rec(%s) parm(%s) requires data slot name\n", precord->name, option[0]);
                goto err2;
            }
            precPrv->dataslot_index = findIndex(idx_dataSlot, option[1]);
            if(precPrv->dataslot_index<0) {
                errlogPrintf("Rec(%s) parm (%s, %s) have an incorrect data slot name\n",
                             precord->name, option[0], option[1]);
                goto err2;
            }
            break;
        default:
            err2:
            recGblRecordError(S_db_badField, (void*) precord,
                              "devAoPau (init_record) Illeagal OUT field");
            free(precPrv);
            precord->udf = TRUE;
            precord->dpvt = NULL;
            return S_db_badField;
    }

    precord->udf  = FALSE;
    precord->dpvt = (void *) precPrv;
   

    return 2; 
}

static long devAoPau_write_ao(aoRecord *precord)
{
    recPrv_ts *precPrv = (recPrv_ts *) precord->dpvt;
    pau_ts    *pPau    = precPrv->pPau;
    mux_ts    *pMux;

    switch(precPrv->commandIndex) {
        case pipIdxType:
            epicsMutexLock(pPau->lockPau);
                pPau->pipelineIdx = (unsigned) precord->val;
            epicsMutexUnlock(pPau->lockPau);
            break;
        case delayType:
            epicsMutexLock(pPau->lockPau);
                pPau->delay_in_usec = precord->val;
            epicsMutexUnlock(pPau->lockPau);
            break;
        case setDataType:
            pMux = precPrv->pMux;
            epicsMutexLock(pMux->lockMux);
                (pMux->dataSlot+precPrv->dataslot_index)->staticData = precord->val;
            epicsMutexUnlock(pMux->lockMux);
            break;
    }
    

    return 0;
}

static long devBoPau_init_record(boRecord *precord)
{
    int     i;
    char    option[4][32];
    recPrv_ts *precPrv = (recPrv_ts *) malloc(sizeof(recPrv_ts));
    memset(precPrv, 0, sizeof(recPrv_ts));

    switch(precord->out.type) {
        case INST_IO:
            i = sscanf(precord->out.value.instio.string, "%s %s %s %s", option[0], option[1], option[2], option[3]);
            precPrv->commandIndex = findIndex(idx_setInfo, option[0]);
            if(precPrv->commandIndex <0) {
                errlogPrintf("Rec(%s) has an improper parm (%s)\n", precord->name, option[0]);
                goto err1;
            }
            break;
        default:
            err1:
            recGblRecordError(S_db_badField, (void*) precord,
                              "devBoPau (init_record) Illegal OUT field");
            free(precPrv);
            precord->udf  = TRUE;
            precord->dpvt = NULL;
            return S_db_badField;
    }

    switch(precPrv->commandIndex) {
        /* to turn on and off the feedback mode for PAU */
        case activatePauType ... fbckModeType:   /* syntax: @setActivatePau pau_name */
                                                 /* syntax: @setActivateUsrPull pau_name */
                                                 /* syntax: @setActivateUsrPush pau_name */
                                                 /* syntax: @setActivateDiag pau_name */
                                                 /* syntax: @setFeebackMode pau_name */
            if(i<2) {
                errlogPrintf("Rec(%s) parm(%s) requires PAU name\n", precord->name, option[0]);
                goto err2;
            }
            precPrv->pPau = findPau(option[1]);
            if(!precPrv->pPau) {
                errlogPrintf("Rec(%s) could not find PAU (%s) \n", precord->name, option[1]);
                goto err2;
            }
            break;

        case fbckModeMuxType:    /* syntax: @setFeedbackModeMux */
            if(i<1) {
                goto err2;
            }
            getAttributeFromName(precord->name, precPrv->attribute);
            precPrv->pMux = findMuxwithOnlyName(precPrv->attribute);
            if(!precPrv->pMux) {
                errlogPrintf("Rec(%s) counld not find Mux/attribute (%s)\n", precord->name, precPrv->attribute);
                goto err2;
            }
            precPrv->pPau = precPrv->pMux->pPau;
            break;


            break;

        default:
            err2:
            recGblRecordError(S_db_badField, (void*) precord,
                              "devBoPau (init_recrod) Illegal OUT field");
            free(precPrv);
            precord->udf  = FALSE;
            precord->dpvt = NULL;
            return S_db_badField;
    }

   
    precord->udf  = FALSE;
    precord->dpvt = (void*) precPrv;
    return 2;
}

static long devBoPau_write_bo(boRecord *precord)
{
    recPrv_ts *precPrv = (recPrv_ts *) precord->dpvt;
    pau_ts    *pPau    = precPrv->pPau;
    mux_ts    *pMux;

    unsigned char  fbckMode;

    switch(precPrv->commandIndex) {
        case activatePauType:
            epicsMutexLock(pPau->lockPau);
                pPau->activatePau = (precord->rval)?1:0;
            epicsMutexUnlock(pPau->lockPau);
            break;
        case activateUsrPullType:
            epicsMutexLock(pPau->lockPau);
                pPau->activateUsrPull = (precord->rval)?1:0;
            epicsMutexUnlock(pPau->lockPau);
            break;
        case activateUsrPushType:
            epicsMutexLock(pPau->lockPau);
                pPau->activateUsrPush = (precord->rval)?1:0;
            epicsMutexUnlock(pPau->lockPau);
            break;
        case activateDiagType:
            epicsMutexLock(pPau->lockPau);
                pPau->activateDiag = (precord->rval)?1:0;
            epicsMutexUnlock(pPau->lockPau);
            break;
        case fbckModeType:
            epicsMutexLock(pPau->lockPau);
                pPau->fbckMode = (precord->rval)?1:0;
            epicsMutexUnlock(pPau->lockPau);
            break;
        case fbckModeMuxType:
            pMux = precPrv->pMux;
            epicsMutexLock(pMux->lockMux);
                fbckMode = (precord->rval)?1:0;
                if(fbckMode) { 
                    updateFcomDataSlotFromStaticDataSlot(pMux, 0 /* don't need mutex lock */);
                    if(pMux->pCbOnFunc) (*pMux->pCbOnFunc)(pMux->pCbOnUsr);
                } else {
                    if(pMux->pCbOffFunc) (*pMux->pCbOffFunc)(pMux->pCbOffUsr);
                } 
                pMux->fbckMode = fbckMode;
            epicsMutexUnlock(pMux->lockMux);
            break;
    }

    return 0;
}

static long devLongoutPau_init_record(longoutRecord *precord)
{
    int       i;
    char      option[4][32];
    recPrv_ts *precPrv = (recPrv_ts *) malloc(sizeof(recPrv_ts));
    memset(precPrv, 0, sizeof(recPrv_ts));

    switch(precord->out.type) {
        case INST_IO:
            i = sscanf(precord->out.value.instio.string, "%s %s %s %s", option[0], option[1], option[2], option[3]);
            precPrv->commandIndex = findIndex(idx_setInfo, option[0]);
            if(precPrv->commandIndex <0) {
                errlogPrintf("Rec(%s) has an improper parm (%s)\n", precord->name, option[0]);
                goto err1;
            }
            break;
        default:
            err1:
            recGblRecordError(S_db_badField, (void*) precord,
                              "devLongoutPau (init_record) Illegal OUT field");
            free(precPrv);
            precord->udf = FALSE;
            precord->dpvt = NULL;
            return S_db_badField;
    }

    switch(precPrv->commandIndex) {
        case beamCodeType ... timeSlotType:      /* syntax: @setBeamCode pau_name ds[0-F] */
                                                 /* syntax: @setTimeSlot pau_name ds[0-F] */
            if(i<3) {
                errlogPrintf("Rec(%s) parm(%s) required PAU name, and data slot name\n", precord->name, option[0]);
                goto err2;
            }
            precPrv->pPau = findPau(option[1]);
            if(!precPrv->pPau) {
                errlogPrintf("Rec(%s) could not find PAU (%s)\n", precord->name, option[1]);
                goto err2;
            }
            precPrv->dataslot_index = findIndex(idx_dataSlot, option[2]);
            if(precPrv->dataslot_index < 0) {
                errlogPrintf("Rec(%s) parm (%s, %s, %s) gave an incorrect data slot name\n",
                             precord->name, option[0], option[1], option[2]);
                goto err2;
            }
            break;
        case inclType ... exclType:      /* syntax: @setInclMask pau_name ds[0-F] incl[0-5] */
                                         /* syntax: @setExclMask pau_name ds[0-F] excl[0-5] */
            if(i<4) {
                errlogPrintf("Rec(%s) parm(%s) requires PAU name, data slot name, and mask index\n", precord->name, option[0]);
                goto err2;
            }
            precPrv->pPau = findPau(option[1]);
            if(!precPrv->pPau) {
                errlogPrintf("Rec(%s) could not find PAU (%s)\n", precord->name, option[1]);
                goto err2;
            }
            precPrv->dataslot_index = findIndex(idx_dataSlot, option[2]);
            if(precPrv->dataslot_index<0) {
                errlogPrintf("Rec(%s) parm (%s, %s, %s) gave an incorrect data slot name\n",
                             precord->name, option[0], option[1], option[2]);
                goto err2;
            }
            precPrv->mask_index = findIndex(idx_pattern, option[3]);
            if(precPrv->mask_index<0) {
                errlogPrintf("Rec(%s) parm(%s, %s, %s, %s) gave an incoreect mask index name\n",
                             precord->name, option[0], option[1], option[2], option[3]);
                goto err2;
            }
            break;
        default:
            err2:
            recGblRecordError(S_db_badField, (void*) precord,
                              "devLongoutPau (init_record) Illegal OUT field");
            free(precPrv);
            precord->udf  = TRUE;
            precord->dpvt = NULL;
            return S_db_badField;
    }


    precord->udf  = FALSE;
    precord->dpvt = (void*) precPrv;

    return 2;
}

static long devLongoutPau_write_lo(longoutRecord *precord)
{
    recPrv_ts  *precPrv      = (recPrv_ts *) precord->dpvt;
    pau_ts     *pPau         = precPrv->pPau;
    pattern_ts *patternData  = pPau->patternData;
    int       dataslot_index = precPrv->dataslot_index;
    int       mask_index     = precPrv->mask_index;

    switch(precPrv->commandIndex) {
        case beamCodeType:
            epicsMutexLock(pPau->lockPau);
                (patternData+dataslot_index)->beamCode = precord->val;
            epicsMutexUnlock(pPau->lockPau);
            break;
        case timeSlotType:
            epicsMutexLock(pPau->lockPau);
                (patternData+dataslot_index)->timeSlot = precord->val;
            epicsMutexUnlock(pPau->lockPau);
            break;
        case inclType:
            epicsMutexLock(pPau->lockPau);
                (patternData+dataslot_index)->inclusion[mask_index] = precord->val;
            epicsMutexUnlock(pPau->lockPau);
            break;
        case exclType:
            epicsMutexLock(pPau->lockPau);
                (patternData+dataslot_index)->exclusion[mask_index] = precord->val;
            epicsMutexUnlock(pPau->lockPau);
            break;
    }

    return 0;
}
