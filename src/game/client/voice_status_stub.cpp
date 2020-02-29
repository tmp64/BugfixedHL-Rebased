#include "hud.h"
#include "cl_util.h"
#include "voice_status.h"

CVoiceStatus *GetClientVoiceMgr()
{
	return CVoiceStatus::Get();
}

DEFINE_HUD_ELEM(CVoiceStatus);

CVoiceStatus::CVoiceStatus() { }
CVoiceStatus::~CVoiceStatus() { }
void CVoiceStatus::SetVoiceStatusHelper(IVoiceStatusHelper *pHelper) { }
void CVoiceStatus::SetParentPanel(vgui::Panel **pParentPanel) { }
void CVoiceStatus::Init()
{
	::HookMessage("VoiceMask", [](const char *pszName, int iSize, void *pbuf) {
		return 1;
	});

	::HookMessage("ReqState", [](const char *pszName, int iSize, void *pbuf) {
		return 1;
	});
}
void CVoiceStatus::VidInit() { }
void CVoiceStatus::Frame(double frametime) { }
void CVoiceStatus::CreateEntities() { }
void CVoiceStatus::UpdateSpeakerStatus(int entindex, qboolean bTalking) { }
void CVoiceStatus::UpdateServerState(bool bForce) { }
void CVoiceStatus::UpdateSpeakerImage(void *pLabel, int iPlayer) { }
void CVoiceStatus::UpdateBanButton(int iClient) { }
void CVoiceStatus::HandleVoiceMaskMsg(int iSize, void *pbuf) { }
void CVoiceStatus::HandleReqStateMsg(int iSize, void *pbuf) { }
void CVoiceStatus::StartSquelchMode() { }
void CVoiceStatus::StopSquelchMode() { }
bool CVoiceStatus::IsInSquelchMode() { return false; }
void *CVoiceStatus::FindVoiceLabel(int clientindex) { return nullptr; }
void *CVoiceStatus::GetFreeVoiceLabel() { return nullptr; }
void CVoiceStatus::RepositionLabels() { }
void CVoiceStatus::FreeBitmaps() { }
bool CVoiceStatus::IsPlayerBlocked(int iPlayer) { return false; }
bool CVoiceStatus::IsPlayerAudible(int iPlayer) { return true; }
void CVoiceStatus::SetPlayerBlockedState(int iPlayer, bool blocked) { }
