// Include it first so all the conflicting WinAPI crap will be undef'd later in VGUI2.h
#include "update_checker.h"

#include <algorithm>
#include <vector>
#include <sstream>
#include <nlohmann/json.hpp>
#include <zip.h>
#include <tier1/checksum_sha1.h>
#include <tier1/strtools.h>
#include <tier2/tier2.h>
#include <FileSystem.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/MessageBox.h>
#include "hud.h"
#include "cl_util.h"
#include "update_installer.h"
#include "update_dialogs.h"
#include "gameui/gameui_viewport.h"
#include "engine_patches.h"

static CUpdateInstaller s_Instance;

static ConVar update_download_sleep("update_download_sleep", "0", FCVAR_DEVELOPMENTONLY, "Waits for N ms when writing update.zip");
static ConVar update_download_error("update_download_error", "0", FCVAR_DEVELOPMENTONLY, "Throws an error when downloading update.zip");
static ConVar update_extract_sleep("update_extract_sleep", "0", FCVAR_DEVELOPMENTONLY, "Waits for N ms when extracting update.zip");
static ConVar update_extract_error("update_extract_error", "0", FCVAR_DEVELOPMENTONLY, "Throws an error when extracting update.zip");
static ConVar update_hash_sleep("update_hash_sleep", "0", FCVAR_DEVELOPMENTONLY, "Waits for N ms when hashing files");
static ConVar update_hash_error("update_hash_error", "0", FCVAR_DEVELOPMENTONLY, "Throws an error when hashing files");
static ConVar update_dry_run("update_dry_run", "0", FCVAR_DEVELOPMENTONLY, "Don't actually replace any files");
static ConVar update_no_quit("update_no_quit", "0", FCVAR_DEVELOPMENTONLY, "Don't quit the game after update");
static ConVar update_move_sleep("update_move_sleep", "0", FCVAR_DEVELOPMENTONLY, "Waits for N ms when moving files");
static ConVar update_move_error("update_move_error", "0", FCVAR_DEVELOPMENTONLY, "Throws an error when moving files");

namespace
{

// Hack for shutting down properly
bool s_bCanCallVGui2 = true;

/**
 * List of file extentions the need to always be replaced
 */
const char *g_szPlatFiles[] = {
	".exe", ".dll", ".pdb", ".so"
};

bool IsPlatFile(const fs::path &path)
{
	std::string ext = path.extension().u8string();

	for (size_t i = 0; i < std::size(g_szPlatFiles); i++)
	{
		if (g_szPlatFiles[i] == ext)
			return true;
	}

	return false;
}

template <typename R>
inline bool IsFutureReady(const std::future<R> &f)
{
	return f.valid() && f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

uint8_t HexCharToVal(char c)
{
	c = tolower(c);

	if (c >= '0' && c <= '9')
	{
		return c - '0';
	}
	else if (c >= 'a' && c <= 'f')
	{
		return 10 + (c - 'a');
	}
	else
	{
		throw std::invalid_argument("invalid char");
	}
}

std::vector<uint8_t> HexStringToBytes(const std::string &str)
{
	std::vector<uint8_t> data;

	if (str.size() % 2 != 0)
		throw std::invalid_argument("str contains odd number of chars");

	data.resize(str.size() / 2);

	for (size_t i = 0; i < str.size() / 2; i++)
	{
		data[i] = (HexCharToVal(str[2 * i + 1])) | (HexCharToVal(str[2 * i]) << 4);
	}

	return data;
}

std::string BytesToHexString(const std::vector<uint8_t> &data)
{
	std::ostringstream ss;

	for (uint8_t i : data)
	{
		ss << std::hex << std::setfill('0') << std::setw(2) << (int)i;
	}

	return ss.str();
}

std::vector<uint8_t> CalcFileSHA1(std::ifstream &file, unsigned long *pulFileSize = nullptr)
{
	// Based on CSHA1::HashFile
	CSHA1 hash;
	constexpr size_t MAX_FILE_READ_BUFFER = 8000;
	unsigned long ulFileSize = 0, ulRest = 0, ulBlocks = 0;
	unsigned long i = 0;
	unsigned char uData[MAX_FILE_READ_BUFFER];

	file.seekg(0, file.end);
	ulFileSize = file.tellg();
	file.seekg(0, file.beg);

	ulRest = ulFileSize % MAX_FILE_READ_BUFFER;
	ulBlocks = ulFileSize / MAX_FILE_READ_BUFFER;

	for (i = 0; i < ulBlocks; i++)
	{
		file.read((char *)uData, MAX_FILE_READ_BUFFER);
		hash.Update(uData, MAX_FILE_READ_BUFFER);
	}

	if (ulRest != 0)
	{
		file.read((char *)uData, ulRest);
		hash.Update(uData, ulRest);
	}

	hash.Final();
	std::vector<uint8_t> hashBytes(SHA1_HASH_SIZE);
	hash.GetHash(hashBytes.data());

	if (pulFileSize)
		*pulFileSize = ulFileSize;

	return hashBytes;
}

/**
 * Returns whether a path is in a subdirectory
 */
bool IsPathInSubdir(fs::path path, fs::path subdir)
{
	path = path.lexically_normal();
	subdir = subdir.lexically_normal();
	auto [rootEnd, nothing] = std::mismatch(subdir.begin(), subdir.end(), path.begin());

	if (rootEnd != subdir.end())
		return false;
	else
		return true;
}

}

CUpdateInstaller &CUpdateInstaller::Get()
{
	return s_Instance;
}

void CUpdateInstaller::Shutdown()
{
	s_bCanCallVGui2 = false;

	if (m_bIsInProcess)
	{
		// Can't use CancelInstallation since it calls to VGUI2
		// VGUI2 is shut down before this method is called (thanks, Valve)
		if (m_Status == Status::Installing)
		{
			// Wait for installation to succeed or fail
			while (m_bIsInProcess)
			{
				RunFrame();
				std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 30));
			}
		}
		else
		{
			if (m_Status == Status::Downloading)
			{
				CHttpClient::Get().AbortCurrentDownload();
			}
			if (m_Status != Status::Downloading && m_Status != Status::Confirming)
			{
				m_bAbortRequested = true;

				while (m_bAbortRequested)
					std::this_thread::yield();
			}

			m_Status = Status::None;
			CleanUp();
			m_bIsInProcess = false;
		}
	}
}

