#include <string_view>
#include "hud.h"
#include "cl_util.h"
#include "opengl.h"
#include "sdl_rt.h"
#include "engine_patches.h"

static CClientOpenGL s_Instance;

#ifdef GLAD_DEBUG
static bool s_bInBegin = false;

static const char *getGlErrorString(GLenum errorCode)
{
	switch (errorCode)
	{
	case GL_NO_ERROR:
		return "GL_NO_ERROR";
	case GL_INVALID_ENUM:
		return "GL_INVALID_ENUM";
	case GL_INVALID_VALUE:
		return "GL_INVALID_VALUE";
	case GL_INVALID_OPERATION:
		return "GL_INVALID_OPERATION";
	case GL_OUT_OF_MEMORY:
		return "GL_OUT_OF_MEMORY";
	default:
		return "< Unknown >";
	}
}

static void GladDebugPostCallback(const char *name, void *funcptr, int len_args, ...)
{
	// Can't call glGetError between glBegin and glEnd
	if (s_bInBegin)
		return;

	GLenum errorCode = glad_glGetError();

	if (errorCode != GL_NO_ERROR)
	{
		ConPrintf(ConColor::Red, "OpenGL Error %s (%d) in %s\n", getGlErrorString(errorCode), errorCode, name);
		DebuggerBreakIfDebugging();
	}
}

static void APIENTRY glBeginDebug(GLenum mode)
{
	Assert(!s_bInBegin);
	s_bInBegin = true;
	glad_glBegin(mode);
}

static void APIENTRY glEndDebug()
{
	Assert(s_bInBegin);
	s_bInBegin = false;
	glad_glEnd();
}
#endif

static void APIENTRY OpenGLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
{
	std::string_view sourceStr;
	std::string_view typeStr;
	std::string_view severityStr;

	//----------------------------------------
	switch (source)
	{
	case GL_DEBUG_SOURCE_API_ARB:
	{
		sourceStr = "GL API";
		break;
	}
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:
	{
		sourceStr = "Window System";
		break;
	}
	case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB:
	{
		sourceStr = "Shader Compiler";
		break;
	}
	case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:
	{
		sourceStr = "Third-Party";
		break;
	}
	case GL_DEBUG_SOURCE_APPLICATION_ARB:
	{
		sourceStr = "Application";
		break;
	}
	case GL_DEBUG_SOURCE_OTHER_ARB:
	{
		sourceStr = "Other";
		break;
	}
	default:
	{
		sourceStr = "Unknown";
		break;
	}
	}

	//----------------------------------------
	switch (type)
	{
	case GL_DEBUG_TYPE_ERROR_ARB:
	{
		typeStr = "Error";
		break;
	}
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB:
	{
		typeStr = "Deprecated Behavior";
		break;
	}
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:
	{
		typeStr = "Undefined Behavior";
		break;
	}
	case GL_DEBUG_TYPE_PORTABILITY_ARB:
	{
		typeStr = "Portability";
		break;
	}
	case GL_DEBUG_TYPE_PERFORMANCE_ARB:
	{
		typeStr = "Performance";
		break;
	}
	case GL_DEBUG_TYPE_OTHER_ARB:
	{
		typeStr = "Other";
		break;
	}
	default:
	{
		typeStr = "Unknown";
		break;
	}
	}

	//----------------------------------------
	switch (severity)
	{
	case GL_DEBUG_SEVERITY_HIGH_ARB:
	{
		severityStr = "High";
		break;
	}
	case GL_DEBUG_SEVERITY_MEDIUM_ARB:
	{
		severityStr = "Medium";
		break;
	}
	case GL_DEBUG_SEVERITY_LOW_ARB:
	{
		severityStr = "Low";
		break;
	}
	default:
	{
		severityStr = "Unknown";
		break;
	}
	}

	//----------------------------------------
	ConPrintf(ConColor::Red, "OpenGL: {}: {}", id, std::string_view(message, length));
	ConPrintf(ConColor::Red, "OpenGL: Source: {}", sourceStr);
	ConPrintf(ConColor::Red, "OpenGL: Type: {}", typeStr);
	ConPrintf(ConColor::Red, "OpenGL: Severity: {}", severityStr);
	DebuggerBreakIfDebugging();
}

CClientOpenGL &CClientOpenGL::Get()
{
	return s_Instance;
}

void CClientOpenGL::Init()
{
	if (CEnginePatches::Get().GetRenderer() != CEnginePatches::Renderer::OpenGL)
		return;

#ifdef GLAD_DEBUG
	glad_set_post_callback(&GladDebugPostCallback);
#endif

	m_bIsLoaded = false;

	if (GetSDL()->IsGood())
	{
		// Load using SDL2
		m_bIsLoaded = gladLoadGLLoader((GLADloadproc)GetSDL()->GL_GetProcAddress);
	}
#ifdef PLATFORM_WINDOWS
	else
	{
		// Load using WGL
		m_bIsLoaded = gladLoadGL();
	}
#endif

	if (m_bIsLoaded)
	{
		gEngfuncs.Con_DPrintf("Loaded client OpenGL %d.%d\n", GLVersion.major, GLVersion.minor);

		if (GLAD_GL_VERSION_2_0)
		{
			gEngfuncs.Con_DPrintf("GL_VERSION: %s\n", glGetString(GL_VERSION));

#ifdef GLAD_DEBUG
			glad_debug_glBegin = &glBeginDebug;
			glad_debug_glEnd = &glEndDebug;
#endif

			if (GLAD_GL_ARB_debug_output)
			{
				gEngfuncs.Con_DPrintf("ARB_debug_output found\n");
				glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
				glDebugMessageCallbackARB(&OpenGLDebugCallback, nullptr);
			}
		}
		else
		{
			ConPrintf(ConColor::Red, "OpenGL 2.0 failed to load on the client.\n");
			m_bIsLoaded = false;
		}
	}
	else
	{
		ConPrintf(ConColor::Red, "OpenGL failed to load on the client.\n");
	}
}

void CClientOpenGL::Shutdown()
{
	if (IsAvailable() && GLAD_GL_ARB_debug_output)
	{
		// Disable debug callback
		glDebugMessageCallbackARB(nullptr, nullptr);
	}
}
