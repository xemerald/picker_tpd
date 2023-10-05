/* */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
/* */
#include <trace_buf.h>
/* */
#include <pick_tpd.h>
#include <circ_buf.h>

/* */
static double calc_tpd( const double, const double, const double, const double, double *, double *, double *, double * );
static void   update_mintpd_buf( TRACEINFO *, const int );
static double calc_delta_tpd_max( MIN_VALUE *, const double, const int, const double );

/* */
static uint8_t   InitMinTpdReady = 0;
static MIN_VALUE InitMinTpd[DEF_MAX_BUFFER_SAMPLES];

/**
 * @brief
 *
 * @param trace_info
 * @param sample
 * @param sample_index
 * @param nsamp
 */
void ptpd_sample( TRACEINFO *trace_info, double sample, const int sample_index, const int nsamp )
{
/* Process the new sample & put it into buffer */
	sample          *= trace_info->cfactor;
	trace_info->avg += 0.001 * (sample - trace_info->avg);
	sample          -= trace_info->avg;
	circ_buf_enbuf( &trace_info->raw_buffer, sample );
/* Then, calculate the new Tpd & also save it in the buffer */
	sample = calc_tpd(
		trace_info->delta, trace_info->alpha, trace_info->beta, sample,
		&trace_info->ldata, &trace_info->xdata, &trace_info->ddata, &trace_info->avg_noise
	);
	circ_buf_enbuf( &trace_info->tpd_buffer, sample );
/* */
	if ( !sample_index )
		update_mintpd_buf( trace_info, nsamp );
	trace_info->max_dtpd = calc_delta_tpd_max( trace_info->min_tpd, sample, sample_index, trace_info->delta );

	return;
}

/**
 * @brief
 *
 * @param trace_info
 * @param trh2
 */
void ptpd_param_init( TRACEINFO *trace_info, const TRACE2_HEADER *trh2 )
{
	trace_info->cfactor    = 1.0;
	trace_info->readycount = 0;
	trace_info->lastsample = 0.0;
	trace_info->lasttime   = trh2->starttime;
	trace_info->delta      = 1.0 / trh2->samprate;
	trace_info->avg        = 0.0;
	trace_info->alpha      = exp(log(0.1) * trace_info->delta / DEF_TIME_WINDOW);
	trace_info->beta       = 1.0 - exp(log(0.1) * trace_info->delta / 100.0);
	trace_info->ldata      = 0.0;
	trace_info->xdata      = 0.0;
	trace_info->ddata      = 0.0;
	trace_info->avg_noise  = 0.0;
	trace_info->max_dtpd   = 0.0;
/* */
	if ( (int)(trh2->samprate * DEF_MAX_BUFFER_SECONDS) > CIRC_BUFFER_MAX_ELMS_GET( &trace_info->raw_buffer ) ) {
		circ_buf_free( &trace_info->raw_buffer );
		circ_buf_free( &trace_info->tpd_buffer );
		circ_buf_init( &trace_info->raw_buffer, trh2->samprate * DEF_MAX_BUFFER_SECONDS );
		circ_buf_init( &trace_info->tpd_buffer, trh2->samprate * DEF_MAX_BUFFER_SECONDS );
	}
/* */
	if ( !InitMinTpdReady ) {
		for ( int i = 0; i < DEF_MAX_BUFFER_SAMPLES; i++ ) {
			InitMinTpd[i].value = -1.0;
			InitMinTpd[i].samp_index = -1;
		}
	}
	memcpy(&trace_info->min_tpd, InitMinTpd, sizeof(trace_info->min_tpd));

	return;
}

/**
 * @brief
 *
 * @param dest
 * @param dest_size
 * @param src
 * @return int
 */
int ptpd_local_load( void *dest, const int dest_size, const void *src )
{
	TRACE2_HEADER *dest_trh2  = (TRACE2_HEADER *)dest;
	TRACE2_HEADER *src_trh2   = (TRACE2_HEADER *)src;
	double        *destdata   = (double *)(dest_trh2 + 1);
	double        *dest_end   = (double *)((uint8_t *)dest + dest_size);
	int            dest_nsamp = 0;

/* */
	if ( dest == src )
		return -1;
/* */
	*dest_trh2 = *src_trh2;
	dest_trh2->datatype[0] = 'f';
	dest_trh2->datatype[1] = '8';
/* Source datatype is integer */
	if ( src_trh2->datatype[1] == '4' && (src_trh2->datatype[0] == 'i' || src_trh2->datatype[0] == 's') ) {
		const int32_t *srcdata_i    = (int32_t *)(src_trh2 + 1);
		const int32_t *srcdata_iend = srcdata_i + src_trh2->nsamp;
	/* */
		for ( ; srcdata_i < srcdata_iend && destdata < dest_end; srcdata_i++, destdata++ ) {
			*destdata = *srcdata_i;
			dest_nsamp++;
		}
	}
/* Source datatype is single precision floating number */
	if ( src_trh2->datatype[1] == '4' && (src_trh2->datatype[0] == 'f' || src_trh2->datatype[0] == 't') ) {
		const float *srcdata_f    = (float *)(src_trh2 + 1);
		const float *srcdata_fend = srcdata_f + src_trh2->nsamp;
	/* */
		for ( ; srcdata_f < srcdata_fend && destdata < dest_end; srcdata_f++, destdata++ ) {
			*destdata = *srcdata_f;
			dest_nsamp++;
		}
	}
/* Source datatype is short integer */
	else if ( src_trh2->datatype[1] == '2' ) {
		const int16_t *srcdata_s    = (int16_t *)(src_trh2 + 1);
		const int16_t *srcdata_send = srcdata_s + src_trh2->nsamp;
	/* */
		for ( ; srcdata_s < srcdata_send && destdata < dest_end; srcdata_s++, destdata++ ) {
			*destdata = *srcdata_s;
			dest_nsamp++;
		}
	}
/* Source datatype is double precision floating number */
	else if ( src_trh2->datatype[1] == '8' ) {
		const double *srcdata_d    = (double *)(src_trh2 + 1);
		const double *srcdata_dend = srcdata_d + src_trh2->nsamp;
	/* */
		for ( ; srcdata_d < srcdata_dend && destdata < dest_end; srcdata_d++, destdata++ ) {
			*destdata = *srcdata_d;
			dest_nsamp++;
		}
	}

	return dest_nsamp;
}

