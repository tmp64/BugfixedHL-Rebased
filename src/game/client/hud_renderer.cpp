#include "hud.h"
#include "cl_util.h"
#include "hud_renderer.h"
#include "engine_patches.h"
#include <com_model.h>
#include <triangleapi.h>

ConVar hud_client_renderer("hud_client_renderer", "1", FCVAR_BHL_ARCHIVE, "Enable client-side HUD rendering (instead of engine renderer)");

static CHudRenderer s_Instance;

CHudRenderer &CHudRenderer::Get()
{
	return s_Instance;
}

bool CHudRenderer::IsAvailable()
{
	return CEnginePatches::Get().GetRenderer() == CEnginePatches::Renderer::OpenGL;
}

void CHudRenderer::HookFuncs()
{
	if (!IsAvailable() || !hud_client_renderer.GetBool())
		return;

	gEngfuncs.pfnSPR_Set = &SpriteSet;
	gEngfuncs.pfnSPR_Draw = &SpriteDraw;
	gEngfuncs.pfnSPR_DrawHoles = nullptr;
	gEngfuncs.pfnSPR_DrawAdditive = &SpriteDrawAdditive;
	gEngfuncs.pfnSPR_EnableScissor = nullptr;
	gEngfuncs.pfnSPR_DisableScissor = nullptr;
	gEngfuncs.pfnSPR_DrawGeneric = nullptr;
}

void CHudRenderer::SpriteSet(HSPRITE hPic, int r, int g, int b)
{
	s_Instance.m_hPic = hPic;
	s_Instance.m_SpriteColor[0] = r;
	s_Instance.m_SpriteColor[1] = g;
	s_Instance.m_SpriteColor[2] = b;
	s_Instance.m_SpriteColor[3] = 255;
}

void CHudRenderer::SpriteDraw(int frame, int x, int y, const wrect_t *prc)
{
	s_Instance.DrawSprite(frame, x, y, -1, -1, prc, SpriteDrawMode::Normal);
}

void CHudRenderer::SpriteDrawAdditive(int frame, int x, int y, const wrect_t *prc)
{
	s_Instance.DrawSprite(frame, x, y, -1, -1, prc, SpriteDrawMode::Additive);
}

void CHudRenderer::DrawSprite(int frame, float x, float y, float width, float height, const wrect_t *prc, SpriteDrawMode mode)
{
	if (m_hPic <= 0)
	{
		gEngfuncs.Con_DPrintf("CHudRenderer::DrawSprite: Invalid sprite %d\n", m_hPic);
		return;
	}

	// This function is based on Xash3D code
	if (width == -1 && height == -1)
	{
		// assume we get sizes from image
		width = gEngfuncs.pfnSPR_Width(m_hPic, frame);
		height = gEngfuncs.pfnSPR_Height(m_hPic, frame);
	}

	float s[2], t[2];

	if (prc)
	{
		wrect_t rc = *prc;

		// Sigh! some stupid modmakers set wrong rectangles in hud.txt
		if (rc.left <= 0 || rc.left >= width)
			rc.left = 0;
		if (rc.top <= 0 || rc.top >= height)
			rc.top = 0;
		if (rc.right <= 0 || rc.right > width)
			rc.right = width;
		if (rc.bottom <= 0 || rc.bottom > height)
			rc.bottom = height;

		// calc user-defined rectangle
		s[0] = (float)rc.left / width;
		t[0] = (float)rc.top / height;
		s[1] = (float)rc.right / width;
		t[1] = (float)rc.bottom / height;
		width = rc.right - rc.left;
		height = rc.bottom - rc.top;
	}
	else
	{
		s[0] = t[0] = 0.0f;
		s[1] = t[1] = 1.0f;
	}

	const model_t *spriteModel = gEngfuncs.GetSpritePointer(m_hPic);

	if (!spriteModel)
	{
		gEngfuncs.Con_DPrintf("CHudRenderer::DrawSprite: Null model for sprite %d\n", m_hPic);
		return;
	}

	gEngfuncs.pTriAPI->Color4ub(m_SpriteColor[0], m_SpriteColor[1], m_SpriteColor[2], m_SpriteColor[3]);
	gEngfuncs.pTriAPI->SpriteTexture(spriteModel, frame);

	if (mode == SpriteDrawMode::Normal)
		gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
	else if (mode == SpriteDrawMode::Additive)
		gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);

	// TriangleAPI functions call OpenGL directly
	gEngfuncs.pTriAPI->Begin(TRI_QUADS);
	gEngfuncs.pTriAPI->TexCoord2f(s[0], t[0]);
	gEngfuncs.pTriAPI->Vertex3f(x, y, 0);

	gEngfuncs.pTriAPI->TexCoord2f(s[1], t[0]);
	gEngfuncs.pTriAPI->Vertex3f(x + width, y, 0);

	gEngfuncs.pTriAPI->TexCoord2f(s[1], t[1]);
	gEngfuncs.pTriAPI->Vertex3f(x + width, y + height, 0);

	gEngfuncs.pTriAPI->TexCoord2f(s[0], t[1]);
	gEngfuncs.pTriAPI->Vertex3f(x, y + height, 0);
	gEngfuncs.pTriAPI->End();

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}
