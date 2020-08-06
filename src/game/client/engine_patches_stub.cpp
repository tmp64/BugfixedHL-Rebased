#include "engine_patches.h"
#include "cl_util.h"

class CEnginePatchesStub : public CEnginePatches
{
protected:
	void PlatformPatchesInit() override
	{
		// All non-Windows versions of HL use SDL.
		m_bIsSDLEngine = true;
		ConPrintf("Engine patch: unsupported platform.");
	}
};

static CEnginePatchesStub s_Singleton;
