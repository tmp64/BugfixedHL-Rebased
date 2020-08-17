#ifndef CHUDTIMER_H
#define CHUDTIMER_H

#include "base.h"

class CHudTimer : public CHudElemBase<CHudTimer>
{
public:
	void Init();
	void VidInit();
	void Think();
	void Draw(float flTime);

	int MsgFunc_Timer(const char *pszName, int iSize, void *pbuf);

	void DoResync();
	void ReadDemoTimerBuffer(int type, const unsigned char *buffer);
	void CustomTimerCommand();

	enum
	{
		SV_AG_NONE = -1,
		SV_AG_UNKNOWN = 0,
		SV_AG_MINI = 1,
		SV_AG_FULL = 2,
	};

	int GetAgVersion();
	float GetHudNextmap();
	const char *GetNextmap();
	void SetNextmap(const char *nextmap);

private:
	enum
	{
		MAX_CUSTOM_TIMERS = 2,
	};

	void SyncTimer(float fTime);
	void SyncTimerLocal(float fTime);
	void SyncTimerRemote(unsigned int ip, unsigned short port, float fTime, double latency);
	void DrawTimerInternal(int time, float ypos, int r, int g, int b, bool redOnLow);

	float m_flDemoSyncTime;
	bool m_bDemoSyncTimeValid;
	float m_flNextSyncTime;
	bool m_flSynced;
	float m_flEndTime;
	float m_flEffectiveTime;
	bool m_bDelayTimeleftReading;
	float m_flCustomTimerStart[MAX_CUSTOM_TIMERS];
	float m_flCustomTimerEnd[MAX_CUSTOM_TIMERS];
	bool m_bCustomTimerNeedSound[MAX_CUSTOM_TIMERS];
	int m_eAgVersion;
	char m_szNextmap[MAX_MAP_NAME];
	bool m_bNeedWriteTimer;
	bool m_bNeedWriteCustomTimer;
	bool m_bNeedWriteNextmap;

	cvar_t *m_pCvarHudTimer;
	cvar_t *m_pCvarHudTimerSync;
	cvar_t *m_pCvarHudNextmap;
	cvar_t *m_pCvarMpTimelimit;
	cvar_t *m_pCvarMpTimeleft;
	cvar_t *m_pCvarSvAgVersion;
	cvar_t *m_pCvarAmxNextmap;

	char m_szPacketBuffer[22400]; // 16x1400 split packets
	int m_iResponceID;
	int m_iReceivedSize;
	int m_iReceivedPackets;
	int m_iReceivedPacketsCount;
};

#endif
