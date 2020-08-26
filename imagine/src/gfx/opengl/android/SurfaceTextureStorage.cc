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

#define LOGTAG "SurfaceTexStorage"
#include "SurfaceTextureStorage.hh"
#include "../private.hh"
#include "../../../base/android/android.hh"
#include <android/native_window_jni.h>

namespace Gfx
{

SurfaceTextureStorage::SurfaceTextureStorage(Renderer &r, GLuint &tex, bool makeSingleBuffered, IG::ErrorCode &err):
	DirectTextureStorage{GL_TEXTURE_EXTERNAL_OES}
{
	using namespace Base;
	if(unlikely(!r.support.hasExternalEGLImages))
	{
		logErr("can't init without OES_EGL_image_external extension");
		err = {ENOTSUP};
		return;
	}
	singleBuffered = makeSingleBuffered;
	r.runGLTaskSync(
		[=, this, &tex]()
		{
			auto env = jEnvForThread();
			assert(!tex);
			glGenTextures(1, &tex);
			auto surfaceTex = makeSurfaceTexture(jEnvForThread(), tex, makeSingleBuffered);
			if(!surfaceTex && makeSingleBuffered)
			{
				// fall back to buffered mode
				surfaceTex = makeSurfaceTexture(jEnvForThread(), tex, false);
			}
			if(unlikely(!surfaceTex))
				return;
			updateSurfaceTextureImage(env, surfaceTex); // set the initial display & context
			this->surfaceTex = env->NewGlobalRef(surfaceTex);
		});
	if(unlikely(!surfaceTex))
	{
		logErr("SurfaceTexture ctor failed");
		err = {EINVAL};
		return;
	}
	logMsg("made%sSurfaceTexture with texture:0x%X",
		singleBuffered ? " " : " buffered ", tex);
	auto env = jEnvForThread();
	auto localSurface = makeSurface(env, surfaceTex);
	if(unlikely(!localSurface))
	{
		logErr("Surface ctor failed");
		err = {EINVAL};
		deinit();
		return;
	}
	surface = env->NewGlobalRef(localSurface);
	nativeWin = ANativeWindow_fromSurface(env, localSurface);
	if(unlikely(!nativeWin))
	{
		logErr("ANativeWindow_fromSurface failed");
		err = {EINVAL};
		deinit();
		return;
	}
	logMsg("native window:%p from Surface:%p%s", nativeWin, localSurface, singleBuffered ? " (single-buffered)" : "");
	err = {};
}

SurfaceTextureStorage::SurfaceTextureStorage(SurfaceTextureStorage &&o)
{
	*this = std::move(o);
}

SurfaceTextureStorage &SurfaceTextureStorage::operator=(SurfaceTextureStorage &&o)
{
	deinit();
	DirectTextureStorage::operator=(o);
	surfaceTex = std::exchange(o.surfaceTex, {});
	surface = std::exchange(o.surface, {});
	nativeWin = std::exchange(o.nativeWin, {});
	bpp = o.bpp;
	singleBuffered = o.singleBuffered;
	return *this;
}

SurfaceTextureStorage::~SurfaceTextureStorage()
{
	deinit();
}

void SurfaceTextureStorage::deinit()
{
	using namespace Base;
	if(nativeWin)
	{
		logMsg("deinit SurfaceTexture, releasing window:%p", nativeWin);
		ANativeWindow_release(nativeWin);
		nativeWin = {};
	}
	auto env = jEnvForThread();
	if(surface)
	{
		releaseSurface(env, surface);
		env->DeleteGlobalRef(surface);
		surface = {};
	}
	if(surfaceTex)
	{
		releaseSurfaceTexture(env, surfaceTex);
		env->DeleteGlobalRef(surfaceTex);
		surfaceTex = {};
	}
}

IG::ErrorCode SurfaceTextureStorage::setFormat(Renderer &, IG::PixmapDesc desc, GLuint &)
{
	logMsg("setting size:%dx%d format:%s", desc.w(), desc.h(), desc.format().name());
	int winFormat = Base::toAHardwareBufferFormat(desc.format());
	if(!winFormat)
	{
		logErr("pixel format not usable");
		return {EINVAL};
	}
	if(ANativeWindow_setBuffersGeometry(nativeWin, desc.w(), desc.h(), winFormat) < 0)
	{
		logErr("ANativeWindow_setBuffersGeometry failed");
		return {EINVAL};
	}
	bpp = desc.format().bytesPerPixel();
	return {};
}

SurfaceTextureStorage::Buffer SurfaceTextureStorage::lock(Renderer &r)
{
	using namespace Base;
	if(unlikely(!nativeWin))
	{
		logErr("called lock when uninitialized");
		return {};
	}
	if(singleBuffered)
	{
		r.runGLTaskSync(
			[this]()
			{
				releaseSurfaceTextureImage(jEnvForThread(), surfaceTex);
			});
	}
	ANativeWindow_Buffer winBuffer;
	// setup the dirty rectangle, not currently needed by our use case
	/*ARect aRect;
	aRect.left = rect.x;
	aRect.top = rect.y;
	aRect.right = rect.x2;
	aRect.bottom = rect.y2;*/
	if(ANativeWindow_lock(nativeWin, &winBuffer, nullptr) < 0)
	{
		logErr("ANativeWindow_lock failed");
		return {};
	}
	/*rect.x = aRect.left;
	rect.y = aRect.top;
	rect.x2 = aRect.right;
	rect.y2 = aRect.bottom;*/
	Buffer buff{winBuffer.bits, (uint32_t)winBuffer.stride * bpp};
	//buff.data = (char*)buff.data + (aRect.top * buff.pitch + aRect.left * bpp);
	//logMsg("locked buffer %p with pitch %d", buff.data, buff.pitch);
	return buff;
}

void SurfaceTextureStorage::unlock(Renderer &r)
{
	using namespace Base;
	if(unlikely(!nativeWin))
	{
		logErr("called unlock when uninitialized");
		return;
	}
	ANativeWindow_unlockAndPost(nativeWin);
	r.runGLTask(
		[this]()
		{
			Base::updateSurfaceTextureImage(Base::jEnvForThread(), surfaceTex);
		});
}

}
