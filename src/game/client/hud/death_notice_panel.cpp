#include <algorithm>
#include <tier1/strtools.h>
#include <vgui/ISurface.h>
#include "hud.h"
#include "cl_util.h"
#include "hud/death_notice_panel.h"
#include "vgui/client_viewport.h"
#include "hud_renderer.h"

extern ConVar hud_deathnotice_time;
ConVar hud_deathnotice_time_self("hud_deathnotice_time_self", "12", FCVAR_BHL_ARCHIVE, "How long should your death notices stay up for");
ConVar hud_deathnotice_vgui("hud_deathnotice_vgui", "1", FCVAR_BHL_ARCHIVE, "Use VGUI deathnotice panel");

static constexpr int KILLFEED_COUNT = 6;
static constexpr int SKULL_SPRITE_HEIGHT = 16;

DEFINE_HUD_ELEM(CHudDeathNoticePanel);

CHudDeathNoticePanel::CHudDeathNoticePanel()
    : vgui2::Panel(NULL, "HudDeathNoticePanel")
{
	AssertFatal(CHudRenderer::Get().IsAvailable());

	SetParent(g_pViewport);
	SetPaintBackgroundEnabled(true);

	m_EntryList[0].resize(KILLFEED_COUNT);
	m_EntryList[1].resize(KILLFEED_COUNT);
}

void CHudDeathNoticePanel::VidInit()
{
	m_HUD_d_skull = gHUD.GetSpriteIndex("d_skull");

	int cornerWide, cornerTall;
	GetCornerTextureSize(cornerWide, cornerTall);

	int minRowTall = std::max(cornerTall * 2, gHUD.GetSpriteRect(m_HUD_d_skull).GetHeight());
	m_iRowTall = std::max(m_iRowHeight, minRowTall);
}

void CHudDeathNoticePanel::InitHudData()
{
	m_iEntryCount = 0;
}

void CHudDeathNoticePanel::Think()
{
	bool shouldBeVisible = hud_deathnotice_vgui.GetBool();

	if (IsVisible() != shouldBeVisible)
	{
		SetVisible(shouldBeVisible);
		m_iEntryCount = 0;

		if (shouldBeVisible)
			m_iFlags &= ~HUD_ACTIVE; // Disable HUD Think since it will be called from VGUI
		else
			m_iFlags |= HUD_ACTIVE; // Enable Think from HUD to update visibility
	}

	if (shouldBeVisible)
	{
		// Remove expired entries
		auto &oldList = m_EntryList[m_nActiveList];
		auto &newList = m_EntryList[!m_nActiveList];

		int newIdx = 0;

		for (int i = 0; i < m_iEntryCount; i++)
		{
			Entry &e = oldList[i];

			if (e.flEndTime > gHUD.m_flTime)
				newList[newIdx++] = e;
		}

		m_iEntryCount = newIdx;
		m_nActiveList = !m_nActiveList;
	}
}

