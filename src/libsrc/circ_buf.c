/**
 * @file circ_buf.c
 * @author Benjamin Ming Yang @ Department of Geoscience, National Taiwan University (b98204032@gmail.com)
 * @brief
 * @date 2023-10-01
 *
 * @copyright Copyright (c) 2023
 *
 */

/**
 * @name Standard C header include
 *
 */
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

/**
 * @name
 *
 */
#include <circ_buf.h>

/**
 * @name Internal functions' prototype
 *
 */
static uint32_t inc_circular( const CIRC_BUFFER *, uint32_t * );
static uint32_t dec_circular( const CIRC_BUFFER *, uint32_t * );
static uint32_t move_circular( const CIRC_BUFFER *, uint32_t *, int );

/**
 * @brief Allocates the buffer region
 *
 * @param circ_buf
 * @param max_elements
 * @return int
 * @retval -1 - if we couldn't get the memory
 * @retval -2 - if the amount implied is bigger than a long
 */
int circ_buf_init( CIRC_BUFFER *circ_buf, uint32_t max_elements )
{
	uint64_t tmp;
	uint64_t malloc_size;

/*
 * This contains the pointers to the start of the message descriptor array
 * and the message storage area
 */
	if ( !circ_buf )
		return -1;
/* Save some arguments to local variables */
	circ_buf->max_elements = max_elements;
	circ_buf->num_elements = 0;
/* */
	tmp  = circ_buf->max_elements * sizeof(double);
	tmp /= max_elements;
	if ( tmp != sizeof(double) )
		return -2;
/* Allocate the array of queue entries */
	malloc_size = max_elements * sizeof(double);
	if ( !(circ_buf->entry = (double *)malloc(malloc_size)) )
		return -1;
/* Set first and last pointers to indicate empty queue */
	circ_buf->first = circ_buf->last = 0; /* in = out = first descriptor */

	return 0;
}

/**
 * @brief Deallocates the array of buffer entries
 *
 * @param circ_buf
 */
void circ_buf_free( CIRC_BUFFER *circ_buf )
{
	if ( circ_buf ) {
		if ( circ_buf->entry ) {
			free(circ_buf->entry);
			circ_buf->entry = NULL;
		}
	/* Just in case */
		circ_buf->first = circ_buf->last = circ_buf->next =
		circ_buf->max_elements = circ_buf->num_elements = 0;
	}

	return;
}

/**
 * @brief Copies the previous message indicated by the pos into caller's space without changing queue
 *        and entry.
 *
 * @param circ_buf
 * @param data
 * @param pos
 * @return int
 * @retval 0 - No error.
 * @retval -1 - The queue is empty or no more old data.
 * @retval -2 - Entry not yet established.
 */
int circ_buf_prev( const CIRC_BUFFER *circ_buf, double *data, uint32_t *pos )
{
	if ( !circ_buf->entry )
		return -2;
/* There is no old data in the queue or empty */
	if ( !(circ_buf->num_elements) || (*pos == circ_buf->first) )
		return -1;
/* Set shortcut for queue element */
	dec_circular( circ_buf, pos );
	if ( data )
		*data = circ_buf->entry[*pos];

	return 0;
}

/**
 * @brief Copies the next message indicated by the pos into caller's space without changing queue
 *        and entry.
 *
 * @param circ_buf
 * @param data
 * @param pos
 * @return int
 * @retval 0 - No error.
 * @retval -1 - The queue is empty or no more new data.
 * @retval -2 - Entry not yet established.
 */
int circ_buf_next( const CIRC_BUFFER *circ_buf, double *data, uint32_t *pos )
{
	if ( !circ_buf->entry )
		return -2;
/* There is no new data in the queue or empty */
	if ( !(circ_buf->num_elements) || (*pos == circ_buf->last) )
		return -1;
/* Set shortcut for queue element */
	inc_circular( circ_buf, pos );
	if ( data )
		*data = circ_buf->entry[*pos];

	return 0;
}

/**
 * @brief Moves message into circular buffer. Copies data to the new element.
 *
 * @param circ_buf
 * @param data
 * @return int
 * @retval 0 - No error.
 * @retval -1 - Not defined yet.
 * @retval -2 - Entry not yet established.
 * @retval -3 - There is overlapping between last time and this time.
 */
int circ_buf_enbuf( CIRC_BUFFER *circ_buf, const double data )
{
	int     result = 0;
/* Pointer to last(youngest) entry in queue */
	double *last_buf;

/* If message is larger than max, do nothing and return error */
	if ( !circ_buf->entry )
		return -2;
/* Set shortcut for last queue element */
	last_buf = circ_buf->entry + circ_buf->next;
/* Move to next buffer */
	circ_buf->last = circ_buf->next;
	inc_circular( circ_buf, &circ_buf->next );
/* Copy the data to the buffer */
	*last_buf = data;
/* Check if the queue is full */
	if ( circ_buf->num_elements == circ_buf->max_elements ) {
	/*
	 * In that case we just overwrote the contents of the first(oldest)
	 * message in the queue, so 'drive' the outpointer forward one.
	 */
		inc_circular( circ_buf, &circ_buf->first );
		circ_buf->num_elements--;  /* We're overwriting an element, so there is one less */
		result = -3;               /* Now we don't care over-writing messages */
	}
/* We're adding an element, so there is one more */
	circ_buf->num_elements++;

	return result;
}

/**
 * @brief
 *
 * @param circ_buf
 * @param pos
 * @param step
 * @return uint32_t
 */
uint32_t circ_buf_move( const CIRC_BUFFER *circ_buf, uint32_t *pos, int step )
{
	return move_circular( circ_buf, pos, step );
}

/**
 * @brief Move supplied pointer to the next buffer descriptor structure in a circular way.
 *
 * @param circ_buf
 * @param pos
 * @return uint32_t
 */
static uint32_t inc_circular( const CIRC_BUFFER *circ_buf, uint32_t *pos )
{
	if ( ++(*pos) >= circ_buf->max_elements )
		*pos = 0;

	return *pos;
}

/**
 * @brief Move supplied pointer to the previous buffer descriptor structure in a circular way.
 *
 * @param circ_buf
 * @param pos
 * @return uint32_t
 */
static uint32_t dec_circular( const CIRC_BUFFER *circ_buf, uint32_t *pos )
{
	int64_t _pos = *pos;

	if ( --_pos < 0 )
		_pos = circ_buf->max_elements - 1;

	*pos = _pos;

	return *pos;
}

/**
 * @brief
 *
 * @param circ_buf
 * @param pos
 * @param step
 * @return uint32_t
 */
static uint32_t move_circular( const CIRC_BUFFER *circ_buf, uint32_t *pos, int step )
{
/*
 * If the step is larger than the max elements, the intention is ambigular,
 * therefore, return the same position.
 */
	if ( abs(step) > circ_buf->max_elements || !step )
		return *pos;

/* Forward step */
	if ( step > 0 )
		*pos = (*pos + step) % circ_buf->max_elements;
/* Backward step */
	else
		*pos = (*pos + step + circ_buf->max_elements) % circ_buf->max_elements;

	return *pos;
}
