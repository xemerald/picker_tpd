
/* */
#include <stdlib.h>
#include <string.h>
/* */
#include <earthworm.h>
#include <transport.h>
/* */
#include <pick_tpd.h>
#include <early_event_msg.h>

/**
 * @brief
 *
 * @param trace_info
 * @param pick_id
 * @param phase
 * @param out_region
 * @param put_logo
 */
void ptpd_pick_report( TRACEINFO *trace_info, int pick_id, const char *phase, SHM_INFO *out_region, MSG_LOGO *put_logo )
{
	EARLY_PICK_MSG outmsg;
	PICK_INFO     *pick = &trace_info->pick;

/* */
	outmsg.pid = pick_id;
/* */
	strncpy(outmsg.station , trace_info->sta , EARLY_PICK_STATION_LEN - 1 );
	strncpy(outmsg.channel , trace_info->chan, EARLY_PICK_CHANNEL_LEN - 1 );
	strncpy(outmsg.network , trace_info->net , EARLY_PICK_NETWORK_LEN - 1 );
	strncpy(outmsg.location, trace_info->loc , EARLY_PICK_LOCATION_LEN - 1);
	strncpy(outmsg.phase   , phase           , EARLY_PICK_PHASE_LEN - 1   );
/* */
	outmsg.flag       ^= outmsg.flag;
	outmsg.polarity    = pick->first_motion;
	outmsg.ps_diff     = 0.0;
	outmsg.pick_time   = pick->time;
	outmsg.pick_weight = pick->weight;
	outmsg.residual    = 0.0;
/* */
#ifdef _DEBUG
	printf(
		"pick_tpd: New phase pick -> %d %s.%s.%s.%s %s %.2lf %c %d\n",
		outmsg.pid, outmsg.station, outmsg.channel, outmsg.network, outmsg.location,
		outmsg.phase, outmsg.pick_time, outmsg.polarity, outmsg.pick_weight
	);
#endif
/* Send the pick to the output ring */
	if ( tport_putmsg( out_region, put_logo, sizeof(EARLY_PICK_MSG), (char *)&outmsg ) != PUT_OK )
		logit( "et", "pick_tpd: Error sending pick to output ring.\n" );

	return;
}

//
void ptpd_coda_report( TRACEINFO *trace_info, int pick_id, const char *phase, SHM_INFO *out_region, MSG_LOGO *put_logo )
{
	return;
}
