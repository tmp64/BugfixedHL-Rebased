// Martin Webrant (BulliT)

#ifndef HUD_AG_LOCATION_H
#define HUD_AG_LOCATION_H
#include "hud/base.h"

constexpr int AG_MAX_LOCATION_NAME = 32;
constexpr int AG_MAX_LOCATIONS = 512;

class AgHudLocation : public CHudElemBase<AgHudLocation>
{
public:
	AgHudLocation()
	    : m_fAt(0)
	    , m_fNear(0)
	    , m_firstLocation(NULL)
	    , m_freeLocation(m_locations)
	    , m_szMap {}
	    , m_pCvarLocationKeywords(NULL)
	{
		for (unsigned int i = 0; i < ARRAYSIZE(m_locations) - 1; i++)
		{
			m_locations[i].m_nextLocation = &m_locations[i + 1];
		}
		m_locations[ARRAYSIZE(m_locations) - 1].m_nextLocation = NULL;
	}

	void Init() override;
	void VidInit() override;
	void Draw(float flTime) override;
	void Reset() override;

private:
	class Location
	{
	public:
		Location *m_nextLocation;

		Location();
		virtual ~Location();

		void Show();

		char m_sLocation[AG_MAX_LOCATION_NAME];
		Vector m_vPosition;
	};

	float m_fAt;
	float m_fNear;
	Vector m_vPlayerLocations[MAX_PLAYERS + 1];
	Location m_locations[AG_MAX_LOCATIONS];
	Location *m_firstLocation;
	Location *m_freeLocation;

	Location *NearestLocation(const Vector &vPosition, float &fNearestDistance);
	char *FillLocation(const Vector &vPosition, char *pszSay, int pszSaySize);

	void InitDistances();
	void Load();
	void Save();

public:
	char m_szMap[32];
	cvar_t *m_pCvarLocationKeywords;

	void ParseAndEditSayString(int iPlayer, char *pszSay, int pszSaySize);

	void UserCmd_AddLocation();
	void UserCmd_DeleteLocation();
	void UserCmd_ShowLocations();

	int MsgFunc_InitLoc(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_Location(const char *pszName, int iSize, void *pbuf);
};

#endif