void CUpdateInstaller::RunFrame()
{
	if (!m_bIsInProcess)
		return;

	switch (m_Status)
	{
	case Status::None:
	case Status::Downloading:
		break;

	case Status::Extracting:
		UpdateExtractionStatus();
		break;
	case Status::Hashing:
		UpdateHashingStatus();
		break;
	case Status::Installing:
		UpdateInstallStatus();
		break;
	}
}

void CUpdateInstaller::StartUpdate()
{
	Assert(!m_bIsInProcess);
	Assert(!m_bAbortRequested);

	if (m_bIsInProcess)
	{
		ConPrintf(ConColor::Red, "Update Installer: already installing.\n");
		return;
	}

	m_bIsInProcess = true;
	m_bAbortRequested = false;
	m_InstallationPath.clear();
	m_TempDir.clear();
	m_UpdateDir.clear();
	m_CurrentMetadata.clear();
	m_NewMetadata.clear();
	m_Status = Status::None;
	m_AsyncResult = std::future<AsyncResult>();
	m_FileHashes.clear();
	m_FilesToAsk.clear();
	m_FilesToUpdate.clear();
	m_iFileToAskIdx = 0;

	if (!ReadMetadata())
		return;

	if (!PrepareTempDir())
		return;

	DownloadZipFile();
}

bool CUpdateInstaller::IsInProcess()
{
	return m_bIsInProcess;
}

void CUpdateInstaller::CancelInstallation()
{
	if (!m_bIsInProcess)
		return;

	ConPrintf("Update Installer: Abort requested\n");

	if (m_Status == Status::Installing)
	{
		ConPrintf("Update Installer: Can't abort file installation.\n");
		ConPrintf("Update Installer: Waiting for it to finish instead.\n");

		// Wait for installation to succeed or fail
		while (m_bIsInProcess)
		{
			RunFrame();
			std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 30));
		}

		return;
	}

	switch (m_Status)
	{
	case Status::None:
		return;
	case Status::Downloading:
		CHttpClient::Get().AbortCurrentDownload();
		CUpdateDownloadStatusDialog::Get()->Close();
		break;
	case Status::Extracting:
		CUpdateFileProgressDialog::Get()->Close();
		break;
	case Status::Hashing:
		CUpdateFileProgressDialog::Get()->Close();
		break;
	case Status::Confirming:
		CUpdateFileReplaceDialog::Get()->Close();
		break;
	}

	// When downloading, m_bAbortRequested is reset in main thread so we can't block
	// Conflict confirmation doesn't have async tasks
	if (m_Status != Status::Downloading && m_Status != Status::Confirming)
	{
		m_bAbortRequested = true;

		while (m_bAbortRequested)
			std::this_thread::yield();
	}

	ErrorOccured(g_pVGuiLocalize->Find("BHL_Update_UserCancel"));
	return;
}

