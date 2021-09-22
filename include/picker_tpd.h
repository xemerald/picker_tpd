#pragma once

/* Standard header include */
#include <stdint.h>
/* Earthworm header include */
#include <trace_buf.h>
/* */
#include <picker_tpd_circ_buf.h>

/* Constants define */
#define  PI     3.141592653589793238462643383279f
#define  PI2    6.283185307179586476925286766559f
#define  PI2_SQ 39.47841760435743447533796399951f

#define  DEF_MAX_BUFFER_SECONDS 5.0f
#define  DEF_MAX_BUFFER_SAMPLES 500l
#define  DEF_TIME_WINDOW        4.5f
#define  DEF_TMX                0.019f
#define  DEF_TMX_SQ             0.000361f

#define  TRIGGER_THRESHOLD_C1   0.015f
#define  TRIGGER_THRESHOLD_C2   0.01f
#define  DEF_C1_HALF_THRESHOLD  0.006f

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
	uint8_t  pick_status;
	uint8_t  coda_status;
	char     first_motion;
	int      pick_weight;
	double   pick_time;

/* */
	CIRC_BUFFER raw_buffer;
	CIRC_BUFFER tpd_buffer;
} TRACEINFO;
