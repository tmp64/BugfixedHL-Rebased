#include <exception>
#include <string>
#include <fstream>
#include <sstream>

#include <appversion.h>
#include "bhlcfg.h"
#include "hud.h"
#include "cl_util.h"

#define BHL_CONFIG_NAME       "bugfixedhl.cfg"
#define BHL_CONFIG_USER_BEGIN "// [USER CONFIG BEGIN - DO NOT EDIT THIS LINE]"

namespace bhlcfg
{
static void SaveConfig(const char *cfgname);
}

void bhlcfg::Init()
{
	gEngfuncs.pfnClientCmd("exec " BHL_CONFIG_NAME);
}

void bhlcfg::Shutdown()
{
	try
	{
		SaveConfig(BHL_CONFIG_NAME);
	}
	catch (const std::exception &)
	{
		// Ignore. Nothing we can do at this point.
	}
}

void bhlcfg::SaveConfig(const char *cfgname)
{
	char filename[256];
	snprintf(filename, sizeof(filename), "%s/%s", gEngfuncs.pfnGetGameDirectory(), cfgname);

	std::fstream file;

	// Read user config
	file.open(filename, std::fstream::in);
	std::string userCfg;

	if (!file.fail()) // Open may fail if file doesn't exist
	{
		std::string line;
		bool isBeginFound = false;
		while (std::getline(file, line))
		{
			if (isBeginFound)
			{
				userCfg += line + "\n";
			}
			else
			{
				if (line == BHL_CONFIG_USER_BEGIN)
					isBeginFound = true;
			}
		}
	}

	file.close();
	file.clear();

	// Save new config
	file.open(filename, std::fstream::out);
	if (file.fail())
	{
		std::stringstream s;
		s << "failed to save config file " << filename << ". errno = " << errno << " [" << strerror(errno) << "]";
		throw std::runtime_error(s.str());
	}

	// Header
	file << "// BugfixedHL v" << APP_VERSION << " config file\n";
	file << "// The first part is generated automatically.\n";
	file << "// The second part can be modified manually.\n";
	file << "// Look for a huge block of comments below.\n";
	file << "\n";

	// Cvars
	for (cvar_t *i = gEngfuncs.GetFirstCvarPtr(); i; i = i->next)
	{
		if (i->flags & FCVAR_BHL_ARCHIVE)
		{
			file << i->name << " \"" << i->string << "\"\n";
		}
	}

	// User commands
	file << "\n\n";
	file << "// You can add your own commands below.\n";
	file << "// It works just like userconfig.cfg, but will only be executed on BugfixedHL clients.\n";
	file << "// It is recomended to add your custom commands to a separate\n";
	file << "// config file and execute it here like this:\n";
	file << "//     exec myconfigname.cfg\n";
	file << "// That way you won't lose your config if this file was damaged\n";
	file << "// and subsequently overwritten.\n";
	file << "// DO NOT TOUCH line just next to this one.\n";
	file << BHL_CONFIG_USER_BEGIN << "\n";
	file << userCfg;
}
