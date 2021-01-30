/***
*
*	Copyright (c) 2012, AGHL.RU. All rights reserved.
*
****/
//
// CHudTimer.cpp
//
// implementation of CHudTimer class
//

#include <time.h>

#include "timer.h"
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "net_api.h"
#include "net.h"
#include "demo.h"
#include "demo_api.h"

#define NET_API gEngfuncs.pNetAPI

#define TIMER_Y             0.04
#define TIMER_Y_NEXT_OFFSET 0.04
#define TIMER_RED_R         255
#define TIMER_RED_G         16
#define TIMER_RED_B         16
#define CUSTOM_TIMER_R      0
#define CUSTOM_TIMER_G      160
#define CUSTOM_TIMER_B      0

CON_COMMAND(customtimer, "Sets a timer to count down from N secs to zero")
{
	CHudTimer::Get()->CustomTimerCommand();
}

enum RulesRequestStatus
{
	SOCKET_NONE = 0,
	SOCKET_IDLE = 1,
	SOCKET_AWAITING_CODE = 2,
	SOCKET_AWAITING_ANSWER = 3,
};
RulesRequestStatus g_eRulesRequestStatus = SOCKET_NONE;
NetSocket g_timerSocket = 0; // We will declare socket here to not include winsocks in hud.h

DEFINE_HUD_ELEM(CHudTimer);

void CHudTimer::Init()
{
	BaseHudClass::Init();

	HookMessage<&CHudTimer::MsgFunc_Timer>("Timer");

	m_iFlags |= HUD_ACTIVE;

	m_pCvarHudTimer = gEngfuncs.pfnRegisterVariable("hud_timer", "1", FCVAR_BHL_ARCHIVE);
	m_pCvarHudTimerSync = gEngfuncs.pfnRegisterVariable("hud_timer_sync", "1", FCVAR_BHL_ARCHIVE);
	m_pCvarHudNextmap = gEngfuncs.pfnRegisterVariable("hud_nextmap", "1", FCVAR_BHL_ARCHIVE);
};

void CHudTimer::VidInit()
{
	m_pCvarMpTimelimit = gEngfuncs.pfnGetCvarPointer("mp_timelimit");
	m_pCvarMpTimeleft = gEngfuncs.pfnGetCvarPointer("mp_timeleft");
	m_pCvarSvAgVersion = gEngfuncs.pfnGetCvarPointer("sv_ag_version");
	m_pCvarAmxNextmap = gEngfuncs.pfnGetCvarPointer("amx_nextmap");

	m_flDemoSyncTime = 0;
	m_bDemoSyncTimeValid = false;
	m_flNextSyncTime = 0;
	m_flSynced = false;
	m_flEndTime = 0;
	m_flEffectiveTime = 0;
	m_bDelayTimeleftReading = true;
	memset(m_flCustomTimerStart, 0, sizeof(m_flCustomTimerStart));
	memset(m_flCustomTimerEnd, 0, sizeof(m_flCustomTimerEnd));
	memset(m_bCustomTimerNeedSound, 0, sizeof(m_bCustomTimerNeedSound));
	m_eAgVersion = SV_AG_UNKNOWN;

	m_bNeedWriteTimer = true;
	m_bNeedWriteCustomTimer = true;
	m_bNeedWriteNextmap = true;

	m_iReceivedSize = 0;
	if (g_timerSocket != NULL)
	{
		NetCloseSocket(g_timerSocket);
		g_timerSocket = NULL;
		g_eRulesRequestStatus = SOCKET_NONE;
	}
};

int CHudTimer::MsgFunc_Timer(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	int timelimit = READ_LONG();
	int effectiveTime = READ_LONG();

	if (!m_flSynced)
	{
		m_flEndTime = timelimit;
		m_flEffectiveTime = effectiveTime;
	}

	return 1;
}

void CHudTimer::DoResync(void)
{
	m_bDelayTimeleftReading = true;
	m_flNextSyncTime = 0;
}

