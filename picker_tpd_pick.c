
/* */
#include <trace_buf.h>
/* */
#include <picker_tpd.h>
#include <picker_tpd_circ_buf.h>
#include <picker_tpd_sample.h>

/* */
static int    scan_for_event( TRACEINFO *, void *, int * );
static double calc_delta_tpd_max( const CIRC_BUFFER * );
static double refine_arrival_time( const CIRC_BUFFER *, const double, const double );
static int    refine_stage_1_2( const CIRC_BUFFER *, const double, const int, uint32_t * );
static int    refine_stage_3( const CIRC_BUFFER *, const double, const int, uint32_t * );
static char   define_first_motion( const CIRC_BUFFER *, uint32_t );

/*
 *
 */
void ptpd_pick( TRACEINFO *trace_info, void *trace_buf )
{
	int  event_found;               /* 1 if an event was found */
    int  event_active;              /* 1 if an event is active */
    int  sample_index = -1;         /* Sample index */


	if ( (trace_info->pick_status > 0) || (trace_info->coda_status > 0) )  {

	/* Next, go into search mode */
	}

/* Search mode */
	while ( 1 ) {
	/* this next call looks for threshold crossing only */
		event_found = scan_for_event( trace_info, trace_buf, &sample_index );

		if ( event_found ) {

		}
		else {
			return;
		}
	/* EventActive() tries to see if the event is valid and still active */
	} /* end of search mode while loop */
}

/*
 *
 */
static int scan_for_event( TRACEINFO *trace_info, void *trace_buf, int *sample_index )
{
	TRACE2_HEADER *trh2  = (TRACE2_HEADER *)trace_buf;
	int32_t       *idata = (int32_t *)(trh2 + 1);
	int            sample_refine = 0;
	double         delta_tpd;

/* Set pick and coda calculations to inactive mode */
	trace_info->pick_status = trace_info->coda_status = 0;

/* Loop through all samples in the message */
	while ( ++(*sample_index) < trh2->nsamp ) {
	/* Update Tpd using the current sample */
		ptpd_sample( trace_info, idata[*sample_index] );
	/*  */
		if ( (delta_tpd = calc_delta_tpd_max( &trace_info->tpd_buffer, trace_info->delta )) > TRIGGER_THRESHOLD_C1 ) {
		/* Rollback the arrival time */
			sample_refine = refine_arrival_time( &trace_info->tpd_buffer, trace_info->delta, delta_tpd );
		/* Initialize pick variables */
			trace_info->pick_time = trh2->starttime + (double)(*sample_index - sample_refine) * trace_info->delta;
		/* Picks/codas are now active */
			trace_info->pick_status = trace_info->coda_status = 1;
			return 1;
		}
	}

/* Message ended; event not found */
	return 0;
}

