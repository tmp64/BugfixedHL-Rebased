#include "update_installer.h"

bool CUpdateInstaller::Plat_IsSpecialFile(const fs::path &path)
{
	std::string ext = path.extension().u8string();

	return (ext == ".so");
}

void CUpdateInstaller::Plat_CopySpecialFile(const fs::path &from, const fs::path &to)
{
	// Unlink original file if it exists
	if (fs::exists(to))
	{
		if (!fs::remove(to))
			throw std::runtime_error("failed to remove original file");
	}

	// Copy new file over it
	if (!fs::copy_file(from, to, fs::copy_options::overwrite_existing))
		throw std::runtime_error("failed to copy file");
}
