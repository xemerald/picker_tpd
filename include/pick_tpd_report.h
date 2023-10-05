#pragma once
/* */
#include <earthworm.h>
#include <transport.h>
/* */
#include <pick_tpd.h>

/* */
void ptpd_pick_report( TRACEINFO *, int, const char *, SHM_INFO *, MSG_LOGO * );
void ptpd_coda_report( TRACEINFO *, int, const char *, SHM_INFO *, MSG_LOGO * );
