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

#include <imagine/input/Device.hh>
#include <imagine/base/EventLoop.hh>
#include <imagine/util/container/ArrayList.hh>
#include <array>

namespace IG
{
class LinuxApplication;
}

namespace IG::Input
{

class EvdevInputDevice : public Device
{
public:
	EvdevInputDevice();
	EvdevInputDevice(int id, int fd, TypeBits, std::string name);
	~EvdevInputDevice();
	void processInputEvents(LinuxApplication &app, input_event *event, uint32_t events);
	bool setupJoystickBits();
	void addPollEvent(LinuxApplication &app);
	std::span<Axis> motionAxes() final;
	int fileDesc() const;

protected:
	static constexpr unsigned AXIS_SIZE = 24;
	int fd{-1};
	StaticArrayList<Axis, AXIS_SIZE> axis{};
	std::array<int, AXIS_SIZE> axisRangeOffset{};
	FDEventSource fdSrc{-1};
};

}