#ifndef VGUI_CLIENT_MOTD_H
#define VGUI_CLIENT_MOTD_H

#include <vgui_controls/Frame.h>
#include "IViewportPanel.h"
#include "viewport_panel_names.h"

class IViewport;

namespace vgui2
{
class HTML;
class Label;
class RichText;
}

class CClientMOTD : public vgui2::Frame, public IViewportPanel
{
public:
	DECLARE_CLASS_SIMPLE(CClientMOTD, Frame);

	static const size_t MAX_HTML_FILENAME_LENGTH = 4096;

public:
	CClientMOTD();
	virtual ~CClientMOTD();

	void SetLabelText(const char *textEntryName, const wchar_t *text);

	virtual bool IsURL(const char *str);

	void PerformLayout() override;
	void OnKeyCodeTyped(vgui2::KeyCode key) override;
	void OnCommand(const char *command) override;

	void Close() override;

	virtual void Activate(const char *title, const char *msg);
	virtual void ActivateHtml(const char *title, const char *msg);

	//IViewportPanel overrides
	const char *GetName() override
	{
		return VIEWPORT_PANEL_MOTD;
	}

	void Reset() override;

	void ShowPanel(bool state) override;

	// VGUI functions:
	vgui2::VPANEL GetVPanel() override final
	{
		return BaseClass::GetVPanel();
	}

	bool IsVisible() override final
	{
		return BaseClass::IsVisible();
	}

	void SetParent(vgui2::VPANEL parent) override final
	{
		BaseClass::SetParent(parent);
	}

private:
	void RemoveTempFile();

private:
	vgui2::RichText *m_pMessage = nullptr;
	vgui2::HTML *m_pMessageHtml = nullptr;
	vgui2::Label *m_pServerName = nullptr;
	bool m_bFileWritten = false;
	char m_szTempFileName[MAX_HTML_FILENAME_LENGTH];
	int m_iScoreBoardKey = 0;
};

#endif
