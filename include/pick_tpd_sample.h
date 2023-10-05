#pragma once
/* */
#include <pick_tpd.h>
/* */
void ptpd_sample( TRACEINFO *, double, const int, const int );
void ptpd_param_init( TRACEINFO *, const TRACE2_HEADER * );
int  ptpd_local_load( void *, const int, const void * );
void ptpd_interpolate( TRACEINFO *, void *, int );
