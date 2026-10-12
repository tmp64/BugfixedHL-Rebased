#include <algorithm>
#include <climits>
#include <cmath>
#include <vector>
#include <tier1/strtools.h>
#include <vgui/ISurface.h>
#include "hud.h"
#include "cl_util.h"
#include "pm_shared.h"
#include "hud/gungame_hud.h"
#include "vgui/client_viewport.h"
#include "vgui/score_panel_cells.h"
#include "hud_renderer.h"

static constexpr int PHASE_WARMUP = 1;
static constexpr int PHASE_COUNTDOWN = 2;
static constexpr int PHASE_FINISHED = 3;

//! Names longer than this (in code points) are cut
static constexpr int MAX_NAME_CHARS = 20;

//! Right edge of the classic armor counter in widths of a digit: offset 10, suit icon 2, number 3
static constexpr int ARMOR_RIGHT_DIGITS = 15;

static constexpr int CIRCLE_TEXTURE_SIZE = 64;

static constexpr wchar_t MARKER_LEFT[] = L"\u2039";
static constexpr wchar_t MARKER_RIGHT[] = L"\u203A";

DEFINE_HUD_ELEM(CHudGunGamePanel);

//! Removes color codes and cuts the name to MAX_NAME_CHARS code points
static void GetShortName(const char *name, wchar_t *buf, int bufSizeBytes)
{
	char clean[128];
	RemoveColorCodes(name, clean, sizeof(clean));

	int chars = 0;

	for (char *c = clean; *c; c++)
	{
		if ((static_cast<unsigned char>(*c) & 0xC0) != 0x80 && chars++ == MAX_NAME_CHARS)
		{
			Q_strncpy(c, "\xE2\x80\xA6", sizeof(clean) - (c - clean));
			break;
		}
	}

	Q_UTF8ToWString(clean, buf, bufSizeBytes, STRINGCONVERT_REPLACE);
}

static int GetGlobalInt(const char *key)
{
	const CScoreboardData &data = CScoreboardData::Get();
	int value = 0;

	if (!data.GetGlobalValue(data.FindColumn(key)).GetNumber(value))
		return 0;

	return value;
}

//! Creates a white anti-aliased disc or ring texture from columns [x0; x1) of the circle
static int CreateCircleTexture(bool ring, int x0, int x1)
{
	constexpr float radius = CIRCLE_TEXTURE_SIZE / 2.0f;
	constexpr float ringWidth = CIRCLE_TEXTURE_SIZE / 6.0f;
	int wide = x1 - x0;
	std::vector<unsigned char> rgba(wide * CIRCLE_TEXTURE_SIZE * 4, 255);

	for (int y = 0; y < CIRCLE_TEXTURE_SIZE; y++)
	{
		for (int x = x0; x < x1; x++)
		{
			float dist = std::hypot(x + 0.5f - radius, y + 0.5f - radius);
			float alpha = clamp(radius - dist, 0.0f, 1.0f);

			if (ring)
				alpha *= clamp(dist - (radius - ringWidth), 0.0f, 1.0f);

			rgba[(y * wide + x - x0) * 4 + 3] = static_cast<unsigned char>(alpha * 255);
		}
	}

	int id = vgui2::surface()->CreateNewTextureID(true);
	vgui2::surface()->DrawSetTextureRGBA(id, rgba.data(), wide, CIRCLE_TEXTURE_SIZE, true, false);
	return id;
}

static void DrawTexture(int id, int x, int y, int wide, int tall, Color color)
{
	vgui2::surface()->DrawSetTexture(id);
	vgui2::surface()->DrawSetColor(color);
	vgui2::surface()->DrawTexturedRect(x, y, x + wide, y + tall);
}

CHudGunGamePanel::CHudGunGamePanel()
    : vgui2::Panel(NULL, "HudGunGamePanel")
{
	SetParent(g_pViewport);
	SetPaintBackgroundEnabled(true);
	SetMouseInputEnabled(false);
	SetKeyBoardInputEnabled(false);
	SetVisible(false);
}

bool CHudGunGamePanel::IsWidgetEnabled()
{
	return CScoreboardData::Get().IsGunGame() && CScoreboardData::IsGunGameHudEnabled();
}

int CHudGunGamePanel::GetLevelBarTop()
{
	CHudGunGamePanel *panel = Get();

	if (!panel || panel->m_iLevelCount == 0)
		return -1;

	int x, y;
	panel->GetPos(x, y);
	return y + panel->m_iBarY;
}