/*
 * ptpd_interpolate() - Interpolate samples and insert them at the beginning of
 *                      the waveform.
 */

/**
 * @brief
 *
 * @param trace_info
 * @param trace_buf
 * @param gaps
 */
void ptpd_interpolate( TRACEINFO *trace_info, void *trace_buf, int gaps )
{
	TRACE2_HEADER *trh2  = (TRACE2_HEADER *)trace_buf;
	double        *data  = (double *)(trh2 + 1);
	const double   delta = (data[0] - trace_info->lastsample) / gaps;
	double         sum_delta;

/* */
	gaps--;
	for ( int i = trh2->nsamp - 1; i >= 0; i-- )
		data[i + gaps] = data[i];
/* */
	sum_delta = delta;
	for ( int i = 0; i < gaps; i++, sum_delta += delta )
		data[i] = trace_info->lastsample + sum_delta;
/* */
	trh2->nsamp += gaps;
	trh2->starttime = trace_info->lasttime + trace_info->delta;

	return;
}

/*
 *
 */
static double calc_tpd(
	const double delta, const double alpha, const double beta, const double sample,
	double *ldata, double *xdata, double *ddata, double *avg_noise
) {
	double result;

	result     = (sample - *ldata) / delta;
	*ldata     = sample;
	*xdata     = alpha * (*xdata) + sample * sample;
	*ddata     = alpha * (*ddata) + result * result;
	*avg_noise = (*avg_noise) + beta * (sample * sample - (*avg_noise)); /* There might be another way to get the value */
	result     = PI2_SQ * (*avg_noise) * DEF_TIME_WINDOW / (delta * DEF_TMX_SQ);
	result     = PI2 * sqrt((*xdata) / ((*ddata) + result));

	return result;
}

/**
 * @brief
 *
 * @param trace_info
 * @param nsamp
 */
static void update_mintpd_buf( TRACEINFO *trace_info, const int nsamp )
{
	CIRC_BUFFER *tpd_buf = &trace_info->tpd_buffer;
	uint32_t     pos     = CIRC_BUFFER_LAST_GET( tpd_buf );
	double       tpd     = CIRC_BUFFER_DATA_GET( tpd_buf, pos );
	MIN_VALUE    mtpd    = { .value = tpd, .samp_index = (int)(DEF_SEARCH_SECONDS / trace_info->delta) - 1 };

/* Re-initialization of min-Tpd array */
	memcpy(&trace_info->min_tpd, InitMinTpd, sizeof(trace_info->min_tpd));
/* */
	for ( int i = mtpd.samp_index; i >= 0; i-- ) {
		if ( !circ_buf_prev( tpd_buf, &tpd, &pos ) && tpd > 0.0 && tpd < mtpd.value ) {
			mtpd.value = tpd;
			mtpd.samp_index = i;
		}
	/* */
		trace_info->min_tpd[i] = mtpd;
	}

	return;
}

/**
 * @brief
 *
 * @param min_tpd
 * @param tpd
 * @param sample_index
 * @param delta
 * @return double
 */
static double calc_delta_tpd_max( MIN_VALUE *min_tpd, const double tpd, const int sample_index, const double delta )
{
	const int  alive_samp = (int)(DEF_SEARCH_SECONDS / delta) + sample_index;
	MIN_VALUE *mtpd_ptr   = &min_tpd[sample_index];
	MIN_VALUE *mtpd_min   = &min_tpd[mtpd_ptr->samp_index + 1];
	MIN_VALUE *mtpd_end   = &min_tpd[alive_samp];
	MIN_VALUE  mtpd_new   = { .value = tpd, .samp_index = alive_samp };
	double     result     = 0.0;

/* */
	if ( mtpd_ptr->samp_index < 0 )
		return -1.0;
/* */
	if ( tpd < mtpd_ptr->value ) {
		for ( mtpd_ptr++; mtpd_ptr < mtpd_min; mtpd_ptr++ )
			*mtpd_ptr = mtpd_new;
	}
	else {
		result = tpd - mtpd_ptr->value;
	}
/* */
	for ( mtpd_ptr = mtpd_min; mtpd_ptr <= mtpd_end; mtpd_ptr++ ) {
		if ( tpd < mtpd_ptr->value || mtpd_ptr->samp_index < 0 )
			*mtpd_ptr = mtpd_new;
		else
			mtpd_ptr = &min_tpd[mtpd_ptr->samp_index];
	}

	return result;
}
