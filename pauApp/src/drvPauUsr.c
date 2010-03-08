#include <epicsPrint.h>
#include <registryFunction.h>
#include <epicsExport.h>

#include <drvPau.h>

typedef struct {
    char string[80];
    unsigned dummy_uint[10];
    double   dummy_val[10];
} myData_ts;



static void pauUsrPullDummyInit(void *parg)
{
    mux_ts *pMux = (mux_ts *)parg;
    pau_ts *pPau = pMux->pPau;


    epicsPrintf("pauUsrPullDummyInit: pau(%s), mux(%s)\n",pPau->name, pMux->name);

    return;
}

static void pauUsrPullDummy(void *parg, unsigned char initFlag)
{
    mux_ts          *pMux          = (mux_ts *)parg;
    muxDebugInfo_ts *pMuxDebugInfo = pMux->pMuxDebugInfo;


    if(initFlag) return pauUsrPullDummyInit((void*) pMux);

    pMuxDebugInfo->usrPullCounter++;    /* increase counter for debugging */


    
}


static void pauUsrPushDummyInit(void *parg)
{
    mux_ts *pMux = (mux_ts *)parg;
    pau_ts *pPau = pMux->pPau;

    epicsPrintf("pauUsrPushDummyInit: pau(%s), mux(%s)\n", pPau->name, pMux->name);

    return;
}


static void pauUsrPushDummy(void *parg, unsigned char initFlag)
{
    mux_ts          *pMux          = (mux_ts *)parg;
    muxDebugInfo_ts *pMuxDebugInfo = pMux->pMuxDebugInfo;

    if(initFlag) return pauUsrPushDummyInit((void*)pMux);

    pMuxDebugInfo->usrPushCounter++;    /* increase counter for debugging */

}

epicsRegisterFunction(pauUsrPullDummy);
epicsRegisterFunction(pauUsrPushDummy);