void CHudTimer::SyncTimer(float fTime)
{
	if (gEngfuncs.pDemoAPI->IsPlayingback())
		return;

	if ((int)m_pCvarHudTimerSync->value == 0)
	{
		m_flSynced = false;
		return;
	}

	// Make sure networking system has started.
	NET_API->InitNetworking();
	// Get net status
	net_status_t status;
	NET_API->Status(&status);
	if (status.connected)
	{
		if (status.remote_address.type == NA_IP)
		{
			SyncTimerRemote(*(unsigned int *)status.remote_address.ip, status.remote_address.port, fTime, status.latency);
			if (g_eRulesRequestStatus == SOCKET_AWAITING_CODE || g_eRulesRequestStatus == SOCKET_AWAITING_ANSWER)
				return;
		}
		else if (status.remote_address.type == NA_LOOPBACK)
		{
			SyncTimerLocal(fTime);
			m_flNextSyncTime = fTime + 5;
		}
		else
		{
			m_flNextSyncTime = fTime + 1;
		}

		if (m_bDelayTimeleftReading)
		{
			m_bDelayTimeleftReading = false;
			// We are not synced via timeleft because it has a delay when server set it after mp_timelimit changed
			// So do an update soon
			m_flNextSyncTime = fTime + 1.5;
		}
	}
	else
	{
		// Close socket if we are not connected anymore
		if (g_timerSocket != NULL)
		{
			NetCloseSocket(g_timerSocket);
			g_timerSocket = NULL;
			g_eRulesRequestStatus = SOCKET_NONE;
		}

		m_flNextSyncTime = fTime + 1;
	}
};

void CHudTimer::SyncTimerLocal(float fTime)
{
	float prevEndtime = m_flEndTime;
	int prevAgVersion = m_eAgVersion;

	// Get timer settings directly from cvars
	if (m_pCvarMpTimelimit && m_pCvarMpTimeleft)
	{
		m_flEndTime = m_pCvarMpTimelimit->value * 60;
		if (!m_bDelayTimeleftReading)
		{
			float timeleft = m_pCvarMpTimeleft->value;
			if (timeleft > 0)
			{
				float endtime = timeleft + fTime;
				if (fabs(m_flEndTime - endtime) > 1.5)
					m_flEndTime = endtime;

				m_flSynced = true;
			}
		}
		if (m_flEndTime != prevEndtime)
			m_bNeedWriteTimer = true;
	}

	// Get AG version
	if (m_eAgVersion == SV_AG_UNKNOWN)
	{
		if (m_pCvarSvAgVersion && m_pCvarSvAgVersion->string[0])
		{
			if (!strcmp(m_pCvarSvAgVersion->string, "6.6") || !strcmp(m_pCvarSvAgVersion->string, "6.3"))
			{
				m_eAgVersion = SV_AG_FULL;
			}
			else // We will assume its miniAG server, which will be true in almost all cases
			{
				m_eAgVersion = SV_AG_MINI;
			}
		}
		else
		{
			m_eAgVersion = SV_AG_NONE;
		}

		if (m_eAgVersion != prevAgVersion)
			m_bNeedWriteTimer = true;
	}

	// Get nextmap
	if (m_pCvarAmxNextmap && m_pCvarAmxNextmap->string[0])
	{
		if (strcmp(m_szNextmap, m_pCvarAmxNextmap->string))
		{
			m_bNeedWriteNextmap = true;
			strncpy(m_szNextmap, m_pCvarAmxNextmap->string, sizeof(m_szNextmap) - 1);
			m_szNextmap[sizeof(m_szNextmap) - 1] = 0;
		}
	}
}

