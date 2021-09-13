/*
 *
 */
#include <picker_tpd.h>

/*
 *
 */
void ptpd_sample( int int_sample, TRACEINFO *trace_info )
{
	double double_sample;

/* */
	double_sample       = int_sample * trace_info->cfactor;
	trace_info->average = get_mavg( double_sample, trace_info->average );
	double_sample      -= trace_info->average;
/* */
	compute_tpd( double_sample );
}

/*
 *
 */
double compute_tpd( double sample, double sample_prev, double delta, double x_prev, double d_prev, double avg_noise_prev )
{
	const double alpha = exp(log(0.1) * delta / DEF_TIME_WINDOW);
	double x_this, d_this, d_stable, avg_noise_this;
	double result;

	result = (sample - sample_prev) / delta;
	x_this = alpha * x_prev + sample * sample;
	d_this = alpha * d_prev + result * result;

	avg_noise_this = avg_noise_prev + (1.0 - exp(log(0.1) * delta / 100.0)) * (sample * sample - avg_noise_prev);
	d_stable = PI_2 * PI_2 * avg_noise_this * DEF_TIME_WINDOW / (delta * DEF_TMX * DEF_TMX);
	result   = PI_2 * sqrt(x_this / (d_this + d_stable));

	return result;
}

/*
 *
 */
static inline double get_mavg( const double sample, const double average )
{
	return average + 0.001 * (sample - average);
}

/*
 *  inc_circular() - Move supplied pointer to the next buffer descriptor structure
 *                   in a circular way.
 *  argument:
 *    circ_buf -
 *    pos     -
 *  returns:
 *
 */
static unsigned int inc_circular( SAMPLE_CIRC_BUFFER *circ_buf, unsigned int *pos )
{
	if ( ++(*pos) >= circ_buf->max_elements )
		*pos = 0;

	return *pos;
}

/*
 *  dec_circular() - Move supplied pointer to the previous buffer descriptor structure
 *                   in a circular way.
 *  argument:
 *    circ_buf -
 *    pos     -
 *  returns:
 *
 */
static unsigned int dec_circular( SAMPLE_CIRC_BUFFER *circ_buf, unsigned int *pos )
{
	int _pos = *pos;

	if ( --_pos < 0 )
		_pos = circ_buf->max_elements - 1;

	*pos = _pos;

	return *pos;
}

/*
 *  move_circular() -
 *  argument:
 *    circ_buf -
 *    pos     -
 *    step    -
 *    drc     -
 *  returns:
 */
static unsigned int move_circular( SAMPLE_CIRC_BUFFER *circ_buf, unsigned int *pos, int step, const int drc )
{
	if ( step > circ_buf->max_elements )
		return *pos;

/* Forward step */
	if ( drc > 0 )
		*pos = (*pos + step) % circ_buf->max_elements;
/* Backward step */
	else
		*pos = (*pos - step + circ_buf->max_elements) % circ_buf->max_elements;

	return *pos;
}

/*
 *  dist_circular() -
 *  argument:
 *    circ_buf -
 *    pos     -
 *    step    -
 *    drc     -
 *  returns:
 */
static unsigned int dist_circular( SAMPLE_CIRC_BUFFER *circ_buf, unsigned int *pos, int step, const int drc )
{
	if ( step > circ_buf->max_elements )
		return *pos;

/* Forward step */
	if ( drc > 0 )
		*pos = (*pos + step) % circ_buf->max_elements;
/* Backward step */
	else
		*pos = (*pos - step + circ_buf->max_elements) % circ_buf->max_elements;

	return *pos;
}
