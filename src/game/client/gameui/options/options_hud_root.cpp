#include "options_hud_root.h"
#include "options_hud.h"
#include "options_hud_colors.h"

CHudSubOptionsRoot::CHudSubOptionsRoot(vgui2::Panel *parent)
    : BaseClass(parent, "HudSubOptionsRoot")
{
	SetSize(100, 100); // Silence "parent not sized yet" warning

	AddPage((m_pOptions = new CHudSubOptions(this)), "Options");
	AddPage((m_pColors = new CHudSubOptionsColors(this)), "Colors");
}

void CHudSubOptionsRoot::OnResetData()
{
	m_pOptions->OnResetData();
	m_pColors->OnResetData();
}

void CHudSubOptionsRoot::OnApplyChanges()
{
	m_pOptions->OnApplyChanges();
	m_pColors->OnApplyChanges();
}