bool CUpdateInstaller::ReadMetadata()
{
	try
	{
		// Locate installation path
		if (!g_pFullFileSystem->FileExists(UPDATE_METADATA_FILE))
		{
			ConPrintf("Update Installer: %s doesn't exist.\n", UPDATE_METADATA_FILE);
			ErrorOccured(g_pVGuiLocalize->Find("BHL_Update_MetadataNotFound"));
			return false;
		}

		char localPath[1024];

		if (!g_pFullFileSystem->GetLocalPath(UPDATE_METADATA_FILE, localPath, sizeof(localPath)))
		{
			ConPrintf("Update Installer: GetLocalPath failed.\n");
			ErrorOccured(g_pVGuiLocalize->Find("BHL_Update_MetadataNotFound"));
			return false;
		}

		fs::path path = fs::u8path(localPath);
		if (path.empty())
		{
			ConPrintf("Update Installer: path is empty.\n");
			ErrorOccured(g_pVGuiLocalize->Find("BHL_Update_MetadataNotFound"));
			return false;
		}

		gEngfuncs.Con_DPrintf("Update Installer: metadata found: %s.\n", path.u8string().c_str());

		// Check that mod directory is correct.
		fs::path modNamePath = path.parent_path().filename();
		std::string modName = modNamePath.u8string();

		const std::string ADDONS_SUFFIX = "_addons";

		if (modName.size() >= ADDONS_SUFFIX.size() && modName.substr(modName.size() - ADDONS_SUFFIX.size()) == ADDONS_SUFFIX)
		{
			// Strip _addons
			modName = modName.substr(0, modName.size() - ADDONS_SUFFIX.size());
		}

		gEngfuncs.Con_DPrintf("Update Installer: mod name: %s\n", modName.c_str());

		if (modName != gEngfuncs.pfnGetGameDirectory())
		{
			ConPrintf("Update Installer: invalid mod directory.\n");
			ConPrintf("Update Installer: expected '%s'\n", gEngfuncs.pfnGetGameDirectory());
			ConPrintf("Update Installer: got '%s'\n", modName.c_str());
			ErrorOccured(g_pVGuiLocalize->Find("BHL_Update_MetadataNotFound"));
			return false;
		}

		m_InstallationPath = path.parent_path();

		if (!fs::is_directory(m_InstallationPath))
		{
			ConPrintf("Update Installer: installation path is not a directory.\n");
			ConPrintf("Update Installer: path: %s\n", path.u8string().c_str());
			ErrorOccured(g_pVGuiLocalize->Find("BHL_Update_InternalError"));
			return false;
		}

		// Read metadata
		FileHandle_t metadataFile = g_pFullFileSystem->Open(UPDATE_METADATA_FILE, "r");

		if (metadataFile == FILESYSTEM_INVALID_HANDLE)
		{
			ConPrintf("Update Installer: failed to open metadata file " UPDATE_METADATA_FILE "\n");
			ErrorOccured(g_pVGuiLocalize->Find("BHL_Update_InternalError"));
			return false;
		}

		std::vector<char> metadataContents(g_pFullFileSystem->Size(metadataFile) + 1);
		int bytesRead = g_pFullFileSystem->Read(metadataContents.data(), metadataContents.size() - 1, metadataFile);
		metadataContents[bytesRead] = '\0';

		try
		{
			m_CurrentMetadata = nlohmann::json::parse(metadataContents.data());
		}
		catch (const std::exception &e)
		{
			ConPrintf("Update Installer: failed to parse metadata file: %s\n", e.what());
			ErrorOccured(g_pVGuiLocalize->Find("BHL_Update_MetadataCorrupted"));
			return false;
		}
	}
	catch (const std::exception &e)
	{
		ConPrintf("Update Installer: unexpected exception: %s\n", e.what());
		ErrorOccured(g_pVGuiLocalize->Find("BHL_Update_InternalError"));
		return false;
	}

	return true;
}

bool CUpdateInstaller::PrepareTempDir()
{
	Assert(m_TempDir.empty());

	try
	{
		fs::path basepath = fs::temp_directory_path();

		if (!fs::exists(basepath))
			throw std::runtime_error("temporary path doesn't exist");

		if (!fs::is_directory(basepath))
			throw std::runtime_error("temporary path is not a directory");

		fs::path path;

		// Try TEMP_DIR_NAME
		path = basepath / TEMP_DIR_NAME;
		if (fs::exists(path))
		{
			// Try TEMP_DIR_NAME-001...
			for (int i = 0; i < 1000; i++)
			{
				char buf[MAX_PATH];
				snprintf(buf, sizeof(buf), TEMP_DIR_NAME "-%03d", i);
				path = basepath / buf;

				if (!fs::exists(path))
					break;

				if (i == 999)
					throw std::runtime_error("temp path counter exhausted");
			}
		}

		// Create directory
		if (!fs::create_directory(path))
			throw std::runtime_error("temp path create dir failed");

		m_TempDir = path;
		Assert(m_TempDir.is_absolute());
	}
	catch (const std::exception &e)
	{
		ConPrintf("Update Installer: PrepareTempDir failed: %s\n", e.what());
		ErrorOccured(g_pVGuiLocalize->Find("BHL_Update_TempDirFailed"));
		return false;
	}

	ConPrintf("Update Installer: temp dir: %s\n", m_TempDir.u8string().c_str());
	return true;
}

void CUpdateInstaller::DownloadZipFile()
{
	try
	{
		const std::string &zipUrl = CUpdateChecker::Get().GetAssetURL();

		if (zipUrl.empty())
		{
			ConPrintf("Update Installer: assert URL is empty\n");
			ErrorOccured(g_pVGuiLocalize->Find("BHL_Update_NoAsset"));
			return;
		}

		ConPrintf("Update Installer: download %s\n", zipUrl.c_str());

		Assert(!m_ZipDownloadFile.is_open());

		if (m_ZipDownloadFile.is_open())
			m_ZipDownloadFile.close();

		fs::path zipPath = m_TempDir / "update.zip";
		m_ZipDownloadFile.open(zipPath, std::ios::out | std::ios::binary);

		if (m_ZipDownloadFile.fail())
		{
			ConPrintf("Update Installer: failed to open download file %s\n", zipPath.u8string().c_str());
			ConPrintf("Update Installer: errno: %s\n", strerror(errno));
			ErrorOccured(g_pVGuiLocalize->Find("BHL_Update_DLWriteOpenFailed"));
			return;
		}

		CHttpClient::Request request(zipUrl);

		request.SetWriteCallback([&](const char *pBuf, size_t iSize) -> size_t {
			if (!m_bIsInProcess)
				return 0;

			if (m_bAbortRequested)
			{
				m_bAbortRequested = false;
				return 0;
			}

			if (update_download_error.GetBool())
				return 0;

			m_ZipDownloadFile.write(pBuf, iSize);

			if (!m_ZipDownloadFile.good())
				return 0;

			if (update_download_sleep.GetInt() > 0)
				std::this_thread::sleep_for(std::chrono::milliseconds(update_download_sleep.GetInt()));

			return iSize;
		});

		request.SetCallback([&](CHttpClient::Response &response) {
			m_ZipDownloadFile.close();
			CUpdateDownloadStatusDialog::Get()->Close();

			if (!m_bIsInProcess)
				return;

			m_Status = Status::None;

			if (m_bAbortRequested)
			{
				m_bAbortRequested = false;
				return;
			}

			if (!response.IsSuccess())
			{
				ConPrintf("Update Installer: failed to download the file: %s\n", response.GetError().c_str());

				wchar_t wbuf[128];
				Q_UTF8ToWString(response.GetError().c_str(), wbuf, sizeof(wbuf));

				wchar_t wbuf2[256];
				g_pVGuiLocalize->ConstructString(wbuf2, sizeof(wbuf2),
				    g_pVGuiLocalize->Find("BHL_Update_DLFailed"), 1, wbuf);
				ErrorOccured(wbuf2);
				return;
			}

			ConPrintf("Update Installer: file downloaded\n");
			ExtractZipFile();
		});

		m_Status = Status::Downloading;
		auto pStatus = CHttpClient::Get().Get(request);
		CUpdateDownloadStatusDialog::Get()->SetStatus(pStatus);
		CUpdateDownloadStatusDialog::Get()->Activate();
	}
	catch (const std::exception &e)
	{
		ConPrintf("Update Installer: unexpected exception: %s\n", e.what());
		ErrorOccured(g_pVGuiLocalize->Find("BHL_Update_InternalError"));
		return;
	}
}

