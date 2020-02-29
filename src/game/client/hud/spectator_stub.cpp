#include "hud.h"
#include "cl_util.h"
#include "spectator.h"

DEFINE_HUD_ELEM(CHudSpectator);

void CHudSpectator::Reset() { }
int CHudSpectator::ToggleInset(bool allowOff) { return 0; }
void CHudSpectator::CheckSettings() { }
void CHudSpectator::InitHudData(void) { }
bool CHudSpectator::AddOverviewEntityToList(HSPRITE sprite, cl_entity_t *ent, double killTime) { return false; }
void CHudSpectator::DeathMessage(int victim) { }
bool CHudSpectator::AddOverviewEntity(int type, struct cl_entity_s *ent, const char *modelname) { return false; }
void CHudSpectator::CheckOverviewEntities() { }
void CHudSpectator::DrawOverview() { }
void CHudSpectator::DrawOverviewEntities() { }
void CHudSpectator::GetMapPosition(float *returnvec) { }
void CHudSpectator::DrawOverviewLayer() { }
void CHudSpectator::LoadMapSprites() { }
bool CHudSpectator::ParseOverviewFile() { return false; }
bool CHudSpectator::IsActivePlayer(cl_entity_t *ent) { return false; }
void CHudSpectator::SetModes(int iMainMode, int iInsetMode) { }
void CHudSpectator::HandleButtonsDown(int ButtonPressed) { }
void CHudSpectator::HandleButtonsUp(int ButtonPressed) { }
void CHudSpectator::FindNextPlayer(bool bReverse) { }
void CHudSpectator::FindPlayer(const char *name) { }
void CHudSpectator::DirectorMessage(int iSize, void *pbuf) { }
void CHudSpectator::SetSpectatorStartPosition() { }
void CHudSpectator::Init() { }
void CHudSpectator::VidInit() { }
void CHudSpectator::Draw(float flTime) { }
void CHudSpectator::AddWaypoint(float time, vec3_t pos, vec3_t angle, float fov, int flags) { }
void CHudSpectator::SetCameraView(vec3_t pos, vec3_t angle, float fov) { }
float CHudSpectator::GetFOV() { return 0; }
bool CHudSpectator::GetDirectorCamera(vec3_t &position, vec3_t &angle) { return false; }
void CHudSpectator::SetWayInterpolation(cameraWayPoint_t *prev, cameraWayPoint_t *start, cameraWayPoint_t *end, cameraWayPoint_t *next) { }
