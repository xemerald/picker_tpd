/**
 * @file early_event_msg.h
 * @author Benjamin Ming Yang @ Department of Geoscience, National Taiwan University (b98204032@gmail.com)
 * @brief Detailed definition of TYPE_EARLY_EVENT, TYPE_EARLY_PICK
 * @date 2023-09-26
 *
 * @copyright Copyright (c) 2023
 *
 */
#pragma once

/**
 * @brief
 *
 */
#include <stdint.h>

/**
 * @brief
 *
 */
#define EARLY_MAG_TABLE \
		X(EARLY_MAG_AVG , "Mavg") \
		X(EARLY_MAG_MPA , "Mpa" ) \
		X(EARLY_MAG_MPV , "Mpv" ) \
		X(EARLY_MAG_MPD , "Mpd" ) \
		X(EARLY_MAG_MTC , "Mtc" ) \
		X(EARLY_MAG_PADJ, "Padj") \
		X(EARLY_MAG_NUM , "nil" )

#define X(a, b) a,
typedef enum {
	EARLY_MAG_TABLE
} EARLY_MAG_TYPE;
#undef X

/**
 * @brief
 *
 */
typedef struct {
	float mall;
	float mpa;
	float mpv;
	float mpd;
	float mtc;
	float padj;
	float weight;
} EARLY_MAG_MSG;

/**
 * @name
 *
 */
#define EARLY_PICK_STATION_LEN   8
#define EARLY_PICK_CHANNEL_LEN   4
#define EARLY_PICK_NETWORK_LEN   8
#define EARLY_PICK_LOCATION_LEN  4
#define EARLY_PICK_PHASE_LEN     8

/**
 * @name Picking flag
 *
 */
#define EARLY_PICK_FLAG_INUSE    0x01
#define EARLY_PICK_FLAG_PRIMARY  0x02
#define EARLY_PICK_FLAG_MAGMASK  0x04
#define EARLY_PICK_FLAG_LOCMASK  0x08
#define EARLY_PICK_FLAG_COSITE   0x10
/* ... */
#define EARLY_PICK_FLAG_REJECT   0x80

/**
 * @brief
 *
 */
#define EARLY_PICK_POLARITY_TABLE \
		X(EARLY_PICK_POLARITY_UP    , 'U') \
		X(EARLY_PICK_POLARITY_DOWN  , 'D') \
		X(EARLY_PICK_POLARITY_INIT  , ' ') \
		X(EARLY_PICK_POLARITY_UNKNOW, '?') \
		X(EARLY_PICK_POLARITY_NUM   , 'X')

#define X(a, b) a,
typedef enum {
	EARLY_PICK_POLARITY_TABLE
} EARLY_PICK_POLARITIES;
#undef X

/**
 * @brief
 *
 */
#define EARLY_PICK_PAMP_TABLE \
		X(EARLY_PICK_PAMP_PA , "Pa" ) \
		X(EARLY_PICK_PAMP_PV , "Pv" ) \
		X(EARLY_PICK_PAMP_PD , "Pd" ) \
		X(EARLY_PICK_PAMP_TC , "Tc" ) \
		X(EARLY_PICK_PAMP_NUM, "nil")

#define X(a, b) a,
typedef enum {
	EARLY_PICK_PAMP_TABLE
} EARLY_PICK_PAMPS;
#undef X

/*
 * pick_id     DBMS ID of pick (unsigned long)
 * seq         Sequence number of pick itself
 * station     Station code (upper case)
 * channel     Component code (upper case)
 * network     Network code (upper case)
 * location    Location code
 * phase_name  Phase code (e.g., 'P', 'S', or 'PKP' etc.), null terminate
 * polarity    First-motion descriptor (U,D,' ','?')
 * weight      Pick weight or quality (0-4)
 * latitude    Geographical station latitude in signed decimal degrees (WGS84)
 * longitude   Geographical station longitude in signed decimal degrees (WGS84)
 * elevation   Station elevation in meters (WGS84)
 * picktime    Time of pick in seconds since 1970
 * recvtime    Time of receiving in seconds since 1970
 * residual    Travel-time residual in seconds
 * pamp        P amplitudes in different types
 * dist        Source-receiver distance in kilometers
 * azi         Receiver azimuth in degrees (from the source)
 * tko         Receiver takeoff angle in degrees (from the source)
 */
typedef struct {
/* */
	uint32_t pid;
/* */
	char     station[EARLY_PICK_STATION_LEN];
	char     channel[EARLY_PICK_CHANNEL_LEN];
	char     network[EARLY_PICK_NETWORK_LEN];
	char     location[EARLY_PICK_LOCATION_LEN];
	char     phase[EARLY_PICK_PHASE_LEN];
/* */
	char     flag;
	char     pick_weight;
	char     polarity;
	char     padding;
/* */
	float    latitude;
	float    longitude;
	float    elevation;
	float    ps_diff;
	float    loc_weight;
	float    dist;
	float    azi;
	float    tko;
/* */
	double   recv_time;
	double   pick_time;
	double   residual;
/* */
	double   pamp[EARLY_PICK_PAMP_NUM];
} EARLY_PICK_MSG;

