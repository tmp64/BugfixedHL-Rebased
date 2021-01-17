#ifndef UPDATER_UPDATE_INSTALLER_H
#define UPDATER_UPDATE_INSTALLER_H
#include <atomic>
#include <filesystem>
#include <fstream>
#include <future>
#include <map>
#include <vector>
#include <nlohmann/json.hpp>

#define UPDATE_METADATA_FILE "bugfixedhl_install_metadata.dat"
#define TEMP_DIR_NAME        "BugfixedHL-Update"

constexpr size_t SHA1_HASH_SIZE = 160 / 8; // 160 bits

namespace fs = std::filesystem;

struct UpdateFileProgress
{
	int iFinishedFiles = 0;
	int iTotalFiles = 0;
	std::string filename;
};

class CUpdateInstaller
{
public:
	static CUpdateInstaller &Get();

	/**
	 * Stops all async tasks.
	 */
	void Shutdown();

	/**
	 * Does per-frame processing.
	 */
	void RunFrame();

	/**
	 * Begins update installation procedure.
	 */
	void StartUpdate();

	/**
	 * Returns whether update installation is in process.
	 */
	bool IsInProcess();

	/**
	 * Cancels the installation.
	 */
	void CancelInstallation();

private:
	using AsyncResult = std::tuple<bool, std::string>;

	/**
	 * Enum of all async statuses.
	 */
	enum class Status
	{
		None,
		Downloading,
		Extracting,
		Hashing,
		Confirming,
		Installing,
	};

	struct AtomicFileProgress
	{
		std::atomic_int iFinishedFiles = 0;
		std::atomic_int iTotalFiles = 0;
		std::string filename;
	};

	struct FileHash
	{
		// Path to the old file (in gamedir) - can be empty
		fs::path oldPath;

		// Path to the new file (from update)
		fs::path newPath;

		// Hash and size of the file from old metadata - can be empty
		std::vector<uint8_t> metaOldHash;
		uint64_t metaOldSize = 0;

		// Hash and size of the file from new metadata
		std::vector<uint8_t> metaNewHash;
		uint64_t metaNewSize = 0;

		// Hash and size of the old file from FS - can be empty
		std::vector<uint8_t> realOldHash;
		uint64_t realOldSize = 0;

		// Hash and size of the new file from FS
		std::vector<uint8_t> realNewHash;
		uint64_t realNewSize = 0;
	};

	std::atomic_bool m_bIsInProcess = false;
	std::atomic_bool m_bAbortRequested = false;

	fs::path m_InstallationPath;
	fs::path m_TempDir;
	fs::path m_UpdateDir;
	nlohmann::json m_CurrentMetadata;
	nlohmann::json m_NewMetadata;
	Status m_Status = Status::None;
	std::future<AsyncResult> m_AsyncResult;
	std::mutex m_AsyncMutex;
	std::map<std::string, FileHash> m_FileHashes;

	// Downloading
	std::ofstream m_ZipDownloadFile;

	// Extracting and hashing
	AtomicFileProgress m_FileProgress;

	// Installing new files
	// List of files that need to be updated
	std::vector<std::string> m_FilesToUpdate;

	// List of files that user needs to be asked about
	std::vector<std::string> m_FilesToAsk;

	size_t m_iFileToAskIdx = 0;

	/**
	 * Locates update installation directory.
	 * Reads installed client metadata.
	 */
	bool ReadMetadata();

	/**
	 * Creates required temporary directories.
	 */
	bool PrepareTempDir();

	/**
	 * Takes asset name from update checker and downloads it into the temp dir.
	 */
	void DownloadZipFile();

	/**
	 * Extracts contents of the zip file into the temp dir.
	 */
	void ExtractZipFile();

	/**
	 * Runs every frame to check if extraction has finished or updates the dialog.
	 */
	void UpdateExtractionStatus();

	/**
	 * Reads metadata of new update.
	 */
	bool ReadUpdateMetadata();

	/**
	 * Fills m_FileHashes with file names.
	 */
	bool PrepareFileList();

	/**
	 * Hashes new and existing files - updates m_FileHashes.
	 */
	void HashFiles();

	/**
	 * Runs every frame to check if hashing has finished or updates the dialog.
	 */
	void UpdateHashingStatus();

	/**
	 * Checks that hashes of new files match with metadata.
	 */
	bool ValidateUpdateFiles();

	/**
	 * Shows a dialog that asks the user to confirm overwriting a file.
	 */
	void ShowReplaceDialog();

	/**
	 * Shows next confirmation dialog
	 */
	void ShowNextReplaceDialog(bool replace, bool forAll);

	/**
	 * Starts an async task to replace all files
	 */
	void BeginFileUpdating();

	/**
	 * Runs every frame to check if installing has finished or updates the dialog.
	 */
	void UpdateInstallStatus();

	/**
	 * Shows an error dialog and cancels the update. Can only be called from main thread.
	 */
	void ErrorOccured(const wchar_t *str);

	/**
	 * Removes temporary files, frees allocated resources.
	 */
	void CleanUp() noexcept;

	/**
	 * Whether or not a file is special (e.g. locked .dll or .exe)
	 */
	bool Plat_IsSpecialFile(const fs::path &path);

	/**
	 * Copies a special file.
	 */
	void Plat_CopySpecialFile(const fs::path &from, const fs::path &to);

	friend class CUpdateFileReplaceDialog;
};

#endif
