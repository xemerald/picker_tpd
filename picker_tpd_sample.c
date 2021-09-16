/* */
#include <stdint.h>
#include <math.h>
/* */
#include <trace_buf.h>
/* */
#include <picker_tpd.h>
#include <picker_tpd_circ_buf.h>

/* */
static double calc_tpd(
	const double, const double, const double, const double, double *, double *, double *, double *
);
static inline double calc_mavg( const double, const double );

/*
 *
 */
void ptpd_sample( TRACEINFO *trace_info, int sample )
{
	double ndata;

/* */
	ndata           = sample * trace_info->cfactor;
	trace_info->avg = calc_mavg( ndata, trace_info->avg );
	ndata          -= trace_info->avg;
/* */
	ndata = calc_tpd(
		trace_info->delta, trace_info->alpha, trace_info->beta, ndata,
		&trace_info->ldata, &trace_info->xdata, &trace_info->ddata, &trace_info->avg_noise
	);
	ptpd_circ_buf_enbuf( &trace_info->tpd_buffer, ndata );

	return;
}

/*
 * ptpd_interpolate() - Interpolate samples and insert them at the beginning of
 *                      the waveform.
 */
void ptpd_interpolate( TRACEINFO *trace_info, void *trace_buf, int gaps )
{
	int            i;
	TRACE2_HEADER *trh2  = (TRACE2_HEADER *)trace_buf;
	int32_t       *idata = (int32_t *)(trh2 + 1);
	const double   delta = (double)(idata[0] - trace_info->lastsample) / gaps;
	double         sum_delta;

/* */
	gaps--;
	for ( i = trh2->nsamp - 1; i >= 0; i-- )
		idata[i + gaps] = idata[i];

	for ( i = 0, sum_delta = delta; i < gaps; i++, sum_delta += delta )
		idata[i] = (int32_t)(trace_info->lastsample + sum_delta + 0.5);

	trh2->nsamp    += gaps;
	trh2->starttime = trace_info->lasttime + (1.0 / trh2->samprate);

	return;
}

/*
 *
 */
static double calc_tpd(
	const double delta, const double alpha, const double beta, const double ndata,
	double *ldata, double *xdata, double *ddata, double *avg_noise
) {
	double result;

	result     = (ndata - *ldata) / delta;
	*ldata     = ndata;
	*xdata     = alpha * (*xdata) + ndata * ndata;
	*ddata     = alpha * (*ddata) + result * result;
	*avg_noise = (*avg_noise) + beta * (ndata * ndata - (*avg_noise));
	result     = PI2_SQ * (*avg_noise) * DEF_TIME_WINDOW / (delta * DEF_TMX_SQ);
	result     = PI2 * sqrt((*xdata) / ((*ddata) + result));

	return result;
}

/*
 *
 */
static inline double calc_mavg( const double sample, const double average )
{
	return average + 0.001 * (sample - average);
}
