#ifdef _OS2
#define INCL_DOSMEMMGR
#define INCL_DOSSEMAPHORES
#include <os2.h>
#endif
/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <float.h>
#include <time.h>

/* Earthworm environment header include */
#include <earthworm.h>
#include <kom.h>
#include <transport.h>
#include <lockfile.h>
#include <trace_buf.h>

/* Local header include */
#include <stalist.h>
#include <recordtype.h>
#include <picker_tpd.h>
#include <picker_tpd_list.h>



/* Functions prototype in this source file */
static void picker_tpd_config( char * );
static void picker_tpd_lookup( void );
static void picker_tpd_status( unsigned char, short, char * );
static void picker_tpd_end( void );                /* Free all the local memory & close socket */

static void init_traceinfo( const TRACE2_HEADER *, TRACEINFO * );
static int  restart_tracing( TRACEINFO *, int, int );
static void interpolate( TRACEINFO *, uint8_t *, int );

/* Ring messages things */
static  SHM_INFO  InRegion;      /* shared memory region to use for i/o    */
static  SHM_INFO  OutRegion;     /* shared memory region to use for i/o    */

#define  MAXLOGO 5

MSG_LOGO  Putlogo;                /* array for output module, type, instid     */
MSG_LOGO  Getlogo[MAXLOGO];                /* array for requesting module, type, instid */
pid_t     myPid;                  /* for restarts by startstop                 */

/* Things to read or derive from configuration file
 **************************************************/
static char     InRingName[MAX_RING_STR];   /* name of transport ring for i/o    */
static char     OutRingName[MAX_RING_STR];  /* name of transport ring for i/o    */
static char     MyModName[MAX_MOD_STR];     /* speak as this module name/id      */
static uint8_t  LogSwitch;                  /* 0 if no logfile should be written */
static uint64_t HeartBeatInterval;          /* seconds between heartbeats        */
static uint32_t MaxGapsThreshold;          /* */
static uint16_t ReadySampleLength;      /* seconds waiting for D.C. */
static uint16_t nLogo = 0;

/* Things to look up in the earthworm.h tables with getutil.c functions
 **********************************************************************/
static int64_t InRingKey;       /* key of transport ring for i/o     */
static int64_t OutRingKey;      /* key of transport ring for i/o     */
static uint8_t InstId;          /* local installation id             */
static uint8_t MyModId;         /* Module Id for this program        */
static uint8_t TypeHeartBeat;
static uint8_t TypeError;
static uint8_t TypeTracebuf2;
static uint8_t TypePickScnl;
static uint8_t TypeCodaScnl;

/* Error messages used by picker_tpd
 *********************************/
#define  ERR_MISSMSG       0   /* message missed in transport ring       */
#define  ERR_TOOBIG        1   /* retreived msg too large for buffer     */
#define  ERR_NOTRACK       2   /* msg retreived; tracking limit exceeded */
#define  ERR_QUEUE         3   /* error queueing message for sending      */
static char Text[150];         /* string for log/error messages          */