void CUpdateInstaller::ExtractZipFile()
{
	auto fnOpenArchive = [&](int flags, std::string &errorStr) -> zip_t * {
		fs::path zipPath = m_TempDir / "update.zip";

		zip_error_t error;
		zip_error_init(&error);

		zip_source_t *src = nullptr;

#ifdef PLATFORM_WINDOWS
		src = zip_source_win32w_create(zipPath.c_str(), 0, -1, &error);
#else
		src = zip_source_file_create(zipPath.c_str(), 0, -1, &error);
#endif

		if (!src)
		{
			errorStr = zip_error_strerror(&error);
			zip_error_fini(&error);
			return nullptr;
		}

		zip_t *zip = zip_open_from_source(src, flags, &error);

		if (!zip)
		{
			errorStr = zip_error_strerror(&error);
			zip_source_free(src);
			zip_error_fini(&error);
			return nullptr;
		}

		zip_error_fini(&error);
		return zip;
	};

	ConPrintf("Update Installer: extracting archive\n");

	std::string errorStr;
	zip_t *zip = fnOpenArchive(ZIP_CHECKCONS | ZIP_RDONLY, errorStr);

	if (!zip)
	{
		ConPrintf("Update Installer: failed to open archive: %s\n", errorStr.c_str());

		wchar_t wbuf[128];
		Q_UTF8ToWString(errorStr.c_str(), wbuf, sizeof(wbuf));

		wchar_t wbuf2[256];
		g_pVGuiLocalize->ConstructString(wbuf2, sizeof(wbuf2),
		    g_pVGuiLocalize->Find("BHL_Update_ZipOpenFailed"), 1, wbuf);
		ErrorOccured(wbuf2);
		return;
	}

	m_Status = Status::Extracting;
	m_FileProgress.iTotalFiles = 0;
	m_FileProgress.iFinishedFiles = 0;
	CUpdateFileProgressDialog::Get()->SetTitle("#BHL_Update_ExtractionTitle", true);
	CUpdateFileProgressDialog::Get()->SetCancelButtonVisible(true);
	CUpdateFileProgressDialog::Get()->Activate();

	m_AsyncResult = std::async([this, zip]() mutable -> AsyncResult {
		bool isSuccess = true;
		std::string error;
		m_UpdateDir.clear();

		try
		{
			fs::path tempDir = m_TempDir;
			fs::path unpackDir = tempDir / "update";

			if (!unpackDir.is_absolute())
				throw std::logic_error("unpack dir path is not absolute? how did that happen?");

			// Creates all directories in a relative path.
			// Checks that the path is safe and can't escape the unpack directory.
			auto fnSafeCreateDir = [&](fs::path path) {
				fs::path resultPath = unpackDir / path;
				resultPath = fs::absolute(resultPath).lexically_normal();

				if (!IsPathInSubdir(resultPath, unpackDir))
					throw std::runtime_error(std::string("path ") + path.u8string() + " is not safe");

				fs::create_directories(path);
			};

			m_FileProgress.iTotalFiles = zip_get_num_entries(zip, 0);

			for (int i = 0; i < zip_get_num_entries(zip, 0); i++)
			{
				if (m_bAbortRequested)
				{
					isSuccess = false;
					error = "aborted";
					break;
				}

				zip_stat_t sb;

				if (zip_stat_index(zip, i, 0, &sb) != 0)
				{
					isSuccess = false;
					error = "zip_stat_index failed";
					break;
				}

				{
					std::lock_guard<std::mutex> lock(m_AsyncMutex);
					m_FileProgress.filename = sb.name;
				}

				if (update_extract_error.GetBool())
					throw std::logic_error("Never Gonna Give You Up");

				if (update_extract_sleep.GetInt() > 0)
					std::this_thread::sleep_for(std::chrono::milliseconds(update_extract_sleep.GetInt()));

				int nameLen = strlen(sb.name);
				fs::path path = unpackDir / fs::u8path(sb.name);

				if (sb.name[nameLen - 1] == '/')
				{
					// A directory
					fnSafeCreateDir(path);
				}
				else
				{
					// A file
					fnSafeCreateDir(path.parent_path());

					if (fs::exists(path))
						throw std::runtime_error(std::string("file ") + path.u8string() + " already exists");

					zip_file_t *zipFile = zip_fopen_index(zip, i, 0);

					if (!zipFile)
						throw std::runtime_error(std::string("file ") + path.u8string() + ": zip_fopen_index failed");

					try
					{
						std::ofstream file;
						file.exceptions(std::ios::badbit | std::ios::failbit);
						file.open(path, std::ios::out | std::ios::binary);

						for (uint64_t size = 0; size != sb.size;)
						{
							char buf[128];
							int64_t len = zip_fread(zipFile, buf, sizeof(buf));

							if (len < 0)
								throw std::runtime_error(std::string("file ") + path.u8string() + ": zip_fread failed");

							file.write(buf, len);
							size += len;
						}

						file.close();
					}
					catch (...)
					{
						zip_fclose(zipFile);
						zipFile = nullptr;
						throw;
					}

					zip_fclose(zipFile);
					zipFile = nullptr;
				}

				m_FileProgress.iFinishedFiles++;
			}

			if (isSuccess)
			{
				m_UpdateDir = unpackDir;

				if (!fs::is_directory(m_UpdateDir))
					throw std::runtime_error(m_UpdateDir.u8string() + " is not a directory");

				if (fs::is_empty(m_UpdateDir))
					throw std::runtime_error(m_UpdateDir.u8string() + " is empty");

				fs::directory_iterator contents(m_UpdateDir);
				m_UpdateDir = contents->path() / "valve_addon";

				if (!fs::is_directory(m_UpdateDir))
					throw std::runtime_error(m_UpdateDir.u8string() + " is not a directory");
			}
		}
		catch (const std::exception &e)
		{
			isSuccess = false;
			error = e.what();
		}

		// Clean up
		zip_close(zip);
		zip = nullptr;

		// Return result
		AsyncResult result(isSuccess, error);

		// Abort handling
		if (m_bAbortRequested)
		{
			// Reset status since RunFrame won't be run after abort
			m_Status = Status::None;
			m_bAbortRequested = false;
		}

		return result;
	});
}