void CHudTimer::SyncTimerRemote(unsigned int ip, unsigned short port, float fTime, double latency)
{
	float prevEndtime = m_flEndTime;
	int prevAgVersion = m_eAgVersion;
	char buffer[2048];
	int len = 0;

	// Check for query timeout and just do a resend
	if (fTime - m_flNextSyncTime > 3 && (g_eRulesRequestStatus == SOCKET_AWAITING_CODE || g_eRulesRequestStatus == SOCKET_AWAITING_ANSWER))
		g_eRulesRequestStatus = SOCKET_IDLE;

	// Retrieve settings from the server
	switch (g_eRulesRequestStatus)
	{
	case SOCKET_NONE:
	case SOCKET_IDLE:
		m_iResponceID = 0;
		m_iReceivedSize = 0;
		m_iReceivedPackets = 0;
		m_iReceivedPacketsCount = 0;
		NetClearSocket(g_timerSocket);
		NetSendUdp(ip, port, "\xFF\xFF\xFF\xFFV\xFF\xFF\xFF\xFF", 9, &g_timerSocket);
		g_eRulesRequestStatus = SOCKET_AWAITING_CODE;
		m_flNextSyncTime = fTime; // set time for timeout checking
		return;
	case SOCKET_AWAITING_CODE:
		len = NetReceiveUdp(ip, port, buffer, sizeof(buffer), g_timerSocket);
		if (len < 5)
			return;
		if (*(int *)buffer == -1 /*0xFFFFFFFF*/ && buffer[4] == 'A' && len == 9)
		{
			// Answer is challenge response, send request again with the code
			buffer[4] = 'V';
			NetSendUdp(ip, port, buffer, 9, &g_timerSocket);
			g_eRulesRequestStatus = SOCKET_AWAITING_ANSWER;
			m_flNextSyncTime = fTime; // set time for timeout checking
			return;
		}
		// Answer should be rules response
		g_eRulesRequestStatus = SOCKET_AWAITING_ANSWER;
		break;
	case SOCKET_AWAITING_ANSWER:
		len = NetReceiveUdp(ip, port, buffer, sizeof(buffer), g_timerSocket);
		if (len < 5)
			return;
		break;
	}

	// Check for split packet
	if (*(int *)buffer == -2 /*0xFEFFFFFF*/)
	{
		if (len < 9)
			return;
		int currentPacket = *((unsigned char *)buffer + 8) >> 4;
		int totalPackets = *((unsigned char *)buffer + 8) & 0x0F;
		if (currentPacket >= totalPackets)
			return; // broken split packet
		if (m_iReceivedPackets == 0)
			m_iResponceID = *(int *)(buffer + 4);
		else if (*(int *)(buffer + 4) != m_iResponceID)
			return; // packet is from different responce
		if (m_iReceivedPackets & (1 << currentPacket))
			return; // already has this packet
		if (currentPacket < totalPackets - 1 && len != 1400)
			return; // strange split packet
		// Copy into merge buffer
		int pos = (1400 - 9) * currentPacket;
		memcpy(m_szPacketBuffer + pos, buffer + 9, len - 9);
		m_iReceivedSize += len - 9;
		m_iReceivedPackets |= (1 << currentPacket);
		m_iReceivedPacketsCount++;
		// Check for completion
		if (m_iReceivedPacketsCount < totalPackets)
			return;
	}
	else if (*(int *)buffer == -1 /*0xFFFFFFFF*/)
	{
		memcpy(m_szPacketBuffer, buffer, len);
		m_iReceivedSize += len;
	}
	else
	{
		return;
	}

	// Check that this is actually rules responce
	if (*(int *)m_szPacketBuffer != -1 /*0xFFFFFFFF*/ || m_szPacketBuffer[4] != 'E')
	{
		return;
	}

	m_flSynced = true;
	g_eRulesRequestStatus = SOCKET_IDLE;
	m_flNextSyncTime = fTime + 10; // Don't sync offten, we get update notifications via svc_print

	// Parse rules
	// Get map end time
	char *value = NetGetRuleValueFromBuffer(m_szPacketBuffer, m_iReceivedSize, "mp_timelimit");
	if (value && value[0])
	{
		m_flEndTime = atof(value) * 60;
	}
	else
	{
		m_flEndTime = 0;
	}
	value = NetGetRuleValueFromBuffer(m_szPacketBuffer, m_iReceivedSize, "mp_timeleft");
	if (value && value[0] && !gHUD.m_iIntermission && !m_bDelayTimeleftReading)
	{
		float timeleft = atof(value);
		if (timeleft > 0)
		{
			float endtime = timeleft + (int)(fTime - latency + 0.5);
			if (fabs(m_flEndTime - endtime) > 1.5)
				m_flEndTime = endtime;
		}
	}
	if (m_flEndTime != prevEndtime)
		m_bNeedWriteTimer = true;

	// Get AG version
	if (m_eAgVersion == SV_AG_UNKNOWN)
	{
		value = NetGetRuleValueFromBuffer(m_szPacketBuffer, m_iReceivedSize, "sv_ag_version");
		if (value && value[0])
		{
			if (!strcmp(value, "6.6") || !strcmp(value, "6.3"))
			{
				m_eAgVersion = SV_AG_FULL;
			}
			else // We will assume its miniAG server, which will be true in almost all cases
			{
				m_eAgVersion = SV_AG_MINI;
			}
		}
		else
		{
			m_eAgVersion = SV_AG_NONE;
		}

		if (m_eAgVersion != prevAgVersion)
			m_bNeedWriteTimer = true;
	}

	// Get nextmap
	value = NetGetRuleValueFromBuffer(m_szPacketBuffer, m_iReceivedSize, "amx_nextmap");
	if (value && value[0])
	{
		if (strcmp(m_szNextmap, value))
		{
			m_bNeedWriteNextmap = true;
			strncpy(m_szNextmap, value, sizeof(m_szNextmap) - 1);
			m_szNextmap[sizeof(m_szNextmap) - 1] = 0;
		}
	}
}

