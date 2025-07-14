/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <cstdint>
#include <algorithm>
#include <tuple>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

////////////////////////////////
// Windows API 利用の補助関数．
////////////////////////////////
namespace sigma_lib::W32
{
	template<bool work, uint32_t default_to = MONITOR_DEFAULTTONEAREST>
	struct monitor {
		// work area: the portion of the screen excluding the task bar area (and maybe others).
		RECT bound;
		bool is_primary;

		explicit monitor(int x, int y) : monitor(POINT{ x, y }) {}
		explicit monitor(POINT const& point) : monitor(::MonitorFromPoint(point, default_to)) {}
		explicit monitor(int left, int top, int right, int bottom) : monitor(RECT{ left, top, right, bottom }) {}
		explicit monitor(RECT const& rect) : monitor(::MonitorFromRect(&rect, default_to)) {}
		explicit monitor(HWND hwnd) : monitor(::MonitorFromWindow(hwnd, default_to)) {}

		monitor(HMONITOR monitor = nullptr)
		{
			MONITORINFO mi{ .cbSize = sizeof(mi) };
			::GetMonitorInfoW(monitor == nullptr ?
				::MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY) : // null yields primary.
				monitor, &mi);
			if constexpr (work) bound = mi.rcWork;
			else bound = mi.rcMonitor;
			is_primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
		}

		constexpr int width() const { return bound.right - bound.left; }
		constexpr int height() const { return bound.bottom - bound.top; }

		constexpr int clamp_x(int x) const {
			return std::clamp<int>(x, bound.left, bound.right);
		}
		constexpr std::pair<int, int> clamp_x(int left, int right) const {
			return clamp_core(left, right, bound.left, bound.right);
		}
		constexpr int clamp_y(int y) const {
			return std::clamp<int>(y, bound.top, bound.bottom);
		}
		constexpr std::pair<int, int> clamp_y(int top, int bottom) const {
			return clamp_core(top, bottom, bound.top, bound.bottom);
		}
		constexpr POINT clamp(POINT const& point) const {
			int const x = clamp_x(point.x), y = clamp_y(point.y);
			return { x, y };
		}

		constexpr monitor& expand(int left, int top, int right, int bottom)
		{
			bound.left -= left;
			bound.top -= top;
			bound.right += right;
			bound.bottom += bottom;
			return *this;
		}
		constexpr monitor& expand(int horiz, int vert) { return expand(horiz, vert, horiz, vert); }
		constexpr monitor& expand(int len) { return expand(len, len); }

	private:
		constexpr static std::pair<int, int> clamp_core(int lbd, int ubd, int min, int max)
		{
			bool oversized = ubd - lbd > max - min;
			if ((lbd < min) ^ oversized)
				return { min, min + ubd - lbd };
			else if ((ubd > max) ^ oversized)
				return { max + lbd - ubd, max };
			else return { lbd, ubd };
		}
	};
}