#include <unistd.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <link.h>
#include <sys/stat.h>
#include <map>
#include <string>
#include <fstream>

#include "hud.h"
#include "cl_util.h"
#include "cl_dll.h"
#include "engine_patches.h"

static const int MPROTECT_PAGESIZE = sysconf(_SC_PAGE_SIZE);
static const int MPROTECT_PAGEMASK = ~(MPROTECT_PAGESIZE - 1);

struct sizebuf_t
{
	const char *buffername;
	uint16_t flags;
	byte *data;
	int maxsize;
	int cursize;
};

class CEnginePatchesLinux : public CEnginePatches
{
public:
	virtual Renderer GetRenderer() override;
	virtual void HookSvcHandlers(SvcParseFunc array[SVC_MSG_COUNT]) override;

protected:
	virtual void PlatformPatchesInit() override;
	virtual void PlatformPatchesShutdown() override;
	virtual void FindMsgBuf() override;
	virtual void FindSvcArray() override;
	virtual void FindUserMessageList() override;

private:
	std::map<uintptr_t, std::pair<uintptr_t, int>> g_AddrToProtect;
	void *m_hEngineModule = nullptr;

	/**
	 * Unlike dlsym, this function can get addr of non-exported symbols
	 * Based on AMX Mod X's MemoryUtils::ResolveSymbol
	 * https://github.com/alliedmodders/amxmodx/blob/master/public/memtools/MemoryUtils.cpp
	 * @return Address of the symbol or nullptr
	 */
	void *ResolveSymbol(void *handle, const char *symbol);

	/**
	 * Fills g_AddrToProtect with entries from /proc/self/maps
	 */
	bool LoadProtectFromProc();

	/**
	 * Returns original protection mask of address.
	 */
	int GetProtectForAddr(size_t addr);

	/**
	 * Replaces double word on specified address with new dword, returns old dword
	 */
	uint32_t HookDWord(size_t origAddr, uint32_t newDWord);
};

static CEnginePatchesLinux s_EnginePatchesInstance;

CEnginePatches::Renderer CEnginePatchesLinux::GetRenderer()
{
	// Linux only supports OpenGL
	return Renderer::OpenGL;
}

void CEnginePatchesLinux::HookSvcHandlers(SvcParseFunc array[SVC_MSG_COUNT])
{
	Assert(GetSvcArray());

	for (size_t i = 0; i < SVC_MSG_COUNT; i++)
	{
		if (array[i] && GetSvcArray()[i].pfnParse != array[i])
		{
			uintptr_t addr = reinterpret_cast<uintptr_t>(&GetSvcArray()[i].pfnParse);
			uint32_t newVal = reinterpret_cast<uint32_t>(array[i]);
			HookDWord(addr, newVal);
		}
	}
}

void CEnginePatchesLinux::PlatformPatchesInit()
{
	// All non-Windows version of HL use SDL
	m_bIsSDLEngine = true;

	if (!LoadProtectFromProc())
		return;

	// Load engine module
	m_hEngineModule = dlopen("hw.so", RTLD_NOW | RTLD_NOLOAD);

	if (!m_hEngineModule)
	{
		ConPrintf(ConColor::Red, "Engine patch: failed to load engine module\n");
		ConPrintf(ConColor::Red, "%s\n", dlerror());
		return;
	}

	// Reduce refcount, it will be loaded for as long as client.so exists
	dlclose(m_hEngineModule);
}

void CEnginePatchesLinux::PlatformPatchesShutdown()
{
	g_AddrToProtect.clear();
	m_hEngineModule = nullptr;
}

void CEnginePatchesLinux::FindMsgBuf()
{
	if (!m_hEngineModule)
		return;

	sizebuf_t *buf = static_cast<sizebuf_t *>(ResolveSymbol(m_hEngineModule, "net_message"));

	if (!buf)
	{
		ConPrintf(ConColor::Red, "Engine patch: net_message not found in the engine\n");
		return;
	}

	m_MsgBuf.m_EngineBuf = reinterpret_cast<void **>(&buf->data);
	m_MsgBuf.m_EngineBufSize = &buf->cursize;
	m_MsgBuf.m_EngineReadPos = static_cast<int *>(ResolveSymbol(m_hEngineModule, "msg_readcount"));

	if (!m_MsgBuf.m_EngineReadPos)
	{
		ConPrintf(ConColor::Red, "Engine patch: msg_readcount not found in the engine\n");
		return;
	}
}