int CHudGunGamePanel::GetLevelBarOffset()
{
	int barTop = GetLevelBarTop();

	if (barTop == -1)
		return 0;

	CHudGunGamePanel *panel = Get();
	int lineBottom = ScreenHeight - gHUD.m_iFontHeight / 2;
	return max(0, lineBottom - barTop + panel->m_iCellGap);
}

void CHudGunGamePanel::Init()
{
	m_iFlags |= HUD_ACTIVE;
}

void CHudGunGamePanel::VidInit()
{
	m_iSpriteSerial = -1;
}

void CHudGunGamePanel::Think()
{
	bool visible = UpdateState() && !g_pViewport->IsScoreBoardVisible();

	if (IsVisible() != visible)
		SetVisible(visible);

	// VGUI calls Think only while the panel is visible
	if (visible)
		m_iFlags &= ~HUD_ACTIVE;
	else
		m_iFlags |= HUD_ACTIVE;
}

bool CHudGunGamePanel::UpdateState()
{
	m_iLevelCount = 0;

	if (!IsWidgetEnabled() || hud_draw.GetFloat() <= 0 || gHUD.m_iIntermission || (gHUD.m_iHideHUDDisplay & HIDEHUD_ALL))
		return false;

	CPlayerInfo *pi = nullptr;

	if (g_iUser1 == OBS_NONE)
		pi = GetThisPlayerInfo();
	else if (g_iUser1 == OBS_IN_EYE)
		pi = GetPlayerInfoSafe(g_iUser2);

	if (!pi || !pi->IsConnected() || pi->IsSpectator())
		return false;

	int phase = GetGlobalInt("phase");

	if (phase == PHASE_FINISHED)
		return false;

	const CScoreboardData &data = CScoreboardData::Get();
	int sortCol = data.GetFirstSortColumn();
	int team = pi->GetTeamNumber();

	m_iTarget = pi->GetIndex();
	m_iTeam = (sortCol != -1 && team > 0 && !data.GetTeamValue(team, sortCol).IsEmpty()) ? team : 0;
	m_wszTitle[0] = 0;
	m_wszInfo[0] = 0;
	m_iMeterMax = 0;
	m_iInfoY = m_iPlaceY + vgui2::surface()->GetFontTall(m_LargeFont) + m_iLineGap;
	m_iMeterY = m_iInfoY + vgui2::surface()->GetFontTall(m_TextFont) + m_iMeterGap;

	if (phase == PHASE_WARMUP || phase == PHASE_COUNTDOWN)
	{
		int left = max(GetGlobalInt("phase_left"), 0);
		wchar_t time[16];

		if (phase == PHASE_WARMUP)
		{
			V_snwprintf(time, std::size(time), L"%d:%02d", left / 60, left % 60);
			ConstructScoreText(m_wszTitle, sizeof(m_wszTitle), "BHL_GG_Warmup", 1, time);
		}
		else
		{
			V_snwprintf(time, std::size(time), L"%d", left);
			ConstructScoreText(m_wszTitle, sizeof(m_wszTitle), "BHL_GG_StartIn", 1, time);
		}

		return true;
	}

	UpdatePlace();

	const CScoreboardData::Value &progress = GetCell("progress");

	if (progress.type == bhl::sb::E_ValueType::Progress && progress.iMax > 0)
	{
		m_iMeterMax = progress.iMax;
		m_iMeterCur = clamp(progress.iValue, 0, progress.iMax);
	}

	UpdateLevelBar();
	return true;
}