void CHudTimer::Think(void)
{
	float flTime = gEngfuncs.GetClientTime();
	// Check for time reset (can it happen?)
	if (m_flNextSyncTime - flTime > 60)
		m_flNextSyncTime = flTime;
	// Do sync. We do it always, so message hud can hide miniAG timer, and timer could work just as it is enabled
	if (m_flNextSyncTime <= flTime)
		SyncTimer(flTime);

	// If we are recording write changes to the demo
	if (gEngfuncs.pDemoAPI->IsRecording())
	{
		int i = 0;
		unsigned char buffer[100];
		// Current game time
		if ((int)m_flDemoSyncTime != (int)flTime)
		{
			i = 0;
			*(float *)&buffer[i] = flTime;
			i += sizeof(float);
			Demo_WriteBuffer(TYPE_TIME, i, buffer);
			m_flDemoSyncTime = flTime;
		}
		// End time and AG version
		if (m_bNeedWriteTimer)
		{
			i = 0;
			*(float *)&buffer[i] = m_flEndTime;
			i += sizeof(float);
			*(int *)&buffer[i] = m_eAgVersion;
			i += sizeof(int);
			Demo_WriteBuffer(TYPE_TIMER, i, buffer);
			m_bNeedWriteTimer = false;
		}
		if (m_bNeedWriteCustomTimer)
		{
			i = 0;
			*(int *)&buffer[i] = MAX_CUSTOM_TIMERS;
			i += sizeof(int);
			for (int number = 0; number < MAX_CUSTOM_TIMERS; number++)
			{
				*(float *)&buffer[i] = m_flCustomTimerStart[number];
				i += sizeof(float);
				*(float *)&buffer[i] = m_flCustomTimerEnd[number];
				i += sizeof(float);
				*(bool *)&buffer[i] = m_bCustomTimerNeedSound[number];
				i += sizeof(bool);
			}
			Demo_WriteBuffer(TYPE_CUSTOM_TIMER, i, buffer);
			m_bNeedWriteCustomTimer = false;
		}
		if (m_bNeedWriteNextmap)
		{
			Demo_WriteBuffer(TYPE_NEXTMAP, sizeof(m_szNextmap) - 1, (unsigned char *)m_szNextmap);
			m_bNeedWriteNextmap = false;
		}
	}
}

void CHudTimer::ReadDemoTimerBuffer(int type, const unsigned char *buffer)
{
	int i = 0, count = 0;
	float time = 0;
	switch (type)
	{
	case TYPE_TIME:
		// HACK for a first update, GetClientTime returns "random" value, so we use some magic number
		time = !m_bDemoSyncTimeValid ? 2 : gEngfuncs.GetClientTime();
		m_flDemoSyncTime = *(float *)&buffer[i] - time;
		i += sizeof(float);
		m_bDemoSyncTimeValid = true;
		break;
	case TYPE_TIMER:
		m_flEndTime = *(float *)&buffer[i];
		i += sizeof(float);
		m_eAgVersion = *(int *)&buffer[i];
		i += sizeof(int);
		break;
	case TYPE_CUSTOM_TIMER:
		count = *(int *)&buffer[i];
		i += sizeof(int);
		if (count > MAX_CUSTOM_TIMERS)
			count = MAX_CUSTOM_TIMERS;
		for (int number = 0; number < count; number++)
		{
			m_flCustomTimerStart[number] = *(float *)&buffer[i];
			i += sizeof(float);
			m_flCustomTimerEnd[number] = *(float *)&buffer[i];
			i += sizeof(float);
			m_bCustomTimerNeedSound[number] = *(bool *)&buffer[i];
			i += sizeof(bool);
		}
		break;
	case TYPE_NEXTMAP:
		strncpy(m_szNextmap, (char *)buffer, sizeof(m_szNextmap) - 1);
		m_szNextmap[sizeof(m_szNextmap) - 1] = 0;
		break;
	}
}

