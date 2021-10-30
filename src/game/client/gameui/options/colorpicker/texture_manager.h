#ifndef COLORPICKER_CTEXTUREMANAGER_H
#define COLORPICKER_CTEXTUREMANAGER_H
#include <atomic>
#include <thread>
#include <vector>
#include <Color.h>

namespace colorpicker
{
class CTextureManager
{
public:
	static double constexpr SCALE = 0.25;

	class CWorker
	{
	public:
		static int constexpr PIXEL_SIZE = 4;

		CWorker(CTextureManager *pParent);
		CWorker(CWorker &) = delete;
		CWorker(CWorker &&) = delete;

		void StartThread();
		void StopThread();

		std::vector<unsigned char> &GetBarRgba();
		std::vector<std::vector<unsigned char>> &GetPickerRgba();

		void ClearRgba();

	private:
		CTextureManager *m_pParent = nullptr;
		std::thread m_Thread;
		std::vector<unsigned char> m_BarRgba;
		std::vector<std::vector<unsigned char>> m_PickerRgba;

		void operator()();
		void GenerateBarTexture();
		void GeneratePickerTexture();
	};

	CTextureManager(int wide, int tall);
	CTextureManager(CTextureManager &) = delete;
	CTextureManager(CTextureManager &&) = delete;

	void Init();
	void RunFrame();
	void Shutdown();

	bool IsReady();

	int GetWide();
	int GetTall();

	int GetBarTextureId();
	int GetPickerTexture(int idx);
	int GetPickerTextureIndex(float hue);

	Color GetColorForPickerPixel(int idx, int x, int y);

private:
	bool m_bIsReady = false;
	bool m_bIsError = false;
	int m_iWide = 0;
	int m_iTall = 0;
	CWorker m_Worker;

	// These are set up in RunFrame after worker thread has finished
	std::atomic_bool m_bIsThreadReady;
	int m_iBarTexture = -1;
	std::vector<int> m_PickerTextures;

	int GetTextureWide();
	int GetTextureTall();

	friend class CWorker;
};

extern CTextureManager gTexMgr;

}

inline bool colorpicker::CTextureManager::IsReady()
{
	return m_bIsReady;
}

#endif
