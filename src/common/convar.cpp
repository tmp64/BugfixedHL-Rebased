#include <cassert>
#include <map>
#include <string>
#include "convar.h"

#ifdef SERVER_DLL
#include "extdll.h"
#include "eiface.h"
#include "util.h"
#else
#include "wrect.h"
#include "cl_dll.h"
#endif

//---------------------------------------------------
// ConItemBase
//---------------------------------------------------
ConItemBase *ConItemBase::m_pFirstItem = nullptr;

ConItemBase::ConItemBase(const char *name)
    : ConItemBase(name, "")
{
}

ConItemBase::ConItemBase(const char *name, const char *desc)
{
	m_pName = name;
	m_pDesc = desc;
	m_pNextItem = m_pFirstItem;
	m_pFirstItem = this;
}

ConItemBase::~ConItemBase()
{
}

const char *ConItemBase::GetName()
{
	return m_pName;
}

const char *ConItemBase::GetDescription()
{
	return m_pDesc;
}

//---------------------------------------------------
// ConVar
//---------------------------------------------------
ConVar::ConVar(const char *name, const char *def_val, int flags)
    : ConVar(name, def_val, flags, "")
{
}

ConVar::ConVar(const char *name, const char *def_val, int flags, const char *desc)
    : ConItemBase(name, desc)
{
	m_pDefVal = def_val;
	m_iFlags = flags;
}

ConItemType ConVar::GetType()
{
	return ConItemType::ConVar;
}

const char *ConVar::GetDefaultValue()
{
	return m_pDefVal;
}

int ConVar::GetFlags()
{
	return m_iFlags;
}

float ConVar::GetFloat()
{
	return GetCvar()->value;
}

int ConVar::GetInt()
{
	return (int)GetCvar()->value;
}

bool ConVar::GetBool()
{
	return !!GetInt();
}

const char *ConVar::GetString()
{
	return GetCvar()->string;
}

void ConVar::SetValue(float val)
{
#ifdef SERVER_DLL
	CVAR_SET_FLOAT(GetName(), val);
#else
	gEngfuncs.Cvar_SetValue(GetName(), val);
#endif
}

void ConVar::SetValue(int val)
{
	SetValue((float)val);
}

void ConVar::SetValue(const char *val)
{
#ifdef SERVER_DLL
	CVAR_SET_STRING(GetName(), val);
#else
	gEngfuncs.Cvar_Set(GetName(), val);
#endif
}

//---------------------------------------------------
// ConVarRef
//---------------------------------------------------
ConVarRef::ConVarRef(const char *name)
{
#ifdef SERVER_DLL
	m_pCvar = CVAR_GET_POINTER(name);
#else
	m_pCvar = gEngfuncs.pfnGetCvarPointer(name);
#endif
	m_pName = name;
}

ConVarRef::ConVarRef(cvar_t *cvar)
{
	if (cvar)
	{
		m_pCvar = cvar;
		m_pName = cvar->name;
	}
}

ConVarRef::ConVarRef(ConVar &cvar)
    : ConVarRef(cvar.GetCvar())
{
}

ConVarRef::ConVarRef(ConVar *cvar)
    : ConVarRef(*cvar)
{
}

const char *ConVarRef::GetName()
{
	return m_pName;
}

bool ConVarRef::IsValid()
{
	return m_pCvar != nullptr;
}

float ConVarRef::GetFloat()
{
	if (m_pCvar)
	{
		return m_pCvar->value;
	}
	else
	{
		return 0;
	}
}

int ConVarRef::GetInt()
{
	return (int)GetFloat();
}

bool ConVarRef::GetBool()
{
	return !!GetInt();
}

const char *ConVarRef::GetString()
{
	if (m_pCvar)
	{
		return m_pCvar->string;
	}
	else
	{
		return "";
	}
}

void ConVarRef::SetValue(float val)
{
	if (m_pCvar)
	{
#ifdef SERVER_DLL
		CVAR_SET_FLOAT(m_pCvar->name, val);
#else
		gEngfuncs.Cvar_SetValue(m_pCvar->name, val);
#endif
	}
}

