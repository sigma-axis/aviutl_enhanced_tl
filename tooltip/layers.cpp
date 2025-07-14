/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <cmath>
#include <tuple>
#include <bit>
#include <string>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <CommCtrl.h>

using byte = uint8_t;
#include <exedit.hpp>

#include <../str_encodes.hpp>
#include <../monitors.hpp>

#include "../enhanced_tl.hpp"
#include "../timeline.hpp"
#include "../tooltip.hpp"
#include "tip_contents.hpp"
#include "layers.hpp"


#define NS_BEGIN(...) namespace __VA_ARGS__ {
#define NS_END }
NS_BEGIN()

////////////////////////////////
// レイヤー部分のツールチップ．
////////////////////////////////
using namespace enhanced_tl::tooltip;
namespace tl = enhanced_tl::timeline;
using sigma_lib::string::encode_sys;

constexpr int pad_left = 2, pad_top = 2;

static int last_layer = -1;
char const* last_sent = nullptr;

// returns empty if the layer is not named.
static char const* layer_name(int layer)
{
	if (layer < 0 || layer >= tl::constants::num_layers) std::unreachable();

	auto const& layer_setting = exedit.LayerSettings[tl::constants::num_layers * *exedit.current_scene + layer];
	if (layer_setting.name == nullptr) return "";
	return layer_setting.name;
}
NS_END


////////////////////////////////
// Exported functions.
////////////////////////////////
namespace expt = enhanced_tl::tooltip;

RECT expt::Layers::get_rect()
{
	return {
		.left = 0,
		.top = tl::constants::top_layer_area,
		.right = tl::constants::width_layer_area,
		.bottom = exedit.timeline_size_in_pixels->cy,
	};
}

void expt::Layers::update(int x, int y)
{
	int const curr_layer = tl::point_to_layer(y);
	if (curr_layer == last_layer) return; // no change.
	last_layer = curr_layer;
	last_sent = nullptr;

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

void expt::Layers::on_get_dispinfo_core(NMTTDISPINFOA& info)
{
	if (last_sent == nullptr) {
		last_sent = ""; // placeholder.
		if (!is_editing() || ::GetCapture() != nullptr) return; // disable while dragging.

		auto const name = layer_name(last_layer);
		if (name[0] == '\0') return;

		auto const text_size = internal::measure_text(encode_sys::to_wide_str(name), exedit.fp->hwnd);
		if (text_size.cx < tl::constants::width_layer_area)
			// The text is short enough, no need of the tooltip.
			return;

		last_sent = name;
	}
	info.lpszText = const_cast<char*>(last_sent);
}

LRESULT expt::Layers::on_show()
{
	auto const hwnd = exedit.fp->hwnd;

	// get the content position and the size of the tooltip.
	RECT rc; ::GetWindowRect(tip, &rc);
	::SendMessageW(tip, TTM_ADJUSTRECT, FALSE, reinterpret_cast<LPARAM>(&rc));

	// move the rect.
	::MapWindowPoints(nullptr, hwnd, reinterpret_cast<POINT*>(&rc), 2);
	{
		int const dx = settings.layer_name.centralize ?
			(tl::constants::width_layer_area - rc.left - rc.right) >> 1 :
			pad_left - rc.left;
		rc.left += dx; rc.right += dx;

		int const dy = pad_top + tl::point_from_layer(last_layer) - rc.top
			+ ((*exedit.layer_height - (rc.bottom - rc.top)) >> 1);
		rc.top += dy; rc.bottom += dy;
	}
	::MapWindowPoints(hwnd, nullptr, reinterpret_cast<POINT*>(&rc), 2);

	// convert back to the window size.
	::SendMessageW(tip, TTM_ADJUSTRECT, TRUE, reinterpret_cast<LPARAM>(&rc));

	if (settings.layer_name.centralize) {
		// clamp into the monitor size. necessary only when the layer names are centralized.
		using monitor = sigma_lib::W32::monitor<true>;
		std::tie(rc.left, rc.right) = monitor{ exedit.fp->hwnd }.expand(-8) // shrink by 8 pixels.
			.clamp_x(rc.left, rc.right);
	}

	// move the tooltip.
	::SetWindowPos(tip, nullptr, rc.left, rc.top, 0, 0,
		SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
	return TRUE;
}

void expt::Layers::on_activate()
{
	POINT pt; ::GetCursorPos(&pt);
	::ScreenToClient(exedit.fp->hwnd, &pt);
	last_layer = tl::point_to_layer(pt.y);
	last_sent = nullptr;
}
