/***
*
*	Copyright (c) 2020, AGHL.RU. All rights reserved.
*
****/
//
// results.h
//
// Functions for storing game results files.
//
#ifndef RESULTS_H
#define RESULTS_H
// Another caveat of compiling for for i386 with a 10.13 SDK, no access to filesystem - they're very slow to adapt
#ifndef PLATFORM_MACOS
#include <filesystem>
#else
#endif
#include <tier0/platform.h>

class CResults
{
public:
	static CResults &Get();

	/**
	 * Results initialization.
	 */
	void Init();

	/**
	 * Monitors MaxClients changes to do results stopping, also monitors demo recording.
	 * Called every frame.
	 */
	void Frame();

	/**
	 * Starts demo recording after map initialized
	 * Called every CHud::Think.
	 */
	void Think();

	/**
	 * Opens log file and adds text there if results are started.
	 */
	void AddLog(const char *text, bool chat);

private:
	// Contains path to gamedir with a trailing path separator
	// e.g. "d:\\games\\half-life\\valve\\"
	char m_szFullGameDirPath[MAX_PATH] = "";
#ifndef PLATFORM_MACOS
	std::filesystem::path m_fsFullGameDirPath;
#else
#endif

	char m_szTempDemoList[MAX_PATH] = "";
	char m_szCurrentResultsDemo[MAX_PATH] = "";
	char m_szCurrentResultsLog[MAX_PATH] = "";
	char m_szCurrentResultsStats[MAX_PATH] = "";
	char m_szCurrentMap[MAX_PATH] = "";

	bool m_bResultsStarted = false;
	int m_iLastMaxClients = 0;

	bool m_bDemoRecording = false;
	bool m_bDemoRecordingStartIssued = false;
	int m_bDemoRecordingFrame = 0;

	FILE *m_pLogFile = nullptr;

	/**
	 * Starts results.
	 */
	void Start();

	/**
	 * Stops log and demo recordings.
	 */
	void Stop();

	/**
	 * Closes opened results files.
	 */
	void CloseFiles();

	/**
	 * Starts demo recording.
	 */
	void StartDemoRecording();

	/**
	 * Purges old demos.
	 */
	void PurgeDemos();

	/**
	 * Creates path to a file in results/<date>/<map>-<date and time>.<extension>
	 * @param	extension	File extension.
	 * @param	filename	Output for filename realtive to gamedir. Must be of size MAX_PATH.
	 * @param	fullpath	Output for full path to the file. Must be of size MAX_PATH.
	 */
	bool GetResultsFilename(const char *extension, char *filename, char *fullpath);

	/**
	 * Creates directories up to and including relPath in gamedir.
	 * @param	relPath		Relative path from gamedir.
	 */
#ifndef PLATFORM_MACOS
	void CreateDirectoryStructure(std::filesystem::path relPath);
#else
#endif
};

#endif
