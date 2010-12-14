#ifndef DRV_PAU_H
#define DRV_PAU_H

#include <stdint.h>
#include <epicsMutex.h>
#include <epicsTime.h>          /* epicsTimeStamp */
#include <dbScan.h>             /* IOSCANPVT */
#include <epicsRingBytes.h>
#include <registryFunction.h>   /* REGISTRYFUNCTION */

#include <fcom_api.h>
#include <fcomUtil.h>

#include <evrTime.h>
#include <evrPattern.h>
#include <drvPauTimer.h>

#define PAU_FCOM_OK              0
#define PAU_FCOM_COMMERR        -1
#define PAU_FCOM_WRONGTIMMING    1
#define PAU_FCOM_MISMATCH       -2




#ifdef __cplusplus
{
#endif


#define MAXNUM_DATASLOTS         16
#define MAKE_MUX_ID(pau, mux)    (((pau)<<16)|(mux))

typedef void (*PAUQUEUEFUNC)(void*);
typedef void (*PAUUSRFUNC)(void*,unsigned char);
typedef void (*PAUCBFUNC) (void *);
typedef void (*PAUFIDUCIALFUNC)(void*);

typedef struct {
    PAUQUEUEFUNC pfunc;
    void         *parg;
} pauQueueComp_ts;


/* Data Pull functions */

typedef struct {
    ELLNODE node;
    REGISTRYFUNCTION func;
    char func_name[80];
    void *parg;
} pauDataPullFunc_ts;


ELLLIST pauDataPullFuncList_s;


/* Data Push function */

typedef struct {
    ELLNODE node;
    REGISTRYFUNCTION func;
    char func_name[80];
    void *parg;
} pauDataPushFunc_ts;


typedef struct { 
    uint32_t    tick_cnt_begin;
    uint32_t    tick_cnt_fiducial;
    uint32_t    tick_cnt_timerIsr;
    uint32_t    tick_cnt_pullFunc;
    uint32_t    tick_cnt_pushFunc;
    uint32_t    tick_cnt_diagFunc;
    double      delay_timerIsr_usec;
    double      delay_pullFunc_usec;
    double      delay_pushFunc_usec;
    double      delay_diagFunc_usec;
    double      spinTime_fiduFunc_usec;
    double      spinTime_diagFunc_usec;
} pauDebugInfo_ts;

typedef struct {
    uint32_t    tick_cnt_pullStart;
    uint32_t    tick_cnt_pullEnd;
    uint32_t    tick_cnt_pushStart;
    uint32_t    tick_cnt_pushEnd;

    double      spinTime_pullFunc_usec;
    double      spinTime_pushFunc_usec; 
    unsigned    usrPullCounter;
    unsigned    usrPushCounter;
} muxDebugInfo_ts;



typedef struct {
    unsigned long   beamCode;
    unsigned long   timeSlot;
    evrModifier_ta  inclusion;
    evrModifier_ta  exclusion;
} pattern_ts;

typedef struct {
  void *pUsr1;
  void *pUsr2;
  void *pUsr3;

    double fcomData;
    double staticData;
} dataSlot_ts;


typedef struct {
    epicsTimeStamp     timestamp;
    epicsTimeStamp     prev_timestamp[MAXNUM_DATASLOTS];
    unsigned int       matchedDataSlot;
    unsigned int       usedFlag;
    unsigned long      patternStatus;
    unsigned long      missingCounter;
    epicsRingBytesId   id_history;
    int                snapshot_history[360];
} patternMatches_ts;




typedef struct {
    ELLNODE            node;
    unsigned           id;                          /* mux ID */
    char               name[32];                    /* mux name */
    char               description[80];
    char               device[32];                  /* device name */
    muxDebugInfo_ts    *pMuxDebugInfo;
    epicsMutexId       lockMux;                     /* lock for mux */
    PAUUSRFUNC         pPullFunc;            /* user function for data Pull from FCOM blob */
    PAUUSRFUNC         pPushFunc;            /* user function to implement set value */
    void               *pPullUsr;
    void               *pPushUsr;
    PAUCBFUNC          pCbOnFunc;            /* callback function for feedback ON */
    PAUCBFUNC          pCbOffFunc;           /* callback function for feedback OFF */
    void               *pCbOnUsr;
    void               *pCbOffUsr;
    char               pullFuncName[32];
    char               pushFuncName[32];
    void               *pPau;                       /* pointer for PAU, who is my parent */
    unsigned char      fbckMode;                    /* feedback mode */
    IOSCANPVT          ioscanpvt;                   /* IO SCAN for the mux */
    void               *pAssociation;
    dataSlot_ts        dataSlot[MAXNUM_DATASLOTS];   /* data slots */
} mux_ts;



typedef struct {
    ELLNODE            node;
    unsigned           id;                           /* pau ID */
    char               name[32];                     /* pau name */
    char               description[80];
    unsigned           fiducialCounter;
    unsigned           callbackCallCounter;
    unsigned           pipelineIdx;    
    unsigned char      activatePau;
    unsigned char      activateUsrPull;
    unsigned char      activateUsrPush;
    unsigned char      activateDiag;
    unsigned char      fbckMode;
    PAUFIDUCIALFUNC    pFiducial;
    epicsMutexId       lockPau;                      /* lock for the pau */
    IOSCANPVT          ioscanpvt;                    /* IO SCAN for the pau */
    pauTimer_ts        *pPauTimer;
    double             delay_in_usec;
    pauQueueComp_ts    *pProcessQueueComp;
    pauDebugInfo_ts    *pPauDebugInfo;
    pattern_ts         patternData[MAXNUM_DATASLOTS]; /* pre-defined matched patterns for each data slots */ 
    patternMatches_ts  current;
    patternMatches_ts  advance;
    epicsTimeStamp     timestamp;           
    ELLLIST            muxList;                      /* linked list for muxes in the pau */
} pau_ts;



int pauInitialize(void);

pau_ts* findFirstPau(void);
pau_ts* findPau(char *pauName);
pau_ts* findPaubyId(unsigned id);
mux_ts* findMux(char *pauName, char *attribute);
mux_ts* findMuxbyId(unsigned pauId, unsigned muxId);
mux_ts* findMuxwithOnlyName(char *attribute);
mux_ts* findMuxwithOnlyId(unsigned muxId);

int putInclusionMask(pau_ts *pPau, unsigned slot, unsigned idx, unsigned mask);
int getInclusionMask(pau_ts *pPau, unsigned slot, unsigned idx, unsigned *mask);
int putExclusionMask(pau_ts *pPau, unsigned slot, unsigned idx, unsigned mask);
int getExclusionMask(pau_ts *pPau, unsigned slot, unsigned idx, unsigned *mask);

int putInclusionModifier(pau_ts *pPau, unsigned slot, evrModifier_ta modifier);
int getInclusionModifier(pau_ts *pPau, unsigned slot, evrModifier_ta modifier);
int putExclusionModifier(pau_ts *pPau, unsigned slot, evrModifier_ta modifier);
int getExclusionModifier(pau_ts *pPau, unsigned slot, evrModifier_ta modifier);

char *getPVNameFromMux(mux_ts *pMux);
char *getDevNameFromMux(mux_ts *pMux);
double getDataFromDataSlot(mux_ts *pMux);
double getDataFromDataSlot_vMux(mux_ts *pMux);
int setDataToFcomDataSlot(mux_ts *pMux, double data);
void updateFcomDataSlotFromStaticDataSlot(mux_ts *pMux, unsigned mutex_protection);
void updateStaticDataSlotFromFcomDataSlot(mux_ts *pMux, unsigned mutex_protection);

int  makeFcomPVNamewithSlotNumber(const char *muxName, const char *fcomPVName, int slot_number);
int  pauVerifyPulseIdFcomBlob(FcomBlob *pBlob, mux_ts *pMux);



#ifdef __cplusplus
}
#endif


#endif
