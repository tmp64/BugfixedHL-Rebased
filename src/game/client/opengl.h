#ifndef OPENGL_H
#define OPENGL_H
#include <glad/glad.h>

class CClientOpenGL
{
public:
	static CClientOpenGL &Get();

	void Init();
	void Shutdown();
	inline bool IsAvailable() { return m_bIsLoaded; }

private:
	bool m_bIsLoaded = false;
};

#endif
