#define NOMINMAX
#include <Windows.h>

#include <tier1/strtools.h>
#include "update_installer.h"

static std::string GetErrorAsString(DWORD error)
{
	wchar_t wbuf[512];
	char buf[1024];
	FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	    NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
	    wbuf, (sizeof(wbuf) / sizeof(wchar_t)), NULL);

	Q_WStringToUTF8(wbuf, buf, sizeof(buf));
	return buf;
}

bool CUpdateInstaller::Plat_IsSpecialFile(const fs::path &path)
{
	std::string ext = path.extension().u8string();

	return (ext == ".dll" || ext == ".exe" || ext == ".pdb");
}

void CUpdateInstaller::Plat_CopySpecialFile(const fs::path &from, const fs::path &to)
{
	// We assume that `to` is locked.
	// We firstly rename that file without copying
	// Then copy the `from` file to `to` that no longer exists (it was renamed).
	fs::path tempPath = fs::u8path(to.u8string() + ".old");

	if (!MoveFileExW(to.c_str(), tempPath.c_str(), MOVEFILE_REPLACE_EXISTING))
	{
		DWORD error = GetLastError();
		throw std::runtime_error("MoveFileExW failed: " + GetErrorAsString(error));
	}

	if (!CopyFileW(from.c_str(), to.c_str(), false))
	{
		DWORD error = GetLastError();
		throw std::runtime_error("CopyFileW failed: " + GetErrorAsString(error));
	}
}