void CHudTimer::CustomTimerCommand(void)
{
	if (gEngfuncs.pDemoAPI->IsPlayingback())
		return;

	if (gEngfuncs.Cmd_Argc() <= 1)
	{
		gEngfuncs.Con_Printf("usage:  customtimer <interval in seconds> [timer number 1|2]\n");
		return;
	}

	// Get interval value
	int interval;
	char *intervalString = gEngfuncs.Cmd_Argv(1);
	if (!intervalString || !intervalString[0])
		return;
	interval = atoi(intervalString);
	if (interval < 0)
		return;
	if (interval > 86400)
		interval = 86400;

	// Get timer number
	int number = 0;
	char *numberString = gEngfuncs.Cmd_Argv(2);
	if (numberString && numberString[0])
	{
		number = atoi(numberString) - 1;
		if (number < 0 || number >= MAX_CUSTOM_TIMERS)
			return;
	}

	// Set custom timer
	m_flCustomTimerStart[number] = gEngfuncs.GetClientTime();
	m_flCustomTimerEnd[number] = gEngfuncs.GetClientTime() + interval;
	m_bCustomTimerNeedSound[number] = true;

	m_bNeedWriteCustomTimer = true;
}

int CHudTimer::GetAgVersion()
{
	return m_eAgVersion;
}

float CHudTimer::GetHudNextmap()
{
	return m_pCvarHudNextmap->value;
}

const char *CHudTimer::GetNextmap()
{
	return m_szNextmap;
}

void CHudTimer::SetNextmap(const char *nextmap)
{
	strncpy(m_szNextmap, nextmap, ARRAYSIZE(m_szNextmap) - 1);
	m_szNextmap[ARRAYSIZE(m_szNextmap) - 1] = 0;
}

