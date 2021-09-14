/* Standard C header include */
#include <stdlib.h>
#include <stdint.h>

/*
 *  ptpd_circ_buf_init() - Allocates the buffer region
 *  argument:
 *    queue -
 *    max_elements -
 *  returns:
 *    -1 - if we couldn't get the memory
 *    -2 - if the amount implied is bigger than a long
 */
int ptpd_circ_buf_init( CIRC_BUFFER *circ_buf, uint32_t max_elements )
{
	uint64_t tmp;
	uint64_t malloc_size;

/*
 * This contains the pointers to the start of the message descriptor array
 * and the message storage area
 */
	if ( circ_buf == (CIRC_BUFFER*)NULL ) return -1;

/* Save some arguments to local variables */
	circ_buf->max_elements = max_elements;
	circ_buf->num_elements = 0;

/* */
	tmp = circ_buf->max_elements * sizeof(double);
	tmp = tmp / max_elements;
	if ( tmp != sizeof(double) ) return -2;

/* Allocate the array of queue entries */
	malloc_size     = max_elements * sizeof(double);
	circ_buf->entry = (double *)malloc(malloc_size);
	if ( circ_buf->entry == (double *)NULL ) return -1;

/* Set first and last pointers to indicate empty queue */
	circ_buf->first = circ_buf->last = 0; /* in = out = first descriptor */

	return 0;
}

/*
 *  ptpd_circ_buf_free() - Deallocates the array of buffer entries
 *  argument:
 *    queue -
 *  returns:
 *    None.
 */
void ptpd_circ_buf_free( CIRC_BUFFER *circ_buf )
{
	if ( circ_buf != (CIRC_BUFFER *)NULL ) {
		if ( circ_buf->entry != (double *)NULL ) {
			free(circ_buf->entry);
			circ_buf->entry = NULL;
		}
	}

	return;
}

/*
 *  ptpd_circ_buf_prev() - Copies the previous message indicated by the pos
 *                         into caller's space without changing queue
 *                         and entry.
 *  argument:
 *    circ_buf  -
 *    data      -
 *    pos       -
 *  returns:
 *     0 - No error.
 *    -1 - The queue is empty or no more old data.
 *    -2 - Entry not yet established.
 */
int ptpd_circ_buf_prev( CIRC_BUFFER *circ_buf, double *data, uint32_t *pos )
{
	int result = 0;

	if ( circ_buf->entry == (double *)NULL ) return -2;
	if ( !(circ_buf->num_elements) || (*pos == circ_buf->first) )  /* There is no old data in the queue or empty */
		return -1;

/* Set shortcut for queue element */
	dec_circular( circ_buf, pos );
	*data = circ_buf->entry[*pos];

	return result;
}

/*
 *  ptpd_circ_buf_next() - Copies the next message indicated by the pos
 *                         into caller's space without changing queue
 *                         and entry.
 *  argument:
 *    circ_buf  -
 *    data      -
 *    pos       -
 *  returns:
 *     0 - No error.
 *    -1 - The queue is empty or no more new data.
 *    -2 - Entry not yet established.
 */
int ptpd_circ_buf_next( CIRC_BUFFER *circ_buf, double *data, uint32_t *pos )
{
	int result = 0;

	if ( circ_buf->entry == (double *)NULL ) return -2;
	if ( !(circ_buf->num_elements) || (*pos == circ_buf->last) )  /* There is no new data in the queue or empty */
		return -1;

/* Set shortcut for queue element */
	inc_circular( circ_buf, pos );
	*data = circ_buf->entry[*pos];

	return result;
}

/*
 *  ptpd_circ_buf_enbuf() - Moves message into circular buffer.
 *                          Copies string x to the new element.
 *  argument:
 *    circ_buf  -
 *    data      -
 *  returns:
 *     0 - No error.
 *    -1 - Not defined yet.
 *    -2 - Entry not yet established.
 *    -3 - There is overlapping between last time and this time.
 */
int ptpd_circ_buf_enbuf( CIRC_BUFFER *circ_buf, const double data )
{
	int     result = 0;
/* Pointer to last(youngest) entry in queue */
	double *last_buf;

/* If message is larger than max, do nothing and return error */
	if ( circ_buf->entry == (double *)NULL ) return -2;

/* Move to next buffer */
	inc_circular( circ_buf, &(circ_buf->last) );
/* Set shortcut for last queue element */
	last_buf = circ_buf->entry + circ_buf->last;
/* Copy the data to the buffer */
	*last_buf = data;
/* Check if the queue is full */
	if ( circ_buf->num_elements == circ_buf->max_elements ) {
	/*
	 * In that case we just overwrote the contents of the first(oldest)
	 * message in the queue, so 'drive' the outpointer forward one.
	 */
		inc_circular( circ_buf, &(circ_buf->first) );
		circ_buf->num_elements--;  /* We're overwriting an element, so there is one less */
		result = -3;               /* Now we don't care over-writing messages */
	}
/* We're adding an element, so there is one more */
	circ_buf->num_elements++;

	return result;
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
static uint32_t inc_circular( CIRC_BUFFER *circ_buf, uint32_t *pos )
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
static uint32_t dec_circular( CIRC_BUFFER *circ_buf, uint32_t *pos )
{
	int64_t _pos = *pos;

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
static uint32_t move_circular( CIRC_BUFFER *circ_buf, uint32_t *pos, int step )
{
	if ( abs(step) > circ_buf->max_elements )
		return *pos;

/* Forward step */
	if ( step > 0 )
		*pos = (*pos + step) % circ_buf->max_elements;
/* Backward step */
	else
		*pos = (*pos + step + circ_buf->max_elements) % circ_buf->max_elements;

	return *pos;
}
