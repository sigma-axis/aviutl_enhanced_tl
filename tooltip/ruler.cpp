/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <cmath>
#include <bit>
#include <string>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using byte = uint8_t;
#include <exedit.hpp>

#include <../monitors.hpp>

#include "../enhanced_tl.hpp"
#include "../timeline.hpp"
#include "../tooltip.hpp"
#include "tip_contents.hpp"
#include "ruler.hpp"


#define NS_BEGIN(...) namespace __VA_ARGS__ {
#define NS_END }
NS_BEGIN()

////////////////////////////////
// ルーラー部分のツールチップ．
////////////////////////////////
using namespace enhanced_tl::tooltip;
namespace tl = enhanced_tl::timeline;

constexpr int top_gap = 4, invalid_x = -1 << 15;

static constinit int last_x = invalid_x, last_frame = -2;
static constinit SIZE last_text_size{};

static std::wstring frame_to_text(int frame)
{
	// get the time from the frame number.
	// FPS is fi.video_rate / fi.video_scale.
	AviUtl::FileInfo fi;
	int const fps_numer = exedit.fp->exfunc->get_file_info(*exedit.editp, &fi);
	int const centi_sec = (fi.video_rate > 0 && fi.video_scale > 0) ? static_cast<int>(
		100LL * frame * fi.video_scale / fi.video_rate) : 0;

	// Get the timecode.
	int const h = centi_sec / 3600'00;
	int const m = (centi_sec / 60'00) % 60;
	int const s = (centi_sec / 1'00) % 60;
	int const cs = centi_sec % 1'00;
	constexpr wchar_t pat[] = L"フレーム: %d\n%d:%02d:%02d.%02d";
	wchar_t buf[std::bit_ceil(std::size(pat) + (8 + 3 + 2 + 2 + 2) - (2 + 2 + 4 + 4 + 4))];
	return { buf, static_cast<size_t>(::swprintf_s(buf, pat,
		frame + (settings.ruler.frame_1_origin ? 1 : 0), h, m, s, cs)) };
}
NS_END


////////////////////////////////
// Exported functions.
////////////////////////////////
namespace expt = enhanced_tl::tooltip;

RECT expt::Ruler::get_rect()
{
	return {
		.left = tl::constants::width_layer_area,
		.top = tl::constants::scrollbar_thick,
		.right = exedit.timeline_size_in_pixels->cx,
		.bottom = tl::constants::top_layer_area,
	};
}

void expt::Ruler::update(int x, int y)
{
	auto const hwnd = exedit.fp->hwnd;

	// update the tooltip text.
	int const frame = is_editing() ? tl::point_to_frame(x) : -1;
	if (last_frame != frame) {
		last_frame = frame;

		std::wstring text = L"";
		if (frame >= 0) {
			last_x = invalid_x; // force update.
			text = frame_to_text(frame);
			last_text_size = internal::measure_text(text, tip);
		}
		else last_x = x; // suppress update.

		TTTOOLINFOW ti{
			.cbSize = TTTOOLINFOW_V1_SIZE,
			.hwnd = hwnd,
			.uId = uid(),
			.lpszText = const_cast<wchar_t*>(text.c_str()),
		};
		::SendMessageW(tip, TTM_UPDATETIPTEXTW, 0, reinterpret_cast<LPARAM>(&ti));
	}

	// update the tooltip position.
	if (last_x != x) {
		last_x = x;

		// adjust the position so the center-bottom of the tooltip is just above the cursor.
		POINT pt{
			std::clamp<int>(tl::point_from_frame(frame), tl::constants::width_layer_area, exedit.timeline_size_in_pixels->cx),
			tl::constants::scrollbar_thick - top_gap,
		};
		::ClientToScreen(hwnd, &pt);

		RECT rc = {
			.left = 0, .top = 0,
			.right = last_text_size.cx + internal::pad_right_extra,
			.bottom = last_text_size.cy + internal::pad_bottom_extra,
		};
		::MapWindowPoints(tip, nullptr, reinterpret_cast<POINT*>(&rc), 2);
		::SendMessageW(tip, TTM_ADJUSTRECT, TRUE, reinterpret_cast<LPARAM>(&rc));
		pt.x -= (rc.right - rc.left) >> 1;
		pt.y -= rc.bottom - rc.top;

		// clamp the position into the monitor area.
		using monitor = sigma_lib::W32::monitor<true>;
		monitor const mon = monitor{ hwnd }.expand(-8); // shrink by 8 pixels.

		pt.x = mon.clamp_x(pt.x, pt.x + rc.right - rc.left).first;
		if (pt.y < mon.bound.top) {
			// if the tooltip is above the monitor area, move it below.
			POINT pt2 = { tl::constants::width_layer_area, tl::constants::top_layer_area + top_gap };
			::ClientToScreen(hwnd, &pt2);
			pt.y = pt2.y;
		}

		::SendMessageW(tip, TTM_TRACKPOSITION, 0, (pt.x & 0xffff) | (pt.y << 16));
	}
}

bool expt::Ruler::on_activate(int x, int y, bool active)
{
	return !active || ::GetCapture() == nullptr;
}