int main ( int argc, char **argv )
{
	int res;

	int64_t  recsize = 0;

	MSG_LOGO   reclogo;
	time_t     timeNow;          /* current time                  */
	time_t     timeLastBeat;     /* time last heartbeat was sent  */

	char   *lockfile;
	int32_t lockfile_fd;

	uint8_t       *trace_buf;  /* message which is sent to share ring    */
	TRACE2_HEADER *trh2;  /* message which is sent to share ring    */
	TRACEINFO     *trace_info;

/* Check command line arguments */
	if ( argc != 2 ) {
		fprintf(stderr, "Usage: picker_tpd <configfile>\n");
		exit(0);
	}
/* Initialize name of log-file & open it */
	logit_init(argv[1], 0, 256, 1);

/* Read the configuration file(s) */
	picker_tpd_config( argv[1] );
	logit("" , "%s: Read command file <%s>\n", argv[0], argv[1]);

/* Look up important info from earthworm.h tables */
	picker_tpd_lookup();

/* Reinitialize logit to desired logging level */
	logit_init(argv[1], 0, 256, LogSwitch);
	lockfile = ew_lockfile_path(argv[1]);
	if ( (lockfile_fd = ew_lockfile(lockfile) ) == -1 ) {
		fprintf(stderr, "one instance of %s is already running, exiting\n", argv[0]);
		exit(-1);
	}

/* Get process ID for heartbeat messages */
	myPid = getpid();
	if ( myPid == -1 ) {
		logit("e","picker_tpd: Cannot get pid. Exiting.\n");
		exit(-1);
	}

/* Build the message */
	Putlogo.instid = InstId;
	Putlogo.mod    = MyModId;
	Putlogo.type   = TypePickScnl;

/* Attach to Input/Output shared memory ring */
	tport_attach(&InRegion, InRingKey);
	logit(
		"", "picker_tpd: Attached to public memory region %s: %ld\n",
		InRingName, InRingKey
	);
/* Flush the transport ring */
	tport_flush(&InRegion, Getlogo, nLogo, &reclogo);

	tport_attach(&OutRegion, OutRingKey);
	logit(
		"", "picker_tpd: Attached to public memory region %s: %ld\n",
		OutRingName, OutRingKey
	);

/* Allocate the waveform buffer */
	trace_buf = (uint8_t *)malloc(MAX_TRACEBUF_SIZ + sizeof(int32_t) * (MaxGapsThreshold + 1));
	if ( trace_buf == NULL ) {
		logit("e", "picker_tpd: Cannot allocate waveform buffer!\n");
		exit(-1);
	}
/* Point to header and data portions of waveform message */
	trh2 = (TRACE2_HEADER *)trace_buf;

/* Force a heartbeat to be issued in first pass thru main loop */
	timeLastBeat = time(&timeNow) - HeartBeatInterval - 1;

/*----------------------- setup done; start main loop -------------------------*/
	while(1)
	{
	/* Send picker_tpd's heartbeat */
		if ( time(&timeNow) - timeLastBeat >= (int64_t)HeartBeatInterval ) {
			timeLastBeat = timeNow;
			picker_tpd_status( TypeHeartBeat, 0, "" );
		}

	/* Process all new messages */
		do {
		/* See if a termination has been requested */
			if ( tport_getflag( &InRegion ) == TERMINATE || tport_getflag( &InRegion ) == myPid ) {
			/* write a termination msg to log file */
				logit("t", "picker_tpd: Termination requested; exiting!\n");
				fflush(stdout);
			/* detach from shared memory */
				picker_tpd_end();
				ew_unlockfile(lockfile_fd);
				ew_unlink_lockfile(lockfile);
				exit(0);
			}

		/* Get msg & check the return code from transport */
			res = tport_getmsg(&InRegion, Getlogo, nLogo, &reclogo, &recsize, trace_buf, MAX_TRACEBUF_SIZ);

		/* no more new messages */
			if ( res == GET_NONE ) {
				break;
			}
		/* next message was too big */
			else if ( res == GET_TOOBIG ) {
			/* complain and try again */
				sprintf(
					Text, "Retrieved msg[%ld] (i%u m%u t%u) too big for Buffer[%ld]",
					recsize, reclogo.instid, reclogo.mod, reclogo.type, MAX_TRACEBUF_SIZ
				);
				picker_tpd_status( TypeError, ERR_TOOBIG, Text );
				continue;
			}
		/* got a msg, but missed some */
			else if ( res == GET_MISS ) {
				sprintf(
					Text, "Missed msg(s)  i%u m%u t%u  %s.",
					reclogo.instid, reclogo.mod, reclogo.type, InRingName
				);
				picker_tpd_status( TypeError, ERR_MISSMSG, Text );
			}
		/* got a msg, but can't tell */
			else if ( res == GET_NOTRACK ) {
			/* if any were missed */
				sprintf(
					Text, "Msg received (i%u m%u t%u); transport.h NTRACK_GET exceeded",
					reclogo.instid, reclogo.mod, reclogo.type
				);
				picker_tpd_status( TypeError, ERR_NOTRACK, Text );
			}

		/* Process the message */
			if ( reclogo.type == TypeTracebuf2 ) {
				if ( !TRACE2_HEADER_VERSION_IS_21(&(tracebuffer.trh2)) ) {
					printf("picker_tpd: %s.%s.%s.%s version is invalid, please check it!\n",
						tracebuffer.trh2.sta, tracebuffer.trh2.chan, tracebuffer.trh2.net, tracebuffer.trh2.loc);
					continue;
				}

				if ( tracebuffer.trh2x.datatype[0] != 'f' && tracebuffer.trh2x.datatype[0] != 't' ) {
					printf("picker_tpd: %s.%s.%s.%s datatype[%s] is invalid, skip it!\n",
						tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc,
						tracebuffer.trh2x.datatype);
					continue;
				}

				trace_info = ptpd_list_find( trh2->sta, trh2->chan, trh2->net, trh2->loc );

				if ( trace_info == NULL ) {
				/* This trace is not in the list */
					continue;
				}

			/* First time initialization */
				if ( trace_info->firsttime || fabs(1.0 / trh2->samprate - trace_info->delta) > FLT_EPSILON ) {
					printf(
						"picker_tpd: New SCNL(%s.%s.%s.%s) received, starting to trace!\n",
						trace_info->sta, trace_info->chan, trace_info->net, trace_info->loc
					);
					init_traceinfo( trh2, trace_info );
				}

			/*
			 * Compute the number of samples since the end of the previous message.
			 * If (GapSize == 1), no data has been lost between messages.
			 * If (1 < GapSize <= Gparm.MaxGap), data will be interpolated.
			 * If (GapSize > Gparm.MaxGap), the picker will go into restart mode.
			 */
				double tmp_time = trh2->samprate * (trh2->starttime - traceptr->lasttime);
				int    gap_size;
				if ( tmp_time < 0.0 )          /* Invalid. Time going backwards. */
					gap_size = 0;
				else
					gap_size = (int)(tmp_time + 0.5);

			/* Interpolate missing samples and prepend them to the current message */
				if ((gap_size > 1) && (gap_size <= MaxGapsThreshold) ) {
					interpolate( trace_info, trace_buf, gap_size );
				}
			/* Announce large sample gaps */
				else if ( gap_size > MaxGapsThreshold ) {
				/*  */
					logit(
						"t", "picker_tpd: Found %d sample gaps. Restarting channel %s.%s.%s.%s\n",
						gap_size, trace_info->sta, trace_info->chan, trace_info->net, trace_info->loc
					);
					init_traceinfo( trh2, trace_info );
				}

			/*
			 * For big gaps, enter restart mode. In restart mode, calculate
			 * STAs and LTAs without picking.  Start picking again after a
			 * specified number of samples has been processed.
			 */
				if ( restart_tracing( trace_info, trh2->nsamp, gap_size ) ) {
					for ( i = 0; i < trh2->nsamp; i++ )
						ptpd_sample( , trace_info );
				}
				else {
					/* Placeholder */
				}
			/* Save time and amplitude of the end of the current message */
				traceptr->lastsample = 0;
				traceptr->lasttime   = tracebuffer.trh2x.endtime;
			}
		} while ( 1 );  /* end of message-processing-loop */
	/* no more messages; wait for new ones to arrive */
		sleep_ew(50);
	}
/*-----------------------------end of main loop-------------------------------*/
	picker_tpd_end();
	return 0;
}

