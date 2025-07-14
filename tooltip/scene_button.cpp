/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <string>

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
#include "scene_button.hpp"


#define NS_BEGIN(...) namespace __VA_ARGS__ {
#define NS_END }
NS_BEGIN()

////////////////////////////////
// シーン名のツールチップ．
////////////////////////////////
using namespace enhanced_tl::tooltip;
namespace tl = enhanced_tl::timeline;
using sigma_lib::string::encode_sys;

constexpr int pad_left = 2, pad_top = 3;

#ifdef NDEBUG
constinit
#endif
static std::string last_name{};
static constinit SIZE last_size{};

// returns empty if the scene is not named.
static char const* scene_name()
{
	auto const& scene_setting = exedit.SceneSettings[*exedit.current_scene];
	return scene_setting.name;
}
NS_END


////////////////////////////////
// Exported functions.
////////////////////////////////
namespace expt = enhanced_tl::tooltip;

RECT expt::SceneButton::get_rect()
{
	return {
		.left = 0,
		.top = 0,
		.right = tl::constants::width_layer_area,
		.bottom = tl::constants::top_zoom_gauge,
	};
}

void expt::SceneButton::on_get_dispinfo(NMTTDISPINFOA& info)
{
	if (!is_editing()) return;

	char const* const name = scene_name();
	if (name == nullptr) return;
	if (name != last_name) {
		last_name = name;
		last_size = internal::measure_text(encode_sys::to_wide_str(last_name), tip);
	}
	if (last_size.cx < tl::constants::width_layer_area)
		return; // no overflow, no need of the tooltip.
	info.lpszText = const_cast<char*>(name);
}

LRESULT expt::SceneButton::on_show()
{
	// get the top-left corner of the button.
	POINT pt{ pad_left, pad_top };
	::ClientToScreen(exedit.fp->hwnd, &pt);

	// adjust the position of the tooltip.
	RECT rc; ::GetWindowRect(tip, &rc);
	::SendMessageW(tip, TTM_ADJUSTRECT, FALSE, reinterpret_cast<LPARAM>(&rc));
	rc.right += pt.x - rc.left; rc.bottom += pt.y - rc.top;
	rc.left = pt.x; rc.top = pt.y;
	::SendMessageW(tip, TTM_ADJUSTRECT, TRUE, reinterpret_cast<LPARAM>(&rc));

	// move the tooltip.
	::SetWindowPos(tip, nullptr, rc.left, rc.top, 0, 0,
		SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	return TRUE;
}
