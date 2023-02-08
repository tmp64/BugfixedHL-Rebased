// Martin Webrant (BulliT)

#include <tier0/platform.h>
#include "hud.h"
#include "cl_entity.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "event_api.h"
#include "ag_location.h"

//! Maximum location file size. The file is read into memory so this is just an arbitrary sane limit.
constexpr int MAX_LOCATION_FILE_SIZE = 4 * 1024 * 1024; // 4 MB

DEFINE_HUD_ELEM(AgHudLocation);

AgHudLocation::Location::Location()
    : m_nextLocation(NULL)
{
	m_vPosition = Vector(0, 0, 0);
}

AgHudLocation::Location::~Location()
{
}

void AgHudLocation::Location::Show()
{
	const int spot = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/laserdot.spr");
	gEngfuncs.pEfxAPI->R_TempSprite(m_vPosition, vec3_origin, 1, spot, kRenderTransAlpha, kRenderFxNoDissipation, 255.0, 10, FTENT_SPRCYCLE);
}

void AgHudLocation::Init()
{
	m_fAt = 0;
	m_fNear = 0;
	m_iFlags = 0;
	m_szMap[0] = '\0';

	for (int i = 1; i <= MAX_PLAYERS; i++)
		m_vPlayerLocations[i] = Vector(0, 0, 0);

	HookMessage<&AgHudLocation::MsgFunc_Location>("Location");
	HookMessage<&AgHudLocation::MsgFunc_InitLoc>("InitLoc");

	HookCommand<&AgHudLocation::UserCmd_AddLocation>("agaddloc");
	HookCommand<&AgHudLocation::UserCmd_DeleteLocation>("agdelloc");
	HookCommand<&AgHudLocation::UserCmd_ShowLocations>("aglistloc");

	m_pCvarLocationKeywords = gEngfuncs.pfnRegisterVariable("cl_location_keywords", "0", FCVAR_BHL_ARCHIVE);
}

void AgHudLocation::VidInit()
{
}

void AgHudLocation::Reset()
{
	m_iFlags &= ~HUD_ACTIVE;
}

void AgHudLocation::Draw(float fTime)
{
}

void AgHudLocation::UserCmd_AddLocation()
{
	if (gEngfuncs.Cmd_Argc() != 2)
		return;

	char *locationName = gEngfuncs.Cmd_Argv(1);
	if (locationName[0] == 0)
		return;

	if (m_freeLocation == NULL)
	{
		ConsolePrint("Locations limit reached. Can't add new location.");
		return;
	}

	Location *pLocation = m_firstLocation;
	while (pLocation->m_nextLocation != NULL)
		pLocation = pLocation->m_nextLocation;
	pLocation->m_nextLocation = m_freeLocation;
	m_freeLocation = m_freeLocation->m_nextLocation;
	pLocation = pLocation->m_nextLocation;
	pLocation->m_nextLocation = NULL;

	strncpy(pLocation->m_sLocation, locationName, AG_MAX_LOCATION_NAME - 1);
	pLocation->m_sLocation[AG_MAX_LOCATION_NAME - 1] = 0;
	//pLocation->m_vPosition = m_vPlayerLocations[gEngfuncs.GetLocalPlayer()->index];
	pLocation->m_vPosition = gEngfuncs.GetLocalPlayer()->attachment[0];
	pLocation->Show();

	Save();
	InitDistances();

	char szMsg[128];
	sprintf(szMsg, "Added Location %s.\n", (const char *)locationName);
	ConsolePrint(szMsg);
}

void AgHudLocation::UserCmd_DeleteLocation()
{
	if (gEngfuncs.Cmd_Argc() != 2)
		return;

	char *locationName = gEngfuncs.Cmd_Argv(1);
	if (locationName[0] == 0)
		return;

	Location **prevNextLocPointer = &m_firstLocation;
	for (Location *pLocation = m_firstLocation; pLocation != NULL; pLocation = pLocation->m_nextLocation)
	{
		if (_stricmp(pLocation->m_sLocation, locationName) != 0)
		{
			prevNextLocPointer = &pLocation->m_nextLocation;
			continue;
		}

		*prevNextLocPointer = pLocation->m_nextLocation;

		pLocation->m_nextLocation = m_freeLocation;
		m_freeLocation = pLocation;

		char szMsg[128];
		sprintf(szMsg, "Deleted Location %s.\n", (const char *)locationName);
		ConsolePrint(szMsg);

		Save();
		InitDistances();
		break;
	}
}