void CUpdateInstaller::UpdateExtractionStatus()
{
	if (IsFutureReady(m_AsyncResult))
	{
		m_Status = Status::None;
		CUpdateFileProgressDialog::Get()->DeletePanel();

		AsyncResult result = m_AsyncResult.get();
		m_AsyncResult = std::future<AsyncResult>();

		if (std::get<0>(result))
		{
			ConPrintf("Update Installer: extraction finished\n");
			if (!ReadUpdateMetadata())
				return;

			if (!PrepareFileList())
				return;

			HashFiles();
		}
		else
		{
			std::string &error = std::get<1>(result);
			ConPrintf("Update Installer: failed to extract archive: %s\n", error.c_str());

			wchar_t wbuf[128];
			Q_UTF8ToWString(error.c_str(), wbuf, sizeof(wbuf));

			wchar_t wbuf2[256];
			g_pVGuiLocalize->ConstructString(wbuf2, sizeof(wbuf2),
			    g_pVGuiLocalize->Find("BHL_Update_ZipExtractFailed"), 1, wbuf);
			ErrorOccured(wbuf2);
		}
	}
	else
	{
		// Update progress
		UpdateFileProgress pr;
		{
			std::lock_guard<std::mutex> lock(m_AsyncMutex);
			pr.iTotalFiles = m_FileProgress.iTotalFiles;
			pr.iFinishedFiles = m_FileProgress.iFinishedFiles;
			pr.filename = m_FileProgress.filename;
		}

		CUpdateFileProgressDialog::Get()->UpdateProgress(pr);
	}
}

bool CUpdateInstaller::ReadUpdateMetadata()
{
	ConPrintf("Update Installer: reading new metadata\n");

	try
	{
		std::ifstream metadataFile;
		metadataFile.exceptions(std::ios::badbit | std::ios::failbit);
		metadataFile.open(m_UpdateDir / UPDATE_METADATA_FILE);
		m_NewMetadata.clear();
		metadataFile >> m_NewMetadata;
	}
	catch (const std::exception &e)
	{
		ConPrintf("Update Installer: ReadUpdateMetadata failed: %s\n", e.what());
		ErrorOccured(g_pVGuiLocalize->Find("BHL_Update_NewMetaCorrupted"));
		return false;
	}

	return true;
}

bool CUpdateInstaller::PrepareFileList()
{
	ConPrintf("Update Installer: preparing file list\n");

	using nlohmann::json;

	try
	{
		for (auto &file : m_NewMetadata.at("files").get<json::object_t>())
		{
			const std::string &filename = file.first;
			FileHash hash;

			// Old file path
			hash.oldPath = m_InstallationPath / fs::u8path(filename);
			if (!IsPathInSubdir(hash.oldPath, m_InstallationPath))
				throw std::runtime_error("path " + filename + " is not safe");

			if (!fs::exists(hash.oldPath))
				hash.oldPath.clear();

			// New file path
			hash.newPath = m_UpdateDir / fs::u8path(filename);
			if (!IsPathInSubdir(hash.newPath, m_UpdateDir))
				throw std::runtime_error("path " + filename + " is not safe");

			if (!fs::exists(hash.newPath))
				throw std::runtime_error("file " + filename + " not found in the update");

			// Old file metadata hash and size
			{
				auto &files = m_CurrentMetadata.at("files");
				auto it = files.find(filename);
				if (it != files.end())
				{
					hash.metaOldHash = HexStringToBytes(it->at("hash_sha1").get<std::string>());
					hash.metaOldSize = it->at("size").get<uint64_t>();

					if (hash.metaOldHash.size() != SHA1_HASH_SIZE)
						throw std::runtime_error("file " + filename + " has invalid hash in old metadata");
				}
			}

			// New file metadata hash and size
			hash.metaNewHash = HexStringToBytes(file.second.at("hash_sha1").get<std::string>());
			hash.metaNewSize = file.second.at("size").get<uint64_t>();

			if (hash.metaNewHash.size() != SHA1_HASH_SIZE)
				throw std::runtime_error("file " + filename + " has invalid hash in new metadata");

			// Insert into the map
			m_FileHashes.insert({ filename, hash });
		}

		return true;
	}
	catch (const std::exception &e)
	{
		ConPrintf("Update Installer: PrepareFileList failed: %s\n", e.what());
		ErrorOccured(g_pVGuiLocalize->Find("BHL_Update_InternalError"));
		return false;
	}
}

