/**
 * @file pick_tpd.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-10-04
 *
 * @copyright Copyright (c) 2023
 *
 */
#pragma once
/* Standard header include */
#include <stdint.h>
/* Earthworm header include */
#include <trace_buf.h>
/* */
#include <circ_buf.h>

/* Constants define */
#define  PI     3.141592653589793238462643383279f
#define  PI2    6.283185307179586476925286766559f
#define  PI2_SQ 39.47841760435743447533796399951f

#define  OUTPUT_PICK_PHASE      "P"

#define  DEF_MAX_BUFFER_SECONDS 5.0f
#define  DEF_SEARCH_SECONDS     3.0f
#define  DEF_MAX_BUFFER_SAMPLES 2048l
#define  DEF_TIME_WINDOW        4.5f
#define  DEF_TMX                0.019f
#define  DEF_TMX_SQ             0.000361f
#define  DETRIGGER_SECONDS      20.0f

#define  TRIGGER_THRESHOLD_C1     0.015f
#define  TRIGGER_THRESHOLD_C2     0.01f
#define  TRIGGER_THRESHOLD_C6     0.024f
#define  DETRIGGER_THRESHOLD_C3   0.01f
#define  DEF_C1_HALF_THRESHOLD    0.006f

/**
 * @brief
 *
 */
typedef struct {
	uint8_t  status;        /* Pick status : 0 = picker is in idle mode, 1 = pick active but not complete, 2 = pick is complete but not reported, 3 = pick has been reported */
	char     first_motion;  /* First motion: ? = Not determined, U = Up, D = Down */
	int      weight;        /* Pick weight (0-3) */
	double   time;          /* Pick time */
	double   dtpd;          /* Trigger delta Tpd */
	double   amp[3];        /* Absolute value of first three extrema after pick */
} PICK_INFO;

/**
 * @brief
 *
 */
typedef struct {
	uint8_t  status;
	int      aav[6];           /* aav of preferred windows */
	int      len_sec;          /* Coda length in seconds */
	int      len_out;          /* Coda length in seconds (sometimes * -1) */
	int      len_win;          /* Coda length in number of windows */
} CODA_INFO;

/**
 * @brief
 *
 */
typedef struct {
	double value;
	int    samp_index;
} MIN_VALUE;

/**
 * @brief
 *
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
	double   lastsample;
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
	double   max_dtpd;
/* */
	PICK_INFO pick;
	CODA_INFO coda;
/* */
	CIRC_BUFFER raw_buffer;
	CIRC_BUFFER tpd_buffer;
	MIN_VALUE   min_tpd[DEF_MAX_BUFFER_SAMPLES];
/* */
	const void *match;
} TRACEINFO;
