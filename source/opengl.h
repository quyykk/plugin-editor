// SPDX-License-Identifier: GPL-3.0

#ifndef ES_OPENGL_H_
#define ES_OPENGL_H_

#include "glad/glad.h"

#include <SDL2/SDL.h>



// A helper class for various OpenGL platform specific calls.
class OpenGL
{
public:
	static bool InitializeLoader(SDL_Window *window);

	static bool HasAdaptiveVSyncSupport();
	static bool HasSwizzleSupport();
};



#endif