void CUpdateInstaller::HashFiles()
{
	ConPrintf("Update Installer: hasing files\n");

	m_Status = Status::Hashing;
	m_FileProgress.iTotalFiles = 0;
	m_FileProgress.iFinishedFiles = 0;
	CUpdateFileProgressDialog::Get()->SetTitle("#BHL_Update_HashingTitle", true);
	CUpdateFileProgressDialog::Get()->SetCancelButtonVisible(true);
	CUpdateFileProgressDialog::Get()->Activate();

	m_AsyncResult = std::async([this]() mutable -> AsyncResult {
		bool isSuccess = true;
		std::string error;

		try
		{
			auto fnCalcFileHashAndSize = [](const fs::path &path, unsigned long &fileSize) {
				std::ifstream file;
				file.exceptions(std::ios::badbit | std::ios::failbit);
				file.open(path, std::ios::in | std::ios::binary);
				return CalcFileSHA1(file, &fileSize);
			};

			m_FileProgress.iTotalFiles = m_FileHashes.size();

			for (auto &file : m_FileHashes)
			{
				if (m_bAbortRequested)
				{
					isSuccess = false;
					error = "aborted";
					break;
				}

				{
					std::lock_guard<std::mutex> lock(m_AsyncMutex);
					m_FileProgress.filename = file.first;
				}

				if (!fs::exists(file.second.newPath))
					throw std::runtime_error("file " + file.second.newPath.u8string() + " doesn't exist");

				unsigned long fileSize = 0;

				file.second.realNewHash = fnCalcFileHashAndSize(file.second.newPath, fileSize);
				file.second.realNewSize = fileSize;

				if (!file.second.oldPath.empty())
				{
					if (!fs::exists(file.second.oldPath))
						throw std::runtime_error("file " + file.second.oldPath.u8string() + " doesn't exist");

					file.second.realOldHash = fnCalcFileHashAndSize(file.second.oldPath, fileSize);
					file.second.realOldSize = fileSize;
				}

				if (update_hash_sleep.GetInt() > 0)
					std::this_thread::sleep_for(std::chrono::milliseconds(update_hash_sleep.GetInt()));

				if (update_hash_error.GetBool())
					throw std::logic_error("Never Gonna Let You Down");

				m_FileProgress.iFinishedFiles++;
			}
		}
		catch (const std::exception &e)
		{
			isSuccess = false;
			error = e.what();
		}

		// Return result
		AsyncResult result(isSuccess, error);

		// Abort handling
		if (m_bAbortRequested)
		{
			// Reset status since RunFrame won't be run after abort
			m_Status = Status::None;
			m_bAbortRequested = false;
		}

		return result;
	});
}

void CUpdateInstaller::UpdateHashingStatus()
{
	if (IsFutureReady(m_AsyncResult))
	{
		m_Status = Status::None;
		CUpdateFileProgressDialog::Get()->DeletePanel();

		AsyncResult result = m_AsyncResult.get();
		m_AsyncResult = std::future<AsyncResult>();

		if (std::get<0>(result))
		{
			ConPrintf("Update Installer: hashing finished\n");

			if (!ValidateUpdateFiles())
				return;

			if (!m_FilesToAsk.empty())
				ShowReplaceDialog();
			else
				BeginFileUpdating();
		}
		else
		{
			std::string &error = std::get<1>(result);
			ConPrintf("Update Installer: failed to hash files: %s\n", error.c_str());

			wchar_t wbuf[128];
			Q_UTF8ToWString(error.c_str(), wbuf, sizeof(wbuf));

			wchar_t wbuf2[256];
			g_pVGuiLocalize->ConstructString(wbuf2, sizeof(wbuf2),
			    g_pVGuiLocalize->Find("BHL_Update_HashFailed"), 1, wbuf);
			ErrorOccured(wbuf2);
		}
	}
	else
	{
		// Update progress
		UpdateFileProgress pr;
		{
			std::lock_guard<std::mutex> lock(m_AsyncMutex);
			pr.iTotalFiles = m_FileProgress.iTotalFiles;
			pr.iFinishedFiles = m_FileProgress.iFinishedFiles;
			pr.filename = m_FileProgress.filename;
		}
		CUpdateFileProgressDialog::Get()->UpdateProgress(pr);
	}
}

