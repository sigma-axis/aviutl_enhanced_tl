/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <cstdint>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "../tooltip.hpp"
#include "tip_contents.hpp"


////////////////////////////////
// オブジェクトのツールチップ．
////////////////////////////////
namespace enhanced_tl::tooltip
{
	inline constinit struct Objects : tip_content_semitracking {
		friend struct Draggings;
		LRESULT on_show() override;
		LRESULT on_draw(NMTTCUSTOMDRAW const& info) override;
	protected:
		RECT get_rect() override;
		void update(int x, int y) override;
		void on_get_dispinfo_core(NMTTDISPINFOA& info) override;
		void on_activate() override;
		bool should_deactivate(int x, int y) override;
	} objects{};

	namespace internal
	{
		struct pos_cand {
			// all fields are in screen coordinate.
			POINT point; // candidate for the top-left point of the tooltip.
			int exclude_top, exclude_bottom;
		};
		pos_cand tooltip_pos(int layer);
		pos_cand tooltip_pos(int x_screen, int y_screen, int layer);

		POINT clamp_into_monitor(pos_cand const& pos, int width, int height, HWND hwnd_monitor);
	}
}
