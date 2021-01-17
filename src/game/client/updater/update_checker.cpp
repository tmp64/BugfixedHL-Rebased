#include <nlohmann/json.hpp>
#include <bhl_urls.h>
#include <appversion.h>
#include "hud.h"
#include "cl_util.h"
#include "update_checker.h"
#include "update_dialogs.h"

static CUpdateChecker s_Instance;

ConVar cl_check_for_updates("cl_check_for_updates", "1", FCVAR_BHL_ARCHIVE, "Check for updates on game launch.");

CON_COMMAND(update_check, "Check for updates")
{
	if (CUpdateChecker::Get().IsInProgress())
	{
		ConPrintf("Update check is in progress.\n");
		return;
	}

	CUpdateChecker::Get().CheckForUpdates();
	ConPrintf("Checking... You will be notified if there is an update.\n");
}

CON_COMMAND(update_changelog, "Show update changelog")
{
	if (CUpdateChecker::Get().IsInProgress())
	{
		ConPrintf("Update check is in progress.\n");
		return;
	}

	if (!CUpdateChecker::Get().IsUpdateFound())
	{
		ConPrintf("Client is on the latest version.\n");
		return;
	}

	// Maximum chars that can be printed to the console in one call.
	constexpr size_t MAX_CON_OUT = 1024;
	const std::string &log = CUpdateChecker::Get().GetChangelog();

	for (size_t i = 0; i < log.size(); i += MAX_CON_OUT)
	{
		ConPrintf("%s", log.data() + i);
	}
}

static const char *VerToString(const CGameVersion &ver)
{
	static char buf[128];
	int major, minor, patch;
	ver.GetVersion(major, minor, patch);
	snprintf(buf, sizeof(buf), "%d.%d.%d", major, minor, patch);
	return buf;
}

CUpdateChecker &CUpdateChecker::Get()
{
	return s_Instance;
}

void CUpdateChecker::Init()
{
	const char *verstr = APP_VERSION;

	char *overrideVer = nullptr;
	if (gEngfuncs.CheckParm("-bhl_ver_override", &overrideVer))
	{
		verstr = overrideVer;
	}

	bool result = m_CurVersion.TryParse(verstr);

	if (!result)
	{
		ConPrintf(ConColor::Red, "Failed to parse app version string \"%s\"\n", verstr);
		Assert(m_CurVersion.TryParse("0.0.0-error+error.eeeeeee"));
	}
}

void CUpdateChecker::RunFrame()
{
	// Wait for config to execute
	static int frameDelay = 5;
	if (frameDelay > 0)
	{
		frameDelay--;
	}
	else if (frameDelay == 0)
	{
		frameDelay--;

		// Config ready
		if (cl_check_for_updates.GetBool())
			CheckForUpdates();
	}
}

bool CUpdateChecker::IsInProgress()
{
	return m_bInProgress;
}

bool CUpdateChecker::IsUpdateFound()
{
	return m_bUpdateFound;
}

const CGameVersion &CUpdateChecker::GetCurVersion()
{
	return m_CurVersion;
}

const CGameVersion &CUpdateChecker::GetLatestVersion()
{
	return m_LatestVersion;
}

const std::string &CUpdateChecker::GetChangelog()
{
	return m_Changelog;
}

const std::string &CUpdateChecker::GetAssetURL()
{
	return m_ZipURL;
}

void CUpdateChecker::CheckForUpdates()
{
	FetchReleaseList();
}

void CUpdateChecker::FetchReleaseList()
{
	if (m_bInProgress)
		return;

	m_bInProgress = true;
	m_bUpdateFound = false;
	m_Changelog.clear();

	char url[1024] = BHL_GITHUB_API "releases";

	char *overrideUrl = nullptr;
	if (gEngfuncs.CheckParm("-bhl_api_url", &overrideUrl))
	{
		snprintf(url, sizeof(url), "%s/releases", overrideUrl);
	}

	CHttpClient::Request req(url);
	req.AddHeader("Accept: application/vnd.github.v3+json");
	req.AddHeader("User-Agent: tmp64-BugfixedHL-Rebased");
	req.SetCallback([this](CHttpClient::Response &response) {
		OnDataLoaded(response);
	});
	CHttpClient::Get().Get(req);
}