/* Returns negative values if problems:
-1 if coda too short and event aborted
-2 if no recent zero crossings; aborted
-3 noise spike; aborted
aborted means status set to 0 for both Coda and Pick
*/
static int active_for_event( TRACEINFO *trace_info, void *trace_buf, int *sample_index )
{
	TRACE2_HEADER *trh2  = (TRACE2_HEADER *)trace_buf;
	int32_t       *idata = (int32_t *)(trh2 + 1);
	int            sample_refine = 0;
	double         delta_tpd;

/* An event (pick and/or coda) is active. See if it should be declared over. */
	while ( ++(*sample_index) < trh2->nsamp ) {
	/* Update Tpd using the current sample */
		ptpd_sample( trace_info, idata[*sample_index] );

		/********************************************************
		*                 BEGIN CODA CALCULATION               *
		********************************************************/

		/* A coda is active.  Measure coda length and amplitudes.
		Warning. The coda length calculation is correct only
		if the sampling rate is a whole number.
		*****************************************************/
		if ( trace_info->coda_status == 1 ) {

		}

	/* A pick is active */
		if ( trace_info->pick_status == 1 ) {
			int    i;              /* Peak index */
			int    k;              /* Index into sarray */
			int    itrm;           /* Number of small counts allowed before */
		/*   pick is declared over */
			double xon;            /* Used in pick weight calculation */
			double xpc;            /* ditto */
			double xp0,xp1,xp2;    /* ditto */

		/* A valid pick was found. Determine the first motion. */
			trace_info->first_motion = define_first_motion( &trace_info->raw_buffer, pos );

		/* Pick weight calculation */
			xpc = ( Pick->xpk[0] > fabs( (double)Sta->sarray[0] ) ) ?
			Pick->xpk[0] : Pick->xpk[1];
			xon = fabs( (double)Sta->xdot / Sta->xfrz );
			xp0 = Pick->xpk[0] / Sta->xfrz;
			xp1 = Pick->xpk[1] / Sta->xfrz;
			xp2 = Pick->xpk[2] / Sta->xfrz;

			Pick->weight = 3;

			if ( (xp0 > 2.) && (xon > .5) && (xpc > 25.) )
			Pick->weight = 2;

			if ( (xp0 > 3.) && ((xp1 > 3.) || (xp2 > 3.)) && (xon > .5)
			&& (xpc > 100.) )
			Pick->weight = 1;

			if ( (xp0 > 4.) && ((xp1 > 6.) || (xp2 > 6.)) && (xon > .5)
			&& (xpc > 200.) )
			Pick->weight = 0;

			Pick->status = 2;               /* Pick calculated but not reported */

			/* Report pick and coda
			********************/
			if ( Coda->status == 2 ) {
			}
		}

		/******************************************************
		*                 END PICK CALCULATION               *
		******************************************************/

		/* If the pick is over and coda measurement
		is done, scan for a new event.
		****************************************/
		if ( (Pick->status == 0) && (Coda->status == 0) )
		return 0;
}
return 1;       /* Event is still active */
}


/*
 *
 */
static double calc_delta_tpd_max( const CIRC_BUFFER *tpd_buf, const double delta )
{
	int       i;
	const int nsamp   = (int)(3.0 / delta);
	uint32_t  pos     = PTPD_CIRC_BUFFER_LAST_GET( tpd_buf );
	double    tpd     = PTPD_CIRC_BUFFER_DATA_GET( tpd_buf, pos );
	double    tpd_min = tpd;

	for ( i = 0; i < nsamp; i++ ) {
		if ( ptpd_circ_buf_prev( tpd_buf, &tpd, &pos ) < 0 )
			break;
		else if ( tpd > 0. && tpd < tpd_min )
			tpd_min = tpd;
	}
/* */
	tpd = PTPD_CIRC_BUFFER_DATA_GET( tpd_buf, PTPD_CIRC_BUFFER_LAST_GET( tpd_buf ) );

	return tpd - tpd_min;
}

/*
 *
 */
static double refine_arrival_time( const CIRC_BUFFER *tpd_buf, const double delta, const double delta_tpd )
{
	int       result = 0;
	uint32_t  pos = PTPD_CIRC_BUFFER_LAST_GET( tpd_buf );
	const int samprate = (int)(1.0 / delta);

/* 1st & 2nd repick with delta_tpd */
	result = refine_stage_1_2( tpd_buf, delta_tpd, samprate, &pos );
/* 3rd repick by tpd derivative (max 1s) */
	if ( result < (int)PTPD_CIRC_BUFFER_MAX_ELMS_GET( tpd_buf ) )
		result += refine_stage_3( tpd_buf, delta, samprate, &pos );

	return result;
}

/*
 *
 */
