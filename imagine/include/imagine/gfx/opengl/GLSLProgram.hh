#pragma once

/*  This file is part of Imagine.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Imagine.  If not, see <http://www.gnu.org/licenses/> */

#include <imagine/config/defs.hh>
#include "glIncludes.h"
#include "defs.hh"
#include <imagine/util/memory/UniqueResource.hh>
#include <utility>
#include <compare>

namespace Gfx
{

class RendererTask;
class RendererCommands;

// Shader

using NativeShader = GLuint;

void destroyGLShader(RendererTask &, NativeShader);

struct GLShaderDeleter
{
	RendererTask *rTask{};

	void operator()(NativeShader s) const
	{
		destroyGLShader(*rTask, s);
	}
};
using UniqueGLShader = IG::UniqueResource<NativeShader, GLShaderDeleter>;

using ShaderImpl = UniqueGLShader;

// Program

using NativeProgram = GLuint;

void destroyGLProgram(RendererTask &, NativeProgram);

struct GLProgramDeleter
{
	RendererTask *rTask{};

	void operator()(NativeProgram p) const
	{
		destroyGLProgram(*rTask, p);
	}
};
using UniqueGLProgram = IG::UniqueResource<NativeProgram, GLProgramDeleter>;

struct NativeProgramBundle
{
	NativeProgram program{};
	GLint mvpUniform = -1;
};

class GLSLProgram
{
public:
	constexpr GLSLProgram() {}
	constexpr GLint modelViewProjectionUniform() const { return mvpUniform; };
	constexpr NativeProgram glProgram() const { return program_; }
	constexpr bool operator ==(GLSLProgram const &rhs) const { return program_.get() == rhs.program_.get(); }
	constexpr NativeProgramBundle programBundle() const { return {program_, mvpUniform}; }
	constexpr NativeProgramBundle releaseProgramBundle() { return {program_.release(), mvpUniform}; }
	constexpr RendererTask &task() { return *program_.get_deleter().rTask; }

protected:
	UniqueGLProgram program_{};
	GLint mvpUniform = -1;
};

using ProgramImpl = GLSLProgram;

}
