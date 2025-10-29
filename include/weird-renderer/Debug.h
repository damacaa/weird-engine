#pragma once

#include <iostream>
#include <glad/glad.h>

inline void CheckOpenGLError(const char *file, int line)
{
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR)
	{
		std::cerr << "OpenGL Error (" << err << ") at " << file << ":" << line << std::endl;
	}
}

#ifndef NDEBUG
#define GL_CHECK_ERROR() CheckOpenGLError(__FILE__, __LINE__)
#else
#define GL_CHECK_ERROR()
#endif