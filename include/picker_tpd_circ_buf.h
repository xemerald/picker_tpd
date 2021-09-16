#pragma once
/* Standard header include */
#include <stdint.h>

/*
 *
 */
typedef struct {
/* pointer to start of queue element array */
    double  *entry;
/* position of the first(oldest) element of the queue */
    uint32_t first;
/* position of the last(youngest) element of the queue */
    uint32_t last;
/* number of message buffers in ring */
    uint32_t max_elements;
    uint32_t num_elements;
} CIRC_BUFFER;

/* */
#define PTPD_CIRC_BUFFER_FIRST_GET(BUFFER)     ((BUFFER)->first)
#define PTPD_CIRC_BUFFER_LAST_GET(BUFFER)      ((BUFFER)->last)
#define PTPD_CIRC_BUFFER_MAX_ELMS_GET(BUFFER)  ((BUFFER)->max_elements)
#define PTPD_CIRC_BUFFER_NUM_ELMS_GET(BUFFER)  ((BUFFER)->num_elements)

/* */
int  ptpd_circ_buf_init( CIRC_BUFFER *, uint32_t );
void ptpd_circ_buf_free( CIRC_BUFFER * );
int  ptpd_circ_buf_prev( CIRC_BUFFER *, double *, uint32_t * );
int  ptpd_circ_buf_next( CIRC_BUFFER *, double *, uint32_t * );
int  ptpd_circ_buf_enbuf( CIRC_BUFFER *, const double );
