/**
 * @file scnlfilter.h
 * @author Origin by Lynn Dietz, 981112
 * @author Benjamin Ming Yang @ Department of Geoscience, National Taiwan University
 * @brief
 * @date 2022-03-28
 *
 * @copyright Copyright (c) 2022
 *
 */
#pragma once

/**
 * @name
 *
 */
int   scnlfilter_com( const char * );
int   scnlfilter_extra_com( void *(*)( const char * ) );
int   scnlfilter_init( const char * );
int   scnlfilter_apply( const char *, const char *, const char *, const char *, const void ** );
int   scnlfilter_remap( char *, char *, char *, char *, const void * );
void *scnlfilter_extra_get( const void * );
void  scnlfilter_end( void (*)( void * ) );