static int refine_stage_1_2(
	const CIRC_BUFFER *tpd_buf, const double delta_tpd, const int samprate, uint32_t *last_pos
) {
	int    i, i_end;
	int    result = 0;
	double delta_thr;
	double tpd;
	const double tpd_last = PTPD_CIRC_BUFFER_DATA_GET( tpd_buf, last_pos );

/* 1st repick with delta_tpd */
	i_end     = 3 * samprate;
	delta_thr = delta_tpd * 0.5;
	for ( i = 0; i < i_end; i++ ) {
	/* */
		if ( ptpd_circ_buf_prev( tpd_buf, &tpd, last_pos ) < 0 )
			break;
	/* */
		if ( (tpd_last - tpd) > delta_thr ) {
			result = i;
			break;
		}
	/* Start to use the 2nd refine condition */
		if ( i > (int)(samprate * 0.15) && !result ) {
			delta_thr = delta_tpd * 0.8;
			result = i;
		}
	}

	return result;
}

/*
 *
 */
static int refine_stage_3( const CIRC_BUFFER *tpd_buf, const double delta, const int samprate, uint32_t *pos )
{
	int      i, j;
	int      result = 0;
	double   dtpd;
	double   tpd1, tpd2;
	uint32_t pos_1, pos_2;

/* */
	for ( i = 0; i < samprate; i++ ) {
		pos_1 = *pos;
		ptpd_circ_buf_prev( tpd_buf, NULL, &pos_1 );
		pos_2 = pos_1;
	/* */
		if ( !ptpd_circ_buf_prev( tpd_buf, NULL, &pos_2 ) ) {
			tpd1 = 0.0;
			tpd2 = 0.0;
			for ( j = 0; j < 3; j++ ) {
				tpd1 += PTPD_CIRC_BUFFER_DATA_GET( tpd_buf, pos_1 );
				tpd2 += PTPD_CIRC_BUFFER_DATA_GET( tpd_buf, pos_2 );
				ptpd_circ_buf_next( tpd_buf, NULL, &pos_1 );
				ptpd_circ_buf_next( tpd_buf, NULL, &pos_2 );
			}
			dtpd = (tpd1 - tpd2) / delta / 3.0;
		}
		else {
			tpd1 = PTPD_CIRC_BUFFER_DATA_GET( tpd_buf, *pos );
			tpd2 = PTPD_CIRC_BUFFER_DATA_GET( tpd_buf, pos_1 );
			dtpd = (tpd1 - tpd2) / delta;
		}
	/* */
		if ( dtpd < DEF_THRESHOLD_C2 ) {
			result = i;
			break;
		}
	/* */
		if ( ptpd_circ_buf_prev( tpd_buf, NULL, pos ) )
			break;
	}

	return result;
}

/*
 *
 */
static char define_first_motion( const CIRC_BUFFER *raw_buf, uint32_t first_pos )
{
	int      i;
	int      direction = 0;
	char     result = ' ';
	uint32_t next_pos = first_pos;

/* */
	if ( !ptpd_circ_buf_next( raw_buf, NULL, &next_pos ) ) {
		direction = (int)(PTPD_CIRC_BUFFER_DATA_GET( raw_buf, next_pos ) - PTPD_CIRC_BUFFER_DATA_GET( raw_buf, first_pos ));
		for ( i = 0; i < 10; i++ ) {
			ptpd_circ_buf_next( raw_buf, NULL, &first_pos );
			ptpd_circ_buf_next( raw_buf, NULL, &next_pos );
		/* */
			if ( direction <= 0 ) {
				if ( (PTPD_CIRC_BUFFER_DATA_GET( raw_buf, next_pos ) > PTPD_CIRC_BUFFER_DATA_GET( raw_buf, first_pos )) ) {
					if ( i == 0 )
						break;
					result = '-';  /* First motion is negative */
					break;
				}
			}
			else {
				if ( (PTPD_CIRC_BUFFER_DATA_GET( raw_buf, next_pos ) < PTPD_CIRC_BUFFER_DATA_GET( raw_buf, first_pos )) ) {
					if ( i == 0 )
						break;
					result = '+';  /* First motion is positive */
					break;
				}
			}
		}
	/* */
		if ( i >= 10 )
			result = direction <= 0 ? '-' : '+';
	}

	return result;
}
