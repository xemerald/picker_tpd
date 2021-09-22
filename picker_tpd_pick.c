
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

/*
 *
 */
void ptpd_pick( TRACEINFO *trace_info, void *trace_buf )
{
	int  event_found;               /* 1 if an event was found */
    int  event_active;              /* 1 if an event is active */
    int  sample_index = -1;         /* Sample index */


	if ( (trace_info->pick_status > 0) || (trace_info->coda_status > 0) )  {
		//event_active = scan_event( trace_info, trace_buf, &sample_index );

		if ( event_active == 1 )           /* Event active at end of message */
			return;

		if ( event_active == -1 )
			logit( "e", "Coda too short. Event aborted.\n" );

		if ( event_active == -2 )
			logit( "e", "No recent zero crossings. Event aborted.\n" );

		if ( event_active == -3 )
			logit( "e", "Noise pick. Event aborted.\n" );

		if ( event_active == 0 )
			logit( "e", "Event over. Picks/codas reported.\n" );
	/* Next, go into search mode */
	}

/* Search mode */
	while ( 1 ) {
	/* this next call looks for threshold crossing only */
		event_found = scan_for_event( trace_info, trace_buf, &sample_index );

		if ( event_found ) {
			if ( Gparm->Debug ) logit( "e", "Event found.\n" );
		}
		else {
			if ( Gparm->Debug ) logit( "e", "Event not found.\n" );
			return;
		}
	/* EventActive() tries to see if the event is valid and still active */
		//event_active = EventActive( Sta, WaveBuf, Gparm, Ewh, &sample_index );

		if ( event_active == 1 )           /* Event active at end of message */
			return;

		if ( (event_active == -1) && Gparm->Debug )
			logit( "e", "Coda too short. Event aborted.\n" );

		if ( (event_active == -2) && Gparm->Debug )
			logit( "e", "No recent zero crossings. Event aborted.\n" );

		if ( (event_active == -3) && Gparm->Debug )
			logit( "e", "Noise pick. Event aborted.\n" );

		if ( (event_active == 0) && Gparm->Debug )
			logit( "e", "Event over. Picks/codas reported.\n" );
	} /* end of search mode while loop */
}

/*
 *
 */
static int scan_for_event( TRACEINFO *trace_info, void *trace_buf, int *sample_index )
{
	TRACE2_HEADER *trh2  = (TRACE2_HEADER *)trace_buf;
	int32_t       *idata = (int32_t *)(trh2 + 1);

/* Set pick and coda calculations to inactive mode */
	trace_info->pick_status = trace_info->coda_status = 0;

/* Loop through all samples in the message */
	while ( ++(*sample_index) < trh2->nsamp ) {
	/* Update Tpd using the current sample */
		ptpd_sample( trace_info, idata[*sample_index] );
	/*  */
		if ( calc_delta_tpd_max( &trace_info->tpd_buffer, trace_info->delta ) > TRIGGER_THRESHOLD_C1 ) {
		/* Initialize pick variables */
			trace_info->pick_time = trh2->starttime + (double)*sample_index * trace_info->delta;
		/* Picks/codas are now active */
			trace_info->pick_status = trace_info->coda_status = 0;
			return 1;
		}
	}

/* Message ended; event not found */
	return 0;
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