void CHudGunGamePanel::UpdatePlace()
{
	const CScoreboardData &data = CScoreboardData::Get();
	int sortCol = data.GetFirstSortColumn();

	if (sortCol == -1)
		return;

	bool teamplay = m_iTeam != 0;
	int total = 0;
	int place = teamplay ? data.GetTeamPlace(m_iTeam, &total) : data.GetPlayerPlace(m_iTarget, &total);
	int value = 0;
	const CScoreboardData::Value &ownValue = teamplay ? data.GetTeamValue(m_iTeam, sortCol) : data.GetPlayerValue(m_iTarget, sortCol);

	if (place == 0 || !ownValue.GetNumber(value))
		return;

	wchar_t placeText[16], totalText[16];
	V_snwprintf(placeText, std::size(placeText), L"%d", place);
	V_snwprintf(totalText, std::size(totalText), L"%d", total);
	ConstructScoreText(m_wszTitle, sizeof(m_wszTitle), "BHL_GG_Place", 2, placeText, totalText);

	// Nearest rival ahead and the gap to the best rival behind (docs/scoreboard-protocol.md, 8.2)
	int sign = data.GetColumn(sortCol).IsSortAscending() ? -1 : 1;
	int rivals = 0;
	int ahead = 0;
	int aheadGap = 0;
	int behindGap = INT_MAX;

	auto fnCheckRival = [&](int id, const CScoreboardData::Value &rivalValue) {
		int rival = 0;

		if (!rivalValue.GetNumber(rival))
			return;

		int gap = (rival - value) * sign;
		rivals++;

		if (gap <= 0)
		{
			behindGap = min(behindGap, -gap);
			return;
		}

		bool isNearer = ahead == 0 || gap < aheadGap;

		if (!isNearer && gap == aheadGap)
			isNearer = (teamplay ? data.CompareTeams(id, ahead) : data.ComparePlayers(id, ahead)) > 0;

		if (isNearer)
		{
			ahead = id;
			aheadGap = gap;
		}
	};

	if (teamplay)
	{
		bool hasPlayers[MAX_TEAMS + 1] = {};

		for (int i = 1; i <= MAX_PLAYERS; i++)
		{
			CPlayerInfo *pi = GetPlayerInfoSafe(i);

			if (pi && !pi->IsSpectator())
				hasPlayers[pi->GetTeamNumber()] = true;
		}

		for (int i = 1; i <= MAX_TEAMS; i++)
		{
			if (i != m_iTeam && hasPlayers[i])
				fnCheckRival(i, data.GetTeamValue(i, sortCol));
		}
	}
	else
	{
		for (int i = 1; i <= MAX_PLAYERS; i++)
		{
			CPlayerInfo *pi = GetPlayerInfoSafe(i);

			if (pi && i != m_iTarget && !pi->IsSpectator())
				fnCheckRival(i, data.GetPlayerValue(i, sortCol));
		}
	}

	wchar_t kills[64];

	if (rivals == 0)
	{
		ConstructScoreText(m_wszInfo, sizeof(m_wszInfo), "BHL_GG_Waiting", 0, nullptr);
	}
	else if (ahead != 0)
	{
		wchar_t name[64];
		GetShortName(teamplay ? GetTeamInfo(ahead)->GetDisplayName() : GetPlayerInfo(ahead)->GetDisplayName(), name, sizeof(name));
		FormatScoreKills(kills, sizeof(kills), max(aheadGap + 1, 1));
		ConstructScoreText(m_wszInfo, sizeof(m_wszInfo), "BHL_GG_Overtake", 2, name, kills);
	}
	else if (behindGap == 0)
	{
		ConstructScoreText(m_wszInfo, sizeof(m_wszInfo), "BHL_GG_Tied", 0, nullptr);
	}
	else
	{
		FormatScoreKills(kills, sizeof(kills), behindGap);
		ConstructScoreText(m_wszInfo, sizeof(m_wszInfo), "BHL_GG_Lead", 1, kills);
	}
}

