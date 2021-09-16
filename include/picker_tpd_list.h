#pragma once

/* Standard header include */
#include <search.h>
/* */
#include <picker_tpd.h>

/* */
TRACEINFO *ptpd_list_find( const char *, const char *, const char *, const char * );
void       ptpd_list_end( void );
void       ptpd_list_walk( void (*)(const void *, const VISIT, const int) );
int        ptpd_list_total_traces( void );
int        ptpd_list_scnl_line_parse( void **, const char * );
void      *ptpd_list_root_reg( void * );
void       ptpd_list_root_destroy( void * );