void ConVarRef::SetValue(int val)
{
	SetValue((float)val);
}

void ConVarRef::SetValue(const char *val)
{
	if (m_pCvar)
	{
#ifdef SERVER_DLL
		CVAR_SET_STRING(m_pCvar->name, val);
#else
		gEngfuncs.Cvar_Set(m_pCvar->name, val);
#endif
	}
}

//---------------------------------------------------
// ConCommand
//---------------------------------------------------
ConCommand::ConCommand(const char *name, CmdFunc func, const char *desc)
    : ConItemBase(name, desc)
{
	m_fnCmdFunc = func;
}

ConItemType ConCommand::GetType()
{
	return ConItemType::ConCommand;
}

ConCommand::CmdFunc ConCommand::GetCmdFunc()
{
	return m_fnCmdFunc;
}

int ConCommand::ArgC()
{
#ifdef SERVER_DLL
	return CMD_ARGC();
#else
	return gEngfuncs.Cmd_Argc();
#endif
}

const char *ConCommand::ArgV(int i)
{
#ifdef SERVER_DLL
	return CMD_ARGV(i);
#else
	return gEngfuncs.Cmd_Argv(i);
#endif
}

//---------------------------------------------------
// CvarSystem
//---------------------------------------------------
static std::map<std::string, ConItemBase *> s_ItemNameMap;
static std::map<cvar_t *, ConVar *> s_CvarMap;

void CvarSystem::RegisterCvars()
{
	bool bInDevMode = false;

#ifdef SERVER_DLL
#else
	if (gEngfuncs.CheckParm("-dev", nullptr))
		bInDevMode = true;
#endif

	ConItemBase *item = ConItemBase::m_pFirstItem;
	for (; item; item = item->m_pNextItem)
	{
		s_ItemNameMap[item->GetName()] = item;

		if (item->GetType() == ConItemType::ConVar)
		{
			ConVar *cvar = static_cast<ConVar *>(item);

			if (cvar->GetFlags() & FCVAR_DEVELOPMENTONLY && !bInDevMode)
			{
				// Do not register that cvar
#ifndef SERVER_DLL
				// On client create a dummy cvar_t
				cvar->m_pCvar = new cvar_t();
				cvar->m_pCvar->name = cvar->GetName();
				cvar->m_pCvar->string = cvar->GetDefaultValue();
				cvar->m_pCvar->flags = cvar->GetFlags();
				cvar->m_pCvar->value = std::atof(cvar->GetDefaultValue());
				cvar->m_pCvar->next = nullptr;
#endif
				continue;
			}

#ifdef SERVER_DLL
			CVAR_REGISTER(cvar->GetCvar());
#else
			cvar->m_pCvar = gEngfuncs.pfnRegisterVariable(cvar->GetName(), cvar->GetDefaultValue(), cvar->GetFlags());
#endif
			s_CvarMap[cvar->GetCvar()] = cvar;
		}
		else if (item->GetType() == ConItemType::ConCommand)
		{
			ConCommand *cmd = static_cast<ConCommand *>(item);

#ifdef SERVER_DLL
			g_engfuncs.pfnAddServerCommand((char *)cmd->GetName(), cmd->GetCmdFunc());
#else
			gEngfuncs.pfnAddCommand(cmd->GetName(), cmd->GetCmdFunc());
#endif
		}
	}
}

ConItemBase *CvarSystem::FindItem(const char *name)
{
	auto it = s_ItemNameMap.find(name);
	if (it == s_ItemNameMap.end())
		return nullptr;
	else
		return it->second;
}

ConVar *CvarSystem::FindCvar(const char *name)
{
	ConItemBase *item = FindItem(name);
	if (item && item->GetType() == ConItemType::ConVar)
		return static_cast<ConVar *>(item);
	else
		return nullptr;
}

ConVar *CvarSystem::FindCvar(cvar_t *cvar)
{
	auto it = s_CvarMap.find(cvar);
	if (it == s_CvarMap.end())
		return nullptr;
	else
		return it->second;
}