void CHudGunGamePanel::UpdateLevelBar()
{
	const CScoreboardData &data = CScoreboardData::Get();
	const CScoreboardData::Value &level = GetCell("level");
	const CScoreboardData::List *list = data.FindList("levels");
	int count = (list && list->iCount > 0) ? list->iCount : level.iMax;

	if (level.type != bhl::sb::E_ValueType::Progress || count <= 0)
		return;

	if (m_iSpriteSerial != data.GetSerial())
	{
		m_iSpriteSerial = data.GetSerial();

		for (int i = 0; i < bhl::sb::MAX_LIST_ITEMS; i++)
		{
			bool hasIcon = list && i < list->iCount && list->items[i].type == bhl::sb::E_ValueType::Icon && list->items[i].szSprite[0];
			m_iSprites[i] = hasIcon ? gHUD.GetSpriteIndex(list->items[i].szSprite) : -1;
		}
	}

	m_iLevelCount = count;
	m_iLevel = clamp(level.iValue - 1, 0, count - 1);

	// The bar stands between the armor and the ammo numbers. If it doesn't fit there, it moves above them.
	int digitWide = gHUD.GetSpriteRect(gHUD.m_HUD_number_0).GetWidth();
	int freeWide = GetWide() - 2 * (ARMOR_RIGHT_DIGITS * digitWide + m_iCellGap);
	int minWide = count * m_iCellMinWide + (count - 1) * m_iCellGap;
	bool lifted = minWide > freeWide && freeWide < m_iBarMaxWide;
	int maxWide = lifted ? m_iBarMaxWide : min(m_iBarMaxWide, freeWide);

	m_iBarCellWide = clamp((maxWide - (count - 1) * m_iCellGap) / count, m_iCellMinWide, m_iCellWide);
	m_iBarFirst = 0;
	m_iBarShown = count;

	if (minWide > maxWide)
	{
		int markerWide = vgui2::surface()->GetCharacterWidth(m_TextFont, MARKER_LEFT[0]) + m_iCellGap;
		m_iBarShown = clamp((maxWide - 2 * markerWide + m_iCellGap) / max(m_iCellMinWide + m_iCellGap, 1), 1, count);
		m_iBarFirst = clamp(m_iLevel - m_iBarShown / 2, 0, count - m_iBarShown);
	}

	int lineTop = GetTall() - gHUD.m_iFontHeight - gHUD.m_iFontHeight / 2;
	m_iBarX = (GetWide() - GetLevelBarWide()) / 2;
	m_iBarY = lifted ? lineTop - m_iCellGap - m_iCellTall : lineTop + (gHUD.m_iFontHeight - m_iCellTall) / 2;
}

const CScoreboardData::Value &CHudGunGamePanel::GetCell(const char *key)
{
	const CScoreboardData &data = CScoreboardData::Get();
	int col = data.FindColumn(key);
	const CScoreboardData::Value &value = data.GetPlayerValue(m_iTarget, col);

	if (value.IsEmpty() && m_iTeam != 0)
		return data.GetTeamValue(m_iTeam, col);

	return value;
}

Color CHudGunGamePanel::GetColor(int alpha)
{
	Color color = gHUD.GetHudColor(HudPart::Common, 0);
	return Color(color.r(), color.g(), color.b(), static_cast<int>(clamp(alpha, 0, 255) * gHUD.GetHudTransparency()));
}

int CHudGunGamePanel::GetMeterBarWide()
{
	return bhl::sb::MAX_DOTS * m_iDotSize + (bhl::sb::MAX_DOTS - 1) * m_iDotGap;
}

int CHudGunGamePanel::GetLevelBarWide()
{
	return m_iBarShown * m_iBarCellWide + (m_iBarShown - 1) * m_iCellGap;
}

void CHudGunGamePanel::DrawRoundedBar(int x, int y, int wide, Color color)
{
	int radius = m_iDotSize / 2;
	wide = max(wide, 2 * radius);

	DrawTexture(m_iCapTextures[0], x, y, radius, m_iDotSize, color);
	vgui2::surface()->DrawSetColor(color);
	vgui2::surface()->DrawFilledRect(x + radius, y, x + wide - radius, y + m_iDotSize);
	DrawTexture(m_iCapTextures[1], x + wide - radius, y, radius, m_iDotSize, color);
}

void CHudGunGamePanel::DrawCenteredText(vgui2::HFont font, int y, const wchar_t *text, Color color)
{
	if (!text[0])
		return;

	int wide, tall;
	vgui2::surface()->GetTextSize(font, text, wide, tall);
	DrawCellText(font, (GetWide() - wide) / 2, y, color, text);
}

