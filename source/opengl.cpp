// SPDX-License-Identifier: GPL-3.0

#include "opengl.h"

#if defined(__linux__) && !defined(ES_GLES)
#include "glad/glad_glx.h"
#elif defined(_WIN32)
#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "glad/glad_wgl.h"
#endif

#include <SDL2/SDL_syswm.h>

#include <cstring>



bool OpenGL::InitializeLoader(SDL_Window *window)
{
	// Initialize glad.
#ifdef ES_GLES
	if(!gladLoadGLES2Loader(&SDL_GL_GetProcAddress))
		return false;
#else
	if(!gladLoadGLLoader(&SDL_GL_GetProcAddress))
		return false;
#endif

	// Initialize WGL or GLX if necessary.
	SDL_SysWMinfo handle;
	SDL_VERSION(&handle.version);
	if(!SDL_GetWindowWMInfo(window, &handle))
		return false;

#if defined(__linux__) && !defined(ES_GLES)
	if(handle.subsystem == SDL_SYSWM_X11
		&& !gladLoadGLXLoader(
			&SDL_GL_GetProcAddress,
			handle.info.x11.display,
			DefaultScreen(handle.info.x11.display)))
		return false;
#elif defined(_WIN32)
	if(handle.subsystem == SDL_SYSWM_WINDOWS
		&& !gladLoadWGLLoader(
			&SDL_GL_GetProcAddress,
			handle.info.win.hdc))
		return false;
#endif

	return true;
}



bool OpenGL::HasAdaptiveVSyncSupport()
{
#ifdef __APPLE__
	// macOS doesn't support Adaptive VSync for OpenGL.
	return false;
#elif defined(ES_GLES)
	// EGL always support Adapative VSync.
	return true;
#elif defined(_WIN32)
	return GLAD_WGL_EXT_swap_control_tear;
#else
	return GLAD_GLX_EXT_swap_control_tear;
#endif
}



bool OpenGL::HasSwizzleSupport()
{
	return GLAD_GL_ARB_texture_swizzle || GLAD_GL_EXT_texture_swizzle;
}
