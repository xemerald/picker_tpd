/**
 * @file circ_buf.h
 * @author Benjamin Yang @ Department of Geoscience, National Taiwan University
 * @brief
 * @date 2023-10-02
 *
 * @copyright Copyright (c) 2023
 *
 */
#pragma once

/**
 * @name Standard header include
 *
 */
#include <stdint.h>

/**
 * @brief
 *
 */
typedef struct {
/* pointer to start of queue element array */
	double  *entry;
/* position of the first(oldest) element of the queue */
	uint32_t first;
/* position of the last(youngest) element of the queue */
	uint32_t last;
/* */
	uint32_t next;
/* number of message buffers in ring */
	uint32_t max_elements;
	uint32_t num_elements;
} CIRC_BUFFER;

/**
 * @name
 *
 */
#define CIRC_BUFFER_ZERO_SET(BUFFER)       (*(BUFFER) = (CIRC_BUFFER){ NULL, 0, 0, 0, 0, 0 })
#define CIRC_BUFFER_FIRST_GET(BUFFER)      ((BUFFER)->first)
#define CIRC_BUFFER_LAST_GET(BUFFER)       ((BUFFER)->last)
#define CIRC_BUFFER_MAX_ELMS_GET(BUFFER)   ((BUFFER)->max_elements)
#define CIRC_BUFFER_NUM_ELMS_GET(BUFFER)   ((BUFFER)->num_elements)
#define CIRC_BUFFER_DATA_GET(BUFFER, POS)  ((BUFFER)->entry[(POS)])

/**
 * @name
 *
 */
int      circ_buf_init( CIRC_BUFFER *, uint32_t );
void     circ_buf_free( CIRC_BUFFER * );
int      circ_buf_prev( const CIRC_BUFFER *, double *, uint32_t * );
int      circ_buf_next( const CIRC_BUFFER *, double *, uint32_t * );
int      circ_buf_enbuf( CIRC_BUFFER *, const double );
uint32_t circ_buf_move( const CIRC_BUFFER *, uint32_t *, int );
