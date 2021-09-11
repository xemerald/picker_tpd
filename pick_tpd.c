#include <math.h>

#define  PI                3.1415926f
#define  PI_2              6.28f

#define  DEF_TIME_WINDOW   4.5f
#define  DEF_TMX           0.019f

#define  DEF_C1_THRESHOLD       0.015f
#define  DEF_C1_HALF_THRESHOLD  0.006f

typedef struct {
    double      *entry;
	double       delta;
    unsigned int first;
    unsigned int last;
    unsigned int max_elements;
    unsigned int num_elements;
} TPD_QUEUE;

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
double find_delta_tpd_max( TPD_QUEUE *t_queue )
{
	int i;
	int nsamp   = (int)(3.0 / t_queue->delta);
	unsigned int pos = t_queue->last;
	double       tpd_min = t_queue->entry[pos];

	dec_circular( t_queue, &pos );
	for ( i = 0; i < nsamp; i++, dec_circular( t_queue, &pos )) {
		if ( t_queue->entry[pos] > 0. && t_queue->entry[pos] < tpd_min )
			tpd_min = t_queue->entry[pos];
	}

	return t_queue->entry[t_queue->last] - tpd_min;
}

/*
 *
 */
int refine_arrival_time( TPD_QUEUE *t_queue, double delta_tpd )
{
	int i;
	int nsamp = (int)(1.0 / t_queue->delta);
	int tmp   = -1;

	double       d_tmp;
	unsigned int result;
	unsigned int pos = t_queue->last;

/* 1st repick with delta_tpd (max 1sec) */
	move_circular( t_queue, &pos, nsamp, -1 );
	for ( i = 0; i < nsamp; i++, inc_circular( t_queue, &pos ) ) {
		d_tmp = t_queue->entry[t_queue->last] - t_queue->entry[pos];
		if ( d_tmp > DEF_C1_HALF_THRESHOLD )
			result = pos;
		if ( d_tmp > delta_tpd * 0.8 )
			tmp = pos;
	}

/* 2nd repick with histogram of tpd (max 3s) */
	if ( result > 0.15 * nsamp ) {
		if ( tmp == -1 ) {
			for (i=3*nsamp; i>=1; i--) {
			for (j=0; j<5; j++) {
			if(xtpd[idx][(jd[idx]-i-1+nc)%nc] <= difftpd*0.1*(j+2)+xmin){
			mxrepick[j]=i-1;
			}
			}
			}
			nrepick_tmp=mxrepick[0];
			for (j=0; j<4; j++) {
			if(mxrepick[j]-mxrepick[j+1]>nsamp){
			if(j<3)  nrepick_tmp=mxrepick[j+2];
			if(j==3) nrepick_tmp=1;
			}
			}
		}
		nrepick=nrepick_tmp;
	}

	return 0;
}

/*
 *
 */


/*
 *  inc_circular() - Move supplied pointer to the next buffer descriptor structure
 *                   in a circular way.
 *  argument:
 *    t_queue -
 *    pos     -
 *  returns:
 *
 */
static unsigned int inc_circular( TPD_QUEUE *t_queue, unsigned int *pos )
{
	if ( ++(*pos) >= t_queue->max_elements )
		*pos = 0;

	return *pos;
}

/*
 *  dec_circular() - Move supplied pointer to the previous buffer descriptor structure
 *                   in a circular way.
 *  argument:
 *    t_queue -
 *    pos     -
 *  returns:
 *
 */
static unsigned int dec_circular( TPD_QUEUE *t_queue, unsigned int *pos )
{
	if ( --(*pos) < 0 )
		*pos = queue->max_elements - 1;

	return *pos;
}

/*
 *  move_circular() -
 *  argument:
 *    t_queue -
 *    pos     -
 *    step    -
 *    drc     -
 *  returns:
 */
static unsigned int move_circular( TPD_QUEUE *t_queue, unsigned int *pos, int step, const int drc )
{
	if ( drc > 0 ) {
		for ( ; step > 0; step-- )
			inc_circular( t_queue, pos );
	}
	else {
		for ( ; step > 0; step-- )
			dec_circular( t_queue, pos );
	}

	return *pos;
}