void CUpdateChecker::OnDataLoaded(CHttpClient::Response &resp)
{
	m_bInProgress = false;

	try
	{
		if (!resp.IsSuccess())
			throw std::runtime_error("request failed: " + resp.GetError());

		std::string releasesJsonString(resp.GetResponseData().data(), resp.GetResponseData().size());
		nlohmann::json releases = nlohmann::json::parse(releasesJsonString);

		if (!releases.is_array())
			throw std::runtime_error("response is not an array");

		if (releases.empty())
		{
			// No releases, this one is definitely the latest.
			return;
		}

		CGameVersion latestVersion;
		std::string changelog;
		bool updateFound = false;
		nlohmann::json latestRelease;

		for (const nlohmann::json &release : releases)
		{
			// Skip drafts
			if (release.at("draft").get<bool>())
				continue;

			CGameVersion version;
			const std::string &name = release.at("name").get<std::string>();
			const std::string &tag = release.at("tag_name").get<std::string>();

			// tag_name: 'v1.0.0'
			// substr to remove 'v' prefix.
			// Minimum size is 6: v1.0.0
			if (tag.size() < 6 || !version.TryParse(tag.substr(1).c_str()))
			{
				gEngfuncs.Con_DPrintf("Update Checker: release '%s' has invalid tag name '%s'\n", name.c_str(), tag.c_str());
				continue;
			}

			// Set latest version if not set
			if (!latestVersion.IsValid())
			{
				// First non-draft release is the latest
				latestVersion = version;
				latestRelease = release;
			}

			if (version <= m_CurVersion)
			{
				// Versions are sorted. Break on first version older or equal to current.
				break;
			}

			updateFound = true;
			changelog += name + "\n\n";
			changelog += release.at("body").get<std::string>() + "\n\n\n";
		}

		m_bUpdateFound = updateFound;
		m_Changelog = std::move(changelog);
		m_LatestVersion = latestVersion;

		// Select asset
		m_ZipURL.clear();
		const nlohmann::json &assets = latestRelease.at("assets");

		for (auto &asset : assets)
		{
			const char *platformName = nullptr;

			if (IsWindows())
				platformName = "windows";
			else if (IsLinux())
				platformName = "linux";
			else
				Assert(false);

			const std::string &name = asset.at("name").get<std::string>();
			if (name.find("client") != name.npos && name.find(platformName) != name.npos)
			{
				m_ZipURL = asset.at("browser_download_url").get<std::string>();
				break;
			}
		}

		if (updateFound)
		{
			OnUpdateFound();
		}
		else
		{
			ConPrintf("Current version: %s\n", VerToString(m_CurVersion));
			ConPrintf("Latest version: %s\n", VerToString(m_LatestVersion));
			ConPrintf("Game is up to date.\n");
		}
	}
	catch (const std::exception &e)
	{
		ConPrintf(ConColor::Red, "Update checker failed to load list of releases.\n");
		ConPrintf(ConColor::Red, "%s\n", e.what());
		return;
	}
}

void CUpdateChecker::OnUpdateFound()
{
	// Print to console
	console::SetColor(Color(0, 255, 0, 255));
	ConPrintf("\n");
	ConPrintf("************************************************\n");
	ConPrintf("* A new update was released.\n");
	ConPrintf("* Your version: %s\n", VerToString(m_CurVersion));
	ConPrintf("* New version:  %s\n", VerToString(m_LatestVersion));
	ConPrintf("************************************************\n");
	ConPrintf("* Type 'update_changelog' to see what's new.\n");
	ConPrintf("* " BHL_GITHUB_URL "\n");
	ConPrintf("************************************************\n");
	ConPrintf("\n");
	console::ResetColor();

	// Show dialog
	CUpdateNotificationDialog::Get()->Activate();
}