/*
 * picker_tpd_config() processes command file(s) using kom.c functions;
 *                     exits if any errors are encountered.
 */
static void picker_tpd_config( char *configfile )
{
	char  init[8];     /* init flags, one byte for each required command */
	char *com;
	char *str;

	uint32_t ncommand;     /* # of required commands you expect to process   */
	uint32_t nmiss;        /* number of required commands that were missed   */
	uint32_t nfiles;
	uint32_t success;
	uint32_t i;

/* Set to zero one init flag for each required command */
	ncommand = 8;
	for ( i = 0; i < ncommand; i++ )
		init[i] = 0;

/* Open the main configuration file */
	nfiles = k_open( configfile );
	if ( nfiles == 0 ) {
		logit("e", "picker_tpd: Error opening command file <%s>; exiting!\n", configfile);
		exit(-1);
	}
/* Process all command files */
/* While there are command files open */
	while ( nfiles > 0 ) {
	/* Read next line from active file  */
		while ( k_rd() ) {
		/* Get the first token from line */
			com = k_str();
		/* Ignore blank lines & comments */
			if ( !com )
				continue;
			if ( com[0] == '#' )
				continue;
		/* Open a nested configuration file */
			if ( com[0] == '@' ) {
				success = nfiles + 1;
				nfiles  = k_open(&com[1]);
				if ( nfiles != success ) {
					logit("e", "picker_tpd: Error opening command file <%s>; exiting!\n", &com[1]);
					exit(-1);
				}
				continue;
			}

		/* Process anything else as a command */
	/*0*/   if( k_its("LogFile") ) {
				LogSwitch = k_int();
				init[0] = 1;
			}
	/*1*/   else if( k_its("MyModuleId") ) {
				str = k_str();
				if ( str ) strcpy( MyModName, str );
				init[1] = 1;
			}
	/*2*/   else if( k_its("InWaveRing") ) {
				str = k_str();
				if ( str ) strcpy( InRingName, str );
				init[2] = 1;
			}
	/*3*/   else if( k_its("OutWaveRing") ) {
				str = k_str();
				if ( str ) strcpy( OutRingName, str );
				init[3] = 1;
			}
	/*4*/   else if( k_its("HeartBeatInterval") ) {
				HeartBeatInterval = k_long();
				init[4] = 1;
			}
	/*6*/   else if( k_its("DriftCorrectThreshold") ) {
				DriftCorrectThreshold = k_long();
				init[6] = 1;
			}
		/* Enter installation & module to get event messages from */
	/*7*/   else if( k_its("GetEventsFrom") ) {
				if ( nLogo + 1 >= MAXLOGO ) {
					logit("e", "picker_tpd: Too many <GetEventsFrom> commands in <%s>", configfile);
					logit("e", "; max=%d; exiting!\n", (int) MAXLOGO);
					exit(-1);
				}
				if ( ( str = k_str() ) ) {
					if ( GetInst(str, &Getlogo[nLogo].instid) != 0 ) {
						logit("e", "picker_tpd: Invalid installation name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				if ( ( str = k_str() ) ) {
					if ( GetModId(str, &Getlogo[nLogo].mod) != 0 ) {
						logit("e", "picker_tpd: Invalid module name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				if ( ( str = k_str() ) ) {
					if ( GetType(str, &Getlogo[nLogo].type) != 0 ) {
						logit("e", "picker_tpd: Invalid message type name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				nLogo++;
				init[7] = 1;
			}
		 /* Unknown command */
			else {
				logit("e", "picker_tpd: <%s> Unknown command in <%s>.\n", com, configfile);
				continue;
			}

		/* See if there were any errors processing the command */
			if ( k_err() ) {
				logit("e", "picker_tpd: Bad <%s> command in <%s>; exiting!\n", com, configfile);
				exit(-1);
			}
		}
		nfiles = k_close();
	}

/* After all files are closed, check init flags for missed commands */
	nmiss = 0;
	for ( i = 0; i < ncommand; i++ ) if ( !init[i] ) nmiss++;
	if ( nmiss ) {
		logit( "e", "picker_tpd: ERROR, no " );
		if ( !init[0] ) logit( "e", "<LogFile> "              );
		if ( !init[1] ) logit( "e", "<MyModuleId> "           );
		if ( !init[2] ) logit( "e", "<InputRing> "            );
		if ( !init[3] ) logit( "e", "<OutputRing> "           );
		if ( !init[4] ) logit( "e", "<HeartBeatInterval> "    );
		if ( !init[5] ) logit( "e", "<OutputType> "           );
		if ( !init[6] ) logit( "e", "<DriftCorrectThreshold> ");
		if ( !init[7] ) logit( "e", "any <GetEventsFrom> "    );

		logit("e", "command(s) in <%s>; exiting!\n", configfile);
		exit(-1);
	}

	return;
}

/*
 * picker_tpd_lookup( )   Look up important info from earthworm.h tables
 */
static void picker_tpd_lookup( void )
{
/* Look up keys to shared memory regions */
	if ( ( InRingKey = GetKey(InRingName) ) == -1 ) {
		fprintf(stderr, "picker_tpd: Invalid ring name <%s>; exiting!\n", InRingName);
		exit(-1);
	}
	if ( ( OutRingKey = GetKey(OutRingName) ) == -1 ) {
		fprintf(stderr, "picker_tpd: Invalid ring name <%s>; exiting!\n", OutRingName);
		exit(-1);
	}

/* Look up installations of interest */
	if ( GetLocalInst(&InstId) != 0 ) {
		fprintf(stderr, "picker_tpd: error getting local installation id; exiting!\n");
		exit(-1);
	}

/* Look up modules of interest */
	if ( GetModId(MyModName, &MyModId) != 0 ) {
		fprintf(stderr, "picker_tpd: Invalid module name <%s>; exiting!\n", MyModName);
		exit(-1);
	}

/* Look up message types of interest */
	if ( GetType("TYPE_HEARTBEAT", &TypeHeartBeat) != 0 ) {
		fprintf(stderr, "picker_tpd: Invalid message type <TYPE_HEARTBEAT>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_ERROR", &TypeError) != 0 ) {
		fprintf(stderr, "picker_tpd: Invalid message type <TYPE_ERROR>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_TRACEBUF2", &TypeTracebuf2) != 0 ) {
		fprintf(stderr, "picker_tpd: Invalid message type <TYPE_TRACEBUF2>; exiting!\n");
		exit(-1);
	}

	return;
}

/*
 * picker_tpd_status() builds a heartbeat or error message & puts it into
 *                     shared memory.  Writes errors to log file & screen.
 */
static void picker_tpd_status( unsigned char type, short ierr, char *note )
{
	MSG_LOGO    logo;
	char        msg[256];
	uint64_t    size;
	time_t      t;

/* Build the message */
	logo.instid = InstId;
	logo.mod    = MyModId;
	logo.type   = type;

	time( &t );

	if ( type == TypeHeartBeat ) {
		sprintf(msg, "%ld %ld\n", (long)t, (long)myPid);
	}
	else if ( type == TypeError ) {
		sprintf(msg, "%ld %hd %s\n", (long)t, ierr, note);
		logit("et", "picker_tpd: %s\n", note);
	}
	size = strlen( msg );   /* don't include the null byte in the message */

/* Write the message to shared memory */
	if ( tport_putmsg(&InRegion, &logo, size, msg) != PUT_OK ) {
		if ( type == TypeHeartBeat ) {
		   logit("et", "picker_tpd:  Error sending heartbeat.\n");
		}
		else if ( type == TypeError ) {
		   logit("et", "picker_tpd:  Error sending error:%d.\n", ierr);
		}
	}

	return;
}

/*
 * picker_tpd_end()  free all the local memory & close socket
 */
static void picker_tpd_end( void )
{
	tport_detach(&InRegion);
	tport_detach(&OutRegion);
	ptpd_list_end();

	return;
}

/*
 *
 */
static void init_traceinfo( const TRACE2_HEADER *trh2, TRACEINFO *trace_info )
{
	trace_info->readycount = 0;
	trace_info->lastsample = 0;
	trace_info->lasttime   = trh2->starttime;
	trace_info->delta      = 1.0 / trh2->samprate;
	trace_info->avg        = 0.0;
	trace_info->alpha      = exp(log(0.1) * trace_info->delta / DEF_TIME_WINDOW);
	trace_info->bets       = 1.0 - exp(log(0.1) * trace_info->delta / 100.0);
	trace_info->ldata      = 0.0;
	trace_info->xdata      = 0.0;
	trace_info->ddata      = 0.0;
	trace_info->avg_noise  = 0.0;
/* */
	if ( (int)(trh2->samprate * DEF_MAX_BUFFER_SECONDS) > DEF_MAX_BUFFER_SAMPLES ) {
		ptpd_circ_buf_free( &trace_info->tpd_buffer );
		ptpd_circ_buf_init( &trace_info->tpd_buffer, trh2->samprate * DEF_MAX_BUFFER_SECONDS );
	}

	return;
}

/*
 * restart_tracing() Check for breaks in the time sequence of messages.
 *
 *  Returns 1 if the picker is in restart mode
 *          0 if the picker is not in restart mode
 */

static int restart_tracing( TRACEINFO *trace_info, int nsamp, int gaps )
{
	int result = 0;
/*
 * Begin a restart sequence. Initialize internal variables.
 * Save the number of samples processed in restart mode.
 */
	if ( gaps > MaxGapsThreshold ) {
		trace_info->readycount = nsamp;
		result = 1;
	}
	else {
	/* The restart sequence is over */
		if ( trace_info->readycount >= ReadySampleLength ) {
			result = 0;
		}
	/* The restart sequence is still in progress */
		else {
			trace_info->readycount += nsamp;
			result = 1;
		}
	}

	return result;
}


/*
 * interpolate() Interpolate samples and insert them at the beginning of
 *               the waveform.
 */
static void interpolate( TRACEINFO *trace_info, uint8_t *trace_buf, int gaps )
{
	int            i;
	TRACE2_HEADER *trh2  = (TRACE2_HEADER *)trace_buf;
	int32_t       *idata = (int32_t *)(trh2 + 1);
	const double   delta = (double)(idata[0] - trace_info->lastsample) / gaps;
	double         sum_delta;

/* */
	gaps--;
	for ( i = trh2->nsamp - 1; i >= 0; i-- )
		idata[i + gaps] = idata[i];

	for ( i = 0, sum_delta = delta; i < gaps; i++, sum_delta += delta )
		idata[i] = (int32_t)(trace_info->lastsample + sum_delta + 0.5);

	trh2->nsamp    += gaps;
	trh2->starttime = trace_info->lasttime + (1.0 / trh2->samprate);

	return;
}
