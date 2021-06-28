/***
*
*	Copyright (c) 2020, AGHL.RU. All rights reserved.
*
****/
//
// results.cpp
//
// Functions for storing game results files.
//

#include <ctime>

#ifdef PLATFORM_WINDOWS

#include <winsani_in.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <winsani_out.h>

#endif

#include <tier1/strtools.h>
#include <tier2/tier2.h>
#include <FileSystem.h>
#include <demo_api.h>
#include "hud.h"
#include "cl_util.h"
#include "engine_patches.h"
#include "results.h"

static CResults s_Results;

#if HAS_STD_FILESYSTEM
// File format passed to strftime
// <map> will be replaced with mapname
static constexpr char FILENAME_FORMAT[] = "results/%Y-%m/<map>-%Y%m%d-%H%M%S";

static ConVar results_demo_autorecord("results_demo_autorecord", "0", FCVAR_BHL_ARCHIVE, "Record demos when joining a server");
static ConVar results_demo_keepdays("results_demo_keepdays", "14", FCVAR_BHL_ARCHIVE, "Days to keep automatically recorded demos");
static ConVar results_log_chat("results_log_chat", "0", FCVAR_BHL_ARCHIVE, "Enable chat logging into a file");
static ConVar results_log_other("results_log_other", "0", FCVAR_BHL_ARCHIVE, "Enable other messages (like kill messages and others in the console) logging into a file");
#endif

CResults &CResults::Get()
{
	return s_Results;
}

void CResults::Init()
{
#if HAS_STD_FILESYSTEM
	HookCommand("agrecord", []() { CResults::Get().StartDemoRecording(); });

	char buf[MAX_PATH];
	if (!gEngfuncs.COM_ExpandFilename("liblist.gam", buf, sizeof(buf)))
	{
		ConPrintf(ConColor::Red, "Results: Failed to expand filename of liblist.gam.\n");
		return;
	}

	V_ExtractFilePath(buf, m_szFullGameDirPath, sizeof(m_szFullGameDirPath));

#ifdef PLATFORM_WINDOWS
	if (!CEnginePatches::Get().IsSDLEngine())
	{
		// Get language name from the registry, because older engines write demos in languaged directory
		HKEY rKey;
		char value[MAX_PATH];
		DWORD valueSize = sizeof(value);
		if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_QUERY_VALUE, &rKey) == ERROR_SUCCESS)
		{
			if (RegQueryValueEx(rKey, "language", NULL, NULL, (unsigned char *)value, &valueSize) == ERROR_SUCCESS && value[0] && strcmp(value, "english") != 0 && strlen(m_szFullGameDirPath) + strlen(value) + 1 < MAX_PATH - 1)
			{
				m_szFullGameDirPath[strlen(m_szFullGameDirPath) - 1] = '\0';
				V_strcat_safe(m_szFullGameDirPath, "_");
				V_strcat_safe(m_szFullGameDirPath, value);
				m_szFullGameDirPath[strlen(m_szFullGameDirPath) - 1] = CORRECT_PATH_SEPARATOR;
			}
			RegCloseKey(rKey);
		}
	}
#endif

	m_fsFullGameDirPath = std::filesystem::u8path(m_szFullGameDirPath);

	// Get temporary demos file list name and purge old demos
	snprintf(m_szTempDemoList, sizeof(m_szTempDemoList), "%stempdemolist.txt", m_szFullGameDirPath);
	PurgeDemos();
#endif
}

void CResults::Frame()
{
#if HAS_STD_FILESYSTEM
	int maxClients = gEngfuncs.GetMaxClients();
	if (maxClients != m_iLastMaxClients)
	{
		// Connection state changed, do results stop, besides if they are started (this is needed due to agrecord command)
		Stop();

		// Store maxClients value for later usages in other functions
		m_iLastMaxClients = maxClients;

		// Startup results here, but without demo recording
		if (maxClients > 1)
		{
			Start();
		}
	}

	// Demo watchdog: If user stopped demo that was autorecorded, clean mark so we don't stop the demo later
	if (m_bDemoRecording)
	{
		if (m_bDemoRecordingFrame < 10)
		{
			m_bDemoRecordingFrame++;
		}
		else if (!gEngfuncs.pDemoAPI->IsRecording())
		{
			m_bDemoRecording = false;
		}
	}
#endif
}