void CEnginePatchesLinux::FindSvcArray()
{
	if (!m_hEngineModule)
		return;

	m_pSvcArray = static_cast<SvcHandler *>(ResolveSymbol(m_hEngineModule, "cl_parsefuncs"));

	if (!m_pSvcArray)
	{
		ConPrintf(ConColor::Red, "Engine patch: cl_parsefuncs not found in the engine\n");
		return;
	}

	// Check first 3 elements just to be sure
	auto fnCheck = [&](size_t idx, const char *name) {
		return strcmp(m_pSvcArray[idx].pszName, name) == 0;
	};

	if (!fnCheck(0, "svc_bad") || !fnCheck(1, "svc_nop") || !fnCheck(2, "svc_disconnect"))
	{
		m_pSvcArray = nullptr;
		ConPrintf(ConColor::Red, "Engine patch: SvcMessagesTable check has failed.\n");
		return;
	}

	// Copy engine handler into an array
	for (size_t i = 0; i < SVC_MSG_COUNT; i++)
	{
		m_EngineSvcHandlers.array[i] = GetSvcArray()[i].pfnParse;
	}
}

void CEnginePatchesLinux::FindUserMessageList()
{
	if (!m_hEngineModule)
		return;

	m_pUserMsgList = static_cast<UserMessage **>(ResolveSymbol(m_hEngineModule, "gClientUserMsgs"));

	if (!m_pUserMsgList)
	{
		ConPrintf(ConColor::Red, "Engine patch: gClientUserMsgs not found in the engine\n");
		return;
	}
}

void *CEnginePatchesLinux::ResolveSymbol(void *handle, const char *symbol)
{
	void *addr = dlsym(handle, symbol);

	if (addr)
	{
		return addr;
	}

	struct link_map *dlmap;
	struct stat dlstat;
	int dlfile;
	uintptr_t map_base;
	Elf32_Ehdr *file_hdr;
	Elf32_Shdr *sections, *shstrtab_hdr, *symtab_hdr, *strtab_hdr;
	Elf32_Sym *symtab;
	const char *shstrtab, *strtab;
	uint16_t section_count;
	uint32_t symbol_count;

	dlmap = (struct link_map *)handle;
	symtab_hdr = nullptr;
	strtab_hdr = nullptr;

	/* If symbol isn't in our table, then we have open the actual library */
	dlfile = open(dlmap->l_name, O_RDONLY);
	if (dlfile == -1 || fstat(dlfile, &dlstat) == -1)
	{
		close(dlfile);
		return nullptr;
	}

	/* Map library file into memory */
	file_hdr = (Elf32_Ehdr *)mmap(nullptr, dlstat.st_size, PROT_READ, MAP_PRIVATE, dlfile, 0);
	map_base = (uintptr_t)file_hdr;
	if (file_hdr == MAP_FAILED)
	{
		close(dlfile);
		return nullptr;
	}
	close(dlfile);

	if (file_hdr->e_shoff == 0 || file_hdr->e_shstrndx == SHN_UNDEF)
	{
		munmap(file_hdr, dlstat.st_size);
		return nullptr;
	}

	sections = (Elf32_Shdr *)(map_base + file_hdr->e_shoff);
	section_count = file_hdr->e_shnum;
	/* Get ELF section header string table */
	shstrtab_hdr = &sections[file_hdr->e_shstrndx];
	shstrtab = (const char *)(map_base + shstrtab_hdr->sh_offset);

	/* Iterate sections while looking for ELF symbol table and string table */
	for (uint16_t i = 0; i < section_count; i++)
	{
		Elf32_Shdr &hdr = sections[i];
		const char *section_name = shstrtab + hdr.sh_name;

		if (strcmp(section_name, ".symtab") == 0)
		{
			symtab_hdr = &hdr;
		}
		else if (strcmp(section_name, ".strtab") == 0)
		{
			strtab_hdr = &hdr;
		}
	}

	/* Uh oh, we don't have a symbol table or a string table */
	if (symtab_hdr == nullptr || strtab_hdr == nullptr)
	{
		munmap(file_hdr, dlstat.st_size);
		return nullptr;
	}

	symtab = (Elf32_Sym *)(map_base + symtab_hdr->sh_offset);
	strtab = (const char *)(map_base + strtab_hdr->sh_offset);
	symbol_count = symtab_hdr->sh_size / symtab_hdr->sh_entsize;

	void *pFoundSymbol = nullptr;

	/* Iterate symbol table starting from the begining */
	for (uint32_t i = 0; i < symbol_count; i++)
	{
		Elf32_Sym &sym = symtab[i];
		unsigned char sym_type = ELF32_ST_TYPE(sym.st_info);
		const char *sym_name = strtab + sym.st_name;

		/* Skip symbols that are undefined or do not refer to functions or objects */
		if (sym.st_shndx == SHN_UNDEF || (sym_type != STT_FUNC && sym_type != STT_OBJECT))
		{
			continue;
		}

		if (strcmp(symbol, sym_name) == 0)
		{
			pFoundSymbol = (void *)(dlmap->l_addr + sym.st_value);
			break;
		}
	}

	munmap(file_hdr, dlstat.st_size);
	return pFoundSymbol;
}

