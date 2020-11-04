#ifndef UPDATER_UPDATE_DIALOGS_H
#define UPDATER_UPDATE_DIALOGS_H
#include <vgui_controls/Frame.h>
#include <vgui_controls/RichText.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/CheckButton.h>

/**
 * Dialog that shows that a new version is available and the changelog.
 */
class CUpdateNotificationDialog : public vgui2::Frame
{
	DECLARE_CLASS_SIMPLE(CUpdateNotificationDialog, vgui2::Frame);

public:
	inline static CUpdateNotificationDialog *Get()
	{
		static vgui2::DHANDLE<CUpdateNotificationDialog> handle;

		if (!handle.Get())
			handle = new CUpdateNotificationDialog();

		return handle;
	}

	CUpdateNotificationDialog();
	virtual void PerformLayout() override;
	virtual void OnCommand(const char *pszCmd) override;
	virtual void Activate() override;
	virtual void OnClose() override;

private:
	vgui2::Label *m_pOldVersionLabel = nullptr;
	vgui2::Label *m_pOldVersionTextLabel = nullptr;
	vgui2::Label *m_pNewVersionLabel = nullptr;
	vgui2::Label *m_pNewVersionTextLabel = nullptr;
	vgui2::RichText *m_pChangelog = nullptr;
	vgui2::CheckButton *m_pUpdatesCheckBtn = nullptr;
	vgui2::Button *m_pCloseBtn = nullptr;
	vgui2::Button *m_pUpdateBtn = nullptr;
};

#endif