bool CUpdateInstaller::ValidateUpdateFiles()
{
	// Validate downloaded files
	ConPrintf("Update Installer: validating downloaded files\n");
	for (auto &i : m_FileHashes)
	{
		if (i.second.realNewHash.empty())
		{
			ConPrintf("Update Installer: %s new hash is empty\n", i.first.c_str());
			ErrorOccured(g_pVGuiLocalize->Find("BHL_Update_InternalError"));
			return false;
		}

		if (i.second.realNewHash != i.second.metaNewHash)
		{
			ConPrintf("Update Installer: %s hash mismatch\n", i.first.c_str());
			ConPrintf("Update Installer: expected %s\n", BytesToHexString(i.second.metaNewHash).c_str());
			ConPrintf("Update Installer: got %s\n", BytesToHexString(i.second.realNewHash).c_str());

			wchar_t wbuf[MAX_PATH];
			Q_UTF8ToWString(i.first.c_str(), wbuf, sizeof(wbuf));

			wchar_t wbuf2[2048];
			g_pVGuiLocalize->ConstructString(wbuf2, sizeof(wbuf2),
			    g_pVGuiLocalize->Find("BHL_Update_HashMismatch"), 1, wbuf);
			ErrorOccured(wbuf2);
			return false;
		}
	}

	// Create a list of files to show the user
	for (auto &i : m_FileHashes)
	{
		// Don't replace if file in FS is same as in update
		if (!i.second.realOldHash.empty() && i.second.metaNewHash == i.second.realOldHash)
			continue;

		bool isPlatFile = !i.second.oldPath.empty() && IsPlatFile(i.second.oldPath);

		// If file was updated OR a platform file
		if (isPlatFile || i.second.metaOldHash.empty() || i.second.metaOldHash != i.second.metaNewHash)
		{
			bool needToAsk = false;

			if (isPlatFile)
			{
				// Platform files are always replaced
				needToAsk = false;
			}
			else if (i.second.metaOldHash.empty())
			{
				// File was added in new version

				if (i.second.realOldHash.empty())
				{
					// File doesn't exist in FS
					needToAsk = false;
				}
				else
				{
					// File exists in FS
					// Only ask if it's different from updated file
					needToAsk = (i.second.metaNewHash != i.second.realOldHash);
				}
			}
			else
			{
				// File was updated
				// Only ask if it was maually edited AND it's different from new file
				needToAsk = (i.second.metaOldHash != i.second.realOldHash) && (i.second.metaNewHash != i.second.realOldHash);
			}

			if (needToAsk)
			{
				// Ask the user to confirm overwrite
				m_FilesToAsk.push_back(i.first);
				ConPrintf(ConColor::Yellow, "Update Installer: %s manually edited\n", i.first.c_str());

				gEngfuncs.Con_DPrintf("Old file in meta: %s\n", BytesToHexString(i.second.metaOldHash).c_str());
				gEngfuncs.Con_DPrintf("Old file in fs: %s\n", BytesToHexString(i.second.realOldHash).c_str());
				gEngfuncs.Con_DPrintf("New file: %s\n", BytesToHexString(i.second.metaNewHash).c_str());
			}
			else
			{
				// Update the file
				m_FilesToUpdate.push_back(i.first);
				ConPrintf("Update Installer: %s updated\n", i.first.c_str());
			}
		}
	}

	return true;
}

void CUpdateInstaller::ShowReplaceDialog()
{
	Assert(!m_FilesToAsk.empty());
	m_iFileToAskIdx = 0;
	m_Status = Status::Confirming;
	CUpdateFileReplaceDialog::Get()->Activate(m_FilesToAsk[m_iFileToAskIdx]);
}

void CUpdateInstaller::ShowNextReplaceDialog(bool replace, bool forAll)
{
	size_t maxIdx = 0;

	if (forAll)
		maxIdx = m_FilesToAsk.size();
	else
		maxIdx = m_iFileToAskIdx + 1;

	for (size_t i = m_iFileToAskIdx; i < maxIdx; i++)
	{
		if (replace)
		{
			m_FilesToUpdate.push_back(m_FilesToAsk[i]);
			ConPrintf(ConColor::Yellow, "Update Installer: %s will be replaced\n", m_FilesToAsk[i].c_str());
		}
		else
		{
			ConPrintf("Update Installer: %s will be kept as is\n", m_FilesToAsk[i].c_str());
		}
	}

	m_iFileToAskIdx++;

	if (forAll || m_iFileToAskIdx == m_FilesToAsk.size())
	{
		// All files processed
		CUpdateFileReplaceDialog::Get()->DeletePanel();
		m_Status = Status::None;
		m_iFileToAskIdx = 0;
		BeginFileUpdating();
	}
	else
	{
		// Show next dialog
		CUpdateFileReplaceDialog::Get()->Activate(m_FilesToAsk[m_iFileToAskIdx]);
	}
}

