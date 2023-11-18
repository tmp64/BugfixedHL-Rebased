#include <Color.h>
#include "compat/ISurfaceCompat.h"

using namespace vgui2;

class CSurfaceCompat final : public ISurface
{
public:
	CSurfaceCompat(ISurfaceCompat* pIface)
	{
		m_pIface = pIface;
	}

	virtual void Shutdown() override
	{
		m_pIface->Shutdown();
	}

	virtual void RunFrame() override
	{
		m_pIface->RunFrame();
	}

	virtual VPANEL GetEmbeddedPanel() override
	{
		return m_pIface->GetEmbeddedPanel();
	}

	virtual void SetEmbeddedPanel(VPANEL pPanel) override
	{
		m_pIface->SetEmbeddedPanel(pPanel);
	}

	virtual void PushMakeCurrent(VPANEL panel, bool useInsets) override
	{
		m_pIface->PushMakeCurrent(panel, useInsets);
	}

	virtual void PopMakeCurrent(VPANEL panel) override
	{
		m_pIface->PopMakeCurrent(panel);
	}

	virtual void DrawSetColor(int r, int g, int b, int a) override
	{
		m_pIface->DrawSetColor(r, g, b, a);
	}

	virtual void DrawSetColor(Color col) override
	{
		m_pIface->DrawSetColor(col);
	}

	virtual void DrawFilledRect(int x0, int y0, int x1, int y1) override
	{
		m_pIface->DrawFilledRect(x0, y0, x1, y1);
	}

	virtual void DrawOutlinedRect(int x0, int y0, int x1, int y1) override
	{
		m_pIface->DrawOutlinedRect(x0, y0, x1, y1);
	}

	virtual void DrawLine(int x0, int y0, int x1, int y1) override
	{
		m_pIface->DrawLine(x0, y0, x1, y1);
	}

	virtual void DrawPolyLine(int *px, int *py, int numPoints) override
	{
		m_pIface->DrawPolyLine(px, py, numPoints);
	}

	virtual void DrawSetTextFont(HFont font) override
	{
		m_pIface->DrawSetTextFont(font);
	}

	virtual void DrawSetTextColor(int r, int g, int b, int a) override
	{
		m_pIface->DrawSetTextColor(r, g, b, a);
	}

	virtual void DrawSetTextColor(Color col) override
	{
		m_pIface->DrawSetTextColor(col);
	}

	virtual void DrawSetTextPos(int x, int y) override
	{
		m_pIface->DrawSetTextPos(x, y);
	}

	virtual void DrawGetTextPos(int &x, int &y) override
	{
		m_pIface->DrawGetTextPos(x, y);
	}

	virtual void DrawPrintText(const wchar_t *text, int textLen) override
	{
		m_pIface->DrawPrintText(text, textLen);
	}

	virtual void DrawUnicodeChar(wchar_t wch) override
	{
		m_pIface->DrawUnicodeChar(wch);
	}

	virtual void DrawUnicodeCharAdd(wchar_t wch) override
	{
		m_pIface->DrawUnicodeCharAdd(wch);
	}

	virtual void DrawFlushText() override
	{
		m_pIface->DrawFlushText();
	}

	virtual IHTML *CreateHTMLWindow(vgui2::IHTMLEvents *events, VPANEL context) override
	{
		return m_pIface->CreateHTMLWindow(events, context);
	}

	virtual void PaintHTMLWindow(vgui2::IHTML *htmlwin) override
	{
		m_pIface->PaintHTMLWindow(htmlwin);
	}

	virtual void DeleteHTMLWindow(IHTML *htmlwin) override
	{
		m_pIface->DeleteHTMLWindow(htmlwin);
	}

	virtual void DrawSetTextureFile(int id, const char *filename, int hardwareFilter, bool forceReload) override
	{
		m_pIface->DrawSetTextureFile(id, filename, hardwareFilter, forceReload);
	}

	virtual void DrawSetTextureRGBA(int id, const unsigned char *rgba, int wide, int tall, int hardwareFilter, bool forceReload) override
	{
		m_pIface->DrawSetTextureRGBA(id, rgba, wide, tall, hardwareFilter, forceReload);
	}

	virtual void DrawSetTexture(int id) override
	{
		m_pIface->DrawSetTexture(id);
	}

	virtual void DrawGetTextureSize(int id, int &wide, int &tall) override
	{
		m_pIface->DrawGetTextureSize(id, wide, tall);
	}

	virtual void DrawTexturedRect(int x0, int y0, int x1, int y1) override
	{
		m_pIface->DrawTexturedRect(x0, y0, x1, y1);
	}

	virtual void DrawTexturedRectAdd(int x0, int y0, int x1, int y1) override
	{
		m_pIface->DrawTexturedRect(x0, y0, x1, y1);
	}

	virtual bool IsTextureIDValid(int id) override
	{
		return m_pIface->IsTextureIDValid(id);
	}

	virtual int CreateNewTextureID(bool procedural) override
	{
		return m_pIface->CreateNewTextureID(procedural);
	}

