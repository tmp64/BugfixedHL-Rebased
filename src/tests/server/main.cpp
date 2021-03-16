#include <cassert>
#include <cstdlib>
#include <exception>
#include <string>
#include "plat.h"
#include "sv_exports.h"

class CServerTest
{
public:
	static inline CServerTest &Get()
	{
		return *m_sInstance;
	}

	CServerTest();
	~CServerTest();

	int Run(int argc, char **argv);
	[[noreturn]] void FatalError(const std::string &msg);

private:
	static inline CServerTest *m_sInstance = nullptr;
	plat::Module m_ServerLib;
	CreateInterfaceFn m_ServerFactory = nullptr;

	void LoadServerLib(const char *serverPath);

	/**
	 * Loads all exported functions from the server.
	 */
	void CheckServerExports();

	/**
	 * Checks that all required interfaces (tier1/interface.h) are exported.
	 */
	void CheckInterfaces();
};

int main(int argc, char **argv)
{
	CServerTest test;
	return test.Run(argc, argv);
}

[[noreturn]] void PLAT_FatalError(const std::string &msg)
{
	CServerTest::Get().FatalError(msg);
}

CServerTest::CServerTest()
{
	assert(!m_sInstance);
	m_sInstance = this;
}

CServerTest::~CServerTest()
{
	assert(m_sInstance);
	m_sInstance = nullptr;
}

int CServerTest::Run(int argc, char **argv)
{
	try
	{
		if (argc < 2)
			FatalError("Expected server lib path as first argument.");

		LoadServerLib(argv[1]);
		CheckServerExports();
		CheckInterfaces();
	}
	catch (const std::exception &e)
	{
		fprintf(stderr, "%s", e.what());
		FatalError("Unhandled exception.");
	}
	catch (...)
	{
		FatalError("Unhandled exception of non-exception type.");
	}

	return 0;
}

void CServerTest::FatalError(const std::string &msg)
{
	fprintf(stderr, "Fatal Error: %s\n", msg.c_str());
#ifdef _LIBCPP_HAS_QUICK_EXIT
	std::quick_exit(1);
#else
	exit(1);
#endif
}

void CServerTest::LoadServerLib(const char *serverPath)
{
	fprintf(stderr, "Loading server library\n");

	if (!serverPath || !serverPath[0])
		FatalError("Invalid server path");

	m_ServerLib = plat::LoadModuleOrDie(serverPath);

	m_ServerFactory = Sys_GetFactory(m_ServerLib.pHandle);
	if (!m_ServerFactory)
	{
		FatalError("Server doesn't export CreateInterface.");
	}

	fprintf(stderr, "Loaded\n\n");
}

void CServerTest::CheckServerExports()
{
	fprintf(stderr, "Checking exported server functions\n");

	for (size_t i = 0; i < std::size(SERVER_EXPORTED_FUNCTIONS); i++)
	{
		plat::GetProcAddr(m_ServerLib, SERVER_EXPORTED_FUNCTIONS[i], true);
	}

	fprintf(stderr, "%d functions okay\n\n", (int)std::size(SERVER_EXPORTED_FUNCTIONS));
}

void CServerTest::CheckInterfaces()
{
	/*fprintf(stderr, "Checking that all necessary interfaces are exported\n");

	constexpr const char *ifaces[] = {
		""
	};

	for (size_t i = 0; i < std::size(ifaces); i++)
	{
		CreateInterfaceFn factory = reinterpret_cast<CreateInterfaceFn>(m_ClientExports.funcs.pClientFactory());
		void *iface1 = factory(ifaces[i], nullptr);
		void *iface2 = m_ClientFactory(ifaces[i], nullptr);

		if (iface1 != iface2)
		{
			fprintf(stderr, "Interface %s: CreateInterface(...) != ClientFactory(...).\n", ifaces[i]);
			FatalError("Interface check failed.");
		}

		if (!iface1)
		{
			fprintf(stderr, "Client doesn't export %s interface.\n", ifaces[i]);
			FatalError("Interface check failed.");
		}

		fprintf(stderr, "%s good\n", ifaces[i]);
	}

	fprintf(stderr, "\n");*/
}