void CHudTimer::Draw(float fTime)
{
	char text[128];

	if (gHUD.m_iHideHUDDisplay & HIDEHUD_ALL)
		return;

	// We will take time from demo stream if playingback
	float currentTime;
	if (gEngfuncs.pDemoAPI->IsPlayingback())
	{
		if (m_bDemoSyncTimeValid)
			currentTime = m_flDemoSyncTime + fTime;
		else
			currentTime = 0;
	}
	else
	{
		currentTime = fTime;
	}

	// Get the paint color
	int r, g, b;
	float a = 255 * gHUD.GetHudTransparency();
	gHUD.GetHudColor(HudPart::Common, 0, r, g, b);
	ScaleColors(r, g, b, a);

	// Draw timer
	float timeleft = m_flSynced ? (int)(m_flEndTime - currentTime) + 1 : (int)(m_flEndTime - m_flEffectiveTime);
	int hud_timer = (int)m_pCvarHudTimer->value;
	int ypos = ScreenHeight * TIMER_Y;
	switch (hud_timer)
	{
	case 1: // time left
		if (currentTime > 0 && timeleft > 0)
			DrawTimerInternal((int)timeleft, ypos, r, g, b, true);
		break;
	case 2: // time passed
		if (currentTime > 0)
			DrawTimerInternal((int)currentTime, ypos, r, g, b, false);
		break;
	case 3: // local PC time
		time_t rawtime;
		struct tm *timeinfo;
		time(&rawtime);
		timeinfo = localtime(&rawtime);
		sprintf(text, "Clock %d:%02d:%02d", (int)timeinfo->tm_hour, (int)timeinfo->tm_min, (int)timeinfo->tm_sec);
		// Output to screen
		int width = TextMessageDrawString(ScreenWidth + 1, ypos, text, 0, 0, 0);
		TextMessageDrawString((ScreenWidth - width) / 2, ypos, text, r, g, b);
		break;
	}

	// Draw next map
	int hud_nextmap = (int)m_pCvarHudNextmap->value;
	if (m_szNextmap[0] && timeleft < 60 && timeleft >= 0 && m_flEndTime > 0 && (hud_nextmap == 2 || (hud_nextmap == 1 && timeleft >= 37)))
	{
		snprintf(text, sizeof(text), "Nextmap is %s", m_szNextmap);
		ypos = ScreenHeight * (TIMER_Y + TIMER_Y_NEXT_OFFSET);
		int width = TextMessageDrawString(ScreenWidth + 1, ypos, text, 0, 0, 0);
		float a = (timeleft >= 40 || hud_nextmap > 1 ? 255.0 : 255.0 / 3 * ((m_flEndTime - currentTime) + 1 - 37)) * gHUD.GetHudTransparency();
		gHUD.GetHudColor(HudPart::Common, 0, r, g, b);
		ScaleColors(r, g, b, a);
		TextMessageDrawString((ScreenWidth - width) / 2, ypos, text, r, g, b);
	}

	// Draw custom timers
	for (int i = 0; i < MAX_CUSTOM_TIMERS; i++)
	{
		if (m_flCustomTimerStart[i] - currentTime > 0.5 || currentTime == 0)
		{
			// Time resets on changelevel or in playback
			m_flCustomTimerStart[i] = 0;
			m_flCustomTimerEnd[i] = 0;
			m_bCustomTimerNeedSound[i] = false;
		}
		else if (m_flCustomTimerEnd[i] > currentTime)
		{
			timeleft = (int)(m_flCustomTimerEnd[i] - currentTime) + 1;
			sprintf(text, "Timer %d", (int)timeleft);
			// Output to screen
			ypos = ScreenHeight * (TIMER_Y + TIMER_Y_NEXT_OFFSET * (i + 2));
			int width = TextMessageDrawString(ScreenWidth + 1, ypos, text, 0, 0, 0);
			float a = 255 * gHUD.GetHudTransparency();
			r = CUSTOM_TIMER_R, g = CUSTOM_TIMER_G, b = CUSTOM_TIMER_B;
			ScaleColors(r, g, b, a);
			TextMessageDrawString((ScreenWidth - width) / 2, ypos, text, r, g, b);
		}
		else if (m_bCustomTimerNeedSound[i])
		{
			if (currentTime - m_flCustomTimerEnd[i] < 1.5)
				PlaySound("fvox/bell.wav", 1);
			m_flCustomTimerStart[i] = 0;
			m_flCustomTimerEnd[i] = 0;
			m_bCustomTimerNeedSound[i] = false;
		}
	}
}

void CHudTimer::DrawTimerInternal(int time, float ypos, int r, int g, int b, bool redOnLow)
{
	div_t q;
	char text[64];

	// Calculate time parts and format into a text
	if (time >= 86400)
	{
		q = div(time, 86400);
		int d = q.quot;
		q = div(q.rem, 3600);
		int h = q.quot;
		q = div(q.rem, 60);
		int m = q.quot;
		int s = q.rem;
		sprintf(text, "%dd %dh %02dm %02ds", d, h, m, s);
	}
	else if (time >= 3600)
	{
		q = div(time, 3600);
		int h = q.quot;
		q = div(q.rem, 60);
		int m = q.quot;
		int s = q.rem;
		sprintf(text, "%dh %02dm %02ds", h, m, s);
	}
	else if (time >= 60)
	{
		q = div(time, 60);
		int m = q.quot;
		int s = q.rem;
		sprintf(text, "%d:%02d", m, s);
	}
	else
	{
		sprintf(text, "%d", (int)time);
		if (redOnLow)
		{
			float a = 255 * gHUD.GetHudTransparency();
			r = TIMER_RED_R, g = TIMER_RED_G, b = TIMER_RED_B;
			ScaleColors(r, g, b, a);
		}
	}

	// Output to screen
	int width = TextMessageDrawString(ScreenWidth + 1, ypos, text, 0, 0, 0);
	TextMessageDrawString((ScreenWidth - width) / 2, ypos, text, r, g, b);
}
