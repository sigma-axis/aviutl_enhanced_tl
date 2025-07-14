/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <bit>
#include <cmath>
#include <tuple>
#include <string>
#include <memory>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using byte = uint8_t;
#include <exedit.hpp>

#include "memory_protect.hpp"
#include "inifile_op.hpp"
#include "modkeys.hpp"
#include "timeline.hpp"
#include "mouse_override/mouse_actions.hpp"

#include "enhanced_tl.hpp"
#include "mouse_override.hpp"
#include "mouse_override/timeline.hpp"
#include "mouse_override/layers.hpp"
#include "mouse_override/zoom_gauge.hpp"


#define NS_BEGIN(...) namespace __VA_ARGS__ {
#define NS_END }
NS_BEGIN()

////////////////////////////////
// 拡張編集マウス操作の換装．
////////////////////////////////
using namespace enhanced_tl::mouse_override;
using namespace sigma_lib::modifier_keys;
namespace tl = enhanced_tl::timeline;

constexpr WPARAM all_buttons = MK_LBUTTON | MK_RBUTTON | MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2;

static inline drag_state* map_drag(mouse_button btn, int x, int y, modkeys mkeys)
{
	auto area = tl::area_from_point(x, y, tl::area_obj_detection::nearby);
	// identify certain areas as same.
	switch (area) {
		using enum tl::timeline_area;
	case object:
	case layer:
		break;
	case ruler: case blank:
		area = blank; break;
	default: break;
	}

	// select appropriate field based on the mouse button.
	constexpr auto select_button = [](auto& drag, mouse_button btn) -> auto&
	{
		using enum mouse_button;
		switch (btn) {
		default:
		case L:			return drag.L;
		case R:			return drag.R;
		case M:			return drag.M;
		case X1:		return drag.X1;
		case X2:		return drag.X2;
		case L_and_R:	return drag.L_and_R;
		}
	};

	// select a drag action from a pool.
	switch (area) {
		using enum mouse_button;
	case tl::timeline_area::object:
	case tl::timeline_area::blank:
	{
		if (!settings.timeline.enabled) return nullptr;

		switch (select_button(area == tl::timeline_area::object ?
			settings.timeline.drag_obj :
			settings.timeline.drag, btn)[mkeys]) {
			using enum Settings::Timeline::Drag::action;
		default: case none:	return &drags::no_action;
		case l:				return &timeline::drags::l;
		case obj_l:			return &timeline::drags::obj_l;
		case obj_ctrl_l:	return &timeline::drags::obj_ctrl_l;
		case obj_shift_l:	return &timeline::drags::obj_shift_l;
		case bk_l:			return &timeline::drags::bk_l;
		case bk_ctrl_l:		return &timeline::drags::bk_ctrl_l;
		case alt_l:			return &timeline::drags::alt_l;
		case zoom_bi:		return &timeline::drags::zoom_bi;
		case step_bound:	return &timeline::drags::step_bound;
		case step_bpm:		return &timeline::drags::step_bpm;

		case bypass: return nullptr;
		}
	}
	case tl::timeline_area::layer:
	{
		if (!settings.layer.enabled) return nullptr;

		switch (select_button(settings.layer.drag, btn)[mkeys]) {
			using enum Settings::Layer::Drag::action;
		default: case none:	return &drags::no_action;
		case show_hide:		return &layers::drags::show_hide;
		case lock_unlock:	return &layers::drags::lock_unlock;
		case link_coord:	return &layers::drags::link_coord;
		case mask_above:	return &layers::drags::mask_above;
		case select_all:	return &layers::drags::select_all;
		case drag_move:		return &layers::drags::drag_move;

		case bypass: return nullptr;
		}

	}
	case tl::timeline_area::scene_button:
	{
		if (!settings.scene_button.enabled) return nullptr;

		// no more reference to settings.
		switch (btn) {
		case R: return &drags::no_action;
		default: return nullptr;
		}
	}
	case tl::timeline_area::zoom_gauge:
	{
		if (!settings.zoom_gauge.enabled) return nullptr;

		// no more reference to settings.
		switch (btn) {
		case R: return &drags::no_action;
		default: return nullptr;
		}
	}
	default: return nullptr;
	}
}
static inline click_func* map_click(mouse_button btn, bool is_double, int x, int y, modkeys mkeys)
{
	auto area = tl::area_from_point(x, y, tl::area_obj_detection::nearby);
	// identify certain areas as same.
	switch (area) {
		using enum tl::timeline_area;
	case object:
	case layer:
		break;
	case ruler: case blank:
		area = blank; break;
	default: break;
	}

	// select appropriate field based on the mouse button.
	constexpr auto select_button = [](auto& click, mouse_button btn) -> auto&
	{
		using enum mouse_button;
		switch (btn) {
		default:
		case L:		return click.L;
		case R:		return click.R;
		case M:		return click.M;
		case X1:	return click.X1;
		case X2:	return click.X2;
		}
	};

	// select a click action from a pool.
	switch (area) {
		using enum mouse_button;
	case tl::timeline_area::object:
	case tl::timeline_area::blank:
	{
		if (!settings.timeline.enabled) return nullptr;

		switch (select_button(area == tl::timeline_area::object ?
			is_double ? settings.timeline.dbl_click_obj : settings.timeline.click_obj :
			is_double ? settings.timeline.dbl_click : settings.timeline.click, btn)[mkeys]) {
			using enum Settings::Timeline::Click::action;
		default: case none:		return &clicks::no_action;
		case obj_ctrl_shift_l:	return &timeline::clicks::obj_ctrl_shift_l;
		case obj_l_dbl:			return &timeline::clicks::obj_l_dbl;
		case rclick:			return &timeline::clicks::rclick;
		case toggle_midpt:		return &timeline::clicks::toggle_midpt;
		case select_line_left:	return &timeline::clicks::select_line_left;
		case select_line_right:	return &timeline::clicks::select_line_right;
		case select_all:		return &timeline::clicks::select_all;
		case squeeze_left:		return &timeline::clicks::squeeze_left;
		case squeeze_right:		return &timeline::clicks::squeeze_right;
		case toggle_active:		return &timeline::clicks::toggle_active;

		case bypass: return nullptr;
		}
	}
	case tl::timeline_area::layer:
	{
		if (!settings.layer.enabled) return nullptr;

		switch (select_button(is_double ?
			settings.layer.dbl_click : settings.layer.click, btn)[mkeys]) {
			using enum Settings::Layer::Click::action;
		default: case none:	return &clicks::no_action;
		case rclick:		return &timeline::clicks::rclick;
		case rename:		return &layers::clicks::rename;
		case toggle_others:	return &layers::clicks::toggle_others;
		case insert:		return &layers::clicks::insert;
		case remove:		return &layers::clicks::remove;

		case bypass: return nullptr;
		}
	}
	case tl::timeline_area::scene_button:
	{
		if (!settings.scene_button.enabled) return nullptr;

		// no more reference to settings.
		if (is_double) return nullptr;
		switch (btn) {
		case R: return &timeline::clicks::rclick;
		default: return nullptr;
		}
	}
	case tl::timeline_area::zoom_gauge:
	{
		if (!settings.zoom_gauge.enabled) return nullptr;

		if (is_double) return nullptr;
		switch (btn) {
		case R: return &timeline::clicks::rclick;
		default: return nullptr;
		}
	}
	default: return nullptr;
	}
}
static inline std::pair<wheel_func*, bool> map_wheel(bool r_button, int client_x, int client_y, modkeys mkeys)
{
	auto area = tl::area_from_point(client_x, client_y, tl::area_obj_detection::nearby);
	// identify certain areas as same.
	switch (area) {
		using enum tl::timeline_area;
	case ruler: case object: case blank:
		// any part of timeline.
		area = blank; break;
	default: break;
	}

	// select appropriate field based on the mouse button.
	constexpr auto select_button = [](auto& wheel, bool r_button) -> auto&
	{
		if (r_button) return wheel.r_button;
		return wheel.normal;
	};

	// select a wheel action from a pool.
	switch (area) {
		using enum mouse_button;
	case tl::timeline_area::blank: // any part of timeline.
	case tl::timeline_area::layer:
	{
		if (area == tl::timeline_area::blank ?
			!settings.timeline.enabled : !settings.layer.enabled) return { nullptr, false };

		switch (auto const action = (area == tl::timeline_area::blank ?
			select_button(settings.timeline.wheel, r_button) :
			select_button(settings.layer.wheel, r_button))[mkeys]) {
			using enum Settings::Timeline::Wheel::action;
		default: case none:
			return { &wheels::no_action, false };

			// scrolling.
		case scroll_h_p: case scroll_h_n:
			return { &timeline::wheels::scroll_h,			action == scroll_h_n };
		case scroll_v_p: case scroll_v_n:
			return { &timeline::wheels::scroll_v,			action == scroll_v_n };

			// scaling.
		case zoom_h_p: case zoom_h_n:
			return {
				// allow slightly different behavior.
				area == tl::timeline_area::blank ? &timeline::wheels::zoom_h : &layers::wheels::zoom_h,
				action == zoom_h_n
			};
		case zoom_v_p: case zoom_v_n:
			return { &timeline::wheels::zoom_v,				action == zoom_v_n };

			// moving frame by certain length.
		case move_one_p: case move_one_n:
			return { &timeline::wheels::move_frame_1,		action == move_one_n };
		case move_len_p: case move_len_n:
			return { &timeline::wheels::move_frame_len,		action == move_len_n };

			// moving frame to boundaries of objects.
		case move_midpt_layer_p: case move_midpt_layer_n:
			return { &timeline::wheels::step_midpt_layer,	action == move_midpt_layer_n };
		case move_obj_layer_p: case move_obj_layer_n:
			return { &timeline::wheels::step_obj_layer,		action == move_obj_layer_n };
		case move_midpt_all_p: case move_midpt_all_n:
			return { &timeline::wheels::step_midpt_scene,	action == move_midpt_all_n };
		case move_obj_all_p: case move_obj_all_n:
			return { &timeline::wheels::step_obj_scene,		action == move_obj_all_n };

			// moving frame to the BPM grid.
		case move_bpm_p: case move_bpm_n:
			return { &timeline::wheels::step_bpm_grid,		action == move_bpm_n };

		case change_scene_p: case change_scene_n:
			return { &timeline::wheels::change_scene,		action == change_scene_n };

		case bypass: return { nullptr, false };
		}
	}
	case tl::timeline_area::scene_button:
	{
		if (!settings.scene_button.enabled || settings.scene_button.wheel == 0)
			return { nullptr, false };
		return { &timeline::wheels::change_scene, settings.scene_button.wheel < 0 };
	}
	case tl::timeline_area::zoom_gauge:
	{
		if (!settings.zoom_gauge.enabled || settings.zoom_gauge.wheel == 0)
			return { nullptr, false };
		return { &zoom_gauge::wheels::zoom_h, settings.zoom_gauge.wheel < 0 };
	}
	case tl::timeline_area::scrollbar_v:
	{
		if (!settings.timeline.enabled || !settings.timeline.wheel_vertical_scrollbar)
			return { nullptr, false };
		return { &timeline::wheels::scroll_v, false };
	}
	default: return { nullptr, false };
	}
}

