/*
 *
 */
#include <picker_tpd.h>
#include <picker_tpd_circ_buf.h>

static double calc_tpd(
	const double, const double, const double, const double, double *, double *, double *, double *
);
static inline double calc_mavg( const double, const double );


/*
 *
 */
void ptpd_sample( int sample, TRACEINFO *trace_info )
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
