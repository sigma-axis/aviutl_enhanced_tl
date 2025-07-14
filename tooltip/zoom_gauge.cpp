/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <cmath>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <CommCtrl.h>

using byte = uint8_t;
#include <exedit.hpp>

#include <../str_encodes.hpp>

#include "../enhanced_tl.hpp"
#include "../timeline.hpp"
#include "../tooltip.hpp"
#include "tip_contents.hpp"
#include "zoom_gauge.hpp"


#define NS_BEGIN(...) namespace __VA_ARGS__ {
#define NS_END }
NS_BEGIN()

////////////////////////////////
// 拡大率ゲージのツールチップ．
////////////////////////////////
using namespace enhanced_tl::tooltip;
namespace tl = enhanced_tl::timeline;

static int last_gauge_value = -1;
static char last_gauge_digits[4]{};

static void update_gauge(int gauge_value)
{
	last_gauge_value = gauge_value;
	::sprintf_s(last_gauge_digits, "%d",
		std::clamp(gauge_value + 1, 1, tl::constants::num_zoom_levels));
}
NS_END


////////////////////////////////
// Exported functions.
////////////////////////////////
namespace expt = enhanced_tl::tooltip;

RECT expt::ZoomGauge::get_rect()
{
	return {
		.left = 0,
		.top = tl::constants::top_zoom_gauge,
		.right = tl::constants::width_layer_area,
		.bottom = tl::constants::top_layer_area,
	};
}

void expt::ZoomGauge::update(int x, int y)
{
	if (last_gauge_value != *exedit.curr_timeline_zoom_level) {
		// update the tooltip text.
		TTTOOLINFOW ti{
			.cbSize = TTTOOLINFOW_V1_SIZE,
			.hwnd = exedit.fp->hwnd,
			.uId = uid(),
			.hinst = exedit.fp->hinst_parent,
			.lpszText = LPSTR_TEXTCALLBACKW,
		};
		::SendMessageW(tip, TTM_UPDATETIPTEXTW, 0, reinterpret_cast<LPARAM>(&ti));
	}
}

void expt::ZoomGauge::on_get_dispinfo_core(NMTTDISPINFOA& info)
{
	if (!is_editing()) return;

	if (auto const gauge_value = *exedit.curr_timeline_zoom_level;
		last_gauge_value != gauge_value)
		update_gauge(gauge_value);
	info.lpszText = last_gauge_digits;
}