void CResults::Think()
{
#if HAS_STD_FILESYSTEM
	// Do start, but not in single-player and only once
	if (m_iLastMaxClients > 1 && !m_bDemoRecordingStartIssued && !gHUD.m_iIntermission)
	{
		m_bDemoRecordingStartIssued = true;

		// Start demo auto-recording if engine doesn't record a demo already by some direct user request.
		if (results_demo_autorecord.GetBool() && !gEngfuncs.pDemoAPI->IsRecording())
		{
			StartDemoRecording();
		}
	}
#endif
}

void CResults::AddLog(const char *text, bool chat)
{
#if HAS_STD_FILESYSTEM
	// Check that we need to log this type of event
	if (chat && !results_log_chat.GetBool() || !chat && !results_log_other.GetBool())
		return;

	// Check that results are started
	if (!m_bResultsStarted)
		return;

	// Open log file in case it is not already
	if (!m_pLogFile)
		m_pLogFile = fopen(m_szCurrentResultsLog, "a");
	if (!m_pLogFile)
		return;

	int len = strlen(text);
	fprintf(m_pLogFile, "%s", text);
	fflush(m_pLogFile);
#endif
}

#if HAS_STD_FILESYSTEM

void CResults::Start()
{
	m_bResultsStarted = true;

	// Prepare file names
	m_szCurrentResultsLog[0] = '\0';
	m_szCurrentResultsStats[0] = '\0';
	GetResultsFilename("log", nullptr, m_szCurrentResultsLog);
	GetResultsFilename("txt", nullptr, m_szCurrentResultsStats);
}

void CResults::Stop()
{
	m_bResultsStarted = false;
	m_iLastMaxClients = 0;

	// Clear map name
	m_szCurrentMap[0] = 0;

	// Close chat log and other files we may have opened
	CloseFiles();

	// Stop demo recording if we started it
	if (m_bDemoRecording && gEngfuncs.pDemoAPI->IsRecording())
	{
		EngineClientCmd("stop\n");
	}

	m_bDemoRecording = false;
	m_bDemoRecordingStartIssued = false;

	// Clear file names
	m_szCurrentResultsDemo[0] = 0;
	m_szCurrentResultsLog[0] = 0;
	m_szCurrentResultsStats[0] = 0;
}

void CResults::CloseFiles()
{
	if (m_pLogFile)
	{
		fclose(m_pLogFile);
		m_pLogFile = nullptr;
	}
}

void CResults::StartDemoRecording()
{
	if (m_bDemoRecording)
	{
		ConPrintf("Already recording.\n");
		return;
	}

	char buf[1024];

	GetResultsFilename("dem", m_szCurrentResultsDemo, nullptr);
	snprintf(buf, sizeof(buf), "record %s\n", m_szCurrentResultsDemo);

	EngineClientCmd(buf);
	m_bDemoRecording = true;
	m_bDemoRecordingFrame = 0;

	// Write autodemo file name to the temporary list
	FILE *file = fopen(m_szTempDemoList, "a+b");
	if (file)
	{
		time_t now;
		time(&now);
		struct tm *current = localtime(&now);
		snprintf(buf, sizeof(buf), "[%04i-%02i-%02i %02i:%02i:%02i] ", current->tm_year + 1900, current->tm_mon + 1, current->tm_mday, current->tm_hour, current->tm_min, current->tm_sec);
		fprintf(file, "%s%s\r\n", buf, m_szCurrentResultsDemo);
		fclose(file);
	}
}

