
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
double refine_arrival_time( TPD_QUEUE *t_queue, double delta_tpd )
{
	int i, i_end, j;
	int result_i = 0;

	double       delta_thr;
	const int    nsamp = (int)(1.0 / t_queue->delta);
	unsigned int pos   = t_queue->last;

/* 1st repick with delta_tpd */
	i_end     = 3 * nsamp;
	delta_thr = delta_tpd * 0.5;
	dec_circular( t_queue, &pos );
	for ( i = 0; i < i_end; i++, dec_circular( t_queue, &pos ) ) {
		double _delta = t_queue->entry[t_queue->last] - t_queue->entry[pos];
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
	if ( result_i < (int)(t_queue->max_elements) ) {
		for ( i = 0; i < nsamp; i++, dec_circular( t_queue, &pos ) ) {
			double tpd1 = 0.0;
			double tpd2 = 0.0;
			double dtpd = 0.0;
			unsigned int pos_1 = pos;
			dec_circular( t_queue, &pos_1 );
			if ( i < nsamp - 1 ) {
				unsigned int pos_2 = pos_1;
				dec_circular( t_queue, &pos_2 );
				for ( j = 0; j < 3; j++, inc_circular( t_queue, &pos_1 ), inc_circular( t_queue, &pos_2 ) ) {
					tpd1 += t_queue->entry[pos_2];
					tpd2 += t_queue->entry[pos_1];
				}
				dtpd = (tpd2 - tpd1) / t_queue->delta / 3.0;
			}
			else {
				tpd1 = t_queue->entry[pos_1];
				tpd2 = t_queue->entry[pos];
				dtpd = (tpd2 - tpd1) / t_queue->delta;
			}
		/* */
			if ( dtpd < DEF_THRESHOLD_C2 ) {
				result_i += i;
				break;
			}
		}
	}

	return result_i * t_queue->delta;
}