void CUpdateInstaller::BeginFileUpdating()
{
	ConPrintf("Update Installer: installing updated files\n");

	bool isDryRun = update_dry_run.GetBool();

	if (isDryRun)
	{
		ConPrintf(ConColor::Cyan, "Update Installer: Dry run enabled. No files will be copied.\n");
	}

	m_Status = Status::Installing;
	m_FileProgress.iTotalFiles = m_FilesToUpdate.size();
	m_FileProgress.iFinishedFiles = 0;
	CUpdateFileProgressDialog::Get()->SetTitle("#BHL_Update_InstallingTitle", true);
	CUpdateFileProgressDialog::Get()->SetCancelButtonVisible(false);
	CUpdateFileProgressDialog::Get()->Activate();
	CEnginePatches::Get().DisableExitCommands();

	m_AsyncResult = std::async([this, isDryRun]() mutable -> AsyncResult {
		bool isSuccess = true;
		std::string error;

		// Copy metadata file (not included in the list)
		if (!isDryRun)
		{
			try
			{
				fs::path metadataFileName = fs::u8path(UPDATE_METADATA_FILE);
				fs::rename(m_UpdateDir / metadataFileName, m_InstallationPath / metadataFileName);
			}
			catch (const std::exception &e)
			{
				isSuccess = false;
				error = std::string(UPDATE_METADATA_FILE) + ": " + e.what();
			}
		}

		// Copy files that need to be copied
		if (isSuccess)
		{
			for (std::string &name : m_FilesToUpdate)
			{
				try
				{
					{
						std::lock_guard<std::mutex> lock(m_AsyncMutex);
						m_FileProgress.filename = name;
					}

					FileHash &hash = m_FileHashes.at(name);

					if (!isDryRun)
					{
						fs::path oldPath = hash.oldPath;
						if (oldPath.empty())
							oldPath = m_InstallationPath / fs::u8path(name);

						if (!hash.oldPath.empty() && Plat_IsSpecialFile(oldPath))
						{
							Plat_CopySpecialFile(hash.newPath, oldPath);
						}
						else
						{
							fs::rename(hash.newPath, oldPath);
						}
					}

					if (update_move_sleep.GetInt() > 0)
						std::this_thread::sleep_for(std::chrono::milliseconds(update_move_sleep.GetInt()));

					if (update_move_error.GetBool())
						throw std::logic_error("Never Gonna Run Around And Desert You");

					m_FileProgress.iFinishedFiles++;
				}
				catch (const std::exception &e)
				{
					isSuccess = false;
					error = name + ": " + e.what();
					break;
				}
			}
		}

		// Return result
		AsyncResult result(isSuccess, error);

		// Abort handling
		if (m_bAbortRequested)
		{
			// Reset status since RunFrame won't be run after abort
			m_Status = Status::None;
			m_bAbortRequested = false;
		}

		return result;
	});
}

void CUpdateInstaller::UpdateInstallStatus()
{
	if (IsFutureReady(m_AsyncResult))
	{
		m_Status = Status::None;

		if (s_bCanCallVGui2)
			CUpdateFileProgressDialog::Get()->DeletePanel();

		AsyncResult result = m_AsyncResult.get();
		m_AsyncResult = std::future<AsyncResult>();

		CEnginePatches::Get().EnableExitCommands();

		if (std::get<0>(result))
		{
			ConPrintf("Update Installer: all files installed\n");
			ConPrintf("Update Installer: update finished successfully\n");

			CleanUp();
			m_bIsInProcess = false;

			// Quit the game
			if (!update_no_quit.GetBool())
				gEngfuncs.pfnClientCmd("quit\n");
		}
		else
		{
			std::string &error = std::get<1>(result);
			ConPrintf("Update Installer: failed to install files:\n%s\n", error.c_str());

			if (s_bCanCallVGui2)
			{
				ErrorOccured(g_pVGuiLocalize->Find("BHL_Update_InternalError2"));
			}
			else
			{
				// Hack for cancelling on shutdown
				CleanUp();
				m_bIsInProcess = false;
			}
		}
	}
	else if (s_bCanCallVGui2)
	{
		// Update progress
		UpdateFileProgress pr;
		{
			std::lock_guard<std::mutex> lock(m_AsyncMutex);
			pr.iTotalFiles = m_FileProgress.iTotalFiles;
			pr.iFinishedFiles = m_FileProgress.iFinishedFiles;
			pr.filename = m_FileProgress.filename;
		}
		CUpdateFileProgressDialog::Get()->UpdateProgress(pr);
	}
}

void CUpdateInstaller::ErrorOccured(const wchar_t *str)
{
	Assert(m_bIsInProcess);
	Assert(!m_bAbortRequested);

	if (!m_bIsInProcess)
	{
		char buf[1024];
		Q_WStringToUTF8(str, buf, sizeof(buf));
		ConPrintf("Update Installer: updater not running\n");
		ConPrintf("Update Installer: original error: %s\n", buf);

		str = g_pVGuiLocalize->Find("#BHL_Update_InternalError");
	}

	if (m_bIsInProcess)
		CleanUp();

	m_bIsInProcess = false;

	vgui2::MessageBox *pMsg = new vgui2::MessageBox(g_pVGuiLocalize->Find("#BHL_Update_ErrorTitle"), str);
	pMsg->Activate();
	pMsg->SetScheme(CGameUIViewport::Get()->GetScheme());
}

void CUpdateInstaller::CleanUp() noexcept
{
	Assert(m_Status == Status::None);
	Assert(m_bIsInProcess);
	Assert(!m_bAbortRequested);

	ConPrintf("Update Installer: cleaning up\n");

	m_Status = Status::None;
	m_FilesToAsk.clear();
	m_FilesToUpdate.clear();
	m_FileHashes.clear();

	try
	{
		if (m_ZipDownloadFile.is_open())
			m_ZipDownloadFile.close();
	}
	catch (...)
	{
		// Do nothing
	}

	m_NewMetadata.clear();
	m_CurrentMetadata.clear();

	if (!m_TempDir.empty())
	{
		try
		{
			// Remove temporary directory
			fs::remove_all(m_TempDir);
		}
		catch (...)
		{
			// Do nothing
		}
	}

	m_UpdateDir.clear();
	m_TempDir.clear();
	m_InstallationPath.clear();
}
