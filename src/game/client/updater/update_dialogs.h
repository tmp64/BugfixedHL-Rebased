#ifndef UPDATER_UPDATE_DIALOGS_H
#define UPDATER_UPDATE_DIALOGS_H
#include <vgui_controls/Frame.h>
#include <vgui_controls/RichText.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ProgressBar.h>
#include "http_client.h"

struct UpdateFileProgress;

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

/**
 * Dialog that shows the progress of downloading
 */
class CUpdateDownloadStatusDialog : public vgui2::Frame
{
public:
	DECLARE_CLASS_SIMPLE(CUpdateDownloadStatusDialog, vgui2::Frame);

	inline static CUpdateDownloadStatusDialog *Get()
	{
		static vgui2::DHANDLE<CUpdateDownloadStatusDialog> handle;

		if (!handle.Get())
			handle = new CUpdateDownloadStatusDialog();

		return handle;
	}

	CUpdateDownloadStatusDialog();
	virtual void OnThink() override;
	virtual void OnCommand(const char *pszCmd) override;

	void SetStatus(std::shared_ptr<CHttpClient::DownloadStatus> &pStatus);

private:
	std::shared_ptr<CHttpClient::DownloadStatus> m_pStatus;
	vgui2::ProgressBar *m_pProgress = nullptr;
	vgui2::Label *m_pProgressLabel = nullptr;

	std::string FormatMemSize(size_t iTotalMem);
};

/**
 * Dialog that shows the progress of ZIP file extraction.
 */
class CUpdateFileProgressDialog : public vgui2::Frame
{
public:
	DECLARE_CLASS_SIMPLE(CUpdateFileProgressDialog, vgui2::Frame);

	inline static CUpdateFileProgressDialog *Get()
	{
		static vgui2::DHANDLE<CUpdateFileProgressDialog> handle;

		if (!handle.Get())
			handle = new CUpdateFileProgressDialog();

		return handle;
	}

	CUpdateFileProgressDialog();
	virtual void OnCommand(const char *pszCmd) override;
	void UpdateProgress(const UpdateFileProgress &progress);
	void SetCancelButtonVisible(bool state);

private:
	vgui2::ProgressBar *m_pProgress = nullptr;
	vgui2::Label *m_pProgressLabel = nullptr;
	vgui2::Label *m_pFileNameLabel = nullptr;
	vgui2::Button *m_pCancelButton = nullptr;
};

class CUpdateFileReplaceDialog : public vgui2::Frame
{
public:
	DECLARE_CLASS_SIMPLE(CUpdateFileReplaceDialog, vgui2::Frame);

	inline static CUpdateFileReplaceDialog *Get()
	{
		static vgui2::DHANDLE<CUpdateFileReplaceDialog> handle;

		if (!handle.Get())
			handle = new CUpdateFileReplaceDialog();

		return handle;
	}

	CUpdateFileReplaceDialog();
	virtual void OnCommand(const char *pszCmd) override;
	void Activate(const std::string &name);

private:
	vgui2::Label *m_pFileNameLabel = nullptr;
};

#endif
