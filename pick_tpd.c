#include <math.h>

#define PI  3.141592653589793238462643383279f
#define PI2 6.283185307179586476925286766559f

#define  DEF_MAX_BUFFER_SEC 5.0f
#define  DEF_TIME_WINDOW    4.5f
#define  DEF_TMX            0.019f

#define  DEF_THRESHOLD_C1       0.015f
#define  DEF_THRESHOLD_C2       0.01f
#define  DEF_C1_HALF_THRESHOLD  0.006f

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
#include <picker_tpd_pmat.h>


/* Functions prototype in this source file */
static void picker_tpd_config( char * );
static void picker_tpd_lookup( void );
static void picker_tpd_status( unsigned char, short, char * );
static void picker_tpd_end( void );                /* Free all the local memory & close socket */

static void TraceInfoInit( const TRACE2X_HEADER *, _TRACEINFO * );
static void OperationInt( _TRACEINFO *, float *, float * const, const int );
static void ModifyChanCode( TracePacket *, const char * const );
static void SkipModifyChanCode( TracePacket *, const char * const );

static inline double get_mavg( const double, const double );

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
static uint32_t MaxGapsThreashold;          /* */
static uint16_t DriftCorrectThreshold;      /* seconds waiting for D.C. */
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

	TRACEINFO   *traceptr;
	TracePacket  tracebuffer;  /* message which is sent to share ring    */

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
			res = tport_getmsg(&InRegion, Getlogo, nLogo, &reclogo, &recsize, tracebuffer.msg, MAX_TRACEBUF_SIZ);

		/* no more new messages */
			if ( res == GET_NONE ) {
				break;
			}
		/* next message was too big */
			else if ( res == GET_TOOBIG ) {
			/* complain and try again */
				sprintf(
					Text, "Retrieved msg[%ld] (i%u m%u t%u) too big for Buffer[%ld]",
					recsize, reclogo.instid, reclogo.mod, reclogo.type, sizeof(tracebuffer) - 1
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

				traceptr = TraListSearch( &tracebuffer.trh2x );

				if ( traceptr == NULL ) {
				/* Error when insert into the tree */
					logit( "e", "picker_tpd: %s.%s.%s.%s insert into trace tree error, drop this trace.\n",
						tracebuffer.trh2x.sta, tracebuffer.trh2x.chan, tracebuffer.trh2x.net, tracebuffer.trh2x.loc );
					continue;
				}

			/* First time initialization */
				if ( traceptr->firsttime || fabs(1.0/tracebuffer.trh2x.samprate - traceptr->delta) > FLT_EPSILON ) {
					printf("picker_tpd: New SCNL(%s.%s.%s.%s) received, starting to trace!\n",
						traceptr->sta, traceptr->chan, traceptr->net, traceptr->loc);
					TraceInfoInit( &tracebuffer.trh2x, traceptr );
				}

			/*
			 * Compute the number of samples since the end of the previous message.
			 * If (GapSize == 1), no data has been lost between messages.
			 * If (1 < GapSize <= Gparm.MaxGap), data will be interpolated.
			 * If (GapSize > Gparm.MaxGap), the picker will go into restart mode.
			 */
				double tmp_time = tracebuffer.trh2x.samprate * (tracebuffer.trh2x.starttime - traceptr->lasttime);
				int    gap_size;
				if ( tmp_time < 0.0 )          /* Invalid. Time going backwards. */
					gap_size = 0;
				else
					gap_size = (int)(tmp_time + 0.5);

			/* Interpolate missing samples and prepend them to the current message */
				if ( gap_size && (gap_size <= MaxGapsThreashold) ) {
					Interpolate( Sta, TraceBuf, gap_size );
				}
			/* Announce large sample gaps */
				else if ( gap_size > MaxGapsThreashold ) {
					/* Placeholder */
				}

			/*
			 * For big gaps, enter restart mode. In restart mode, calculate
			 * STAs and LTAs without picking.  Start picking again after a
			 * specified number of samples has been processed.
			 */
				if ( Restart( Sta, &Gparm, Trace2Head->nsamp, GapSize ) ) {
					for ( i = 0; i < tracebuffer.trh2x.nsamp; i++ )
						Sample( TraceLong[i], Sta );
				}
				else {
					PickRA( Sta, TraceBuf, &Gparm, &Ewh );
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
*/
static void TraceInfoInit( const TRACE2X_HEADER *trh2x, _TRACEINFO *traceptr )
{
	traceptr->firsttime     = FALSE;
	traceptr->readycount    = 0;
	traceptr->lasttime      = 0.0;
	traceptr->lastsample    = 0.0;
	traceptr->average       = 0.0;
	traceptr->delta         = 1.0 / trh2x->samprate;
	traceptr->intsteps      = traceptr->delta * 20.0 / NaturalPeriod + 1.0 - 1.e-5;
	traceptr->pmatrix       = PMatrixSearch( traceptr );
	memset(traceptr->xmatrix, 0, sizeof(traceptr->xmatrix));

	return;
}

/*
 *
 */
static inline double get_mavg( const double sample, const double average )
{
	return average + 0.001 * (sample - average);
}

/*
 * picker_tpd_end()  free all the local memory & close socket
 */
static void picker_tpd_end( void )
{
	tport_detach(&InRegion);
	tport_detach(&OutRegion);

	return;
}


/*
 *
 */
double compute_tpd( double sample, double sample_prev, double delta, double x_prev, double d_prev, double avg_noise_prev )
{
	const double alpha = exp(log(0.1) * delta / DEF_TIME_WINDOW);
	double x_this, d_this, d_stable, avg_noise_this;
	double result;

	result = (sample - sample_prev) / delta;
	x_this = alpha * x_prev + sample * sample;
	d_this = alpha * d_prev + result * result;

	avg_noise_this = avg_noise_prev + (1.0 - exp(log(0.1) * delta / 100.0)) * (sample * sample - avg_noise_prev);
	d_stable = PI_2 * PI_2 * avg_noise_this * DEF_TIME_WINDOW / (delta * DEF_TMX * DEF_TMX);
	result   = PI_2 * sqrt(x_this / (d_this + d_stable));

	return result;
}

/*
 *
 */
double find_delta_tpd_max( TPD_QUEUE *t_queue )
{
	int i;
	int nsamp   = (int)(3.0 / t_queue->delta);
	unsigned int pos = t_queue->last;
	double       tpd_min = t_queue->entry[pos];

	dec_circular( t_queue, &pos );
	for ( i = 0; i < nsamp; i++, dec_circular( t_queue, &pos )) {
		if ( t_queue->entry[pos] > 0. && t_queue->entry[pos] < tpd_min )
			tpd_min = t_queue->entry[pos];
	}

	return t_queue->entry[t_queue->last] - tpd_min;
}

/*
 *
 */
double refine_arrival_time( TPD_QUEUE *t_queue, double delta_tpd )
{
	int i, i_end, j;
	int result_i = 0;

	double       delta_thr;
	const int    nsamp = (int)(1.0 / t_queue->delta);
	unsigned int pos   = t_queue->last;

/* 1st repick with delta_tpd */
	i_end     = 3 * nsamp;
	delta_thr = delta_tpd * 0.5;
	dec_circular( t_queue, &pos );
	for ( i = 0; i < i_end; i++, dec_circular( t_queue, &pos ) ) {
		double _delta = t_queue->entry[t_queue->last] - t_queue->entry[pos];
		if ( _delta > delta_thr ) {
			result_i = i;
			break;
		}
	/* Start to use the 2nd refine condition */
		if ( i > (int)(nsamp * 0.15) && !result_i ) {
			delta_thr = delta_tpd * 0.8;
			result_i = i;
		}
	}

/* 3rd repick by tpd derivative (max 1s) */
	if ( result_i < (int)(t_queue->max_elements) ) {
		for ( i = 0; i < nsamp; i++, dec_circular( t_queue, &pos ) ) {
			double tpd1 = 0.0;
			double tpd2 = 0.0;
			double dtpd = 0.0;
			unsigned int pos_1 = pos;
			dec_circular( t_queue, &pos_1 );
			if ( i < nsamp - 1 ) {
				unsigned int pos_2 = pos_1;
				dec_circular( t_queue, &pos_2 );
				for ( j = 0; j < 3; j++, inc_circular( t_queue, &pos_1 ), inc_circular( t_queue, &pos_2 ) ) {
					tpd1 += t_queue->entry[pos_2];
					tpd2 += t_queue->entry[pos_1];
				}
				dtpd = (tpd2 - tpd1) / t_queue->delta / 3.0;
			}
			else {
				tpd1 = t_queue->entry[pos_1];
				tpd2 = t_queue->entry[pos];
				dtpd = (tpd2 - tpd1) / t_queue->delta;
			}
		/* */
			if ( dtpd < DEF_THRESHOLD_C2 ) {
				result_i += i;
				break;
			}
		}
	}

	return result_i * t_queue->delta;
}

/*
 *
 */


/*
 *  inc_circular() - Move supplied pointer to the next buffer descriptor structure
 *                   in a circular way.
 *  argument:
 *    t_queue -
 *    pos     -
 *  returns:
 *
 */
static unsigned int inc_circular( TPD_QUEUE *t_queue, unsigned int *pos )
{
	if ( ++(*pos) >= t_queue->max_elements )
		*pos = 0;

	return *pos;
}

/*
 *  dec_circular() - Move supplied pointer to the previous buffer descriptor structure
 *                   in a circular way.
 *  argument:
 *    t_queue -
 *    pos     -
 *  returns:
 *
 */
static unsigned int dec_circular( TPD_QUEUE *t_queue, unsigned int *pos )
{
	if ( --(*pos) < 0 )
		*pos = queue->max_elements - 1;

	return *pos;
}

/*
 *  move_circular() -
 *  argument:
 *    t_queue -
 *    pos     -
 *    step    -
 *    drc     -
 *  returns:
 */
static unsigned int move_circular( TPD_QUEUE *t_queue, unsigned int *pos, int step, const int drc )
{
	if ( step > t_queue->max_elements )
		return *pos;

/* Forward step */
	if ( drc > 0 )
		*pos = (*pos + step) % t_queue->max_elements;
/* Backward step */
	else
		*pos = (*pos - step + t_queue->max_elements) % t_queue->max_elements;

	return *pos;
}

/*
 *  dist_circular() -
 *  argument:
 *    t_queue -
 *    pos     -
 *    step    -
 *    drc     -
 *  returns:
 */
static unsigned int dist_circular( TPD_QUEUE *t_queue, unsigned int *pos, int step, const int drc )
{
	if ( step > t_queue->max_elements )
		return *pos;

/* Forward step */
	if ( drc > 0 )
		*pos = (*pos + step) % t_queue->max_elements;
/* Backward step */
	else
		*pos = (*pos - step + t_queue->max_elements) % t_queue->max_elements;

	return *pos;
}
