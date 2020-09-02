#include <cstdlib>
#include <exception>
#include <string>
#include <tier0/dbg.h>
#include <cl_dll/IGameClientExports.h>
#include <IClientVGUI.h>
#include "plat.h"
#include "cl_exports.h"

class CClientTest
{
public:
	static inline CClientTest &Get()
	{
		return *m_sInstance;
	}

	CClientTest();
	~CClientTest();

	int Run(int argc, char **argv);
	[[noreturn]] void FatalError(const std::string &msg);

private:
	static inline CClientTest *m_sInstance = nullptr;
	plat::Module m_ClientLib;
	ClientExports m_ClientExports;
	CreateInterfaceFn m_ClientFactory = nullptr;

	void SetSpewHandler();
	void LoadClientLib(const char *clientPath);

	/**
	 * Loads all exported functions from the client.
	 */
	void LoadClientExports();

	/**
	 * Checks that all required interfaces (tier1/interface.h) are exported.
	 */
	void CheckInterfaces();
};

int main(int argc, char **argv)
{
	CClientTest test;
	return test.Run(argc, argv);
}

[[noreturn]] void PLAT_FatalError(const std::string &msg)
{
	CClientTest::Get().FatalError(msg);
}

CClientTest::CClientTest()
{
	Assert(!m_sInstance);
	m_sInstance = this;

	memset(&m_ClientExports, 0, sizeof(m_ClientExports));
}

CClientTest::~CClientTest()
{
	Assert(m_sInstance);
	m_sInstance = nullptr;
}

int CClientTest::Run(int argc, char **argv)
{
	try
	{
		if (argc < 2)
			FatalError("Expected client lib path as first argument.");

		SetSpewHandler();
		LoadClientLib(argv[1]);
		LoadClientExports();
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

void CClientTest::FatalError(const std::string &msg)
{
	fprintf(stderr, "Fatal Error: %s\n", msg.c_str());
	std::quick_exit(1);
}

void CClientTest::SetSpewHandler()
{
	SpewOutputFunc([](SpewType_t spewType, tchar const *pMsg) -> SpewRetval_t {
		fprintf(stderr, "Dbg Spew: %s", pMsg);

		switch (spewType)
		{
		case SPEW_WARNING:
			CClientTest::Get().FatalError("tier0 spew handler got a warning.");
			break;
		case SPEW_ASSERT:
			CClientTest::Get().FatalError("tier0 assertion failed.");
			break;
		case SPEW_ERROR:
			CClientTest::Get().FatalError("tier0 spew handler got an error.");
			break;
		default:
			CClientTest::Get().FatalError("tier0 spew handler got unknown message.");
			break;
		}

		return SPEW_CONTINUE;
	});
}

void CClientTest::LoadClientLib(const char *clientPath)
{
	fprintf(stderr, "Loading client library\n");

	if (!clientPath || !clientPath[0])
		FatalError("Invalid client path");

	m_ClientLib = plat::LoadModuleOrDie(clientPath);

	m_ClientFactory = Sys_GetFactory(m_ClientLib.pHandle);
	if (!m_ClientFactory)
	{
		FatalError("Client doesn't export CreateInterface.");
	}

	fprintf(stderr, "Loaded\n\n");
}

void CClientTest::LoadClientExports()
{
	fprintf(stderr, "Loading exported functions from the client\n");

	int count = 0;

	for (size_t i = 0; i < CLIENT_EXPORTED_FUNC_COUNT; i++)
	{
		// Skip not exported functions
		if (!CLIENT_FUNC_NAMES[i])
			continue;

		m_ClientExports.array[i] = plat::GetProcAddr(m_ClientLib, CLIENT_FUNC_NAMES[i], true);
		count++;
	}

	fprintf(stderr, "Loaded %d functions\n\n", count);
}

void CClientTest::CheckInterfaces()
{
	fprintf(stderr, "Checking that all necessary interfaces are exported\n");

	constexpr const char *ifaces[] = {
		ICLIENTVGUI_NAME,
		GAMECLIENTEXPORTS_INTERFACE_VERSION,
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

	fprintf(stderr, "\n");
}
