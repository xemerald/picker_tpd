
/* */
#include <trace_buf.h>
/* */
#include <picker_tpd.h>
#include <picker_tpd_circ_buf.h>
#include <picker_tpd_sample.h>

/* */
static double calc_delta_tpd_max( const CIRC_BUFFER * );


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
		event_found = scan_event( trace_info, trace_buf, &sample_index );

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
static int scan_event( TRACEINFO *trace_info, void *trace_buf, int *sample_index )
{
	TRACE2_HEADER *trh2  = (TRACE2_HEADER *)trace_buf;
	int32_t       *idata = (int32_t *)(trh2 + 1);

/* Set pick and coda calculations to inactive mode */
	trace_info->pick_status = trace_info->coda_status = 0;

/* Loop through all samples in the message */
	while ( ++(*sample_index) < trh2->nsamp ) {
		int    old_sample;                  /* Previous sample */
		int    new_sample;                  /* Current sample */
		double old_eref;                    /* Old value of eref */

		new_sample = idata[*sample_index];
		old_sample = trace_info->lastsample;

	/* Update Tpd using the current sample */
		ptpd_sample( trace_info, new_sample );

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
	int      i;
	int      nsamp   = (int)(3.0 / delta);
	uint32_t pos     = PTPD_CIRC_BUFFER_LAST_GET( tpd_buf );
	double   tpd_min = tpd_buf->entry[pos];

	dec_circular( tpd_buf, &pos );
	for ( i = 0; i < nsamp; i++, dec_circular( tpd_buf, &pos )) {
		if ( tpd_buf->entry[pos] > 0. && tpd_buf->entry[pos] < tpd_min )
			tpd_min = tpd_buf->entry[pos];
	}

	return tpd_buf->entry[tpd_buf->last] - tpd_min;
}

/*
 *
 */
static double refine_arrival_time( TPD_QUEUE *tpd_buf, double delta_tpd )
{
	int i, i_end, j;
	int result_i = 0;

	double       delta_thr;
	const int    nsamp = (int)(1.0 / tpd_buf->delta);
	unsigned int pos   = tpd_buf->last;

/* 1st repick with delta_tpd */
	i_end     = 3 * nsamp;
	delta_thr = delta_tpd * 0.5;
	dec_circular( tpd_buf, &pos );
	for ( i = 0; i < i_end; i++, dec_circular( tpd_buf, &pos ) ) {
		double _delta = tpd_buf->entry[tpd_buf->last] - tpd_buf->entry[pos];
		if ( _delta > delta_thr ) {
			result_i = i;
			break;
		}
	/* Start to use the 2nd refine condition */
		if ( i > (int)(nsamp * 0.15) && !result_i ) {
			delta_thr = delta_tpd * 0.8;
			result_i = i;
		}
	}

/* 3rd repick by tpd derivative (max 1s) */
	if ( result_i < (int)(tpd_buf->max_elements) ) {
		for ( i = 0; i < nsamp; i++, dec_circular( tpd_buf, &pos ) ) {
			double tpd1 = 0.0;
			double tpd2 = 0.0;
			double dtpd = 0.0;
			unsigned int pos_1 = pos;
			dec_circular( tpd_buf, &pos_1 );
			if ( i < nsamp - 1 ) {
				unsigned int pos_2 = pos_1;
				dec_circular( tpd_buf, &pos_2 );
				for ( j = 0; j < 3; j++, inc_circular( tpd_buf, &pos_1 ), inc_circular( tpd_buf, &pos_2 ) ) {
					tpd1 += tpd_buf->entry[pos_2];
					tpd2 += tpd_buf->entry[pos_1];
				}
				dtpd = (tpd2 - tpd1) / tpd_buf->delta / 3.0;
			}
			else {
				tpd1 = tpd_buf->entry[pos_1];
				tpd2 = tpd_buf->entry[pos];
				dtpd = (tpd2 - tpd1) / tpd_buf->delta;
			}
		/* */
			if ( dtpd < DEF_THRESHOLD_C2 ) {
				result_i += i;
				break;
			}
		}
	}

	return result_i * tpd_buf->delta;
}