	virtual void GetScreenSize(int &wide, int &tall) override
	{
		m_pIface->GetScreenSize(wide, tall);
	}

	virtual void SetAsTopMost(VPANEL panel, bool state) override
	{
		m_pIface->SetAsTopMost(panel, state);
	}

	virtual void BringToFront(VPANEL panel) override
	{
		m_pIface->BringToFront(panel);
	}

	virtual void SetForegroundWindow(VPANEL panel) override
	{
		m_pIface->SetForegroundWindow(panel);
	}

	virtual void SetPanelVisible(VPANEL panel, bool state) override
	{
		m_pIface->SetPanelVisible(panel, state);
	}

	virtual void SetMinimized(VPANEL panel, bool state) override
	{
		m_pIface->SetMinimized(panel, state);
	}

	virtual bool IsMinimized(VPANEL panel) override
	{
		return m_pIface->IsMinimized(panel);
	}

	virtual void FlashWindow(VPANEL panel, bool state) override
	{
		m_pIface->FlashWindow(panel, state);
	}

	virtual void SetTitle(VPANEL panel, const wchar_t *title) override
	{
		m_pIface->SetTitle(panel, title);
	}

	virtual void SetAsToolBar(VPANEL panel, bool state) override
	{
		m_pIface->SetAsToolBar(panel, state);
	}

	virtual void SetSupportsEsc(bool bSupportsEsc) override
	{
		// Not implemented
		std::abort();
	}

	virtual void CreatePopup(VPANEL panel, bool minimised, bool showTaskbarIcon, bool disabled, bool mouseInput, bool kbInput) override
	{
		m_pIface->CreatePopup(panel, minimised, showTaskbarIcon, disabled, mouseInput, kbInput);
	}

	virtual void SwapBuffers(VPANEL panel) override
	{
		m_pIface->SwapBuffers(panel);
	}

	virtual void Invalidate(VPANEL panel) override
	{
		m_pIface->Invalidate(panel);
	}

	virtual void SetCursor(HCursor cursor) override
	{
		m_pIface->SetCursor(cursor);
	}

	virtual bool IsCursorVisible() override
	{
		return m_pIface->IsCursorVisible();
	}

	virtual void ApplyChanges() override
	{
		m_pIface->ApplyChanges();
	}

	virtual bool IsWithin(int x, int y) override
	{
		return m_pIface->IsWithin(x, y);
	}

	virtual bool HasFocus() override
	{
		return m_pIface->HasFocus();
	}

	virtual bool SupportsFeature(SurfaceFeature_e feature) override
	{
		return m_pIface->SupportsFeature(feature);
	}

	virtual void RestrictPaintToSinglePanel(VPANEL panel) override
	{
		m_pIface->RestrictPaintToSinglePanel(panel);
	}

	virtual void SetModalPanel(VPANEL _arg0) override
	{
		m_pIface->SetModalPanel(_arg0);
	}

	virtual VPANEL GetModalPanel() override
	{
		return m_pIface->GetModalPanel();
	}

	virtual void UnlockCursor() override
	{
		m_pIface->UnlockCursor();
	}

	virtual void LockCursor() override
	{
		m_pIface->LockCursor();
	}

	virtual void SetTranslateExtendedKeys(bool state) override
	{
		m_pIface->SetTranslateExtendedKeys(state);
	}

	virtual VPANEL GetTopmostPopup() override
	{
		return m_pIface->GetTopmostPopup();
	}

	virtual void SetTopLevelFocus(VPANEL panel) override
	{
		m_pIface->SetTopLevelFocus(panel);
	}

	virtual HFont CreateFont() override
	{
		return m_pIface->CreateFont();
	}

	virtual bool AddGlyphSetToFont(HFont font, const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags, int lowRange, int highRange) override
	{
		return m_pIface->AddGlyphSetToFont(font, windowsFontName, tall, weight, blur, scanlines, flags, lowRange, highRange);
	}

	virtual bool AddCustomFontFile(const char *fontFileName) override
	{
		return m_pIface->AddCustomFontFile(fontFileName);
	}

	virtual int GetFontTall(HFont font) override
	{
		return m_pIface->GetFontTall(font);
	}

	virtual void GetCharABCwide(HFont font, int ch, int &a, int &b, int &c) override
	{
		m_pIface->GetCharABCwide(font, ch, a, b, c);
	}

	virtual int GetCharacterWidth(HFont font, int ch) override
	{
		return m_pIface->GetCharacterWidth(font, ch);
	}

	virtual int GetFontBlur(HFont font)
	{
		return 0;
	}

	virtual bool IsAdditive(HFont font)
	{
		return false;
	}

	virtual void GetTextSize(HFont font, const wchar_t *text, int &wide, int &tall) override
	{
		return m_pIface->GetTextSize(font, text, wide, tall);
	}

	virtual VPANEL GetNotifyPanel() override
	{
		return m_pIface->GetNotifyPanel();
	}