static inline modkeys wp_to_modkeys(WPARAM wparam)
{
	return
		((wparam & MK_CONTROL) != 0 ? modkeys::ctrl : modkeys::none) |
		((wparam & MK_SHIFT) != 0 ? modkeys::shift : modkeys::none) |
		(::GetKeyState(VK_MENU) < 0 ? modkeys::alt : modkeys::none);
}

struct {
	struct {
		int32_t tl_wheel, zoom_gauge;
	} refs{};
	bool tl_wheel = false, zoom_gauge = false;
} zoom_centers;

hook_wnd_proc* next_proc = nullptr;


////////////////////////////////
// タイムラインウィンドウのフック．
////////////////////////////////
void unmark_canceled_on_down(WPARAM wparam)
{
	// revert the "canceled" state if all buttons but one are released.
	auto const all_pressed_buttons = wparam & all_buttons;
	if (drag_state::last_status == last_drag_status::canceled &&
		(all_pressed_buttons & (all_pressed_buttons - 1)) == 0)
		drag_state::unmark_canceled();
}

/// @param ret changes the value only if this function returned `true`.
static inline bool on_mouse_down(bool& ret, mouse_button btn, WPARAM wparam, int x, int y)
{
	unmark_canceled_on_down(wparam);

	mouse_button btn_gesture = btn;
	if ((wparam & all_buttons) == (MK_LBUTTON | MK_RBUTTON)) {
		switch (btn) {
		case mouse_button::L:
		case mouse_button::R:
			btn_gesture = mouse_button::L_and_R;
			break;
		default: break;
		}
	}

	// choose a drag action from a pool and settings.
	auto const mkeys = wp_to_modkeys(wparam);
	drag_state* const action = map_drag(btn_gesture, x, y, mkeys);
	if (action == nullptr) return false; // no actions assigned.

	auto const former_kind = *exedit.timeline_drag_kind;

	// begin the assigned action.
	ret |= action->on_mouse_down(exedit.fp->hwnd, x, y, btn_gesture, mkeys);

	// hints for tooltips.
	if (former_kind == drag_kind::none) {
		switch (*exedit.timeline_drag_kind) {
		case drag_kind::move_object:
		case drag_kind::move_object_left:
		case drag_kind::move_object_right:
		{
			using namespace interop;
			pt_mouse_down = { x, y };
			idx_obj_on_mouse = exedit.obj_from_point(x, y);
			if (idx_obj_on_mouse >= 0) {
				auto const& obj = (*exedit.ObjectArray_ptr)[idx_obj_on_mouse];
				if (has_flag_or(obj.flag, ExEdit::Object::Flag::Exist)) {
					former_frame_begin = obj.frame_begin;
					former_frame_end = obj.frame_end;
					former_layer_disp = obj.layer_disp;
				}
				else idx_obj_on_mouse = -1;
			}
			break;
		}
		}
	}

	return true;
}
/// @param ret may change the value even if this function returned `false`.
static inline bool on_mouse_up(bool& ret, mouse_button btn, WPARAM wparam, int x, int y)
{
	auto const all_pressed_buttons = wp_to_buttons(wparam);
	auto const mkeys = wp_to_modkeys(wparam);

	if (drag_state::is_active() && btn <= drag_state::button) {
		// one button participating the drag is released.

		if ((all_pressed_buttons & drag_state::button) != mouse_button::none)
			return true; // keep the drag if some buttons are left pressed.

		// finish drag operation.
		ret |= drag_state::on_mouse_up_all(x, y, mkeys);

		// dismiss click actions if drag didn't finish in a short move.
		if (drag_state::last_status != last_drag_status::clicked)
			return true;
		drag_state::unmark_canceled();
	}
	else if (drag_state::last_status != last_drag_status::clicked &&
		btn <= drag_state::button) {
		// the button participated the canceled drag was released.

		if ((all_pressed_buttons & drag_state::button) == mouse_button::none)
			drag_state::unmark_canceled(); // all participant buttons are released.
		return true; // dismiss the click.
	}
	else if (!drag_state::is_active() && ::GetCapture() != nullptr)
		return false; // unmanaged drag is being performed.

	// choose a click action from a pool and settings.
	click_func* const action = map_click(btn, false, x, y, mkeys);
	if (action == nullptr) return false; // no actions assigned.

	// invoke the assigned action.
	ret |= action(x, y, mkeys);
	return true;
}
/// @param ret changes the value only if this function returned `true`.
static inline bool on_mouse_dblclick(bool& ret, mouse_button btn, WPARAM wparam, int x, int y)
{
	unmark_canceled_on_down(wparam);

	// choose a double-click action from a pool and settings.
	auto const mkeys = wp_to_modkeys(wparam);
	click_func* const action = map_click(btn, true, x, y, mkeys);
	if (action == nullptr) return false; // no actions assigned.

	// invoke the assigned action.
	ret |= action(x, y, mkeys);

	// begin a dummy drag to prevent the following mouse-up event.
	drags::no_action.on_mouse_down(exedit.fp->hwnd, x, y, btn, mkeys);
	drag_state::invalidate_click();
	return true;
}
/// @param ret changes the value only if this function returned `true`.
static inline bool on_mouse_wheel(bool& ret, bool r_button, WPARAM wparam, int screen_x, int screen_y)
{
	// right-click + wheel should suppress the action by right-button up.
	drag_state::invalidate_click();

	POINT pt_client{ screen_x, screen_y };
	::ScreenToClient(exedit.fp->hwnd, &pt_client);
	auto const mkeys = wp_to_modkeys(wparam);

	// choose a wheel action from a pool and settings.
	auto const [action, reverse] = map_wheel(r_button, pt_client.x, pt_client.y, mkeys);
	if (action == nullptr) return false; // no action assigned.

	// invoke the assigned action.
	int delta = static_cast<int16_t>(wparam >> 16);
	if (reverse) delta *= -1;
	ret |= action(screen_x, screen_y, delta, mkeys);
	return true;
}

