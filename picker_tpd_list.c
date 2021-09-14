/*
 *
 */
#define _GNU_SOURCE
/* */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* */
#include <earthworm.h>
/* */
#include <picker_tpd_list.h>

/* Internal functions' prototypes */
static TRACEINFO *add_new_trace( void **, const char *, const char *, const char *, const char *, const double );
static TRACEINFO *enrich_traceinfo_raw(
	TRACEINFO *, const char *, const char *, const char *, const char *,const double
);
static int  compare_scnl( const void *, const void * ); /* The compare function of binary tree search */
static void cal_total_stations( const void *, const VISIT, const int );
static void free_traceinfo( void * );

/* Global variables */
static volatile void *Root        = NULL;       /* Root of serial binary tree */
static volatile int   TotalTraces = 0;

/*
 * ptpd_list_find() -
 */
TRACEINFO *ptpd_list_find( const char *sta, const char *chan, const char *net, const char *loc )
{
	TRACEINFO *result = NULL;
	TRACEINFO  key;

/* Enrich the key */
	strcpy(key.sta,  sta );
	strcpy(key.chan, chan);
	strcpy(key.net,  net );
	strcpy(key.loc,  loc );
/* Find which station */
	if ( (result = tfind(&key, (void **)&Root, compare_scnl)) == NULL ) {
	/* Not found in the trace table */
		return NULL;
	}
	result = *(TRACEINFO **)result;

	return result;
}

/*
 * ptpd_list_end() -
 */
void ptpd_list_end( void )
{
	tdestroy((void *)Root, free_traceinfo);

	return;
}

/*
 * ptpd_list_walk() -
 */
void ptpd_list_walk( void (*action)(const void *, const VISIT, const int) )
{
	twalk((void *)Root, action);
	return;
}

/*
 * ptpd_list_total_traces() -
 */
int ptpd_list_total_traces( void )
{
	TotalTraces = 0;
	ptpd_list_walk( calc_total_traces );

	return TotalTraces;
}

/*
 *
 */
int ptpd_list_scnl_line_parse( void **root, const char *line )
{
	int    result = 0;
	char   sta[TRACE2_STA_LEN]   = { 0 };
	char   chan[TRACE2_CHAN_LEN] = { 0 };
	char   net[TRACE2_NET_LEN]   = { 0 };
	char   loc[TRACE2_LOC_LEN]   = { 0 };
	double cfactor;

/* */
	if ( sscanf(line, "%s %s %s %s %lf", sta, chan, net, loc, &cfactor) == 5 ) {
		if ( add_new_trace( root, sta, chan, net, loc, cfactor ) == NULL )
			result = -2;
	}
	else {
		logit("e", "picker_tpd: ERROR, lack of some station information in local list!\n");
		result = -1;
	}

	return result;
}

/*
 * ptpd_list_root_reg() -
 */
void *ptpd_list_root_reg( void *root )
{
	void *_root = (void *)Root;

/* Switch the tree's root */
	Root = root;
/* Free the old one */
	sleep_ew(500);
	ptpd_list_root_destroy( _root );

	return (void *)Root;
}

/*
 * ptpd_list_root_destroy() -
 */
void ptpd_list_root_destroy( void *root )
{
	if ( root != (void *)NULL )
		tdestroy(root, free_traceinfo);

	return;
}

/*
 * add_new_trace() -
 */
static _STAINFO *add_new_trace(
	void **root, const char *sta, const char *chan, const char *net, const char *loc, const double cfactor
) {
	TRACEINFO *trace_info = (TRACEINFO *)calloc(1, sizeof(TRACEINFO));
	TRACEINFO *result     = NULL;

	if ( trace_info != NULL ) {
		enrich_traceinfo_raw( trace_info, sta, chan, net, loc, cfactor );
	/* */
		if ( (result = tfind(trace_info, root, compare_scnl)) == NULL ) {
		/* Insert the station information into binary tree */
			if ( tsearch(trace_info, root, compare_scnl) != NULL )
				return trace_info;
			else
				logit("e", "picker_tpd: Error insert trace into binary tree!\n");
		}
		else {
			logit(
				"o", "picker_tpd: %s.%s.%s.%s is already in the list, skip it!\n",
				sta, chan, net, lon
			);
			result = *(TRACEINFO **)result;
		}
	/* */
		free(trace_info);
	}

	return result;
}

/*
 *
 */
static TRACEINFO *enrich_traceinfo_raw(
	TRACEINFO *trace_info, const char *sta, const char *chan, const char *net, const char *loc, const double cfactor
) {
/* */
	strcpy(trace_info->sta,  sta );
	strcpy(trace_info->chan, chan);
	strcpy(trace_info->net,  net );
	strcpy(trace_info->loc,  loc );
	trace_info->cfactor = cfactor;
/* */
	trace_info->firsttime = true;
	ptpd_circ_buf_init( &trace_info->tpd_buffer, DEF_MAX_BUFFER_SAMPLES );

	return trace_info;
}

/*
 *
 */
static void calc_total_traces( const void *nodep, const VISIT which, const int depth )
{
	switch ( which ) {
	case postorder:
	case leaf:
		TotalTraces++;
		break;
	case preorder:
	case endorder:
	default:
		break;
	}

	return;
}

/*
 * compare_scnl() -
 */
static int compare_scnl( const void *a, const void *b )
{
	int rc;
	TRACEINFO *tmpa, *tmpb;

	tmpa = (TRACEINFO *)a;
	tmpb = (TRACEINFO *)b;

	rc = strcmp( tmpa->sta, tmpb->sta );
	if ( rc != 0 ) return rc;
	rc = strcmp( tmpa->chan, tmpb->chan );
	if ( rc != 0 ) return rc;
	rc = strcmp( tmpa->net, tmpb->net );
	if ( rc != 0 ) return rc;
	rc = strcmp( tmpa->loc, tmpb->loc );
	return rc;
}

/*
 * free_traceinfo() -
 */
static void free_traceinfo( void *node )
{
	TRACEINFO *trace_info = (TRACEINFO *)node;

	ptpd_circ_buf_free( &trace_info->tpd_buffer );
	free(trace_info);

	return;
}
