/***
*
*	Copyright (c) 2012, AGHL.RU. All rights reserved.
*
****/
//
// Path.h
//
// File path manipulation routines.
//

bool IsValidFilename(const char *path);
void RemoveInvalidFilenameChars(char *path);
void RemoveInvalidPathChars(char *path, bool isRoted);

#ifdef _WIN32
// Creates directory with all intermediate directories.
bool CreateDirectoryFull(char *path);
#endif
