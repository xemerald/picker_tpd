#pragma once

/* Standard header include */
#include <stdint.h>
/* Earthworm header include */
#include <trace_buf.h>

/* Constants define */
#define PI  3.141592653589793238462643383279f
#define PI2 6.283185307179586476925286766559f

#define  DEF_MAX_BUFFER_SEC 5.0f
#define  DEF_TIME_WINDOW    4.5f
#define  DEF_TMX            0.019f

#define  DEF_THRESHOLD_C1       0.015f
#define  DEF_THRESHOLD_C2       0.01f
#define  DEF_C1_HALF_THRESHOLD  0.006f

/*
 *
 */
typedef struct {
    double  *entry;
    double   delta;
    uint32_t first;
    uint32_t last;
    uint32_t max_elements;
    uint32_t num_elements;
} SAMPLE_CIRC_BUFFER;

/*
 * Station info related struct
 */
typedef struct {
	char sta[STA_CODE_LEN];   /* Site name (NULL-terminated) */
	char net[NET_CODE_LEN];   /* Network name (NULL-terminated) */
	char loc[LOC_CODE_LEN];   /* Location code (NULL-terminated) */
	char chan[CHAN_CODE_LEN]; /* Component/channel code (NULL-terminated) */
/* */
	uint8_t     padding;
	uint8_t     firsttime;
	uint16_t    readycount;
	double      lasttime;
	double      lastsample;
	double      average;
	double      delta;
/* */
	SAMPLE_CIRC_BUFFER tpd_buffer;
} TRACEINFO;
