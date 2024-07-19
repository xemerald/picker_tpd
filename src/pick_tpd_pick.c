
/* */
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
/* */
#include <trace_buf.h>
/* */
#include <early_event_msg.h>
#include <pick_tpd.h>
#include <circ_buf.h>
#include <pick_tpd_sample.h>
#include <pick_tpd_report.h>

/* */
static int    scan_for_event( TRACEINFO *, void *, int * );
static int    active_for_event( TRACEINFO *, void *, int *, const int, SHM_INFO *, MSG_LOGO * );
static double refine_arrival_time( const CIRC_BUFFER *, const double, const double, uint32_t * );
static int    refine_stage_1_2( const CIRC_BUFFER *, const double, const int, uint32_t * );
static int    refine_stage_3( const CIRC_BUFFER *, const double, const int, uint32_t * );
static char   define_first_motion( const CIRC_BUFFER *, uint32_t );
static int    define_pick_weight( const CIRC_BUFFER *, const int, uint32_t );

/**
 * @brief
 *
 * @param trace_info
 * @param trace_buf
 * @param pick_id
 * @param region
 * @param put_logo
 */
void ptpd_pick( TRACEINFO *trace_info, void *trace_buf, const int pick_id, SHM_INFO *region, MSG_LOGO *put_logo )
{
	int  event_found;               /* 1 if an event was found */
	int  event_active;              /* 1 if an event is active */
	int  sample_index = -1;         /* Sample index */


	if ( trace_info->pick.status ) {
		if ( active_for_event( trace_info, trace_buf, &sample_index, pick_id, region, put_logo ) == 1 )
			return;
	/* Next, go into search mode */
	}

/* Search mode */
	while ( 1 ) {
	/* this next call looks for threshold crossing only */
		if ( !(event_found = scan_for_event( trace_info, trace_buf, &sample_index )) ) {
			break;
		}
	#ifdef _DEBUG
		printf("pick_tpd: Pick found in %s.%s.%s.%s around %.2lf!\n", trace_info->sta, trace_info->chan, trace_info->net, trace_info->loc, trace_info->lasttime);
	#endif
	/* EventActive() tries to see if the event is valid and still active */
		if ( (event_active = active_for_event( trace_info, trace_buf, &sample_index, pick_id, region, put_logo )) == 1 ) {
			break;
		}
	#ifdef _DEBUG
		printf("pick_tpd: Pick in %s.%s.%s.%s around %.2lf over & reported!\n", trace_info->sta, trace_info->chan, trace_info->net, trace_info->loc, trace_info->lasttime);
	#endif
	} /* end of search mode while loop */

	return;
}

/*
 *
 */
