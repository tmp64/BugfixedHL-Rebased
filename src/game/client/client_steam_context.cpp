//========= Copyright Valve Corporation, All rights reserved. ============//
#include <tier0/platform.h>
#include "hud.h"
#include "client_steam_context.h"
#include "engine_patches.h"

static CClientSteamContext g_ClientSteamContext;
CClientSteamContext &ClientSteamContext()
{
	return g_ClientSteamContext;
}

CSteamAPIContext *steamapicontext = &g_ClientSteamContext;

//-----------------------------------------------------------------------------
CClientSteamContext::CClientSteamContext()
{
	m_bActive = false;
	m_bLoggedOn = false;
	m_nAppID = 0;
}

//-----------------------------------------------------------------------------
CClientSteamContext::~CClientSteamContext()
{
}

//-----------------------------------------------------------------------------
// Purpose: Unload the steam3 engine
//-----------------------------------------------------------------------------
void CClientSteamContext::Shutdown()
{
	if (!m_bActive)
		return;

#if !defined(NO_STEAM)
	if (m_bCallbacksRegistered)
	{
		m_CallbackSteamServersDisconnected.Unregister();
		m_CallbackSteamServerConnectFailure.Unregister();
		m_CallbackSteamServersConnected.Unregister();
	}
#endif

	m_bActive = false;
	m_bLoggedOn = false;
#if !defined(NO_STEAM)
	Clear(); // Steam API context shutdown
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Initialize the steam3 connection
//-----------------------------------------------------------------------------
void CClientSteamContext::Activate()
{
	if (m_bActive)
		return;

	m_bActive = true;

	if (!SteamAPI_IsAvailable())
		return;

#if !defined(NO_STEAM)
	SteamAPI_InitSafe(); // ignore failure, that will fall out later when they don't get a valid logon cookie
	Init(); // Steam API context init

	m_CallbackSteamServersDisconnected.Register(this, &CClientSteamContext::OnSteamServersDisconnected);
	m_CallbackSteamServerConnectFailure.Register(this, &CClientSteamContext::OnSteamServerConnectFailure);
	m_CallbackSteamServersConnected.Register(this, &CClientSteamContext::OnSteamServersConnected);
	m_CallbackGameOverlayActivated.Register(this, &CClientSteamContext::OnGameOverlayActivated);
	m_bCallbacksRegistered = true;

	UpdateLoggedOnState();
	Msg("CClientSteamContext logged on = %d\n", m_bLoggedOn);
#endif
}

void CClientSteamContext::UpdateLoggedOnState()
{
	bool bPreviousLoggedOn = m_bLoggedOn;
	m_bLoggedOn = (SteamUser() && SteamUtils() && SteamUser()->BLoggedOn());

	if (!bPreviousLoggedOn && m_bLoggedOn)
	{
		// update Steam info
		m_SteamIDLocalPlayer = SteamUser()->GetSteamID();
		m_nUniverse = SteamUtils()->GetConnectedUniverse();
		m_nAppID = SteamUtils()->GetAppID();
	}

	if (bPreviousLoggedOn != m_bLoggedOn)
	{
		// Notify any listeners of the change in logged on state
		SteamLoggedOnChange_t loggedOnChange;
		loggedOnChange.bPreviousLoggedOn = bPreviousLoggedOn;
		loggedOnChange.bLoggedOn = m_bLoggedOn;
		InvokeCallbacks(loggedOnChange);
	}
}

#if !defined(NO_STEAM)
void CClientSteamContext::OnSteamServersDisconnected(SteamServersDisconnected_t *pDisconnected)
{
	UpdateLoggedOnState();
	Msg("CClientSteamContext OnSteamServersDisconnected logged on = %d\n", m_bLoggedOn);
}

void CClientSteamContext::OnSteamServerConnectFailure(SteamServerConnectFailure_t *pConnectFailure)
{
	UpdateLoggedOnState();
	Msg("CClientSteamContext OnSteamServerConnectFailure logged on = %d\n", m_bLoggedOn);
}

void CClientSteamContext::OnSteamServersConnected(SteamServersConnected_t *pConnected)
{
	UpdateLoggedOnState();
	Msg("CClientSteamContext OnSteamServersConnected logged on = %d\n", m_bLoggedOn);
}

void IN_SteamOverlayHidden();

void CClientSteamContext::OnGameOverlayActivated(GameOverlayActivated_t *pCallback)
{
	if (!pCallback->m_bActive)
		IN_SteamOverlayHidden();
}
#endif // !defined(NO_STEAM)

void CClientSteamContext::InstallCallback(CUtlDelegate<void(const SteamLoggedOnChange_t &)> delegate)
{
	m_LoggedOnCallbacks.AddToTail(delegate);
}

void CClientSteamContext::RemoveCallback(CUtlDelegate<void(const SteamLoggedOnChange_t &)> delegate)
{
	m_LoggedOnCallbacks.FindAndRemove(delegate);
}

void CClientSteamContext::InvokeCallbacks(const SteamLoggedOnChange_t &loggedOnStatus)
{
	for (int i = 0; i < m_LoggedOnCallbacks.Count(); ++i)
	{
		m_LoggedOnCallbacks[i](loggedOnStatus);
	}
}
