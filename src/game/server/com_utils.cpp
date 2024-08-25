/***
*
*	Copyright (c) 2012, AGHL.RU. All rights reserved.
*
****/
//
// Common_utils.cpp
//
// Functions that can be used on both: client and server.
//

#ifdef _WIN32
#undef ARRAYSIZE
#include <windows.h>
#endif

#include "com_utils.h"
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include "extdll.h"
#include "util.h"

using std::max;
using std::min;

/*============
String functions
strrepl: replaces substrings in a string
============*/
bool strrepl(char *str, int size, const char *find, const char *repl)
{
	if (size < 1)
		return true;
	if (size < 2)
	{
		str[0] = 0;
		return true;
	}

	char *buffer = (char *)malloc(size);
	if (buffer == NULL)
		return false;

	char *c = str;
	char *buf = buffer;
	char *buf_end = buffer + size - 1;
	const char *match = find;
	int len_find = strlen(find);
	while (*c && buf < buf_end)
	{
		if (*match == *c)
		{
			match++;
			if (!*match)
			{
				// Match found: rewind buffer and copy relpacement there
				buf -= len_find - 1;
				size_t len = buf_end - buf;
				if (len < 1)
					break;
				strncpy(buf, repl, len);
				match = find;
				buf += min(strlen(repl), len);
				c++;
				continue;
			}
		}
		else
		{
			// Reset match
			match = find;
		}

		*buf = *c;
		buf++;
		c++;
	}
	*buf = 0;
	UTIL_strncpy(str, buffer, size);
	free(buffer);
	return true;
}

#ifdef _WIN32

/*============
CXMutex (Windows)
============*/

CXMutex::CXMutex()
{
	InitializeCriticalSection(&m_CritSect);
}

CXMutex::~CXMutex()
{
	DeleteCriticalSection(&m_CritSect);
}

void CXMutex::Lock()
{
	EnterCriticalSection(&m_CritSect);
}

void CXMutex::Unlock()
{
	LeaveCriticalSection(&m_CritSect);
}

bool CXMutex::TryLock()
{
	return (TryEnterCriticalSection(&m_CritSect) != FALSE);
}

#else // _WIN32

/*============
CXMutex (Linux)
============*/

CXMutex::CXMutex()
{
	pthread_mutex_init(&m_Mutex, NULL);
}

CXMutex::~CXMutex()
{
	pthread_mutex_destroy(&m_Mutex);
}

void CXMutex::Lock()
{
	pthread_mutex_lock(&m_Mutex);
}

void CXMutex::Unlock()
{
	pthread_mutex_unlock(&m_Mutex);
}

bool CXMutex::TryLock()
{
	return (pthread_mutex_trylock(&m_Mutex) == 0);
}

#endif // _WIN32

// CX initialization mutex
CXMutex gCXInitMutex;

/*============
CXTime: returns high resolution time
============*/
double CXTime()
{
	double res;
	static double startval = 0;
	static bool bStartValDefined = false;

#ifdef _WIN32

	static bool bWinTimersInitialized = false;
	static LARGE_INTEGER TimerFreq;

	LARGE_INTEGER CurTick;

	if (!bWinTimersInitialized)
	{
		gCXInitMutex.Lock();
		if (!bWinTimersInitialized)
		{
			if (QueryPerformanceFrequency(&TimerFreq) != TRUE)
			{
				throw "CX_Time(): QueryPerformanceFrequency() failed.";
			}
			bWinTimersInitialized = true;
		}
		gCXInitMutex.Unlock();
	}
	if (!QueryPerformanceCounter(&CurTick))
	{
		throw "CX_Time(): QueryPerformanceCounter() failed.";
	}
	res = (double)CurTick.QuadPart / (double)TimerFreq.QuadPart;

#else // _WIN32

	timeval tv;
	gettimeofday(&tv, NULL);
	res = (double)tv.tv_sec + (double)tv.tv_usec * 0.000001;

#endif // _WIN32

	if (!bStartValDefined)
	{
		gCXInitMutex.Lock();
		if (!bStartValDefined)
		{
			bStartValDefined = true;
			startval = res;
		}
		gCXInitMutex.Unlock();
	}

	res -= startval;
	return res;
}