static int scan_for_event( TRACEINFO *trace_info, void *trace_buf, int *sample_index )
{
	TRACE2_HEADER *trh2 = (TRACE2_HEADER *)trace_buf;
	double        *data = (double *)(trh2 + 1);

/* Set pick and coda calculations to inactive mode */
	trace_info->pick.status = trace_info->coda.status = 0;

/* Loop through all samples in the message */
	while ( ++(*sample_index) < trh2->nsamp ) {
	/* Update Tpd using the current sample */
		ptpd_sample( trace_info, data[*sample_index], *sample_index, trh2->nsamp );
	/* */
		if ( trace_info->max_dtpd > TRIGGER_THRESHOLD_C1 ) {
		/* Save the trigger max delta Tpd */
			trace_info->pick.dtpd = trace_info->max_dtpd;
		/* Picks/codas are now active */
			trace_info->pick.status = trace_info->coda.status = 1;

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

/**
 * @brief
 *
 * @param trace_info
 * @param trace_buf
 * @param sample_index
 * @param pick_id
 * @param region
 * @param put_logo
 * @return int
 */
static int active_for_event( TRACEINFO *trace_info, void *trace_buf, int *sample_index, const int pick_id, SHM_INFO *region, MSG_LOGO *put_logo  )
{
	TRACE2_HEADER *trh2        = (TRACE2_HEADER *)trace_buf;
	double        *data        = (double *)(trh2 + 1);
	CIRC_BUFFER   *tpd_buf     = &trace_info->tpd_buffer;
	PICK_INFO     *pick        = &trace_info->pick;
	double         time_sample = trh2->starttime + trace_info->delta * (*sample_index);
	int            sample_refine = 0;
	uint32_t       pick_pos;

/* An event (pick and/or coda) is active. See if it should be declared over. */
	while ( ++(*sample_index) < trh2->nsamp ) {
	/* Update Tpd using the current sample */
		ptpd_sample( trace_info, data[*sample_index], *sample_index, trh2->nsamp );
	/* */
		time_sample += trace_info->delta;
	/* */
		if ( pick->status == 2 && pick_pos == CIRC_BUFFER_LAST_GET( &trace_info->raw_buffer ) )
			pick_pos = CIRC_BUFFER_FIRST_GET( &trace_info->raw_buffer );
	/* A pick is active */
		if (
			pick->status == 1 ||
			(time_sample - pick->time > DEF_MAX_BUFFER_SECONDS && trace_info->max_dtpd > TRIGGER_THRESHOLD_C1 && trace_info->max_dtpd > pick->dtpd)
		) {
		/* */
			if ( pick->status == 2 ) {
				pick->dtpd = trace_info->max_dtpd;
				pick->status = trace_info->coda.status = 1;
			}
		/* Rollback the arrival time */
			sample_refine = refine_arrival_time( tpd_buf, trace_info->delta, pick->dtpd, &pick_pos );
		/* Calculate the real pick time */
			pick->time = time_sample - (sample_refine * trace_info->delta);
		/* Complete pick */
			pick->status = 2;
		}

	/* End of the trigger */
		if (
			CIRC_BUFFER_DATA_GET( tpd_buf, CIRC_BUFFER_LAST_GET( tpd_buf ) ) < DETRIGGER_THRESHOLD_C3 &&
			pick->time + DETRIGGER_SECONDS < time_sample
		) {
			pick->status = 0;
			pick->dtpd = 0.0;
		}
	/* If the pick is over and coda measurement is done, scan for a new event. */
		if ( !pick->status )
			return 0;
	}
/* */
	if ( pick->status == 2 ) {
	/* A valid pick was found. Determine the first motion. */
		pick->first_motion = define_first_motion( &trace_info->raw_buffer, pick_pos );
	/* Pick weight calculation */
		pick->weight = define_pick_weight( &trace_info->raw_buffer, (int)(1.0 / trace_info->delta), pick_pos );
	/* Report pick */
		ptpd_pick_report( trace_info, pick_id, OUTPUT_PICK_PHASE, region, put_logo );
		pick->status = 3;
	}

	return 1;  /* Event is still active */
}

/**
 * @brief
 *
 * @param tpd_buf
 * @param delta
 * @param delta_tpd
 * @param pos
 * @return double
 */
static double refine_arrival_time(
	const CIRC_BUFFER *tpd_buf, const double delta, const double delta_tpd, uint32_t *pos
) {
	int       result = 1;
	const int samprate = (int)(1.0 / delta);

/* */
	*pos = CIRC_BUFFER_LAST_GET( tpd_buf );
/* Backward one sample */
	if ( circ_buf_prev( tpd_buf, NULL, pos ) < 0 )
		return 0;
/* 1st & 2nd repick with delta_tpd */
	result += refine_stage_1_2( tpd_buf, delta_tpd, samprate, pos );
/* 3rd repick by tpd derivative (max 1s) */
	if ( result < (int)CIRC_BUFFER_MAX_ELMS_GET( tpd_buf ) )
		result += refine_stage_3( tpd_buf, delta, samprate, pos );

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
	const double tpd_last = CIRC_BUFFER_DATA_GET( tpd_buf, *last_pos );

/* 1st repick with delta_tpd */
	i_end     = DEF_SEARCH_SECONDS * samprate;
	delta_thr = delta_tpd * 0.5;
	for ( i = 0; i < i_end; i++ ) {
	/* */
		if ( circ_buf_prev( tpd_buf, &tpd, last_pos ) < 0 )
			break;
	/* */
		if ( (tpd_last - tpd) > delta_thr ) {
			result = i;
			break;
		}
	/* Start to use the 2nd refine condition */
		if ( !result && i > (int)(samprate * 0.15) ) {
			delta_thr = delta_tpd * 0.8;
			result = i;
		}
	}

	return result;
}

/*
 *
 */
static int refine_stage_3( const CIRC_BUFFER *tpd_buf, const double delta, const int samprate, uint32_t *last_pos )
{
	int      result = 0;
	double   dtpd;
	double   tpd1, tpd2;
	uint32_t pos_1, pos_2;

/* */
	for ( int i = 0; i < samprate; i++ ) {
		pos_1 = *last_pos;
		circ_buf_prev( tpd_buf, NULL, &pos_1 );
		pos_2 = pos_1;
	/* */
		if ( !circ_buf_prev( tpd_buf, NULL, &pos_2 ) ) {
			tpd1 = 0.0;
			tpd2 = 0.0;
			for ( int j = 0; j < 3; j++ ) {
				tpd1 += CIRC_BUFFER_DATA_GET( tpd_buf, pos_1 );
				tpd2 += CIRC_BUFFER_DATA_GET( tpd_buf, pos_2 );
				circ_buf_next( tpd_buf, NULL, &pos_1 );
				circ_buf_next( tpd_buf, NULL, &pos_2 );
			}
			dtpd = (tpd1 - tpd2) / delta / 3.0;
		}
		else {
			tpd1 = CIRC_BUFFER_DATA_GET( tpd_buf, *last_pos );
			tpd2 = CIRC_BUFFER_DATA_GET( tpd_buf, pos_1 );
			dtpd = (tpd1 - tpd2) / delta;
		}
	/* */
		if ( dtpd < TRIGGER_THRESHOLD_C2 ) {
			result = i;
			break;
		}
	/* */
		if ( circ_buf_prev( tpd_buf, NULL, last_pos ) )
			break;
	}

	return result;
}

/*
 *
 */
static char define_first_motion( const CIRC_BUFFER *raw_buf, uint32_t pick_pos )
{
	int    direction = 0;
	char   result    = EARLY_PICK_POLARITY_UNKNOW;
	double now_data  = CIRC_BUFFER_DATA_GET( raw_buf, pick_pos );
	double next_data;

/* */
	if ( !circ_buf_next( raw_buf, &next_data, &pick_pos ) ) {
		direction = (next_data - now_data) < FLT_EPSILON ? -1 : 1;
		for ( int i = 0; i < 8; i++ ) {
			now_data = next_data;
		/* Samples not enough */
			if ( circ_buf_next( raw_buf, &next_data, &pick_pos ) < 0 )
				break;
		/* */
			if ( direction < 0 ) {
				if ( next_data > now_data || i == 7 ) {
					if ( i < 2 )
						break;
					result = EARLY_PICK_POLARITY_DOWN;  /* First motion is negative */
					break;
				}
			}
			else {
				if ( next_data < now_data || i == 7 ) {
					if ( i < 2 )
						break;
					result = EARLY_PICK_POLARITY_UP;  /* First motion is positive */
					break;
				}
			}
		}
	}

	return result;
}

/*
 *
 */
static int define_pick_weight( const CIRC_BUFFER *raw_buf, const int samprate, uint32_t pick_pos )
{
	int      i;
	int      result = 4;
	double   sum0, sum1;
	double   ratio;
	uint32_t _pos = pick_pos;

/*
 * Calculate P arrival's quality, P arrival's quality
 * is defined by the ratio of sum of 1 sec amplitude
 * square after P arrival over sum of 1 sec amplitude
 * aquare before P arrival.
 */

/* calculating sum of 1 sec amplitude square after P arrival */
	sum0 = 0.0;
	for ( i = 0; i < samprate; i++ ) {
		if ( circ_buf_next( raw_buf, NULL, &_pos ) < 0 )
			break;
		sum0 += CIRC_BUFFER_DATA_GET( raw_buf, _pos ) * CIRC_BUFFER_DATA_GET( raw_buf, _pos );
	}
	sum0 /= i;
/* calculating sum of 1 sec amplitude square before P arrival */
	sum1 = 0.0;
	_pos = pick_pos;
	for ( i = 0; i < samprate; i++ ) {
		if ( circ_buf_prev( raw_buf, NULL, &_pos ) < 0 )
			break;
		sum1 += CIRC_BUFFER_DATA_GET( raw_buf, _pos ) * CIRC_BUFFER_DATA_GET( raw_buf, _pos );
	}
	sum1 /= i;

/*
 * Calculating the ratio
 * If Ratio > 30   P arrival's weighting define 0
 * 30 > Ratio > 15 P arrival's weighting define 1
 * 15 > Ratio > 3  P arrival's weighting define 2
 * 3 > Ratio > 1.5 P arrival's weighting define 3
 * 1.5 > Ratio     P arrival's weighting define 4
 */
	if ( sum1 > 0.001 )
		ratio = sum0 / sum1;
	else
		ratio = 0.1;
/* */
	if ( ratio > 30.0 )
		result = 0;
	else if ( ratio > 15.0 )
		result = 1;
	else if ( ratio > 3.0 )
		result = 2;
	else if ( ratio > 1.5 )
		result = 3;
	else
		result = 4;

	return result;
}
