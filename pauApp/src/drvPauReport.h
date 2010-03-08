#ifndef DRV_PAUREPORT_H
#define DRV_PAUREPORT_H


#include <drvPau.h>

int drvPau_pauReport(pau_ts *pPau);
int drvPau_pauPatternReport(pau_ts *pPau);
int drvPau_muxReport(mux_ts *pMux);
int drvPau_muxDataReport(mux_ts *pMux);


int drvPau_show_list(void);
int drvPau_show_pauReport(char *pauName);
int drvPau_show_pauPatternReport(char *pauName);
int drvPau_show_muxRerport(char *muxName);
int drvPau_show_muxDataReport(char *muxName);

#endif