void CHudDeathNoticePanel::AddItem(int killerId, int victimId, const char *killedwith)
{
	if (!GetThisPlayerInfo())
	{
		// Not yet connected
		return;
	}

	Entry e;
	CPlayerInfo *killer = GetPlayerInfoSafe(killerId);
	CPlayerInfo *victim = GetPlayerInfoSafe(victimId);
	int thisPlayerId = GetThisPlayerInfo()->GetIndex();

	// Check for suicide
	if (killerId == victimId || killerId == 0)
	{
		e.bIsSuicide = true;
	}

	// Check for team kill
	if (!strcmp(killedwith, "d_teammate"))
		e.bIsTeamKill = true;

	Color nameColor = m_ColorNameDefault;

	if (victimId == thisPlayerId)
	{
		e.type = EntryType::Death;
		nameColor = m_ColorNameDeath;
	}
	else if (killerId == thisPlayerId)
	{
		e.type = EntryType::Kill;
		nameColor = m_ColorNameKill;
	}

	// Fill killer info
	if (killer && !e.bIsSuicide)
	{
		bool removeColorCodes = killer->GetTeamNumber() != 0;
		e.iKillerLen = Q_UTF8ToWString(killer->GetDisplayName(removeColorCodes), e.wszKiller, sizeof(e.wszKiller), STRINGCONVERT_REPLACE);
		e.iKillerLen /= sizeof(wchar_t);
		e.iKillerLen--; // L'\0'
		e.iKillerWide = GetColoredTextWide(e.wszKiller, e.iKillerLen);
		e.killerColor = gHUD.GetClientColor(killerId, nameColor);
	}

	// Fill victim info
	if (victim)
	{
		bool removeColorCodes = victim->GetTeamNumber() != 0;
		e.iVictimLen = Q_UTF8ToWString(victim->GetDisplayName(removeColorCodes), e.wszVictim, sizeof(e.wszVictim), STRINGCONVERT_REPLACE);
		e.iVictimLen /= sizeof(wchar_t);
		e.iVictimLen--; // L'\0'
		e.iVictimWide = GetColoredTextWide(e.wszVictim, e.iVictimLen);
		e.victimColor = gHUD.GetClientColor(victimId, nameColor);
	}

	// Expiration time
	float ttl = e.type != EntryType::Other ? hud_deathnotice_time_self.GetFloat() : hud_deathnotice_time.GetFloat();
	e.flEndTime = gHUD.m_flTime + ttl;

	// Sprite
	int spriteId = gHUD.GetSpriteIndex(killedwith);

	if (spriteId == -1)
		spriteId = m_HUD_d_skull;

	const wrect_t &rc = gHUD.GetSpriteRect(spriteId);
	e.nSpriteIdx = spriteId;
	e.iSpriteWide = rc.right - rc.left;

	// Insert into the list
	Assert(m_iEntryCount <= KILLFEED_COUNT);

	if (m_iEntryCount < KILLFEED_COUNT)
	{
		// Add to the end
		auto &list = m_EntryList[m_nActiveList];
		list[m_iEntryCount] = e;
		m_iEntryCount++;
	}
	else
	{
		auto &oldList = m_EntryList[m_nActiveList];
		auto &newList = m_EntryList[!m_nActiveList];

		// Find the item with the lowest end time
		int minItem = 0;
		float minEndTime = oldList[minItem].flEndTime;

		for (int i = 0; i < m_iEntryCount; i++)
		{
			Entry &t = oldList[i];

			if (t.flEndTime < minEndTime)
			{
				minEndTime = t.flEndTime;
				minItem = i;
			}
		}

		// Copy all items except the previously found item
		std::copy(oldList.begin(), oldList.begin() + minItem, newList.begin());
		std::copy(oldList.begin() + minItem + 1, oldList.end(), newList.begin() + minItem);

		// Add to the end
		newList[m_iEntryCount - 1] = std::move(e);

		m_nActiveList = !m_nActiveList;
	}
}

void CHudDeathNoticePanel::ApplySettings(KeyValues *inResourceData)
{
	BaseClass::ApplySettings(inResourceData);
}