static BOOL exedit_wndproc(hook_wnd_proc& next, HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, AviUtl::EditHandle* editp, AviUtl::FilterPlugin* fp)
{
	next_proc = &next; // won't change once set.
	if (zoom_centers.zoom_gauge)
		// update the variable that is referred to from certain codes.
		zoom_centers.refs.zoom_gauge = internal::zoom_center_frame(settings.zoom_gauge.zoom_center);

	switch (message) {
		mouse_button btn;
	case WM_LBUTTONDOWN: btn = mouse_button::L; goto mouse_down;
	case WM_RBUTTONDOWN: btn = mouse_button::R; goto mouse_down;
	case WM_MBUTTONDOWN: btn = mouse_button::M; goto mouse_down;
	case WM_XBUTTONDOWN:
		btn = ((wparam >> 16) & XBUTTON1) != 0 ? mouse_button::X1 : mouse_button::X2;
	mouse_down:
	{
		if (bool ret = false; on_mouse_down(ret, btn, wparam,
			static_cast<int16_t>(lparam & 0xffff),
			static_cast<int16_t>(lparam >> 16)))
			return ret ? TRUE : FALSE;
		break;
	}

	case WM_LBUTTONUP: btn = mouse_button::L; goto mouse_up;
	case WM_RBUTTONUP: btn = mouse_button::R; goto mouse_up;
	case WM_MBUTTONUP: btn = mouse_button::M; goto mouse_up;
	case WM_XBUTTONUP:
		btn = ((wparam >> 16) & XBUTTON1) != 0 ? mouse_button::X1 : mouse_button::X2;
	mouse_up:
	{
		bool ret = false;
		if (!on_mouse_up(ret, btn, wparam,
			static_cast<int16_t>(lparam & 0xffff),
			static_cast<int16_t>(lparam >> 16)))
			ret |= next(hwnd, message, wparam, lparam, editp, fp) != FALSE;
		return ret ? TRUE : FALSE;
	}

	case WM_LBUTTONDBLCLK: btn = mouse_button::L; goto mouse_dblclick;
	case WM_RBUTTONDBLCLK: btn = mouse_button::R; goto mouse_dblclick;
	case WM_MBUTTONDBLCLK: btn = mouse_button::M; goto mouse_dblclick;
	case WM_XBUTTONDBLCLK:
		btn = ((wparam >> 16) & XBUTTON1) != 0 ? mouse_button::X1 : mouse_button::X2;
	mouse_dblclick:
	{
		if (bool ret = false; on_mouse_dblclick(ret, btn, wparam,
			static_cast<int16_t>(lparam & 0xffff),
			static_cast<int16_t>(lparam >> 16)))
			return ret ? TRUE : FALSE;
		break;
	}

	case WM_MOUSEMOVE:
	{
		if (drag_state::is_active()) return drag_state::on_mouse_move(
			static_cast<int16_t>(lparam & 0xffff),
			static_cast<int16_t>(lparam >> 16),
			wp_to_modkeys(wparam)) != FALSE;
		break;
	}

	case WM_CAPTURECHANGED:
	{
		if (drag_state::is_active()) return drag_state::cancel(false) != FALSE;
		break;
	}

	case WM_MOUSEWHEEL:
	{
		int const
			screen_x = static_cast<int16_t>(lparam & 0xffff),
			screen_y = static_cast<int16_t>(lparam >> 16);
		if (zoom_centers.tl_wheel)
			// update the variable that is referred to from certain codes.
			zoom_centers.refs.tl_wheel = internal::zoom_center_frame(
				settings.timeline.zoom_center_wheel, screen_x, screen_y);

		if (bool ret = false; on_mouse_wheel(
			ret, (wparam & MK_RBUTTON) != 0, wparam, screen_x, screen_y))
			return ret ? TRUE : FALSE;
		break;
	}

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	{
		if (wparam == VK_ESCAPE && drag_state::is_active())
			return drag_state::cancel(true) ? TRUE : FALSE;
	}
	[[fallthrough]];
	case WM_KEYUP:
	case WM_SYSKEYUP:
	{
		if (bool ret = false; drag_state::handle_key_messages(ret, message, wparam, lparam))
			return ret ? TRUE : FALSE;
		break;
	}
	}

	return next(hwnd, message, wparam, lparam, editp, fp);
}
NS_END


