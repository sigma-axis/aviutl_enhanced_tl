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
#pragma comment(lib, "comctl32")

using byte = uint8_t;
#include <exedit.hpp>

#include "inifile_op.hpp"
#include "color_abgr.hpp"
namespace gdi = sigma_lib::W32::GDI;

#include "enhanced_tl.hpp"
#include "timeline.hpp"
#include "tooltip.hpp"

#include "tooltip/tip_contents.hpp"
#include "tooltip/ruler.hpp"
#include "tooltip/layers.hpp"
#include "tooltip/scene_button.hpp"
#include "tooltip/zoom_gauge.hpp"
#include "tooltip/objects.hpp"
#include "tooltip/draggings.hpp"


#define NS_BEGIN(...) namespace __VA_ARGS__ {
#define NS_END }
NS_BEGIN()

////////////////////////////////
// ツールチップ表示．
////////////////////////////////
using namespace enhanced_tl::tooltip;
namespace tl = enhanced_tl::timeline;

static uintptr_t uid_hook() { return reinterpret_cast<uintptr_t>(&settings); }

static constexpr tip_content_tracking* find_tip(tl::timeline_area area) {
	switch (area) {
	case tl::timeline_area::ruler:
		return settings.ruler.enabled ? &ruler : nullptr;
	case tl::timeline_area::blank:
		return settings.drag_info.enabled ? &draggings : nullptr;
	default: return nullptr;
	}
}

static LRESULT CALLBACK exedit_wndproc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, uintptr_t id, DWORD_PTR data)
{
	tip_content::relay(hwnd, message, wparam, lparam);

	switch (message) {
	case WM_MOUSELEAVE:
	{
		int const x = static_cast<int16_t>(lparam & 0xffff), y = static_cast<int16_t>(lparam >> 16);
		tip_content::on_mouse_move(x, y, true);
		break;
	}
	case WM_MOUSEMOVE:
	case WM_MOUSEWHEEL:
	{
		// first, call the default procedure so the values for the tooltip might change.
		auto const ret = ::DefSubclassProc(hwnd, message, wparam, lparam);

		// get the mouse point as client coordinates.
		POINT pt{
			.x = static_cast<int16_t>(lparam & 0xffff),
			.y = static_cast<int16_t>(lparam >> 16),
		};
		if (message == WM_MOUSEWHEEL) ::ScreenToClient(hwnd, &pt);

		// then send an update.
		if (!tip_content::on_mouse_move(pt.x, pt.y, false)) {
			auto* const new_tip = find_tip(
				tl::area_from_point(pt.x, pt.y, tl::area_obj_detection::no));
			if (new_tip != nullptr) new_tip->activate(pt.x, pt.y, true);
		}
		return ret;
	}
	case WM_SIZE:
	{
		// first, call the default procedure so the field `exedit.timeline_size_in_pixels` is updated.
		auto const ret = ::DefSubclassProc(hwnd, message, wparam, lparam);

		if (settings.layer_name.enabled) layers.on_resize();
		if (settings.ruler.enabled) ruler.on_resize();
		if (settings.object_info.enabled) objects.on_resize();
		if (settings.drag_info.enabled) draggings.on_resize();

		return ret;
	}
	case WM_NOTIFY:
	{
		auto* const nmhdr = reinterpret_cast<NMHDR*>(lparam);
		if (nmhdr->hwndFrom != tip_content::tip) break;
		tip_content* const tip = tip_content::from_uid(nmhdr->idFrom);
		switch (nmhdr->code) {
		case TTN_SHOW: return tip->on_show();
		case NM_CUSTOMDRAW:
		{
			// change the text color of the tooltip if specified.
			auto const& info = *reinterpret_cast<NMTTCUSTOMDRAW*>(nmhdr);
			if (settings.text_color.A == 0 && (info.nmcd.dwDrawStage & CDDS_PREPAINT) != 0)
				::SetTextColor(info.nmcd.hdc, settings.text_color.raw);

			// delegate to the current content.
			return tip->on_draw(info);
		}
		case TTN_GETDISPINFOA: // only ANSI version is called back.
			return tip->on_get_dispinfo(*reinterpret_cast<NMTTDISPINFOA*>(nmhdr)), LRESULT{};
		}
		break;
	}
	default:
		break;
	}

	return ::DefSubclassProc(hwnd, message, wparam, lparam);
}

static void setup_tooltip()
{
	// initialize the tooltip itself.
	tip_content::setup(true);

	// initialize for each content.
	if (settings.scene_button	.enabled) scene_button	.setup();
	if (settings.zoom_gauge		.enabled) zoom_gauge	.setup();
	if (settings.layer_name		.enabled) layers		.setup();
	if (settings.ruler			.enabled) ruler			.setup();
	if (settings.object_info	.enabled) objects		.setup();
	if (settings.drag_info		.enabled) draggings		.setup();
}
NS_END


////////////////////////////////
// Exported functions.
////////////////////////////////
namespace expt = enhanced_tl::tooltip;

void expt::Settings::load(char const* ini_file) {
	using namespace sigma_lib::inifile;

	constexpr int
		offset_min = -128, offset_max = 127,
		min_time = 0, max_time = 60'000,
		max_filters_min = 1, max_filters_max = ExEdit::Object::MAX_FILTER - 1;

#define section(...)	"tooltip" __VA_OPT__(".") #__VA_ARGS__
#define field(fld, ...) __VA_OPT__(__VA_ARGS__.)fld
#define read(func, hd, fld, ...)	field(fld, hd) = read_##func(field(fld, hd), ini_file, section(hd), #fld __VA_OPT__(,) __VA_ARGS__)

	read(int	, , delay,		min_time, max_time);
	read(int	, , duration,	min_time, max_time);
	read(bool	, ,	animation);
	read(int	, ,	offset_x,	offset_min,	offset_max);
	read(int	, ,	offset_y,	offset_min,	offset_max);
	read(color	, ,	text_color);

	read(bool	, scene_button,	enabled);

	read(bool	, zoom_gauge,	enabled);

	read(bool	, ruler,		enabled);
	read(bool	, ruler,		frame_1_origin);

	read(bool	, layer_name,	enabled);
	read(bool	, layer_name,	centralize);

	read(bool	, object_info,	enabled);
	read(int	, object_info,	max_filters, max_filters_min, max_filters_max);

	read(bool	, drag_info,	enabled);

#undef read
#undef field
#undef section
}

bool expt::setup(HWND hwnd, bool initializing)
{
	if (!settings.is_enabled()) return false;

	if (initializing) {
		setup_tooltip();
		::SetWindowSubclass(exedit.fp->hwnd, &exedit_wndproc, uid_hook(), {});
	}
	else {
		tip_content::setup(false);
		::RemoveWindowSubclass(exedit.fp->hwnd, &exedit_wndproc, uid_hook());
	}
	return true;
}