void CHudDeathNoticePanel::PaintBackground()
{
	// HACK ALERT: See Paint()
	auto &entries = m_EntryList[m_nActiveList];
	int panelWide = GetWide();
	int y = 0;

	// Draw background
	for (int i = 0; i < m_iEntryCount; i++)
	{
		Entry &entry = entries[i];
		int wide = 2 * m_iHPadding + GetEntryContentWide(entry);
		int x = panelWide - wide;
		Color bgColor;

		if (entry.type == EntryType::Kill)
			bgColor = m_ColorBgKill;
		else if (entry.type == EntryType::Death)
			bgColor = m_ColorBgDeath;
		else
			bgColor = m_ColorBg;

		// Draw background
		DrawBox(x, y, wide, m_iRowTall, bgColor, 1.0f);

		y += m_iRowTall + m_iVMargin;
	}

	// Draw sprites
	y = 0;

	for (int i = 0; i < m_iEntryCount; i++)
	{
		Entry &entry = entries[i];
		int wide = 2 * m_iHPadding + GetEntryContentWide(entry);
		int x = panelWide - wide;

		// Get sprite info
		HSPRITE hSprite = gHUD.GetSprite(entry.nSpriteIdx);
		const wrect_t &rc = gHUD.GetSpriteRect(entry.nSpriteIdx);

		// Calculate sprite pos
		int iconX = m_iHPadding + entry.iKillerWide;

		if (entry.iKillerWide != 0)
			iconX += m_iIconPadding; // padding on the left only if there is a text

		int iconY = (m_iRowTall - (rc.bottom - rc.top)) / 2;

		// Get sprite color
		Color color;

		if (entry.bIsTeamKill)
			color = m_ColorIconTK;
		else if (entry.type == EntryType::Kill)
			color = m_ColorIconKill;
		else if (entry.type == EntryType::Death)
			color = m_ColorIconDeath;
		else
			color = m_ColorIcon;

		CHudRenderer::SpriteSet(hSprite, color.r(), color.g(), color.b());
		CHudRenderer::SpriteDrawAdditive(0, x + iconX, y + iconY, &rc);

		y += m_iRowTall + m_iVMargin;
	}
}

void CHudDeathNoticePanel::Paint()
{
	// HACK ALERT!
	// Text rendering is done in Paint() because DrawFlushText
	// (called internally during rendering) disables alpha-blending at the end.
	// This causes transparent background to be drawn opaque.
	int panelWide = GetWide();
	int y = 0;
	int fontTall = vgui2::surface()->GetFontTall(m_TextFont);
	int textY = (m_iRowTall - fontTall) / 2;

	vgui2::surface()->DrawSetTextFont(m_TextFont);

	auto &entries = m_EntryList[m_nActiveList];

	for (int i = 0; i < m_iEntryCount; i++)
	{
		Entry &entry = entries[i];
		int wide = 2 * m_iHPadding + GetEntryContentWide(entry);
		int x = panelWide - wide;
		x += m_iHPadding;

		// Draw killer name
		if (entry.iKillerWide != 0)
		{
			DrawColoredText(x, y + textY, entry.wszKiller, entry.iKillerLen, entry.killerColor);
			x += entry.iKillerWide + m_iIconPadding;
		}

		// Skip icon
		x += entry.iSpriteWide + m_iIconPadding;

		// Draw victim name
		DrawColoredText(x, y + textY, entry.wszVictim, entry.iVictimLen, entry.victimColor);
		x += entry.iVictimWide;

		y += m_iRowTall + m_iVMargin;
	}
}

int CHudDeathNoticePanel::GetEntryContentWide(const Entry &e)
{
	int w = e.iKillerWide + m_iIconPadding + e.iSpriteWide + e.iVictimWide;

	if (e.iKillerWide != 0)
		w += m_iIconPadding;

	return w;
}

int CHudDeathNoticePanel::GetColoredTextWide(const wchar_t *str, int len)
{
	int x = 0;

	for (int i = 0; i < len; i++)
	{
		if (i + 1 < len && IsColorCode(str + i))
		{
			// Skip color code
			i++;
			continue;
		}

		x += vgui2::surface()->GetCharacterWidth(m_TextFont, str[i]);
	}

	return x;
}

int CHudDeathNoticePanel::DrawColoredText(int x0, int y0, const wchar_t *str, int len, Color c)
{
	int x = 0;
	vgui2::surface()->DrawSetTextColor(c);

	for (int i = 0; i < len; i++)
	{
		if (i + 1 < len && IsColorCode(str + i))
		{
			// Set color
			i++;
			int idx = str[i] - L'0';
			if (idx == 0 || idx == 9)
				vgui2::surface()->DrawSetTextColor(c);
			else
				vgui2::surface()->DrawSetTextColor(gHUD.GetColorCodeColor(idx));
			continue;
		}

		vgui2::surface()->DrawSetTextPos(x0 + x, y0);
		vgui2::surface()->DrawUnicodeChar(str[i]);
		x += vgui2::surface()->GetCharacterWidth(m_TextFont, str[i]);
	}

	return x;
}