#define EARLY_EVENT_ID_LEN  32

/*
 * event_id    Event identifier, null terminate
 * seq         Sequence number of event msg
 * origin      Event origin time in epoch seconds
 * evlat       Geographical event latitude in signed decimal degrees (WGS84)
 * evlon       Geographical event longitude in signed decimal degrees (WGS84)
 * evdepth     Event depth in kilometers (WGS84)
 * gap         Largest azimuthal gap in degrees
 * nsta        Number of stations associated
 * npha        Number of phases associated
 * suse        Number of stations used in the solution
 * puse        Number of phases used in the solution
 * dmin        Distance to the nearest station in kilometers
 * depth_flag  Fixed depth flag (T if depth was held, F otherwise)*
 * oterr       90% marginal confidence interval for origin time
 * laterr      90% marginal confidence interval for latitude
 * lonerr      90% marginal confidence interval for longitude
 * deperr      90% marginal confidence interval for depth
 * se          Standard error of the residuals in seconds
 * errh        Maximum horizontal projection of the error ellipsoid in kilometers
 * errz        Maximum vertical projection of the error ellipsoid in kilometers
 * avh         Equivalent radius of the horizontal error ellipse in kilometers
 * q           Quality flag (i.e., 'A', 'B', 'C', 'D')
 * axis1-3     Length in kilometers of the principle axies of the error ellipsoid
 * az1-3       Azimuth in degrees of the principle axies of the error ellipsoid
 * dp1-3       Dip in degrees of the principle axies of the error ellipsoid
 * npicks      Number of picks in this full message
 */
typedef struct {
/* */
	char     event_id[EARLY_EVENT_ID_LEN];
	char     q;
	char     depth_flag;
	char     padding[6];
/* */
	uint32_t seq;
	uint32_t npha;
	uint32_t nsta;
	uint32_t pha_use;
	uint32_t sta_use;
/* */
	float    evlat;
	float    evlon;
	float    evdepth;
/* */
	float    gap;
	float    dist_min;
	float    rms_residual;
	float    erh;
	float    erz;
/* */
	float    oterr;
	float    laterr;
	float    lonerr;
	float    deperr;
	float    axis[3];
	float    az[3];
	float    dp[3];
/* */
	double   trigger_time;
	double   origin_time;
} EARLY_EVENT_MSG_HEADER;

/**
 * @brief
 *
 */
#define EARLY_EVENT_QUALITY_TABLE \
		X(EARLY_EVENT_QUALITY_A  , 'A') \
		X(EARLY_EVENT_QUALITY_B  , 'B') \
		X(EARLY_EVENT_QUALITY_C  , 'C') \
		X(EARLY_EVENT_QUALITY_D  , 'D') \
		X(EARLY_EVENT_QUALITY_NUM, 'X')

#define X(a, b) a,
typedef enum {
	EARLY_EVENT_QUALITY_TABLE
} EARLY_EVENT_QUALITIES;
#undef X

/**
 * @brief
 *
 */
#define EARLY_EVENT_DEPTH_FLAG_TABLE \
		X(EARLY_EVENT_DEPTH_FLAG_HELD, 'T') \
		X(EARLY_EVENT_DEPTH_FLAG_FREE, 'F') \
		X(EARLY_EVENT_DEPTH_FLAG_NUM , 'X')

#define X(a, b) a,
typedef enum {
	EARLY_EVENT_DEPTH_FLAG_TABLE
} EARLY_EVENT_DEPTH_FLAGS;
#undef X

/* Max picks in the message */
#define EARLY_EVENT_MAX_PICKS  1024

/**
 * @brief
 *
 */
typedef struct {
/* */
	EARLY_EVENT_MSG_HEADER header;
	EARLY_MAG_MSG          mag;
/* */
	struct {
		EARLY_PICK_MSG     info;
		EARLY_MAG_MSG      mag;
	} picks[EARLY_EVENT_MAX_PICKS];
} EARLY_EVENT_MSG;

/**
 * @brief
 *
 */
#define EARLY_EVENT_HEADER_SIZE  (sizeof(EARLY_EVENT_MSG_HEADER))
#define EARLY_PICK_MSG_SIZE      (sizeof(EARLY_PICK_MSG))
#define EARLY_MAG_MSG_SIZE       (sizeof(EARLY_MAG_MSG))
#define EARLY_EVENT_MSG_SIZE     (sizeof(EARLY_EVENT_MSG))
