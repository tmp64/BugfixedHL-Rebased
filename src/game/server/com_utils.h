/***
*
*	Copyright (c) 2012, AGHL.RU. All rights reserved.
*
****/
//
// Common_utils.h
//
// Functions that can be used on both: client and server.
//

#ifdef _WIN32
#include <windows.h>
#else // _WIN32

#include <pthread.h>
#include <sys/time.h>

#endif

/*============
String functions
strrepl: replaces substrings in a string
============*/
bool strrepl(char *str, int size, const char *find, const char *repl);

/*============
CXMutex
============*/
class CXMutex
{
public:
	CXMutex();
	~CXMutex();
	void Lock();
	void Unlock();
	bool TryLock();

protected:
#ifdef _WIN32
	CRITICAL_SECTION m_CritSect;
#else
	pthread_mutex_t m_Mutex;
#endif
};

/*============
CXTime: returns high resolution time
============*/
extern double CXTime();
