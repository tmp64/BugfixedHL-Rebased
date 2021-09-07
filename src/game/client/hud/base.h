#ifndef HUD_BASE_H
#define HUD_BASE_H
#include <type_traits>

#include "wrect.h"
#include "cl_dll.h"

#define FADE_TIME 100

#define MAX_SPRITE_NAME_LENGTH 24
#define MAX_MAP_NAME           64

typedef struct cvar_s cvar_t;

//----------------------------------------------------------
// Base class of all HUD elements.
//----------------------------------------------------------
class CHudElem
{
public:
	int m_iFlags = 0; // active, moving,

	virtual ~CHudElem();

	virtual void Init();
	virtual void VidInit();
	virtual void Draw(float flTime);
	virtual void Think();
	virtual void Reset();
	virtual void InitHudData();
};

//----------------------------------------------------------
// All HUD elements should actually inherit this class
// to have Elem::Get() method and member function messages.
//----------------------------------------------------------
template <typename ELEM>
class CHudElemBase : public CHudElem
{
public:
	using BaseHudClass = CHudElemBase<ELEM>;

	CHudElemBase()
	{
		m_sInstance = this;
	}

	virtual void Init()
	{
		CHudElem::Init();
	}

	template <int (ELEM::*FUNC)(const char *, int, void *)>
	void HookMessage(const char *name)
	{
		gEngfuncs.pfnHookUserMsg((char *)name, [](const char *pszName, int iSize, void *pbuf) -> int {
			return (ELEM::Get()->*FUNC)(pszName, iSize, pbuf);
		});
	}

	template <void (ELEM::*FUNC)()>
	void HookCommand(const char *name)
	{
		gEngfuncs.pfnAddCommand((char *)name, []() {
			(ELEM::Get()->*FUNC)();
		});
	}

	static ELEM *Get()
	{
		return static_cast<ELEM *>(m_sInstance);
	}

private:
	static BaseHudClass *m_sInstance;
};

#define DEFINE_HUD_ELEM(className) \
	template <>                    \
	CHudElemBase<className> *className::BaseHudClass::m_sInstance = nullptr;

#endif
