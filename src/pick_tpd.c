#ifdef _OS2
#define INCL_DOSMEMMGR
#define INCL_DOSSEMAPHORES
#include <os2.h>
#endif
/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <time.h>
/* Earthworm environment header include */
#include <earthworm.h>
#include <kom.h>
#include <transport.h>
#include <lockfile.h>
#include <trace_buf.h>
/* Local header include */
#include <scnlfilter.h>
#include <pick_tpd.h>
#include <pick_tpd_list.h>
#include <pick_tpd_pick.h>
#include <pick_tpd_sample.h>

/* Functions prototype in this source file */
static void pick_tpd_config( char * );
static void pick_tpd_lookup( void );
static void pick_tpd_status( unsigned char, short, char * );
static void pick_tpd_end( void );                /* Free all the local memory & close socket */

static int  restart_tracing( TRACEINFO *, int, int );

/* Ring messages things */
static  SHM_INFO  InRegion;      /* shared memory region to use for i/o    */
static  SHM_INFO  OutRegion;     /* shared memory region to use for i/o    */

#define  MAXLOGO 5

MSG_LOGO  Putlogo;                /* array for output module, type, instid     */
MSG_LOGO  Getlogo[MAXLOGO];       /* array for requesting module, type, instid */
pid_t     MyPid;                  /* for restarts by startstop                 */

/* Things to read or derive from configuration file
 **************************************************/
static char     InRingName[MAX_RING_STR];   /* name of transport ring for i/o    */
static char     OutRingName[MAX_RING_STR];  /* name of transport ring for i/o    */
static char     MyModName[MAX_MOD_STR];     /* speak as this module name/id      */
static uint8_t  LogSwitch;                  /* 0 if no logfile should be written */
static uint64_t HeartBeatInterval;          /* seconds between heartbeats        */
static uint32_t MaxGapsThreshold;           /* */
static uint16_t ReadySeconds;               /* seconds waiting for D.C. */
static uint16_t nLogo = 0;
static uint8_t  GlobalPickID = 0;
static uint8_t  SCNLFilterSwitch = 0;       /* 0 if no filter command in the file */

/* Things to look up in the earthworm.h tables with getutil.c functions
 **********************************************************************/
static int64_t InRingKey;       /* key of transport ring for i/o     */
static int64_t OutRingKey;      /* key of transport ring for i/o     */
static uint8_t InstId;          /* local installation id             */
static uint8_t MyModId;         /* Module Id for this program        */
static uint8_t TypeHeartBeat;
static uint8_t TypeError;
static uint8_t TypeTracebuf2;
static uint8_t TypeEarlyPick;

/* Error messages used by picker_tpd
 *********************************/
#define  ERR_MISSMSG       0   /* message missed in transport ring       */
#define  ERR_TOOBIG        1   /* retreived msg too large for buffer     */
#define  ERR_NOTRACK       2   /* msg retreived; tracking limit exceeded */
#define  ERR_QUEUE         3   /* error queueing message for sending      */
static char Text[150];         /* string for log/error messages          */

/*
 *
 */
