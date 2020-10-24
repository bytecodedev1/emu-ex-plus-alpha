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

#include <imagine/gfx/PixmapBufferTexture.hh>

struct AHardwareBuffer;

namespace Gfx
{

class AHardwareBufferStorage: public TextureBufferStorage
{
public:
	AHardwareBufferStorage(Renderer &r, TextureConfig config, IG::ErrorCode *errorPtr);
	AHardwareBufferStorage(AHardwareBufferStorage &&o);
	~AHardwareBufferStorage();
	AHardwareBufferStorage &operator=(AHardwareBufferStorage &&o);
	IG::ErrorCode setFormat(IG::PixmapDesc desc) final;
	LockedTextureBuffer lock(uint32_t bufferFlags) final;
	void unlock(LockedTextureBuffer lockBuff, uint32_t writeFlags) final;

protected:
	AHardwareBuffer *hBuff{};
	uint32_t pitchBytes = 0;

	void deinit();
};

}
