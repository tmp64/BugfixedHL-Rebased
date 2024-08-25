/***
*
*	Copyright (c) 2012, AGHL.RU. All rights reserved.
*
****/
//
// Path.cpp
//
// File path manipulation routines.
//

#ifdef _WIN32
#undef ARRAYSIZE
#include <windows.h>
#endif

#include "path.h"

bool IsValidFilename(const char *path)
{
	if (path[0] == 0)
		return false;
	const char *c = path;
	const char *d = path;
	bool haschars = false;
	while (*c)
	{
		if (*c <= 31 || *c == '<' || *c == '>' || *c == '"' || *c == '/' || *c == '|' || *c == '?' || *c == '*' || *c == ':' || *c == '\\')
			return false;
		if (*c != ' ')
			haschars = true;
		c++;
	}
	return haschars;
}

void RemoveInvalidFilenameChars(char *path)
{
	char *c = path;
	char *d = path;
	while (*c)
	{
		if (*c <= 31 || *c == '<' || *c == '>' || *c == '"' || *c == '/' || *c == '|' || *c == '?' || *c == '*' || *c == ':' || *c == '\\')
		{
			c++;
			continue;
		}

		*d = *c;
		c++;
		d++;
	}
	*d = 0;
}

void RemoveInvalidPathChars(char *path, bool isRoted)
{
	char *c = path;
	char *d = path;
	if (isRoted)
		while (*c)
		{
			if (*c <= 31 || *c == '<' || *c == '>' || *c == '"' || *c == '/' || *c == '|' || *c == '?' || *c == '*' || (*c == ':' && d - path != 1))
			{
				c++;
				continue;
			}

			*d = *c;
			c++;
			d++;
		}
	else
		while (*c)
		{
			if (*c <= 31 || *c == '<' || *c == '>' || *c == '"' || *c == '/' || *c == '|' || *c == '?' || *c == '*' || *c == ':')
			{
				c++;
				continue;
			}

			*d = *c;
			c++;
			d++;
		}
	*d = 0;
}

#ifdef _WIN32

// Creates directory with all intermediate directories.
bool CreateDirectoryFull(char *path)
{
	if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
		return true;

	// Construct full path
	char *filepart;
	char fullpath[MAX_PATH];
	GetFullPathName(path, MAX_PATH, fullpath, &filepart);

	char dup[MAX_PATH];
	char *cd = dup;
	char *c;
	if (fullpath[0] != 0 && fullpath[1] == ':')
	{
		cd[0] = fullpath[0];
		cd[1] = fullpath[1];
		cd += 2;
		c = fullpath + 2;
	}
	else
	{
		c = fullpath;
	}
	while (*c)
	{
		while (*c && *c != '\\')
		{
			*cd = *c;
			cd++;
			c++;
		}
		*cd = 0;
		if (GetFileAttributes(dup) == INVALID_FILE_ATTRIBUTES)
		{
			CreateDirectory(dup, 0);
		}
		*cd = *c;
		cd++;
		c++;
	}
	return true;
}

#endif