int main ( int argc, char **argv )
{
	int res;

	int64_t  recsize = 0;
	MSG_LOGO reclogo;
	time_t   timeNow;          /* current time                  */
	time_t   timeLastBeat;     /* time last heartbeat was sent  */
	char    *lockfile;
	int32_t  lockfile_fd;

	uint8_t       *proc_buf;  /* message which is sent to share ring    */
	TRACE2_HEADER *trh2;
	double        *data;
	size_t         pbuf_size;
	TracePacket    inbuffer;  /* message which is sent to share ring    */
	TRACEINFO     *traceptr;
	const void    *_match = NULL;

	double tmp_time;

/* Check command line arguments */
	if ( argc != 2 ) {
		fprintf(stderr, "Usage: %s <configfile>\n", PROG_NAME);
		fprintf(stderr, "Version: %s\n", VERSION);
		fprintf(stderr, "Author:  %s\n", AUTHOR);
		fprintf(stderr, "Compiled at %s %s\n", __DATE__, __TIME__);
		exit( 0 );
	}
/* Initialize name of log-file & open it */
	logit_init( argv[1], 0, 256, 1 );
/* Read the configuration file(s) */
	pick_tpd_config( argv[1] );
	logit( "" , "%s: Read command file <%s>\n", argv[0], argv[1] );
/* Look up important info from earthworm.h tables */
	pick_tpd_lookup();
/* Reinitialize logit to desired logging level */
	logit_init( argv[1], 0, 256, LogSwitch );

	lockfile = ew_lockfile_path(argv[1]);
	if ( (lockfile_fd = ew_lockfile(lockfile) ) == -1 ) {
		fprintf(stderr, "one instance of %s is already running, exiting\n", argv[0]);
		exit (-1);
	}
/* Initialize the SCNL filter required arguments */
	if ( SCNLFilterSwitch )
		scnlfilter_init( "pick_tpd" );
/* Get process ID for heartbeat messages */
	MyPid = getpid();
	if ( MyPid == -1 ) {
		logit("e","pick_tpd: Cannot get pid. Exiting.\n");
		exit (-1);
	}

/* Build the message */
	Putlogo.instid = InstId;
	Putlogo.mod    = MyModId;
	Putlogo.type   = TypeEarlyPick;

/* Attach to Input/Output shared memory ring */
	tport_attach( &InRegion, InRingKey );
	logit("", "pick_tpd: Attached to public memory region %s: %ld\n", InRingName, InRingKey);
/* Flush the transport ring */
	tport_flush( &InRegion, Getlogo, nLogo, &reclogo );
/* */
	tport_attach( &OutRegion, OutRingKey );
	logit("", "pick_tpd: Attached to public memory region %s: %ld\n", OutRingName, OutRingKey);
/* Allocate the waveform buffer */
	pbuf_size = sizeof(TRACE2_HEADER) + ((MAX_TRACEBUF_SIZ - sizeof(TRACE2_HEADER)) / 8 * 2 + MaxGapsThreshold + 1) * sizeof(double);
	if ( (proc_buf = (uint8_t *)malloc(pbuf_size)) == NULL ) {
		logit("e", "pick_tpd: Cannot allocate internal waveform process buffer!\n");
		exit(-1);
	}
	trh2 = (TRACE2_HEADER *)proc_buf;
	data = (double *)(trh2 + 1);
/* Force a heartbeat to be issued in first pass thru main loop */
	timeLastBeat = time(&timeNow) - HeartBeatInterval - 1;
/*----------------------- setup done; start main loop -------------------------*/
	while ( 1 ) {
	/* Send pick_tpd's heartbeat */
		if ( time(&timeNow) - timeLastBeat >= (int64_t)HeartBeatInterval ) {
			timeLastBeat = timeNow;
			pick_tpd_status( TypeHeartBeat, 0, "" );
		}

	/* Process all new messages */
		do {
		/* See if a termination has been requested */
			res = tport_getflag( &InRegion );
			if ( res == TERMINATE || res == MyPid ) {
			/* write a termination msg to log file */
				logit("t", "pick_tpd: Termination requested; exiting!\n");
				fflush(stdout);
			/* */
				goto exit_procedure;
			}

		/* Get msg & check the return code from transport */
			res = tport_getmsg(&InRegion, Getlogo, nLogo, &reclogo, &recsize, inbuffer.msg, MAX_TRACEBUF_SIZ);
		/* No more new messages     */
			if ( res == GET_NONE ) {
				break;
			}
		/* Next message was too big */
			else if ( res == GET_TOOBIG ) {
			/* Complain and try again   */
				sprintf(
					Text, "Retrieved msg[%ld] (i%u m%u t%u) too big for Buffer[%ld]",
					recsize, reclogo.instid, reclogo.mod, reclogo.type, sizeof(inbuffer) - 1
				);
				pick_tpd_status( TypeError, ERR_TOOBIG, Text );
				continue;
			}
		/* Got a msg, but missed some */
			else if ( res == GET_MISS ) {
				sprintf(
					Text, "Missed msg(s)  i%u m%u t%u  %s.", reclogo.instid, reclogo.mod, reclogo.type, InRingName
				);
				pick_tpd_status( TypeError, ERR_MISSMSG, Text );
			}
		/* Got a msg, but can't tell */
			else if ( res == GET_NOTRACK ) {
			/* If any were missed        */
				sprintf(
					Text, "Msg received (i%u m%u t%u); transport.h NTRACK_GET exceeded",
					reclogo.instid, reclogo.mod, reclogo.type
				);
				pick_tpd_status( TypeError, ERR_NOTRACK, Text );
			}

		/* Process the message */
			if ( reclogo.type == TypeTracebuf2 ) {
			/* Initialize for in-list checking */
				traceptr = NULL;
			/* If this trace is already inside the local list, it would skip the SCNL filter */
				if (
					SCNLFilterSwitch &&
					!(traceptr = ptpd_list_find( &inbuffer.trh2 )) &&
					!scnlfilter_trace_apply( inbuffer.msg, reclogo.type, &_match )
				) {
				/* Found a new trace but the SCNL is not in the filter, drop it! */
					continue;
				}
			/* If we can't get the trace pointer to the local list, search it again */
				if ( !traceptr && !(traceptr = ptpd_list_search( &inbuffer.trh2 )) ) {
				/* Error when insert into the tree */
					logit(
						"e", "pick_tpd: SCNL %s.%s.%s.%s insert into trace tree error, drop this trace.\n",
						inbuffer.trh2.sta, inbuffer.trh2.chan, inbuffer.trh2.net, inbuffer.trh2.loc
					);
					continue;
				}

			/* First time initialization */
				if ( traceptr->firsttime || fabs(1.0 / inbuffer.trh2.samprate - traceptr->delta) > FLT_EPSILON ) {
					printf(
						"pick_tpd: New SCNL %s.%s.%s.%s received, starting to trace!\n",
						traceptr->sta, traceptr->chan, traceptr->net, traceptr->loc
					);
					traceptr->firsttime = false;
					ptpd_param_init( traceptr, &inbuffer.trh2 );
				}
			/* Remap the SCNL of this incoming trace */
				if ( SCNLFilterSwitch ) {
					if ( traceptr->match ) {
						scnlfilter_trace_remap( inbuffer.msg, reclogo.type, traceptr->match );
					}
					else {
						if ( scnlfilter_trace_remap( inbuffer.msg, reclogo.type, _match ) ) {
							printf(
								"pick_tpd: Remap received trace SCNL %s.%s.%s.%s to %s.%s.%s.%s!\n",
								traceptr->sta, traceptr->chan, traceptr->net, traceptr->loc,
								inbuffer.trh2.sta, inbuffer.trh2.chan,
								inbuffer.trh2.net, inbuffer.trh2.loc
							);
						}
						traceptr->match = _match;
					}
				}
			/* */
				ptpd_local_load( proc_buf, pbuf_size, &inbuffer );
			/*
			 * Compute the number of samples since the end of the previous message.
			 * If (GapSize == 1), no data has been lost between messages.
			 * If (1 < GapSize <= MaxGapsThreshold), data will be interpolated.
			 * If (GapSize > MaxGapsThreshold), the picker will go into restart mode.
			 */
				tmp_time = trh2->samprate * (trh2->starttime - traceptr->lasttime);
			/* Start processing the gap in trace */
				if ( tmp_time < 0.0 ) {
				/* Invalid. Time going backwards. */
					#ifdef _DEBUG
						printf(
							"pick_tpd: Overlapped in %s.%s.%s.%s, drop it!\n", trh2->sta, trh2->chan, trh2->net, trh2->loc
						);
					#endif
					res = 0;
				}
				else {
					res = (int)(tmp_time + 0.5);
				}
			/* Interpolate missing samples and prepend them to the current message */
				if ( res > 1 && res <= MaxGapsThreshold ) {
				#ifdef _DEBUG
					printf(
						"pick_tpd: Found %d sample gap in %s.%s.%s.%s, data will be interpolated!\n",
						res, trh2->sta, trh2->chan, trh2->net, trh2->loc
					);
				#endif
					ptpd_interpolate( traceptr, proc_buf, res );
				}
			/* Announce large sample gaps */
				else if ( res > MaxGapsThreshold ) {
				/* */
					logit(
						"t", "pick_tpd: Found %d sample gaps. Restarting channel %s.%s.%s.%s\n",
						res, traceptr->sta, traceptr->chan, traceptr->net, traceptr->loc
					);
					ptpd_param_init( traceptr, trh2 );
				}
			/*
			 * For big gaps, enter restart mode. In restart mode, calculate Tpd without picking.
			 * Start picking again after a specified number of samples has been processed.
			 */
				if ( restart_tracing( traceptr, trh2->nsamp, res ) ) {
					for ( int i = 0; i < trh2->nsamp; i++ )
						ptpd_sample( traceptr, data[i], i, trh2->nsamp );
				}
				else {
					ptpd_pick( traceptr, proc_buf, GlobalPickID, &OutRegion, &Putlogo );
				}
			/* Save time and amplitude of the end of the current message */
				traceptr->lastsample = data[trh2->nsamp - 1];
				traceptr->lasttime = trh2->endtime;
			}
		} while ( 1 );  /* end of message-processing-loop */
		sleep_ew(50);   /* no more messages; wait for new ones to arrive */
	}
/*-----------------------------end of main loop-------------------------------*/
exit_procedure:
/* detach from shared memory */
	free(proc_buf);
	pick_tpd_end();

	ew_unlockfile(lockfile_fd);
	ew_unlink_lockfile(lockfile);

	return 0;
}

