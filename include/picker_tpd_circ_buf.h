#pragma once

/* Standard header include */
#include <stdint.h>
/* Earthworm header include */
#include <trace_buf.h>

/* Constants define */
#define PI  3.141592653589793238462643383279f
#define PI2 6.283185307179586476925286766559f

#define  DEF_MAX_BUFFER_SECONDS 5.0f
#define  DEF_MAX_BUFFER_SAMPLES 500l
#define  DEF_TIME_WINDOW        4.5f
#define  DEF_TMX                0.019f

#define  DEF_THRESHOLD_C1       0.015f
#define  DEF_THRESHOLD_C2       0.01f
#define  DEF_C1_HALF_THRESHOLD  0.006f

/*
 *
 */
typedef struct {
    double  *entry;
    uint32_t first;
    uint32_t last;
    uint32_t max_elements;
    uint32_t num_elements;
} CIRC_BUFFER;

/*
 * Station info related struct
 */
typedef struct {
	char sta[TRACE2_STA_LEN];   /* Site name (NULL-terminated) */
	char net[TRACE2_NET_LEN];   /* Network name (NULL-terminated) */
	char loc[TRACE2_LOC_LEN];   /* Location code (NULL-terminated) */
	char chan[TRACE2_CHAN_LEN]; /* Component/channel code (NULL-terminated) */
/* */
	uint8_t  padding;
	uint8_t  firsttime;
	uint16_t readycount;
	uint32_t lastsample;
	double   lasttime;
/* */
	double   cfactor;
	double   delta;
	double   avg;
	double   alpha;
	double   beta;
	double   ldata;
	double   xdata;
	double   ddata;
	double   avg_noise;
/* */
	CIRC_BUFFER tpd_buffer;
} TRACEINFO;