void CHudGunGamePanel::PaintBackground()
{
	// HACK ALERT: Text is drawn in Paint(), see CHudDeathNoticePanel::Paint()
	if (m_iDotTexture == -1)
	{
		m_iDotTexture = CreateCircleTexture(false, 0, CIRCLE_TEXTURE_SIZE);
		m_iRingTexture = CreateCircleTexture(true, 0, CIRCLE_TEXTURE_SIZE);
		m_iCapTextures[0] = CreateCircleTexture(false, 0, CIRCLE_TEXTURE_SIZE / 2);
		m_iCapTextures[1] = CreateCircleTexture(false, CIRCLE_TEXTURE_SIZE / 2, CIRCLE_TEXTURE_SIZE);
	}

	if (m_iMeterMax > bhl::sb::MAX_DOTS)
	{
		int wide = GetMeterBarWide();
		int x = (GetWide() - wide) / 2;
		DrawRoundedBar(x, m_iMeterY, wide, GetColor(m_iAlphaDim));

		if (m_iMeterCur > 0)
			DrawRoundedBar(x, m_iMeterY, wide * m_iMeterCur / m_iMeterMax, GetColor(255));
	}
	else if (m_iMeterMax > 0)
	{
		int x = (GetWide() - (m_iMeterMax * m_iDotSize + (m_iMeterMax - 1) * m_iDotGap)) / 2;

		for (int i = 0; i < m_iMeterMax; i++)
		{
			DrawTexture(i < m_iMeterCur ? m_iDotTexture : m_iRingTexture, x, m_iMeterY, m_iDotSize, m_iDotSize, GetColor(255));
			x += m_iDotSize + m_iDotGap;
		}
	}

	for (int i = 0; i < m_iBarShown && m_iLevelCount > 0; i++)
	{
		int level = m_iBarFirst + i;
		int x = m_iBarX + i * (m_iBarCellWide + m_iCellGap);
		int alpha = level < m_iLevel ? m_iAlphaPassed : (level == m_iLevel ? m_iAlphaCurrent : m_iAlphaFuture);

		vgui2::surface()->DrawSetColor(GetColor(alpha));
		vgui2::surface()->DrawFilledRect(x, m_iBarY, x + m_iBarCellWide, m_iBarY + m_iCellTall);

		if (level == m_iLevel)
		{
			vgui2::surface()->DrawSetColor(GetColor(255));
			vgui2::surface()->DrawOutlinedRect(x, m_iBarY, x + m_iBarCellWide, m_iBarY + m_iCellTall);
		}
	}

	if (m_iLevelCount == 0 || !CHudRenderer::Get().IsAvailable())
		return;

	for (int i = 0; i < m_iBarShown; i++)
	{
		int level = m_iBarFirst + i;
		int sprite = level < bhl::sb::MAX_LIST_ITEMS ? m_iSprites[level] : -1;

		if (sprite == -1)
			continue;

		const wrect_t &rc = gHUD.GetSpriteRect(sprite);

		if (rc.GetWidth() <= 0 || rc.GetHeight() <= 0)
			continue;

		float scale = std::min(float(m_iBarCellWide - 2 * m_iIconPadding) / rc.GetWidth(), float(m_iCellTall - 2 * m_iIconPadding) / rc.GetHeight());
		int wide = static_cast<int>(rc.GetWidth() * scale);
		int tall = static_cast<int>(rc.GetHeight() * scale);

		if (wide <= 0 || tall <= 0)
			continue;

		int x = m_iBarX + i * (m_iBarCellWide + m_iCellGap) + (m_iBarCellWide - wide) / 2;
		int y = m_iBarY + (m_iCellTall - tall) / 2;
		Color color = GetColor(level > m_iLevel ? m_iAlphaDim : 255);

		CHudRenderer::SpriteSet(gHUD.GetSprite(sprite), color.r() * color.a() / 255, color.g() * color.a() / 255, color.b() * color.a() / 255);
		CHudRenderer::SpriteDrawAdditiveScaled(0, x, y, wide, tall, &rc);
	}
}

void CHudGunGamePanel::Paint()
{
	Color color = GetColor(255);

	DrawCenteredText(m_LargeFont, m_iPlaceY, m_wszTitle, color);
	DrawCenteredText(m_TextFont, m_iInfoY, m_wszInfo, color);

	int fontTall = vgui2::surface()->GetFontTall(m_TextFont);

	if (m_iMeterMax > bhl::sb::MAX_DOTS)
	{
		wchar_t counter[32];
		V_snwprintf(counter, std::size(counter), L"%d / %d", m_iMeterCur, m_iMeterMax);
		DrawCellText(m_TextFont, (GetWide() + GetMeterBarWide()) / 2 + m_iCounterGap, m_iMeterY + (m_iDotSize - fontTall) / 2, color, counter);
	}

	if (m_iLevelCount == 0)
		return;

	int y = m_iBarY + (m_iCellTall - fontTall) / 2;

	if (m_iBarFirst > 0)
	{
		int markerWide = vgui2::surface()->GetCharacterWidth(m_TextFont, MARKER_LEFT[0]);
		DrawCellText(m_TextFont, m_iBarX - m_iCellGap - markerWide, y, color, MARKER_LEFT);
	}

	if (m_iBarFirst + m_iBarShown < m_iLevelCount)
		DrawCellText(m_TextFont, m_iBarX + GetLevelBarWide() + m_iCellGap, y, color, MARKER_RIGHT);
}