void CResults::PurgeDemos()
{
	char buf[512], fileName[MAX_PATH];
	int readPos = 0, writePos = 0;
	bool deleteRow = true;

	time_t now;
	time(&now);
	now -= results_demo_keepdays.GetInt() * 24 * 60 * 60; // days to keep

	FILE *file = fopen(m_szTempDemoList, "r+b");
	if (file)
	{
		while (fgets(buf, sizeof(buf), file) != NULL)
		{
			deleteRow = true;
			if (buf[0] == '[' && buf[20] == ']')
			{
				buf[20] = 0;
				struct tm inTm;
				memset(&inTm, 0, sizeof(inTm));
				int scanResult = sscanf(buf + 1, "%4u-%2u-%2u %2u:%2u:%2u", &(inTm.tm_year), &(inTm.tm_mon), &(inTm.tm_mday), &(inTm.tm_hour), &(inTm.tm_min), &(inTm.tm_sec));
				if (scanResult == 6)
				{
					inTm.tm_year -= 1900;
					inTm.tm_mon -= 1;
					time_t inTime = mktime(&inTm);
					if (inTime < now)
					{
						char *fname = &buf[22];
						int len = strlen(fname) - 1;
						while (len >= 0 && fname[len] == '\r' || fname[len] == '\n')
						{
							fname[len--] = 0;
						}
						snprintf(fileName, sizeof(fileName), "%s%s", m_szFullGameDirPath, fname);
						try
						{
							std::filesystem::remove(std::filesystem::u8path(fileName));
						}
						catch (const std::exception &e)
						{
							ConPrintf(ConColor::Red, "Results: Failed to remove file %s: %s.\n", fname, e.what());
						}
					}
					else
					{
						deleteRow = false;
					}
				}
			}

			// Remove row
			if (deleteRow)
			{
				readPos = ftell(file);
				continue;
			}

			// Place string back if we keep it
			if (readPos != writePos)
			{
				buf[20] = ']';
				readPos = ftell(file);
				fseek(file, writePos, SEEK_SET);
				fputs(buf, file);
				writePos = ftell(file);
				fseek(file, readPos, SEEK_SET);
			}
			else
			{
				readPos = ftell(file);
				writePos = readPos;
			}
		}
		fseek(file, writePos, SEEK_SET);
		fclose(file);
		std::filesystem::resize_file(std::filesystem::u8path(m_szTempDemoList), writePos);
	}
}

bool CResults::GetResultsFilename(const char *extension, char *filename, char *fullpath)
{
	// Get map name
	if (m_szCurrentMap[0] == '\0')
	{
		V_FileBase(gEngfuncs.pfnGetLevelName(), m_szCurrentMap, sizeof(m_szCurrentMap));
	}

	// Get time
	time_t t = time(NULL);
	tm *pTm = localtime(&t);

	// Construct file name
	char format[MAX_PATH];
	char filenameBuf[MAX_PATH];
	V_StrSubst(FILENAME_FORMAT, "<map>", m_szCurrentMap, format, sizeof(format), true);
	strftime(filenameBuf, sizeof(filenameBuf), format, pTm);

	char szRelPath[MAX_PATH];
	std::filesystem::path fsRelPath;

	try
	{
		snprintf(szRelPath, sizeof(szRelPath), "%s.%s", filenameBuf, extension);
		fsRelPath = std::filesystem::u8path(szRelPath);

		if (std::filesystem::exists(m_fsFullGameDirPath / fsRelPath))
		{
			// File fount - append counter
			int count = 1;

			do
			{
				snprintf(szRelPath, sizeof(szRelPath), "%s-%03d.%s", filenameBuf, count, extension);
				fsRelPath = std::filesystem::u8path(szRelPath);
				count++;
			} while (count < 999 && std::filesystem::exists(m_fsFullGameDirPath / fsRelPath));

			if (count >= 1000)
			{
				ConPrintf(ConColor::Red, "Results: Couldn't construct filepath: counter exausted.\n");
				return false;
			}
		}
	}
	catch (const std::exception &e)
	{
		ConPrintf(ConColor::Red, "Results: Couldn't construct filepath: %s\n", e.what());
		return false;
	}

	try
	{
		CreateDirectoryStructure(fsRelPath.parent_path());
	}
	catch (const std::exception &e)
	{
		ConPrintf(ConColor::Red, "Results: Couldn't create directories: %s\n", e.what());
		return false;
	}

	if (filename)
		Q_strncpy(filename, szRelPath, MAX_PATH);
	if (fullpath)
		snprintf(fullpath, MAX_PATH, "%s%s", m_szFullGameDirPath, szRelPath);

	return true;
}

void CResults::CreateDirectoryStructure(std::filesystem::path relPath)
{
	std::filesystem::path p = m_fsFullGameDirPath;

	for (auto &i : relPath)
	{
		using std::filesystem::file_type;

		if (i.filename() == "." || i.filename() == "..")
			throw std::runtime_error("Path contains \".\" or \"..\"");
		p /= i;

		std::filesystem::file_status status = std::filesystem::status(p);

		if (status.type() == file_type::directory)
		{
			// OK
			continue;
		}
		else if (status.type() == file_type::not_found)
		{
			// Create directory
			std::filesystem::create_directory(p);
		}
		else
		{
			throw std::runtime_error(std::string("Unexpected file ") + i.u8string());
		}
	}
}
#endif