////////////////////////////////
// Exported functions.
////////////////////////////////
namespace expt = enhanced_tl::mouse_override;

void expt::Settings::load(char const* ini_file)
{
	constexpr auto no_filter = [](auto) { return true; };
	auto load_one = [&](auto& one, auto fallback, char const* name, char const* section, auto&& is_valid)
	{
		one.load(fallback, ini_file, section, name);
		one.normalize(is_valid, fallback);
	};
	auto load_drag = [&](auto& drag, auto const& fallback, char const* section, auto&& is_valid)
	{
		load_one(drag.L,			fallback.L,			"L",	section, is_valid);
		load_one(drag.R,			fallback.R,			"R",	section, is_valid);
		load_one(drag.M,			fallback.M,			"M",	section, is_valid);
		load_one(drag.X1,			fallback.X1,		"X1",	section, is_valid);
		load_one(drag.X2,			fallback.X2,		"X2",	section, is_valid);
		load_one(drag.L_and_R,		fallback.L_and_R,	"L+R",	section, is_valid);
	};
	auto load_click = [&](auto& click, auto const& fallback, char const* section, auto&& is_valid)
	{
		load_one(click.L,			fallback.L,			"L",	section, is_valid);
		load_one(click.R,			fallback.R,			"R",	section, is_valid);
		load_one(click.M,			fallback.M,			"M",	section, is_valid);
		load_one(click.X1,			fallback.X1,		"X1",	section, is_valid);
		load_one(click.X2,			fallback.X2,		"X2",	section, is_valid);
	};
	auto load_wheel = [&](auto& wheel, auto const& fallback, char const* section, auto&& is_valid)
	{
		load_one(wheel.normal,		fallback.normal,	"",		section, is_valid);
		load_one(wheel.r_button,	fallback.r_button,	"R",	section, is_valid);
	};

	using namespace sigma_lib::inifile;
#define section(sub) ("mouse_override." sub)
#define read(type, hd, fld, ...) hd.fld = read_##type(hd.fld, ini_file, section(#hd), #fld __VA_OPT__(,) __VA_ARGS__)

	// settings for timeline.
	load_drag(timeline.drag,			timeline.drag,		section("timeline.drag"),
	[](Timeline::Drag::action action) {
		using enum Timeline::Drag::action;
		switch (action) {
		case obj_l:
		case obj_ctrl_l:
		case obj_shift_l:
			return false; // these actions are not allowed for blank area.
		default: return true;
		}
	});
	load_drag(timeline.drag_obj,		timeline.drag,		section("timeline.drag.object"), no_filter);

	load_click(timeline.click,			timeline.click,		section("timeline.click"),
	[](Timeline::Click::action action) {
		using enum Timeline::Click::action;
		switch (action) {
		case obj_ctrl_shift_l:
		case obj_l_dbl:
		case toggle_midpt:
		case toggle_active:
			return false; // these actions are not allowed for blank area.
		default: return true;
		}
	});
	load_click(timeline.click_obj,		timeline.click,		section("timeline.click.object"), no_filter);

	load_click(timeline.dbl_click,		timeline.dbl_click,	section("timeline.dbl_click"),
	[](Timeline::Click::action action) {
		using enum Timeline::Click::action;
		switch (action) {
		case obj_ctrl_shift_l:
		case obj_l_dbl:
			return false; // these actions are not allowed for blank area.
		default: return true;
		}
	});
	load_click(timeline.dbl_click_obj,	timeline.dbl_click,	section("timeline.dbl_click.object"), no_filter);

	load_wheel(timeline.wheel,			timeline.wheel,		section("timeline.wheel"), no_filter);

	constexpr int
		drag_len_min = -200, drag_len_max = 200,
		drag_ref_min = 1, drag_ref_max = std::max(-drag_len_min, drag_len_max);
	read(bool,	timeline, enabled);
	read(bool,	timeline, change_cursor);
	read(bool,	timeline, wheel_vertical_scrollbar);
	read(int,	timeline, zoom_center_wheel);
	read(int,	timeline, zoom_center_drag);
	read(int,	timeline, zoom_drag_refine_x, drag_ref_min, drag_ref_max);
	read(int,	timeline, zoom_drag_length_x, drag_len_min, drag_len_max);
	read(int,	timeline, zoom_drag_length_y, drag_len_min, drag_len_max);
	read(modkey,timeline, skip_midpt_key);
	read(bool,	timeline, skip_midpt_def);
	read(modkey,timeline, skip_inactives_key);
	read(bool,	timeline, skip_inactives_def);
	timeline.BPM.load(timeline.BPM, ini_file, section("timeline"), "BPM");

	// settings for layer area.
	load_drag(layer.drag,				layer.drag,			section("layer.drag"), no_filter);
	load_click(layer.click,				layer.click,		section("layer.click"), no_filter);
	load_click(layer.dbl_click,			layer.dbl_click,	section("layer.dbl_click"), no_filter);
	load_wheel(layer.wheel,				layer.wheel,		section("layer.wheel"), no_filter);

	constexpr int delay_ms_min = 10, delay_ms_max = 2000;
	read(bool,	layer, enabled);
	read(int,	layer, auto_scroll_delay_ms, delay_ms_min, delay_ms_max);
	read(int,	layer, zoom_center_wheel);

	// settings for zoom gauge.
	read(bool,	zoom_gauge, enabled);
	read(int,	zoom_gauge, wheel, -1, +1);
	read(int,	zoom_gauge, zoom_center);

	// settings for scene button.
	read(bool,	scene_button, enabled);
	read(int,	scene_button, wheel, -1, +1);

#undef read
#undef section
}