	virtual void SetNotifyIcon(VPANEL context, HTexture icon, VPANEL panelToReceiveMessages, const char *text) override
	{
		m_pIface->SetNotifyIcon(context, icon, panelToReceiveMessages, text);
	}

	virtual void PlaySound(const char *fileName) override
	{
		m_pIface->PlaySound(fileName);
	}

	virtual int GetPopupCount() override
	{
		return m_pIface->GetPopupCount();
	}

	virtual VPANEL GetPopup(int index) override
	{
		return m_pIface->GetPopup(index);
	}

	virtual bool ShouldPaintChildPanel(VPANEL childPanel) override
	{
		return m_pIface->ShouldPaintChildPanel(childPanel);
	}

	virtual bool RecreateContext(VPANEL panel) override
	{
		return m_pIface->RecreateContext(panel);
	}

	virtual void AddPanel(VPANEL panel) override
	{
		m_pIface->AddPanel(panel);
	}

	virtual void ReleasePanel(VPANEL panel) override
	{
		m_pIface->ReleasePanel(panel);
	}

	virtual void MovePopupToFront(VPANEL panel) override
	{
		m_pIface->MovePopupToFront(panel);
	}

	virtual void MovePopupToBack(VPANEL panel) override
	{
		m_pIface->MovePopupToBack(panel);
	}

	virtual void SolveTraverse(VPANEL panel, bool forceApplySchemeSettings) override
	{
		m_pIface->SolveTraverse(panel, forceApplySchemeSettings);
	}

	virtual void PaintTraverse(VPANEL panel) override
	{
		m_pIface->PaintTraverse(panel);
	}

	virtual void EnableMouseCapture(VPANEL panel, bool state) override
	{
		m_pIface->EnableMouseCapture(panel, state);
	}

	virtual void GetWorkspaceBounds(int &x, int &y, int &wide, int &tall) override
	{
		m_pIface->GetWorkspaceBounds(x, y, wide, tall);
	}

	virtual void GetAbsoluteWindowBounds(int &x, int &y, int &wide, int &tall) override
	{
		m_pIface->GetAbsoluteWindowBounds(x, y, wide, tall);
	}

	virtual void GetProportionalBase(int &width, int &height) override
	{
		m_pIface->GetProportionalBase(width, height);
	}

	virtual void SetProportionalBase(int width, int height) override
	{
		// Not implemented
		std::abort();
	}

	virtual void CalculateMouseVisible() override
	{
		m_pIface->CalculateMouseVisible();
	}

	virtual bool NeedKBInput() override
	{
		return m_pIface->NeedKBInput();
	}

	virtual bool HasCursorPosFunctions() override
	{
		return m_pIface->HasCursorPosFunctions();
	}

	virtual void SurfaceGetCursorPos(int &x, int &y) override
	{
		m_pIface->SurfaceGetCursorPos(x, y);
	}

	virtual void SurfaceSetCursorPos(int x, int y) override
	{
		m_pIface->SurfaceSetCursorPos(x, y);
	}

	virtual void DrawTexturedPolygon(VGuiVertex *pVertices, int n) override
	{
		m_pIface->DrawTexturedPolygon(pVertices, n);
	}

	virtual int GetFontAscent(HFont font, wchar_t wch) override
	{
		return m_pIface->GetFontAscent(font, wch);
	}

	virtual void SetAllowHTMLJavaScript(bool state) override
	{
		m_pIface->SetAllowHTMLJavaScript(state);
	}

	virtual void SetLanguage(const char *pchLang) override
	{
		m_pIface->SetLanguage(pchLang);
	}

	virtual const char *GetLanguage() override
	{
		return m_pIface->GetLanguage();
	}

	virtual bool DeleteTextureByID(int id) override
	{
		return m_pIface->DeleteTextureByID(id);
	}

	virtual void DrawUpdateRegionTextureBGRA(int nTextureID, int x, int y, const unsigned char *pchData, int wide, int tall) override
	{
		m_pIface->DrawUpdateRegionTextureBGRA(nTextureID, x, y, pchData, wide, tall);
	}

	virtual void DrawSetTextureBGRA(int id, const unsigned char *pchData, int wide, int tall) override
	{
		m_pIface->DrawSetTextureBGRA(id, pchData, wide, tall);
	}

	virtual void CreateBrowser(vgui2::VPANEL panel, IHTMLResponses *pBrowser, bool bPopupWindow, const char *pchUserAgentIdentifier) override
	{
		m_pIface->CreateBrowser(panel, pBrowser, bPopupWindow, pchUserAgentIdentifier);
	}

	virtual void RemoveBrowser(vgui2::VPANEL panel, IHTMLResponses *pBrowser) override
	{
		m_pIface->RemoveBrowser(panel, pBrowser);
	}

	virtual IHTMLChromeController *AccessChromeHTMLController() override
	{
		return m_pIface->AccessChromeHTMLController();
	}

private:
	ISurfaceCompat *m_pIface = nullptr;
};

vgui2::ISurface *Compat_CreateSurface(vgui2::ISurfaceCompat *pIface)
{
	return new CSurfaceCompat(pIface);
}
