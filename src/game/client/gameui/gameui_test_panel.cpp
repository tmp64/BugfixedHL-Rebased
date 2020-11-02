#include <tier1/strtools.h>
#include "hud.h"
#include "gameui_test_panel.h"
#include "gameui_viewport.h"

static const char *s_Messages[] = {
	u8"Hello world!",
	u8"Did you submit your status report to the administrator, today?",
	u8"Freeman, you fool!",
	u8"Do you know who ate all the donuts?",
	u8"\"DO YOU **** *****?\" - Gunnery Sergeant Hartman, your senior drill instructor",
	u8"None of it is real, is it?",
	u8"This is a dream... Wake up",
	u8"Or is it? Hey, vsause, Michael here!",

	u8"Never gonna give you up\n"
	u8"Never gonna let you down\n"
	u8"Never gonna run around and desert you\n"
	u8"Never gonna make you cry\n"
	u8"Never gonna say goodbye\n"
	u8"Never gonna tell a lie and hurt you",

	u8"What is love, baby dont hurt me, dont hurt me\n        - no more",
	u8"I fear no man. But that thing (GoldSrc)... it scares me",

	u8"One shudders to imagine what inhuman thoughts lie behind that engine API...\n"
	u8"What dreams of chronic and sustained cruelty?",

	u8"- Knock-knock\n"
	u8"- Who's there?\n"
	u8"- Joe\n"
	u8"- Joe wh-\n"
	u8"- Joe Death\n",

	u8"RYZEN 5 3600X\n"
	u8"@\n"
	u8"RADEON RX580 8GB\n"
	u8"@\n"
	u8"TO PLAY A 1998 GAME"
};

CGameUITestPanel::CGameUITestPanel(vgui2::Panel *pParent)
    : BaseClass(pParent, "GameUITestPanel")
{
	SetTitle("Message of The Day", false);
	SetSizeable(true);
	SetSize(460, 230);
	SetDeleteSelfOnClose(true);
	MoveToCenterOfScreen();

	m_pText = new vgui2::RichText(this, "Text");
	m_pText->SetUnusedScrollbarInvisible(true);

	SetScheme(CGameUIViewport::Get()->GetScheme());
	InvalidateLayout();
}

void CGameUITestPanel::PerformLayout()
{
	constexpr int OFFSET = 4;
	BaseClass::PerformLayout();

	int x, y, wide, tall;
	GetClientArea(x, y, wide, tall);
	m_pText->SetBounds(x + OFFSET, y + OFFSET, wide - OFFSET * 2, tall - OFFSET * 2);
}

void CGameUITestPanel::Activate()
{
	BaseClass::Activate();

	// Select a random text
	wchar_t wbuf[1024];
	int idx = gEngfuncs.pfnRandomLong(0, std::size(s_Messages) - 1);
	Q_UTF8ToWString(s_Messages[idx], wbuf, sizeof(wbuf));
	m_pText->SetText(wbuf);
}