bool expt::setup(HWND hwnd, bool initializing)
{
	if (!settings.is_enabled()) return false;
	if (initializing) {
		exedit_hook::manager.add(&exedit_wndproc);

		// manipulate the binary codes.
		auto const exedit_base = reinterpret_cast<uintptr_t>(exedit.fp->dll_hinst);

		// change the center of horizontal zoom by changing internal codes.
		// offset addresses where the instruction `mov` is written,
		// all reading from `exedit.curr_edit_frame`.
		constexpr ptrdiff_t mov_addr_zoom_gauge[] = {
			0x03c6ac + 2, // 8b 15 ** ** ** **    mov edx, dword ptr ds:[****] ; dragging the zoom gauge with mouse.
			0x043b82 + 2, // 8b 0d ** ** ** **    mov ecx, dword ptr ds:[****] ; menu command to increment the level.
			0x043ba6 + 1, // a1 ** ** ** **       mov eax, dword ptr ds:[****] ; menu command to decrement the level.
		}, mov_addr_tl_wheel[] = {
			0x03df5e + 2, // 8b 15 ** ** ** **    mov edx, dword ptr ds:[****] ; on ctrl+wheel.
		};

		namespace memory = sigma_lib::memory;
		if (settings.zoom_gauge.enabled &&
			settings.zoom_gauge.zoom_center != Settings::tl_zoom_center::curr_frame) {
			int32_t* target = exedit.timeline_h_scroll_pos;
			if (settings.zoom_gauge.zoom_center != Settings::tl_zoom_center::window_left) {
				target = &zoom_centers.refs.zoom_gauge;
				zoom_centers.zoom_gauge = true;
			}
			for (auto ptr : mov_addr_zoom_gauge)
				memory::ProtectHelper::write(exedit_base + ptr, target);
		}
		if (settings.timeline.enabled &&
			settings.timeline.zoom_center_wheel != Settings::tl_zoom_center::curr_frame) {
			int32_t* target = exedit.timeline_h_scroll_pos;
			if (settings.timeline.zoom_center_wheel != Settings::tl_zoom_center::window_left) {
				target = &zoom_centers.refs.tl_wheel;
				zoom_centers.tl_wheel = true;
			}
			for (auto ptr : mov_addr_tl_wheel)
				memory::ProtectHelper::write(exedit_base + ptr, target);
		}
	}
	return true;
}

