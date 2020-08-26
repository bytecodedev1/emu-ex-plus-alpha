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

#include <imagine/base/Error.hh>
#include <string>

namespace IG
{

class GeneralErrorCategory final : public std::error_category
{
public:
	const char* name() const noexcept final
	{
		return "general";
	}

	std::string message(int condition) const final
	{
		// TODO
		return {};
	}
};

static GeneralErrorCategory generalErrorCategory{};

ErrorCode::ErrorCode() {}

ErrorCode::ErrorCode(int code): std::error_code{code, generalErrorCategory} {}

}
