#ifndef CONVAR_H
#define CONVAR_H
#include <cvardef.h>
#include <MinMax.h>

enum class ConItemType
{
	ConVar,
	ConCommand
};

/**
 * ConItemBase - the base class for ConVar and ConCommand.
 */
class ConItemBase
{
public:
	ConItemBase(const char *name);
	ConItemBase(const char *name, const char *desc);
	virtual ~ConItemBase();

	virtual ConItemType GetType() = 0;

	const char *GetName();
	const char *GetDescription();

private:
	const char *m_pName;
	const char *m_pDesc;

	ConItemBase *m_pNextItem = nullptr;
	static ConItemBase *m_pFirstItem;

	friend class CvarSystem;
};

/**
 * ConVar - a Source-like way to register cvars.
 */
class ConVar : public ConItemBase
{
public:
	ConVar(const char *name, const char *def_val, int flags = 0);
	ConVar(const char *name, const char *def_val, int flags, const char *desc);

	virtual ConItemType GetType();

	const char *GetDefaultValue();
	int GetFlags();

	float GetFloat();
	int GetInt();
	bool GetBool();
	const char *GetString();

	template <typename T>
	T GetEnumClamped()
	{
		int minValue = (int)T::_Min;
		int maxValue = (int)T::_Max;
		return (T)clamp(GetInt(), minValue, maxValue);
	}

	void SetValue(float val);
	void SetValue(int val);
	void SetValue(const char *val);

	cvar_t *GetCvar();

private:
	const char *m_pDefVal;
	int m_iFlags;

#ifdef SERVER_DLL
	cvar_t m_Cvar = {};
#else
	cvar_t *m_pCvar = nullptr;
#endif

	friend class CvarSystem;
};

inline cvar_t *ConVar::GetCvar()
{
#ifdef SERVER_DLL
	return &m_Cvar;
#else
	return m_pCvar;
#endif
}

/**
 * ConVarRef - a reference to an existing cvar.
 */
class ConVarRef
{
public:
	ConVarRef(const char *name);
	ConVarRef(cvar_t *cvar);
	ConVarRef(ConVar &cvar);
	ConVarRef(ConVar *cvar);

	const char *GetName();

	bool IsValid();

	float GetFloat();
	int GetInt();
	bool GetBool();
	const char *GetString();

	void SetValue(float val);
	void SetValue(int val);
	void SetValue(const char *val);

	cvar_t *GetCvar();

private:
	const char *m_pName = nullptr;
	cvar_t *m_pCvar = nullptr;
};

inline cvar_t *ConVarRef::GetCvar()
{
	return m_pCvar;
}

/**
 * ConCommand - class to register console commands
 */
class ConCommand : public ConItemBase
{
public:
	using CmdFunc = void (*)();

	ConCommand(const char *name, CmdFunc func, const char *desc = "");
	virtual ConItemType GetType();
	CmdFunc GetCmdFunc();

	static int ArgC();
	static const char *ArgV(int i);

private:
	CmdFunc m_fnCmdFunc;
};

#define CON_COMMAND(name, desc)                            \
	static void __CmdFunc_##name();                        \
	static ConCommand name(#name, __CmdFunc_##name, desc); \
	static void __CmdFunc_##name()

/**
 * CvarSystem - cvar manager
 */
class CvarSystem
{
public:
	static void RegisterCvars();
	static ConItemBase *FindItem(const char *name);
	static ConVar *FindCvar(const char *name);
	static ConVar *FindCvar(cvar_t *cvar);
};

#endif