BOOL expt::internal::call_next_proc(UINT message, WPARAM wparam, LPARAM lparam)
{
	return (*next_proc)(exedit.fp->hwnd, message, wparam, lparam, *exedit.editp, exedit.fp);
}

int32_t internal::zoom_center_frame(Settings::tl_zoom_center center)
{
	// determines the center frame of zooming.
	switch (center) {
		using enum Settings::tl_zoom_center;
	default: case curr_frame:
		return *exedit.curr_edit_frame;
	case window_left:
		return *exedit.timeline_h_scroll_pos;
	case window_center:
		return *exedit.timeline_h_scroll_pos + (*exedit.timeline_width_in_frames >> 1);
	case window_right:
		return std::max(0, *exedit.timeline_h_scroll_pos + *exedit.timeline_width_in_frames
			- enhanced_tl::timeline::horiz_scroll_margin());
	}
	
}

int32_t internal::zoom_center_frame(Settings::tl_zoom_center center, int client_x)
{
	// determines the center frame of zooming.
	switch (center) {
		using enum Settings::tl_zoom_center;
	case curr_frame:
	case window_left:
	case window_center:
	case window_right:
		return zoom_center_frame(center);
	default: case mouse:
		return enhanced_tl::timeline::point_to_frame(client_x);
	}
}

