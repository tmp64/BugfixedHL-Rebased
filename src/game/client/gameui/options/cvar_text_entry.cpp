#include <vgui/ILocalize.h>
#include "cvar_text_entry.h"
#include "hud.h"

CCvarTextEntry::CCvarTextEntry(vgui2::Panel *parent, const char *panelName, const char *cvarName, CvarType type)
    : vgui2::TextEntry(parent, panelName)
{
	m_pCvar = gEngfuncs.pfnGetCvarPointer(cvarName);
	if (!m_pCvar)
	{
		Msg("%s [CCvarTextEntry]: cvar '%s' not found.\n", panelName, cvarName);
	}
	m_nType = type;
	ResetData();
}

void CCvarTextEntry::ResetData()
{
	if (m_pCvar)
	{
		wchar_t unicode[1024];
		g_pVGuiLocalize->ConvertANSIToUnicode(m_pCvar->string, unicode, sizeof(unicode));
		SetText(unicode);
	}
}

void CCvarTextEntry::ApplyChanges()
{
	if (m_pCvar)
	{
		char buf1[256];
		char buf2[256];
		char cmd[256];

		if (m_nType == CvarType::String)
		{
			GetText(buf1, sizeof(buf1));

			// Copy buf1 to buf2 ignoring "
			char *s = buf1;
			int i = 0;
			while (*s)
			{
				if (*s != '"')
				{
					buf2[i] = *s;
					i++;
				}
				s++;
			}
			buf2[i] = '\0';

			snprintf(cmd, sizeof(cmd), "%s \"%s\"", m_pCvar->name, buf2);
			gEngfuncs.pfnClientCmd(cmd);
		}
		else if (m_nType == CvarType::Int)
		{
			GetText(buf1, sizeof(buf1));
			int value = atoi(buf1);
			snprintf(cmd, sizeof(cmd), "%s \"%d\"", m_pCvar->name, value);
			gEngfuncs.pfnClientCmd(cmd);
		}
		else if (m_nType == CvarType::Float)
		{
			GetText(buf1, sizeof(buf1));
			float value = atof(buf1);
			snprintf(cmd, sizeof(cmd), "%s \"%.2f\"", m_pCvar->name, value);
			gEngfuncs.pfnClientCmd(cmd);
		}
	}
}

void CCvarTextEntry::SetValue(int value)
{
	Assert(m_nType == CvarType::Int || m_nType == CvarType::String);
	char buf[256];
	snprintf(buf, sizeof(buf), "%d", value);
	SetText(buf);
}

void CCvarTextEntry::SetValue(float value)
{
	Assert(m_nType == CvarType::Float || m_nType == CvarType::String);
	char buf[256];
	snprintf(buf, sizeof(buf), "%.2f", value);
	SetText(buf);
}

float CCvarTextEntry::GetFloat()
{
	//return m_pCvar->value;
	char buf[128];
	GetText(buf, sizeof(buf));
	return atof(buf);
}

int CCvarTextEntry::GetInt()
{
	char buf[128];
	GetText(buf, sizeof(buf));
	return atoi(buf);
}

void CCvarTextEntry::FireActionSignal()
{
	BaseClass::FireActionSignal();
	KeyValues *kv = new KeyValues("CvarTextChanged");
	if (m_nType == CvarType::Float)
	{
		kv->SetFloat("value", GetFloat());
	}
	else if (m_nType == CvarType::Int)
	{
		kv->SetInt("value", GetInt());
	}
	else if (m_nType == CvarType::String)
	{
		char buf[128];
		GetText(buf, sizeof(buf));
		kv->SetString("value", buf);
	}
	PostActionSignal(kv);
}
