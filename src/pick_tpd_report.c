
/* */
#include <stdlib.h>
#include <string.h>
/* */
#include <earthworm.h>
#include <transport.h>
/* */
#include <pick_tpd.h>
#include <full_event_msg.h>

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
	FULL_EVENT_PICK_MSG outmsg;
	PICK_INFO          *pick = &trace_info->pick;

/* */
	outmsg.pick_id = pick_id;
	outmsg.seq = 0;
/* */
	strcpy(outmsg.station, trace_info->sta);
	strcpy(outmsg.channel, trace_info->chan);
	strcpy(outmsg.network, trace_info->net);
	strcpy(outmsg.location, trace_info->loc);
/* */
	strcpy(outmsg.phase_name, phase);
	outmsg.polarity = pick->first_motion == ' ' ? '?' : pick->first_motion;
	outmsg.picktime = pick->time;
	outmsg.residual = 0.0;
	outmsg.weight = pick->weight;
/* */
#ifdef _DEBUG
	printf(
		"pick_tpd: Pick %d %s.%s.%s.%s %s %.2lf %c %d\n",
		outmsg.pick_id, outmsg.station, outmsg.channel, outmsg.network, outmsg.location,
		outmsg.phase_name, outmsg.picktime, outmsg.polarity, outmsg.weight
	);
#endif
/* Send the pick to the output ring */
	if ( tport_putmsg( out_region, put_logo, sizeof(FULL_EVENT_PICK_MSG), (char *)&outmsg ) != PUT_OK )
		logit( "et", "pick_tpd: Error sending pick to output ring.\n" );

	return;
}

//
void ptpd_coda_report( TRACEINFO *trace_info, int pick_id, const char *phase, SHM_INFO *out_region, MSG_LOGO *put_logo )
{
	return;
}