int32_t internal::zoom_center_frame(Settings::tl_zoom_center center, int screen_x, int screen_y)
{
	// determines the center frame of zooming.
	switch (center) {
		using enum Settings::tl_zoom_center;
	case curr_frame:
	case window_left:
	case window_center:
	case window_right:
		return zoom_center_frame(center);
	default: case mouse:
	{
		POINT pt_client{ screen_x, screen_y };
		::ScreenToClient(exedit.fp->hwnd, &pt_client);
		return enhanced_tl::timeline::point_to_frame(pt_client.x);
	}
	}
}

bool expt::check_conflict()
{
	// 「TLショトカ移動」のドラッグ操作拡張機能と競合．
	if (settings.timeline.enabled && warn_conflict(L"tl_walkaround.auf",
		L"[mouse_override.timeline]\n"
		"enabled=0")) settings.timeline.enabled = false;

	if (settings.layer.enabled) {
		// 「レイヤー一括切り替え」のドラッグ操作と競合．
		if (warn_conflict(L"toggle_layers.auf",
			L"[mouse_override.layer]\n"
			"enabled=0")) settings.layer.enabled = false;

		// 「可変幅レイヤー」のホイール操作と競合．
		if (warn_conflict(L"layer_resize.auf",
			L"[mouse_override.layer]\n"
			"enabled=0")) settings.layer.enabled = false;
	}

	// Layer Wheel 2 のホイール操作と競合．
	if (((settings.timeline.enabled && settings.timeline.wheel_vertical_scrollbar != 0) ||
		settings.layer.enabled ||
		(settings.zoom_gauge.enabled && settings.zoom_gauge.wheel != 0)) &&
		warn_conflict(L"layer_wheel2.auf",
			L"[mouse_override.timeline]\n"
			"wheel_vertical_scrollbar=0\n\n"
			"[mouse_override.layer]\n"
			"enabled=0\n\n"
			"[mouse_override.zoom_gauge]\n"
			"wheel=0"
		)) {
		settings.timeline.wheel_vertical_scrollbar = 0;
		settings.layer.enabled = false;
		settings.zoom_gauge.wheel = 0;
	}

	return true;
}