void AgHudLocation::UserCmd_ShowLocations()
{
	for (Location *pLocation = m_firstLocation; pLocation != NULL; pLocation = pLocation->m_nextLocation)
	{
		char szMsg[128];
		sprintf(szMsg, "%s\n", (const char *)pLocation->m_sLocation);
		ConsolePrint(szMsg);
		pLocation->Show();
	}
}

void AgHudLocation::InitDistances()
{
	float fMinDistance = -1;
	float fMaxdistance = 0;

	// Calculate max/min distance between all locations.
	for (Location *pLocation1 = m_firstLocation; pLocation1 != NULL; pLocation1 = pLocation1->m_nextLocation)
	{
		for (Location *pLocation2 = m_firstLocation; pLocation2 != NULL; pLocation2 = pLocation2->m_nextLocation)
		{
			if (pLocation1 == pLocation2)
				continue;

			const float distance = (pLocation2->m_vPosition - pLocation1->m_vPosition).Length();

			if (distance < fMinDistance || fMinDistance == -1)
				fMinDistance = distance;

			if (distance > fMaxdistance)
				fMaxdistance = distance;
		}
	}

	// Now calculate when you are at/near a location or at atleast when its closest.
	m_fAt = fMinDistance / 2; // You are at a location if you are one fourth between to locations.
	m_fNear = fMinDistance / 1.1; // Over halfway of the mindistance you are at the "side".
}

void AgHudLocation::Load()
{
	m_firstLocation = NULL;
	m_freeLocation = &m_locations[0];
	for (unsigned int i = 0; i < ARRAYSIZE(m_locations) - 1; i++)
	{
		m_locations[i].m_nextLocation = &m_locations[i + 1];
	}
	m_locations[ARRAYSIZE(m_locations) - 1].m_nextLocation = NULL;

	std::vector<char> szData;
	char szFile[MAX_PATH];
	const char *gameDirectory = gEngfuncs.pfnGetGameDirectory();
	sprintf(szFile, "%s/locs/%s.loc", gameDirectory, m_szMap);
	FILE *pFile = fopen(szFile, "rb");
	if (!pFile)
	{
		// file error
		char szMsg[1024];
		snprintf(szMsg, sizeof(szMsg), "Couldn't open location file %s.\n", szFile);
		ConsolePrint(szMsg);
		return;
	}

	// Get file size
	fseek(pFile, 0, SEEK_END);
	int locFileSize = (int)ftell(pFile);

	if (locFileSize < 0)
	{
		ConPrintf("%s: ftell failed\n", szFile);
		fclose(pFile);
		return;
	}

	if (locFileSize == 0)
	{
		ConPrintf("%s: file is empty\n", szFile);
		fclose(pFile);
		return;
	}

	if (locFileSize == MAX_LOCATION_FILE_SIZE)
	{
		ConPrintf("%s: file is too large\n", szFile);
		fclose(pFile);
		return;
	}

	szData.resize(locFileSize + 1);
	const int iRead = fread(szData.data(), sizeof(char), locFileSize, pFile);
	fclose(pFile);
	pFile = nullptr;

	if (iRead != locFileSize)
	{
		ConPrintf("%s: failed to read the file\n", szFile);
		return;
	}

	szData[iRead] = '\0';

	enum class ParseState
	{
		Location,
		X,
		Y,
		Z
	};
	ParseState parseState = ParseState::Location;
	Location *lastLocation = NULL;
	m_firstLocation = m_freeLocation;
	char *pszParse = strtok(szData.data(), "#");
	if (pszParse)
	{
		while (pszParse)
		{
			switch (parseState)
			{
			case ParseState::Location:
				strncpy(m_freeLocation->m_sLocation, pszParse, AG_MAX_LOCATION_NAME - 1);
				m_freeLocation->m_sLocation[AG_MAX_LOCATION_NAME - 1] = 0;
				parseState = ParseState::X;
				break;
			case ParseState::X:
				m_freeLocation->m_vPosition.x = atof(pszParse);
				parseState = ParseState::Y;
				break;
			case ParseState::Y:
				m_freeLocation->m_vPosition.y = atof(pszParse);
				parseState = ParseState::Z;
				break;
			case ParseState::Z:
				m_freeLocation->m_vPosition.z = atof(pszParse);
				parseState = ParseState::Location;
				lastLocation = m_freeLocation;
				m_freeLocation = m_freeLocation->m_nextLocation;
				break;
			}
			if (m_freeLocation == NULL)
				break;
			pszParse = strtok(NULL, "#");
		}
	}

	if (lastLocation == NULL)
		m_firstLocation = NULL;
	else
		lastLocation->m_nextLocation = NULL;

	InitDistances();
}

