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
#include <imagine/gfx/defs.hh>

#ifdef CONFIG_GFX_OPENGL
#include <imagine/gfx/opengl/GLRendererCommands.hh>
#endif

#include <imagine/gfx/SyncFence.hh>
#include <span>

namespace IG
{
class Viewport;
}

namespace IG::Gfx
{

class Renderer;
class Program;
class Texture;
class Mat4;
class BasicEffect;

enum class Primitive
{
	TRIANGLE = TRIANGLE_IMPL,
	TRIANGLE_STRIP = TRIANGLE_STRIP_IMPL
};

enum class BlendFunc
{
	ZERO = ZERO_IMPL,
	ONE = ONE_IMPL,
	SRC_COLOR = SRC_COLOR_IMPL,
	ONE_MINUS_SRC_COLOR = ONE_MINUS_SRC_COLOR_IMPL,
	DST_COLOR = DST_COLOR_IMPL,
	ONE_MINUS_DST_COLOR = ONE_MINUS_DST_COLOR_IMPL,
	SRC_ALPHA = SRC_ALPHA_IMPL,
	ONE_MINUS_SRC_ALPHA = ONE_MINUS_SRC_ALPHA_IMPL,
	DST_ALPHA = DST_ALPHA_IMPL,
	ONE_MINUS_DST_ALPHA = ONE_MINUS_DST_ALPHA_IMPL,
	CONSTANT_COLOR = CONSTANT_COLOR_IMPL,
	ONE_MINUS_CONSTANT_COLOR = ONE_MINUS_CONSTANT_COLOR_IMPL,
	CONSTANT_ALPHA = CONSTANT_ALPHA_IMPL,
	ONE_MINUS_CONSTANT_ALPHA = ONE_MINUS_CONSTANT_ALPHA_IMPL,
};

class RendererCommands : public RendererCommandsImpl
{
public:
	using RendererCommandsImpl::RendererCommandsImpl;
	void present();
	void presentAtTime(IG::FrameTime time);
	void presentNow();
	void setRenderTarget(Texture &t);
	void setDefaultRenderTarget();
	Renderer &renderer() const;

	// render states

	void setBlend(bool on);
	void setBlendFunc(BlendFunc s, BlendFunc d);
	void setBlendMode(BlendMode);
	void set(BlendMode mode) { setBlendMode(mode); }
	void setBlendEquation(BlendEquation);
	void set(BlendEquation mode) { setBlendEquation(mode); }
	void setZTest(bool on);
	void setClearColor(float r, float g, float b, float a = 1.);
	void setColor(Color4F);
	void setColor(float r, float g, float b, float a = 1.);
	void setColor(float i) { setColor(i, i, i, 1.); }
	void set(ColorName c) { setColor(::IG::Gfx::color(c)); }
	Color4F color() const;
	void setImgMode(EnvMode);
	void setDither(bool on);
	bool dither();
	void setSrgbFramebufferWrite(bool on);
	void setVisibleGeomFace(Faces);
	void setClipTest(bool on);
	void setClipRect(ClipRect b);
	void setTexture(const Texture &t);
	void set(TextureBinding);
	void setTextureSampler(const TextureSampler &sampler);
	void setViewport(Viewport v);
	void restoreViewport();
	void vertexBufferData(const void *v, size_t size);
	void bindTempVertexBuffer();
	void flush();

	// shaders

	void setProgram(NativeProgram);
	void setProgram(const Program &program);
	void uniform(int location, float v1);
	void uniform(int location, float v1, float v2);
	void uniform(int location, float v1, float v2, float v3);
	void uniform(int location, float v1, float v2, float v3, float v4);
	void uniform(int location, int v1);
	void uniform(int location, int v1, int v2);
	void uniform(int location, int v1, int v2, int v3);
	void uniform(int location, int v1, int v2, int v3, int v4);
	void uniform(int location, Mat4);
	BasicEffect &basicEffect();

	// synchronization
	SyncFence addSyncFence();
	void deleteSyncFence(SyncFence fence);
	void clientWaitSync(SyncFence fence, int flags = 0, std::chrono::nanoseconds timeoutNS = SyncFence::IGNORE_TIMEOUT);
	SyncFence clientWaitSyncReset(SyncFence oldFence, int flags = 0, std::chrono::nanoseconds timeoutNS = SyncFence::IGNORE_TIMEOUT);
	void waitSync(SyncFence fence);

	// rendering

	void setVertexAttribs(VertexLayout auto *v) { RendererCommandsImpl::setVertexAttribs(v); }
	void clear();
	void drawPrimitives(Primitive mode, int start, int count);
	void drawPrimitiveElements(Primitive, std::span<const VertexIndex>);
};

}
