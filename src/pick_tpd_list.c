/*
 *
 */
#define _GNU_SOURCE
/* */
#include <stdlib.h>
#include <string.h>
#include <search.h>
/* */
#include <earthworm.h>
/* */
#include <pick_tpd.h>
#include <pick_tpd_list.h>

static int  compare_scnl( const void *, const void * );	/* The compare function of binary tree search */
static void free_node( void * );

static void *Root = NULL;         /* Root of binary tree */

/*
 * ptpd_list_search() - Insert the trace to the tree.
 * Arguments:
 *   trh2 = Pointer of the trace you want to find.
 * Returns:
 *    NULL = Didn't find the trace.
 *   !NULL = The Pointer to the trace.
 */
TRACEINFO *ptpd_list_search( const TRACE2_HEADER *trh2 )
{
	TRACEINFO *traceptr = NULL;

/* Test if already in the tree */
	if ( (traceptr = ptpd_list_find( trh2 )) == NULL ) {
		traceptr = (TRACEINFO *)calloc(1, sizeof(TRACEINFO));
		memcpy(traceptr->sta,  trh2->sta, TRACE2_STA_LEN);
		memcpy(traceptr->net,  trh2->net, TRACE2_NET_LEN);
		memcpy(traceptr->loc,  trh2->loc, TRACE2_LOC_LEN);
		memcpy(traceptr->chan, trh2->chan, TRACE2_CHAN_LEN);

		if ( (traceptr = tsearch(traceptr, &Root, compare_scnl)) == NULL ) {
		/* Something error when insert into the tree */
			return NULL;
		}

		traceptr = *(TRACEINFO **)traceptr;
		traceptr->firsttime = TRUE;
		traceptr->match     = NULL;
	}

	return traceptr;
}

/*
 * ptpd_list_find( ) -- Output the Trace that match the SCNL.
 * Arguments:
 *   trh2 = Pointer of the trace you want to find.
 * Returns:
 *    NULL = Didn't find the trace.
 *   !NULL = The Pointer to the trace.
 */
TRACEINFO *ptpd_list_find( const TRACE2_HEADER *trh2 )
{
	TRACEINFO  key;
	TRACEINFO *traceptr = NULL;

	memcpy(key.sta,  trh2->sta, TRACE2_STA_LEN);
	memcpy(key.net,  trh2->net, TRACE2_NET_LEN);
	memcpy(key.loc,  trh2->loc, TRACE2_LOC_LEN);
	memcpy(key.chan, trh2->chan, TRACE2_CHAN_LEN);

/* Find which trace */
	if ( (traceptr = tfind(&key, &Root, compare_scnl)) == NULL ) {
	/* Not found in trace table */
		return NULL;
	}

	traceptr = *(TRACEINFO **)traceptr;

	return traceptr;
}

/*
 * ptpd_list_end() - End process of Trace list.
 * Arguments:
 *   None.
 * Returns:
 *   None.
 */
void ptpd_list_end( void )
{
	tdestroy(Root, free_node);

	return;
}

/*
 * compare_scnl() - the SCNL compare function of binary tree search
 */
static int compare_scnl( const void *a, const void *b )
{
	TRACEINFO *tmpa = (TRACEINFO *)a;
	TRACEINFO *tmpb = (TRACEINFO *)b;
	int         rc;

	if ( (rc = strcmp(tmpa->sta, tmpb->sta)) != 0 )
		return rc;
	if ( (rc = strcmp(tmpa->chan, tmpb->chan)) != 0 )
		return rc;
	if ( (rc = strcmp(tmpa->net, tmpb->net)) != 0 )
		return rc;
	return strcmp(tmpa->loc, tmpb->loc);
}

/*
 * free_node() - free node of binary tree search
 */
static void free_node( void *node )
{
	TRACEINFO *traceptr = (TRACEINFO *)node;


	free(traceptr);

	return;
}