void AgHudLocation::Save()
{
	if (m_firstLocation == NULL)
		return;

	char szFile[MAX_PATH];
	const char *gameDirectory = gEngfuncs.pfnGetGameDirectory();
	sprintf(szFile, "%s/locs/%s.loc", gameDirectory, m_szMap);
	FILE *pFile = fopen(szFile, "wb");
	if (!pFile)
	{
		// file error
		char szMsg[1024];
		snprintf(szMsg, sizeof(szMsg), "Couldn't create/save location file %s.\n", szFile);
		ConsolePrint(szMsg);
		return;
	}

	// Loop and write the file.
	for (Location *pLocation = m_firstLocation; pLocation != NULL; pLocation = pLocation->m_nextLocation)
	{
		fprintf(pFile, "%s#%f#%f#%f#", (const char *)pLocation->m_sLocation, pLocation->m_vPosition.x, pLocation->m_vPosition.y, pLocation->m_vPosition.z);
	}

	fclose(pFile);
}

AgHudLocation::Location *AgHudLocation::NearestLocation(const Vector &vPosition, float &fNearestDistance)
{
	fNearestDistance = -1;
	Location *pLocation = NULL;

	for (Location *pLocation1 = m_firstLocation; pLocation1 != NULL; pLocation1 = pLocation1->m_nextLocation)
	{
		float fDistance = (vPosition - pLocation1->m_vPosition).Length();

		if (fDistance < fNearestDistance || fNearestDistance == -1)
		{
			fNearestDistance = fDistance;
			pLocation = pLocation1;
		}
	}

	return pLocation;
}

char *AgHudLocation::FillLocation(const Vector &vPosition, char *pszSay, int pszSaySize)
{
	if (m_firstLocation == NULL)
		return pszSay;

	float fNearestDistance = 0;
	Location *pLocation = NearestLocation(vPosition, fNearestDistance);

	if (pLocation == NULL)
		return pszSay;

#ifdef _DEBUG
		//pLocation->Show();
#endif

	if (fNearestDistance < m_fAt || m_pCvarLocationKeywords->value < 1)
	{
		return pszSay + snprintf(pszSay, pszSaySize, "%s", pLocation->m_sLocation);
	}
	if (fNearestDistance < m_fNear)
	{
		return pszSay + snprintf(pszSay, pszSaySize, "Near %s", pLocation->m_sLocation);
	}
	return pszSay + snprintf(pszSay, pszSaySize, "%s Side", pLocation->m_sLocation);
}

void AgHudLocation::ParseAndEditSayString(int iPlayer, char *pszSay, int pszSaySize)
{
	// Make backup
	char *pszText = new char[pszSaySize];
	char *pszSayTemp = pszText;
	strcpy(pszText, pszSay);

	// Now parse for %L and edit it.
	char *pszSayEnd = pszSay + pszSaySize - 1;
	while (*pszSayTemp && pszSay < pszSayEnd) // Dont overflow the string.
	{
		if (*pszSayTemp == '%')
		{
			pszSayTemp++;
			if (*pszSayTemp == 'l' || *pszSayTemp == 'L' || *pszSayTemp == 'd' || *pszSayTemp == 'D')
			{
				pszSay = FillLocation(m_vPlayerLocations[iPlayer], pszSay, pszSayEnd - pszSay);
				pszSayTemp++;
				continue;
			}

			pszSay[0] = '%';
			pszSay++;
			continue;
		}

		*pszSay = *pszSayTemp;
		pszSay++;
		pszSayTemp++;
	}
	*pszSay = '\0';

	delete[] pszText;
}

int AgHudLocation::MsgFunc_Location(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	int iPlayer = READ_BYTE();
	for (int i = 0; i < 3; i++)
		m_vPlayerLocations[iPlayer][i] = READ_COORD();

	return 1;
}

int AgHudLocation::MsgFunc_InitLoc(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	strcpy(m_szMap, READ_STRING());
	Load();

	return 1;
}