/*
 * pick_tpd_config() - processes command file(s) using kom.c functions;
 *                      exits if any errors are encountered.
 */
static void pick_tpd_config( char *configfile )
{
	char  init[8];     /* init flags, one byte for each required command */
	char *com;
	char *str;

	int ncommand;     /* # of required commands you expect to process   */
	int nmiss;        /* number of required commands that were missed   */
	int nfiles;
	int success;
	int i;

/* Set to zero one init flag for each required command */
	ncommand = 8;
	for( i = 0; i < ncommand; i++ )
		init[i] = 0;
/* Open the main configuration file */
	nfiles = k_open( configfile );
	if ( nfiles == 0 ) {
		logit("e", "pick_tpd: Error opening command file <%s>; exiting!\n", configfile);
		exit(-1);
	}
/*
 * Process all command files
 * While there are command files open
 */
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
				success = nfiles+1;
				nfiles  = k_open(&com[1]);
				if ( nfiles != success ) {
					logit("e", "pick_tpd: Error opening command file <%s>; exiting!\n", &com[1]);
					exit(-1);
				}
				continue;
			}
		/* Process anything else as a command */
		/* 0 */
			if ( k_its("LogFile") ) {
				LogSwitch = k_int();
				init[0] = 1;
			}
		/* 1 */
			else if ( k_its("MyModuleId") ) {
				str = k_str();
				if ( str )
					strcpy(MyModName, str);
				init[1] = 1;
			}
		/* 2 */
			else if ( k_its("InRing") ) {
				str = k_str();
				if ( str )
					strcpy(InRingName, str);
				init[2] = 1;
			}
		/* 3 */
			else if ( k_its("OutRing") ) {
				str = k_str();
				if ( str )
					strcpy( OutRingName, str );
				init[3] = 1;
			}
		/* 4 */
			else if ( k_its("HeartBeatInterval") ) {
				HeartBeatInterval = k_long();
				init[4] = 1;
			}
		/* 5 */
			else if ( k_its("ReadySeconds") ) {
				ReadySeconds = k_long();
				init[5] = 1;
			}
		/* 6 */
			else if ( k_its("MaxGap") ) {
				MaxGapsThreshold = k_long();
				init[6] = 1;
			}
			else if ( k_its("GlobalPickID") ) {
				GlobalPickID = k_long();
				logit("o", "pick_tpd: Set the global pick ID to %d.\n", GlobalPickID);
			}
		/* Enter installation & module to get event messages from */
		/* 7 */
			else if ( k_its("GetEventsFrom") ) {
				if ( (nLogo + 1) >= MAXLOGO ) {
					logit("e", "pick_tpd: Too many <GetEventsFrom> commands in <%s>", configfile);
					logit("e", "; max=%d; exiting!\n", (int)MAXLOGO);
					exit(-1);
				}
				if ( (str = k_str()) ) {
					if ( GetInst(str, &Getlogo[nLogo].instid) != 0 ) {
						logit("e", "pick_tpd: Invalid installation name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				if ( (str = k_str()) ) {
					if( GetModId(str, &Getlogo[nLogo].mod) != 0 ) {
						logit("e", "pick_tpd: Invalid module name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				if ( (str = k_str()) ) {
					if( GetType( str, &Getlogo[nLogo].type ) != 0 ) {
						logit("e", "pick_tpd: Invalid message type name <%s>", str);
						logit("e", " in <GetEventsFrom> cmd; exiting!\n");
						exit(-1);
					}
				}
				nLogo++;
				init[7] = 1;
			}
			else if ( scnlfilter_com( "pick_tpd" ) ) {
			/* */
				SCNLFilterSwitch = 1;
			}
		/* Unknown command */
			else {
				logit("e", "pick_tpd: <%s> Unknown command in <%s>.\n", com, configfile);
				continue;
			}

		/* See if there were any errors processing the command */
			if ( k_err() ) {
				logit("e", "pick_tpd: Bad <%s> command in <%s>; exiting!\n", com, configfile);
				exit(-1);
			}
		}
		nfiles = k_close();
	}

/* After all files are closed, check init flags for missed commands */
	nmiss = 0;
	for ( i = 0; i < ncommand; i++ )
		if ( !init[i] )
			nmiss++;
/* */
	if ( nmiss ) {
		logit( "e", "pick_tpd: ERROR, no " );
		if ( !init[0] ) logit("e", "<LogFile> "              );
		if ( !init[1] ) logit("e", "<MyModuleId> "           );
		if ( !init[2] ) logit("e", "<InputRing> "            );
		if ( !init[3] ) logit("e", "<OutputRing> "           );
		if ( !init[4] ) logit("e", "<HeartBeatInterval> "    );
		if ( !init[5] ) logit("e", "<ReadySeconds> "         );
		if ( !init[6] ) logit("e", "<MaxGap> "               );
		if ( !init[7] ) logit("e", "any <GetEventsFrom> "    );

		logit("e", "command(s) in <%s>; exiting!\n", configfile);
		exit(-1);
	}

	return;
}

/*
 * pick_tpd_lookup() - Look up important info from earthworm.h tables
 */
static void pick_tpd_lookup( void )
{
/* Look up keys to shared memory regions */
	if ( ( InRingKey = GetKey(InRingName) ) == -1 ) {
		fprintf(stderr, "pick_tpd:  Invalid ring name <%s>; exiting!\n", InRingName);
		exit(-1);
	}
	if ( ( OutRingKey = GetKey(OutRingName) ) == -1 ) {
		fprintf(stderr, "pick_tpd:  Invalid ring name <%s>; exiting!\n", OutRingName);
		exit(-1);
	}
/* Look up installations of interest */
	if ( GetLocalInst(&InstId) != 0 ) {
		fprintf(stderr, "pick_tpd: error getting local installation id; exiting!\n");
		exit(-1);
	}
/* Look up modules of interest */
	if ( GetModId(MyModName, &MyModId) != 0 ) {
		fprintf(stderr, "pick_tpd: Invalid module name <%s>; exiting!\n", MyModName);
		exit(-1);
	}
/* Look up message types of interest */
	if ( GetType("TYPE_HEARTBEAT", &TypeHeartBeat) != 0 ) {
		fprintf(stderr, "pick_tpd: Invalid message type <TYPE_HEARTBEAT>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_ERROR", &TypeError) != 0 ) {
		fprintf(stderr, "pick_tpd: Invalid message type <TYPE_ERROR>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_TRACEBUF2", &TypeTracebuf2) != 0 ) {
		fprintf(stderr, "pick_tpd: Invalid message type <TYPE_TRACEBUF2>; exiting!\n");
		exit(-1);
	}
	if ( GetType("TYPE_EARLY_PICK", &TypeEarlyPick) != 0 ) {
		fprintf(stderr, "pick_tpd: Invalid message type <TYPE_EARLY_PICK>; exiting!\n");
		exit(-1);
	}

	return;
}

/*
 * pick_tpd_status() - builds a heartbeat or error message & puts it into
 *                      shared memory.  Writes errors to log file & screen.
 */
static void pick_tpd_status( unsigned char type, short ierr, char *note )
{
	MSG_LOGO    logo;
	char        msg[512];
	uint64_t    size;
	time_t      t;

/* Build the message */
	logo.instid = InstId;
	logo.mod    = MyModId;
	logo.type   = type;

	time(&t);

	if ( type == TypeHeartBeat ) {
		sprintf(msg, "%ld %ld\n", (long)t, (long)MyPid);
	}
	else if( type == TypeError ) {
		sprintf(msg, "%ld %hd %s\n", (long)t, ierr, note);
		logit("et", "pick_tpd: %s\n", note);
	}

	size = strlen(msg);  /* don't include the null byte in the message */

/* Write the message to shared memory */
	if ( tport_putmsg(&InRegion, &logo, size, msg) != PUT_OK ) {
		if ( type == TypeHeartBeat ) {
			logit("et", "pick_tpd:  Error sending heartbeat.\n");
		}
		else if ( type == TypeError ) {
			logit("et", "pick_tpd:  Error sending error:%d.\n", ierr);
		}
	}

	return;
}

/*
 * pick_tpd_end() - free all the local memory & close socket
 */
static void pick_tpd_end( void )
{
	tport_detach( &InRegion );
	tport_detach( &OutRegion );
	scnlfilter_end( NULL );
	ptpd_list_end();

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
		if ( trace_info->readycount >= (ReadySeconds / trace_info->delta) ) {
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
