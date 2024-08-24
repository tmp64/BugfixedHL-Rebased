#include "extdll.h"
#include "util.h"

char *UTIL_strncpy(char *dst, const char *src, int len_dst)
{
	if (len_dst <= 0)
	{
		return NULL; // this is bad
	}

	strncpy(dst, src, len_dst);
	dst[len_dst - 1] = '\0';

	return dst;
}
