#ifndef UPDATER_UPDATE_CHECKER_H
#define UPDATER_UPDATE_CHECKER_H
#include <CGameVersion.h>
#include "http_client.h"

class CUpdateChecker
{
public:
	/**
	 * Returns a singleton of CUpdateChecker.
	 */
	static CUpdateChecker &Get();

	/**
	 * Initializes update checker.
	 */
	void Init();

	/**
	 * Called every frame to check that updates were found after config was loaded.
	 */
	void RunFrame();

	/**
	 * True if update check is in progress.
	 */
	bool IsInProgress();

	/**
	 * True if new update was found.
	 */
	bool IsUpdateFound();

	/**
	 * Returns current version of the game.
	 */
	const CGameVersion &GetCurVersion();

	/**
	 * Returns latest version of the game.
	 */
	const CGameVersion &GetLatestVersion();

	/**
	 * Returns changelog string.
	 */
	const std::string &GetChangelog();

	/**
	 * Returns download URL for the asset or an empty string.
	 */
	const std::string &GetAssetURL();

	/**
	 * Check for updates.
	 */
	void CheckForUpdates();

private:
	bool m_bInProgress = false;
	bool m_bUpdateFound = false;
	std::string m_Changelog;
	std::string m_ZipURL;
	CGameVersion m_LatestVersion;

	// Version of installed game
	CGameVersion m_CurVersion;

	/**
	 * Fetches release list from GitHub.
	 */
	void FetchReleaseList();

	/**
	 * Called once HTTP request finishes.
	 */
	void OnDataLoaded(CHttpClient::Response &resp);

	/**
	 * Called if new update was found.
	 */
	void OnUpdateFound();
};

#endif