bool CEnginePatchesLinux::LoadProtectFromProc()
{
	try
	{
		std::ifstream maps("/proc/self/maps");

		std::string line;
		while (std::getline(maps, line))
		{
			if (line.empty())
				break;

			size_t dash = line.find('-');
			if (dash == std::string::npos)
				throw std::runtime_error("Invalid mem map: " + line);

			size_t space = line.find(' ');
			if (space == std::string::npos)
				throw std::runtime_error("Invalid mem map: " + line);

			size_t end = line.find_first_not_of("rwxp-", space + 1);
			if (end == std::string::npos)
				throw std::runtime_error("Invalid mem map: " + line);

			std::string addrBeginStr = line.substr(0, dash);
			std::string addrEndStr = line.substr(dash + 1, space - dash - 1);
			std::string flagsStr = line.substr(space + 1, end - space - 1);

			size_t addrBegin = std::stoul(addrBeginStr, nullptr, 16);
			size_t addrEnd = std::stoul(addrEndStr, nullptr, 16);
			int flags = 0;

			for (char i : flagsStr)
			{
				if (i == 'r')
					flags |= PROT_READ;
				else if (i == 'w')
					flags |= PROT_WRITE;
				else if (i == 'x')
					flags |= PROT_EXEC;
			}

			g_AddrToProtect[addrBegin] = std::make_pair(addrEnd, flags);
		}

		return true;
	}
	catch (const std::exception &e)
	{
		ConPrintf(ConColor::Red, "Engine patch: LoadProtectFromProc()\n");
		ConPrintf(ConColor::Red, "Exception occurred: %s\n", e.what());
		return false;
	}
}

int CEnginePatchesLinux::GetProtectForAddr(uintptr_t addr)
{
	Assert(g_AddrToProtect.size() > 0);
	auto it = g_AddrToProtect.upper_bound(addr);

	if (it == g_AddrToProtect.end())
		HUD_FatalError("Failed to find protect flags for address %08llX", (unsigned long long)addr);

	it--;

	if (addr >= it->second.first)
	{
		HUD_FatalError("Address %08llX is out of range for protect entry %08llX-%08llX and no other entry was found",
		    (unsigned long long)addr,
		    it->first,
		    it->second.first);
	}

	return it->second.second;
}

uint32_t CEnginePatchesLinux::HookDWord(uintptr_t origAddr, uint32_t newDWord)
{
	uint32_t origDWord = *reinterpret_cast<uint32_t *>(origAddr);
	mprotect(reinterpret_cast<void *>(origAddr & MPROTECT_PAGEMASK), MPROTECT_PAGESIZE, PROT_READ | PROT_WRITE);
	*reinterpret_cast<uint32_t *>(origAddr) = newDWord;
	mprotect(reinterpret_cast<void *>(origAddr & MPROTECT_PAGEMASK), MPROTECT_PAGESIZE, GetProtectForAddr(origAddr));
	return origDWord;
}
