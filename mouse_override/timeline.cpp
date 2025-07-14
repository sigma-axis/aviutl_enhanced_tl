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
#include <set>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using byte = uint8_t;
#include <exedit.hpp>

#include "../key_states.hpp"
#include "../modkeys.hpp"
#include "../timeline.hpp"
#include "mouse_actions.hpp"

#include "../enhanced_tl.hpp"
#include "../mouse_override.hpp"
#include "../layer_resize.hpp"
#include "../walkaround.hpp"
#include "../bpm_grid.hpp"

#include "timeline.hpp"


#define NS_BEGIN(...) namespace __VA_ARGS__ {
#define NS_END }
NS_BEGIN()

////////////////////////////////
// 拡張編集マウス操作の換装．
////////////////////////////////
using sigma_lib::modifier_keys::modkeys, sigma_lib::W32::UI::ForceKeyState;
using sigma_lib::W32::UI::key_map;
using namespace enhanced_tl::mouse_override::timeline;
namespace tl = enhanced_tl::timeline;
namespace wa = enhanced_tl::walkaround;
namespace lr = enhanced_tl::layer_resize::internal;

// smooth horizontal zoom by drags::Zoom_Bi.
namespace smooth_zoom
{
	static int center_frame{}, center_point{};
	static void pivot(int frame) {
		// to address cummulative error.
		smooth_zoom::center_frame = frame;
		smooth_zoom::center_point = tl::point_from_frame(frame);
	}

	// identifies "refined" zoom level at the current state.
	static int get(int divs)
	{
		int32_t const len = *exedit.curr_timeline_zoom_len;

		// identify the "raw" zoom level.
		int m = 0;
		for (int M = tl::constants::num_zoom_levels - 1; m < M;) {
			int const k = (M + m) >> 1;
			if (len >= exedit.timeline_zoom_lengths[k + 1])
				m = k + 1;
			else M = k;
		}
		if (m >= tl::constants::num_zoom_levels - 1) return divs * (tl::constants::num_zoom_levels - 1);

		// divide the interval by `divs` sections.
		int const dlen = exedit.timeline_zoom_lengths[m + 1] - exedit.timeline_zoom_lengths[m];
		int const dlev = std::clamp(((len - exedit.timeline_zoom_lengths[m]) * divs + (dlen >> 1)) / dlen, 0, divs);
		return m * divs + dlev;
	}

	// applies "refined" zoom level.
	static void set(int level, int divs)
	{
		int32_t const len = *exedit.curr_timeline_zoom_len;

		// calculate the length and the "raw" level, corresponding to the "refined" level.
		int len2, m;
		if (level <= 0 || level >= divs * (tl::constants::num_zoom_levels - 1)) {
			m = level <= 0 ? 0 : tl::constants::num_zoom_levels - 1;
			len2 = exedit.timeline_zoom_lengths[m];
		}
		else {
			m = level / divs; int f = level % divs;
			int dlen = exedit.timeline_zoom_lengths[m + 1] - exedit.timeline_zoom_lengths[m];
			len2 = exedit.timeline_zoom_lengths[m] + (dlen * f) / divs;
			if (2 * f >= divs) m++; // round to the nearest.
		}
		if (len == len2) return; // no need to modify.

		// remove cummulative error.
		*exedit.timeline_h_scroll_pos +=
			smooth_zoom::center_frame - tl::point_to_frame(smooth_zoom::center_point);

		// apply the zoom level under a tweaked state.
		*exedit.curr_timeline_zoom_level = -1;
		auto const t = std::exchange(exedit.timeline_zoom_lengths[m], len2);
		exedit.set_timeline_zoom(m, center_frame);
		exedit.timeline_zoom_lengths[m] = t;
	}
}

static constexpr LPARAM point_to_wp(int x, int y) {
	return (y << 16) | static_cast<uint16_t>(x);
}
static constexpr WPARAM keys_to_wp(modkeys mkeys) {
	return
		(mkeys.has_flags(modkeys::ctrl) ? MK_CONTROL : 0) |
		(mkeys.has_flags(modkeys::shift) ? MK_SHIFT : 0);
}
static constexpr WPARAM keys_to_wp(enhanced_tl::mouse_override::mouse_button btn, modkeys mkeys) {
	return buttons_to_wp(btn) | keys_to_wp(mkeys);
}
static constexpr bool key_changed(LPARAM l) {
	return ((l ^ (l >> 1)) >> 30) == 0;
}

static bool is_obj_edge_nearby(int client_x, int client_y)
{
	constexpr int thresh_near = 16;
	int const idx = exedit.obj_from_point(client_x, client_y);
	if (idx < 0) return false;

	auto const& obj = (*exedit.ObjectArray_ptr)[idx];
	return std::abs(tl::point_from_frame(obj.frame_begin) - client_x) < thresh_near
		|| std::abs(tl::point_from_frame(obj.frame_end) - client_x) < thresh_near;
}

static std::pair<int, int> select_num_beats_BPM(modkeys mkeys)
{
	int const N = enhanced_tl::mouse_override::settings.timeline.BPM[mkeys];
	if (N >= 0) return { 1, std::max(N, 1) }; // 1 / N beats.
	return { (-N) * *exedit.timeline_BPM_num_beats, 1 }; // (-N) measure lines.
}

static bool move_frame(int frame)
{
	if (frame == *exedit.curr_edit_frame) return false; // current frame didn't change.

	ForceKeyState k{ VK_SHIFT, wa::settings.suppress_shift ? key_map::off : key_map::id };
	exedit.fp->exfunc->set_frame(*exedit.editp, frame);
	return true; // refresh the current frame.
}

static bool step_layer(int screen_x, int screen_y, int delta, bool skip_midpt)
{
	wa::on_menu_command(enhanced_tl::this_fp->hwnd, wa::menu::step_midpt_left, *exedit.editp);

	POINT pt_client{ screen_x, screen_y };
	::ScreenToClient(exedit.fp->hwnd, &pt_client);
	int const layer = std::max(tl::point_to_layer(pt_client.y), *exedit.timeline_v_scroll_pos),
		pos = *exedit.curr_edit_frame,
		new_pos = delta > 0 ?
		tl::find_adjacent_left(pos, layer, skip_midpt, wa::settings.skip_inactive_objects) :
		tl::find_adjacent_right(pos, layer, skip_midpt, wa::settings.skip_inactive_objects, *exedit.curr_scene_len);
	return wa::move_frame(new_pos, *exedit.editp);
}

// adding mid-point.
static bool add_midpt(int x, int frame, int layer)
{
	// find the frame to add a mid-point.
	constexpr int thresh_near = 8;
	if (int const nearby = exedit.find_nearby_frame(frame, layer, 0);
		std::abs(x - tl::point_from_frame(nearby)) < thresh_near) frame = nearby;

	// find the objects to add a mid-point to.
	std::set<int32_t> targets;
	if (auto const* const objects = *exedit.ObjectArray_ptr;
		*exedit.SelectedObjectNum_ptr > 0) {
		// if objects are multi-selected, they are the targets.
		targets = get_multi_selected_objects();

		// limit to the objects that don't contain the specified frame.
		for (auto it = targets.begin(), e = targets.end(); it != e; ) {
			auto const curr = it++;
			auto const idx_obj = *curr;

			auto& obj = objects[idx_obj];
			if (!(obj.frame_begin < frame && frame <= obj.frame_end))
				targets.erase(curr);
		}
	}
	else {
		// otherwise the clicked object is the target.
		int const idx_obj = tl::object_at_frame(frame, layer);
		if (idx_obj < 0) return false;

		auto& obj = objects[idx_obj];
		if (obj.frame_begin == frame) return false;
		targets = { idx_obj };
	}
	if (targets.empty()) return false;

	// prepare undo buffer.
	exedit.nextundo();

	bool modified = false;
	for (auto idx_obj : targets) {
		// add a mid-point.
		int const idx_new = exedit.add_midpoint(idx_obj, frame, 0);
		if (idx_new < 0) [[unlikely]] continue;
		modified = true;

		// add to the multi-object selection.
		if (*exedit.SelectedObjectNum_ptr > 0)
			exedit.SelectedObjectIndex[(*exedit.SelectedObjectNum_ptr)++] = idx_new;
	}
	if (!modified) return false;

	// update the setting dialog.
	if (targets.contains(*exedit.SettingDialogObjectIndex))
		*exedit.SettingDialogObjectIndex = exedit.NextObjectIdxArray[*exedit.SettingDialogObjectIndex];
	if (*exedit.SettingDialogObjectIndex >= 0) {
		ForceKeyState k{ VK_CONTROL, true };
		update_setting_dialog(*exedit.SettingDialogObjectIndex);
	}
	return true;
}

// deleting mid-point.
static bool delete_midpt(int x, int frame, int layer)
{
	// find the nearest mid-point at the clicked position.
	int const
		i = tl::find_left_sorted_index(frame, layer);
	if (i < 0)
		// either no object on the layer,
		// or the nearest point is the beginning of an object.
		return false;

	ExEdit::Object const* const obj_l = exedit.SortedObject[i];
	int const frame_l = obj_l->frame_begin, frame_r = obj_l->frame_end + 1;
	if (frame_r <= frame)
		// an object ends on the left and another begins on the right,
		// both of which are not mid-points.
		return false;
	int const idx_leader = obj_l->index_midpt_leader;
	if (idx_leader < 0)
		// in the middle of an object without a mid-point.
		return false;

	// determine the object whose beginning frame is the mid-point to delete.
	int idx_midpt, frame_midpt;
	if (int const
		idx_l = obj_l - *exedit.ObjectArray_ptr,
		idx_r = exedit.NextObjectIdxArray[idx_l];
		(idx_leader != idx_l) && ((idx_r < 0) || (2 * frame <= frame_l + frame_r))) {
		// choose the left.
		idx_midpt = idx_l;
		frame_midpt = frame_l;
	}
	else {
		// choose the right.
		idx_midpt = idx_r;
		frame_midpt = frame_r;
	}

	// see if it's sufficiently near from the clicked point.
	constexpr int thresh_near = 16;
	if (std::abs(x - tl::point_from_frame(frame_midpt)) >= thresh_near)
		return false;

	// determine the all mid-points to remove.
	std::set<int32_t> selected = get_multi_selected_objects(), targets;
	if (auto const* const objects = *exedit.ObjectArray_ptr;
		!selected.empty()) {
		// if objects are multi-selected, they are the targets.
		targets = selected;

		// limit to only the objects that begin with the mid-point at the specified frame.
		for (auto it = targets.begin(), e = targets.end(); it != e; ) {
			auto const curr = it++;
			auto const idx_obj = *curr;

			auto& obj = objects[idx_obj];
			if (obj.frame_begin == frame_midpt) { // begins at the specified frame.
				if (int const idx_leader = obj.index_midpt_leader;
					idx_leader >= 0 && idx_obj != idx_leader) // the beginning frame is a mid-point.
					continue;
			}
			targets.erase(curr);
		}
		if (targets.empty()) return false;
	}
	else // otherwise the clicked object is the target.
		targets = { idx_midpt };

	// prepare undo buffer.
	exedit.nextundo();

	bool modified = false;
	for (auto idx_obj : targets) {
		// remove the mid-point.
		int const idx_old = exedit.delete_midpoint(idx_obj, frame_midpt);
		if (idx_old < 0) continue;
		modified = true;

		// remove the object from the multi-object selection.
		selected.erase(idx_old);
	}
	if (!modified) return false;
	set_multi_selected_objects(selected);

	return true;
}

// buffering the selection origin.
static int32_t back_selection_origin = 0;

// backup values for drag operations.
static int prev_frame{}, prev_layer{}, prev_zoom_h{}, prev_zoom_v{};
static int32_t prev_scene{};
NS_END


////////////////////////////////
// Exported functions.
////////////////////////////////
namespace expt = enhanced_tl::mouse_override::timeline;

////////////////////////////////
// ドラッグ．
////////////////////////////////
// drags::L
bool expt::drags::L::on_mouse_down_core(modkeys mkeys)
{
	back_selection_origin = *exedit.idx_obj_selection_origin;

	bool ret;
	{
		bool const suppress_shift =
			exedit.obj_from_point(pt_start.x, pt_start.y) >= 0 &&
			mkeys.has_flags(modkeys::ctrl);
		ForceKeyState k{ VK_SHIFT, suppress_shift ? key_map::off : key_map::id };
		ret = FALSE != internal::call_next_proc(WM_LBUTTONDOWN,
			keys_to_wp(mouse_button::L, suppress_shift ? (mkeys & ~modkeys::shift) : mkeys),
			point_to_wp(pt_start.x, pt_start.y));
	}

	// save states to rewind.
	switch (*exedit.timeline_drag_kind) {
	case drag_kind::move_frame:
		prev_frame = *exedit.curr_edit_frame;
		break;
	case drag_kind::scroll:
		prev_frame = tl::point_to_frame(pt_start.x);
		prev_layer = tl::point_to_layer(pt_start.y);
		if (settings.timeline.change_cursor)
			::SetCursor(::LoadCursorW(nullptr, reinterpret_cast<wchar_t const*>(IDC_SIZEALL)));
		break;
	}

	return ret;
}
bool expt::drags::L::on_mouse_move_core(modkeys mkeys)
{
	return FALSE != internal::call_next_proc(WM_MOUSEMOVE,
		keys_to_wp(mouse_button::L, mkeys),
		point_to_wp(pt_curr.x, pt_curr.y));
}
bool expt::drags::L::on_mouse_up_core(modkeys mkeys)
{
	return FALSE != internal::call_next_proc(WM_LBUTTONUP,
		keys_to_wp(mkeys),
		point_to_wp(pt_curr.x, pt_curr.y));
}
bool expt::drags::L::on_mouse_cancel_core(bool release)
{
	if (!release) {
		switch (*exedit.timeline_drag_kind) {
		case drag_kind::move_object:
		case drag_kind::move_object_left:
		case drag_kind::move_object_right:
			// releasing Ctrl aborts dragging and deselects objects.
			internal::call_next_proc(WM_KEYUP, VK_CONTROL, 0);
			break;
		case drag_kind::move_frame:
			// rewind to the previous frame.
			exedit.fp->exfunc->set_frame(*exedit.editp, prev_frame);
			break;
		case drag_kind::scroll:
			// rewind the scroll states.
			wa::set_frame_scroll(*exedit.timeline_h_scroll_pos + prev_frame - tl::point_to_frame(pt_start.x), *exedit.editp);
			wa::set_layer_scroll(*exedit.timeline_v_scroll_pos + prev_layer - tl::point_to_layer(pt_start.y), *exedit.editp);
			break;
		default:
			break; // others need no special treats.
		}
	}

	*exedit.timeline_drag_kind = drag_kind::none;
	if (release) ::ReleaseCapture();
	return false;
}

// drags::Obj_L
bool expt::drags::Obj_L::can_continue() const { return is_editing(); }
bool expt::drags::Obj_L::on_mouse_down_core(modkeys mkeys)
{
	back_selection_origin = *exedit.idx_obj_selection_origin;

	// assumes the point is on a object.
	ForceKeyState k{
		VK_CONTROL, false,
		VK_SHIFT, false,
		VK_MENU, false,
	};
	return FALSE != internal::call_next_proc(WM_LBUTTONDOWN,
		buttons_to_wp(mouse_button::L),
		point_to_wp(pt_start.x, pt_start.y));
}
bool expt::drags::Obj_L::on_mouse_move_core(modkeys mkeys)
{
	ForceKeyState k{
		VK_CONTROL, false,
		VK_MENU, false,
	};
	return FALSE != internal::call_next_proc(WM_MOUSEMOVE,
		keys_to_wp(mouse_button::L, mkeys & modkeys::shift),
		point_to_wp(pt_curr.x, pt_curr.y));
}
bool expt::drags::Obj_L::on_mouse_up_core(modkeys mkeys)
{
	ForceKeyState k{
		VK_CONTROL, false,
		VK_MENU, false,
	};
	return FALSE != internal::call_next_proc(WM_LBUTTONUP,
		keys_to_wp(mkeys & modkeys::shift),
		point_to_wp(pt_curr.x, pt_curr.y));
}
bool expt::drags::Obj_L::on_mouse_cancel_core(bool release)
{
	if (!release)
		// releasing Ctrl aborts dragging and deselects objects.
		internal::call_next_proc(WM_KEYUP, VK_CONTROL, 0);

	*exedit.timeline_drag_kind = drag_kind::none;
	if (release) ::ReleaseCapture();
	return false;
}
bool expt::drags::Obj_L::handle_key_messages_core(bool& ret, UINT message, WPARAM wparam, LPARAM lparam)
{
	if (wparam == VK_SHIFT && key_changed(lparam)) return false;
	return true;
}

// drags::Obj_Ctrl_L
bool expt::drags::Obj_Ctrl_L::can_continue() const { return is_editing(); }
bool expt::drags::Obj_Ctrl_L::on_mouse_down_core(modkeys mkeys)
{
	back_selection_origin = *exedit.idx_obj_selection_origin;

	// assumes the point is on a object.
	ForceKeyState k{
		VK_CONTROL, true,
		VK_SHIFT, false,
		VK_MENU, false,
	};
	return FALSE != internal::call_next_proc(WM_LBUTTONDOWN,
		keys_to_wp(mouse_button::L, modkeys::ctrl),
		point_to_wp(pt_start.x, pt_start.y));
}
bool expt::drags::Obj_Ctrl_L::on_mouse_move_core(modkeys mkeys)
{
	ForceKeyState k{
		VK_CONTROL, true,
		VK_MENU, false,
	};
	return FALSE != internal::call_next_proc(WM_MOUSEMOVE,
		keys_to_wp(mouse_button::L, (mkeys & modkeys::shift) | modkeys::ctrl),
		point_to_wp(pt_curr.x, pt_curr.y));
}
bool expt::drags::Obj_Ctrl_L::on_mouse_up_core(modkeys mkeys)
{
	ForceKeyState k{
		VK_CONTROL, true,
		VK_MENU, false,
	};
	return FALSE != internal::call_next_proc(WM_LBUTTONUP,
		keys_to_wp((mkeys & modkeys::shift) | modkeys::ctrl),
		point_to_wp(pt_curr.x, pt_curr.y));
}
bool expt::drags::Obj_Ctrl_L::on_mouse_cancel_core(bool release)
{
	if (!release)
		// releasing Ctrl aborts dragging and deselects objects.
		internal::call_next_proc(WM_KEYUP, VK_CONTROL, 0);

	*exedit.timeline_drag_kind = drag_kind::none;
	if (release) ::ReleaseCapture();
	return false;
}
bool expt::drags::Obj_Ctrl_L::handle_key_messages_core(bool& ret, UINT message, WPARAM wparam, LPARAM lparam)
{
	if (wparam == VK_SHIFT && key_changed(lparam)) return false;
	return true;
}

// drags::Obj_Shift_L
bool expt::drags::Obj_Shift_L::can_continue() const { return is_editing(); }
bool expt::drags::Obj_Shift_L::on_mouse_down_core(modkeys mkeys)
{
	back_selection_origin = *exedit.idx_obj_selection_origin;

	// assumes the point is on a object.

	// check if an edge of an object is nearby.
	if (!is_obj_edge_nearby(pt_start.x, pt_start.y))
		// fallback to obj_l.
		return fallback_on_down(obj_l, mkeys);

	ForceKeyState k{
		VK_CONTROL, false,
		VK_SHIFT, true,
		VK_MENU, false,
	};
	return FALSE != internal::call_next_proc(WM_LBUTTONDOWN,
		keys_to_wp(mouse_button::L, modkeys::shift),
		point_to_wp(pt_start.x, pt_start.y));
}
bool expt::drags::Obj_Shift_L::on_mouse_move_core(modkeys mkeys)
{
	ForceKeyState k{
		VK_CONTROL, false,
		VK_SHIFT, true,
		VK_MENU, false,
	};
	return FALSE != internal::call_next_proc(WM_MOUSEMOVE,
		keys_to_wp(mouse_button::L, modkeys::shift),
		point_to_wp(pt_curr.x, pt_curr.y));
}
bool expt::drags::Obj_Shift_L::on_mouse_up_core(modkeys mkeys)
{
	ForceKeyState k{
		VK_CONTROL, false,
		VK_SHIFT, true,
		VK_MENU, false,
	};
	return FALSE != internal::call_next_proc(WM_LBUTTONUP,
		keys_to_wp(modkeys::shift),
		point_to_wp(pt_curr.x, pt_curr.y));
}
bool expt::drags::Obj_Shift_L::on_mouse_cancel_core(bool release)
{
	if (!release)
		// releasing Ctrl aborts dragging and deselects objects.
		internal::call_next_proc(WM_KEYUP, VK_CONTROL, 0);

	*exedit.timeline_drag_kind = drag_kind::none;
	if (release) ::ReleaseCapture();
	return false;
}
bool expt::drags::Obj_Shift_L::handle_key_messages_core(bool& ret, UINT message, WPARAM wparam, LPARAM lparam)
{
	return true;
}

// drags::Bk_L
bool expt::drags::Bk_L::can_continue() const { return is_editing() && (!active() || prev_scene == *exedit.current_scene); }
bool expt::drags::Bk_L::on_mouse_down_core(modkeys mkeys)
{
	prev_frame = *exedit.curr_edit_frame;
	prev_scene = *exedit.current_scene;
	auto const frame = tl::point_to_frame(pt_start.x);

	// check if the cursor is on an object.
	if (exedit.obj_from_point(pt_start.x, pt_start.y) < 0) {
		ForceKeyState k{
			VK_CONTROL, false,
			VK_SHIFT, false,
			VK_MENU, false,
		};
		return FALSE != internal::call_next_proc(WM_LBUTTONDOWN,
			buttons_to_wp(mouse_button::L),
			point_to_wp(pt_start.x, pt_start.y));
	}

	// if the cursor is on an object, initiate Alt+drag and change the mode.
	// initiating Alt+drag is in order to update the click position for pasting,
	// which involves modified code by patch.aul.
	{
		ForceKeyState k{ VK_MENU, true };
		internal::call_next_proc(WM_LBUTTONDOWN,
			MK_LBUTTON,
			point_to_wp(pt_start.x, pt_start.y));
	}

	// then turn to frame-moving drag.
	exedit.adjust_h_scroll(pt_start.x);
	exedit.fp->exfunc->set_frame(*exedit.editp, frame);
	*exedit.timeline_drag_kind = drag_kind::move_frame;
	return true;
}
bool expt::drags::Bk_L::on_mouse_move_core(modkeys mkeys)
{
	return FALSE != internal::call_next_proc(WM_MOUSEMOVE,
		keys_to_wp(mouse_button::L, mkeys),
		point_to_wp(pt_curr.x, pt_curr.y));
}
bool expt::drags::Bk_L::on_mouse_up_core(modkeys mkeys)
{
	return FALSE != internal::call_next_proc(WM_LBUTTONUP,
		keys_to_wp(mkeys),
		point_to_wp(pt_curr.x, pt_curr.y));
}
bool expt::drags::Bk_L::on_mouse_cancel_core(bool release)
{
	*exedit.timeline_drag_kind = drag_kind::none;
	if (release) ::ReleaseCapture();
	return prev_scene == *exedit.current_scene
		&& wa::move_frame(prev_frame, *exedit.editp);
}
bool expt::drags::Bk_L::handle_key_messages_core(bool& ret, UINT message, WPARAM wparam, LPARAM lparam)
{
	if (wparam == VK_SHIFT && key_changed(lparam)) return false;
	return true;
}

// drags::Bk_Ctrl_L
bool expt::drags::Bk_Ctrl_L::can_continue() const { return is_editing() && (!active() || prev_scene == *exedit.current_scene); }
bool expt::drags::Bk_Ctrl_L::on_mouse_down_core(modkeys mkeys)
{
	prev_scene = *exedit.current_scene;

	// check if the cursor is on an object.
	if (exedit.obj_from_point(pt_start.x, pt_start.y) < 0) {
		ForceKeyState k{
			VK_CONTROL, true,
			VK_SHIFT, false,
			VK_MENU, false,
		};
		return FALSE != internal::call_next_proc(WM_LBUTTONDOWN,
			keys_to_wp(mouse_button::L, modkeys::ctrl),
			point_to_wp(pt_start.x, pt_start.y));
	}

	// if the cursor is on an object, initiate Alt+drag and change the mode.
	// initiating Alt+drag is in order to update the click position for pasting,
	// which involves modified code by patch.aul.
	{
		ForceKeyState k{ VK_MENU, true };
		internal::call_next_proc(WM_LBUTTONDOWN,
			MK_LBUTTON,
			point_to_wp(pt_start.x, pt_start.y));
	}

	// then turn to range-selection drag.
	exedit.begin_range_selection(pt_start.x, pt_start.y);
	*exedit.timeline_drag_kind = drag_kind::range_select;
	return false;
}
bool expt::drags::Bk_Ctrl_L::on_mouse_move_core(modkeys mkeys)
{
	ForceKeyState k{
		VK_CONTROL, true,
		VK_SHIFT, false,
		VK_MENU, false,
	};
	return FALSE != internal::call_next_proc(WM_MOUSEMOVE,
		keys_to_wp(mouse_button::L, modkeys::ctrl),
		point_to_wp(pt_curr.x, pt_curr.y));
}
bool expt::drags::Bk_Ctrl_L::on_mouse_up_core(modkeys mkeys)
{
	ForceKeyState k{
		VK_CONTROL, true,
		VK_SHIFT, false,
		VK_MENU, false,
	};
	return FALSE != internal::call_next_proc(WM_LBUTTONUP,
		keys_to_wp(modkeys::ctrl),
		point_to_wp(pt_curr.x, pt_curr.y));
}
bool expt::drags::Bk_Ctrl_L::on_mouse_cancel_core(bool release)
{
	// releasing Ctrl aborts dragging and deselects objects.
	internal::call_next_proc(WM_KEYUP, VK_CONTROL, 0);
	return false;
}
bool expt::drags::Bk_Ctrl_L::handle_key_messages_core(bool& ret, UINT message, WPARAM wparam, LPARAM lparam)
{
	return true;
}

// drags::Alt_L
bool expt::drags::Alt_L::on_mouse_down_core(modkeys mkeys)
{
	prev_frame = tl::point_to_frame(pt_start.x);
	prev_layer = tl::point_to_layer(pt_start.y);
	prev_scene = *exedit.current_scene;

	ForceKeyState k{
		VK_CONTROL, false,
		VK_SHIFT, false,
		VK_MENU, true,
	};
	if (settings.timeline.change_cursor)
		::SetCursor(::LoadCursorW(nullptr, reinterpret_cast<wchar_t const*>(IDC_SIZEALL)));
	return FALSE != internal::call_next_proc(WM_LBUTTONDOWN,
		MK_LBUTTON,
		point_to_wp(pt_start.x, pt_start.y));
}
bool expt::drags::Alt_L::on_mouse_move_core(modkeys mkeys)
{
	return FALSE != internal::call_next_proc(WM_MOUSEMOVE,
		keys_to_wp(mouse_button::L, mkeys),
		point_to_wp(pt_curr.x, pt_curr.y));
}
bool expt::drags::Alt_L::on_mouse_up_core(modkeys mkeys)
{
	return FALSE != internal::call_next_proc(WM_LBUTTONUP,
		keys_to_wp(mkeys),
		point_to_wp(pt_curr.x, pt_curr.y));
}
bool expt::drags::Alt_L::on_mouse_cancel_core(bool release)
{
	*exedit.timeline_drag_kind = drag_kind::none;
	if (release) ::ReleaseCapture();

	if (prev_scene == *exedit.current_scene) {
		wa::set_frame_scroll(*exedit.timeline_h_scroll_pos + prev_frame - tl::point_to_frame(pt_start.x), *exedit.editp);
		wa::set_layer_scroll(*exedit.timeline_v_scroll_pos + prev_layer - tl::point_to_layer(pt_start.y), *exedit.editp);
	}
	return false;
}

// drags::Zoom_Bi
bool expt::drags::Zoom_Bi::can_continue() const { return !active() || prev_scene == *exedit.current_scene; }
bool expt::drags::Zoom_Bi::on_mouse_down_core(modkeys mkeys)
{
	prev_zoom_h = smooth_zoom::get(settings.timeline.zoom_drag_refine_x);
	prev_frame = *exedit.timeline_h_scroll_pos;
	prev_zoom_v = lr::get_layer_size_delayed();
	smooth_zoom::pivot(internal::zoom_center_frame(settings.timeline.zoom_center_drag, pt_start.x));
	prev_scene = *exedit.current_scene;

	::SetCapture(target);
	::SetCursor(::LoadCursorW(nullptr, reinterpret_cast<wchar_t const*>(IDC_CROSS)));
	return false;
}
bool expt::drags::Zoom_Bi::on_mouse_move_core(modkeys mkeys)
{
	// calculate the delta values of each scalings.
	int const delta_h = settings.timeline.zoom_drag_length_x == 0 ? 0 :
		((pt_curr.x - pt_start.x) * settings.timeline.zoom_drag_refine_x)
		/ settings.timeline.zoom_drag_length_x;
	int const delta_v = settings.timeline.zoom_drag_length_y == 0 ? 0 :
		(pt_curr.y - pt_start.y) / settings.timeline.zoom_drag_length_y;

	// apply zooming; layer-wise comes first.
	lr::set_layer_size(prev_zoom_v + delta_v, true);
	smooth_zoom::set(prev_zoom_h + delta_h, settings.timeline.zoom_drag_refine_x);
	return false;
}
bool expt::drags::Zoom_Bi::on_mouse_up_core(modkeys mkeys)
{
	::ReleaseCapture();
	return false;
}
bool expt::drags::Zoom_Bi::on_mouse_cancel_core(bool release)
{
	if (release) ::ReleaseCapture();

	// rewind the states.
	if (prev_scene == *exedit.current_scene) {
		lr::set_layer_size(prev_zoom_v, false);
		smooth_zoom::set(prev_zoom_h, settings.timeline.zoom_drag_refine_x);
		wa::set_frame_scroll(prev_frame, *exedit.editp);
	}
	return false;
}

// drags::Step_Bound
bool expt::drags::Step_Bound::on_mouse_down_core(modkeys mkeys)
{
	// save the state to rewind later.
	prev_frame = *exedit.curr_edit_frame;
	::SetCapture(target);
	::SetCursor(::LoadCursorW(nullptr, reinterpret_cast<wchar_t const*>(IDC_UPARROW)));
	return on_mouse_move_core(mkeys);
}
bool expt::drags::Step_Bound::on_mouse_move_core(modkeys mkeys)
{
	// determine whether to skip mid-points and inactive objects.
	bool const skip_midpt
		= settings.timeline.skip_midpt_def
		^ mkeys.has_flags_any(settings.timeline.skip_midpt_key);
	bool const skip_inactives
		= settings.timeline.skip_inactives_def
		^ mkeys.has_flags_any(settings.timeline.skip_inactives_key);

	constexpr int thresh_near = 16;

	// get the current frame and layer.
	int const
		frame = tl::point_to_frame(pt_curr.x),
		layer = tl::point_to_layer(pt_curr.y);

	// find the nearby boundary.
	auto [l, r] = tl::find_interval(frame, layer, skip_midpt, skip_inactives);
	if (r < 0) r = *exedit.curr_scene_len - 1;
	int const l_x = tl::point_from_frame(l), r_x = tl::point_from_frame(r);

	// choose the closer one.
	int dest, dist;
	if (2 * pt_curr.x <= l_x + r_x) {
		dest = l; dist = pt_curr.x - l_x;
	}
	else {
		dest = r; dist = r_x - pt_curr.x;
	}

	return dist <= thresh_near && dest != *exedit.curr_edit_frame
		&& move_frame(dest);
}
bool expt::drags::Step_Bound::on_mouse_up_core(modkeys mkeys)
{
	::ReleaseCapture();
	return false; // no need to refresh the current frame.
}
bool expt::drags::Step_Bound::on_mouse_cancel_core(bool release)
{
	if (release) ::ReleaseCapture();

	// rewind the current frame.
	return move_frame(prev_frame);
}

// drags::Step_BPM
bool expt::drags::Step_BPM::on_mouse_down_core(modkeys mkeys)
{
	// save the state to rewind later.
	prev_frame = *exedit.curr_edit_frame;
	::SetCapture(target);
	::SetCursor(::LoadCursorW(nullptr, reinterpret_cast<wchar_t const*>(IDC_UPARROW)));
	return on_mouse_move_core(mkeys);
}
bool expt::drags::Step_BPM::on_mouse_move_core(modkeys mkeys)
{
	namespace bpm = enhanced_tl::BPM_Grid;
	constexpr int thresh_near = 16;

	// get the current frame.
	int const frame = tl::point_to_frame(pt_curr.x);

	// find the nearby grid line.
	auto const [beat_numer, beat_denom] = select_num_beats_BPM(mkeys);
	bpm::BPM_Grid const grid{ beat_numer, beat_denom };
	auto const beat = grid.beat_from_pos(frame);
	int const
		l = grid.pos_from_beat(beat),
		r = grid.pos_from_beat(beat + 1);
	int const
		l_x = tl::point_from_frame(l),
		r_x = tl::point_from_frame(r);

	// choose the closer one.
	int dest, dist;
	if (2 * pt_curr.x <= l_x + r_x) {
		dest = l; dist = pt_curr.x - l_x;
	}
	else {
		dest = r; dist = r_x - pt_curr.x;
	}

	return dist <= thresh_near && dest != *exedit.curr_edit_frame
		&& move_frame(dest);
}
bool expt::drags::Step_BPM::on_mouse_up_core(modkeys mkeys)
{
	::ReleaseCapture();
	return false; // no need to refresh the current frame.
}
bool expt::drags::Step_BPM::on_mouse_cancel_core(bool release)
{
	if (release) ::ReleaseCapture();

	// rewind the current frame.
	return move_frame(prev_frame);
}

////////////////////////////////
// クリック．
////////////////////////////////
// clicks::obj_ctrl_shift_l
bool expt::clicks::obj_ctrl_shift_l(int x, int y, modkeys mkeys)
{
	// assumes the point is on a object.
	int const idx_obj = exedit.obj_from_point(x, y); // >= 0.
	// remove this object from the selection.
	for (int i = *exedit.SelectedObjectNum_ptr; --i >= 0; ) {
		if (exedit.SelectedObjectIndex[i] == idx_obj) {
			exedit.SelectedObjectIndex[i]
				= exedit.SelectedObjectIndex[--(*exedit.SelectedObjectNum_ptr)];
			break;
		}
	}
	// revert the "last clicked object" by one step.
	if (idx_obj == *exedit.idx_obj_selection_origin &&
		back_selection_origin >= 0) {
		if (auto const& obj = (*exedit.ObjectArray_ptr)[back_selection_origin];
			has_flag_or(obj.flag, ExEdit::Object::Flag::Exist) &&
			obj.scene_set == *exedit.current_scene)
			*exedit.idx_obj_selection_origin = back_selection_origin;
	}

	ForceKeyState k{
		VK_CONTROL, true,
		VK_SHIFT, true,
		VK_MENU, false,
	};
	bool ret = FALSE != internal::call_next_proc(WM_LBUTTONDOWN,
		keys_to_wp(mouse_button::L, modkeys::ctrl | modkeys::shift),
		point_to_wp(x, y));
	ret |= FALSE != internal::call_next_proc(WM_LBUTTONUP,
		keys_to_wp(modkeys::ctrl | modkeys::shift),
		point_to_wp(x, y));
	return ret;
}

// clicks::obj_l_dbl
bool expt::clicks::obj_l_dbl(int x, int y, modkeys mkeys)
{
	// assumes the point is on a object.
	return FALSE != internal::call_next_proc(WM_LBUTTONDBLCLK,
		keys_to_wp(mouse_button::L, mkeys),
		point_to_wp(x, y));
}

// clicks::rclick
bool expt::clicks::rclick(int x, int y, modkeys mkeys)
{
	return FALSE != internal::call_next_proc(WM_RBUTTONDOWN,
		keys_to_wp(mouse_button::R, mkeys),
		point_to_wp(x, y));
}

// clicks::toggle_midpt
bool expt::clicks::toggle_midpt(int x, int y, modkeys mkeys)
{
	if (!is_editing()) return false;

	// first try to delete a mid-point, and if failed, try to add.
	int const
		frame = tl::point_to_frame(x),
		layer = tl::point_to_layer(y);
	if (delete_midpt(x, frame, layer) || add_midpt(x, frame, layer)) {
		// redraw the timeline.
		::InvalidateRect(exedit.fp->hwnd, nullptr, FALSE);
		return true;
	}
	return false;
}

// clicks::select_line_left
bool expt::clicks::select_line_left(int x, int y, modkeys mkeys)
{
	if (!is_editing()) return false;

	int const
		frame = tl::point_to_frame(x),
		layer = tl::point_to_layer(y);

	// ignore if the layer is locked.
	if (has_flag_or(exedit.LayerSettings[layer + tl::constants::num_layers  * *exedit.current_scene].flag,
		ExEdit::LayerSetting::Flag::Locked))
		return false;

	int const L = exedit.SortedObjectLayerBeginIndex[layer], R = exedit.SortedObjectLayerEndIndex[layer];
	if (L > R) return false; // no objects in the layer.
	int j = tl::find_left_sorted_index(frame, layer);
	if (j < 0) return false; // no objects on the left.

	// manipulate as sets.
	std::set<int> selected = get_multi_selected_objects(), targets{};

	bool contained = true;
	auto const* const head_ptr = *exedit.ObjectArray_ptr;
	for (; j >= L; j--) {
		int const idx = exedit.SortedObject[j] - head_ptr;
		targets.insert(idx);
		if (selected.erase(idx) == 0) contained = false;
	}
	if (!contained) selected.insert_range(targets);

	// write to the memory.
	set_multi_selected_objects(selected);

	// redraw the timeline.
	::InvalidateRect(exedit.fp->hwnd, nullptr, FALSE);

	return false;
}

// clicks::select_line_right
bool expt::clicks::select_line_right(int x, int y, modkeys mkeys)
{
	if (!is_editing()) return false;

	int const
		frame = tl::point_to_frame(x),
		layer = tl::point_to_layer(y);

	// ignore if the layer is locked.
	if (has_flag_or(exedit.LayerSettings[layer + tl::constants::num_layers * *exedit.current_scene].flag,
		ExEdit::LayerSetting::Flag::Locked))
		return false;

	int const L = exedit.SortedObjectLayerBeginIndex[layer], R = exedit.SortedObjectLayerEndIndex[layer];
	if (L > R) return false; // no objects in the layer.
	int j = tl::find_left_sorted_index(frame, layer);
	if (j < 0) j = L;
	else if (exedit.SortedObject[j]->frame_end < frame && ++j > R)
		return false; // no objects on the right.

	// manipulate as sets.
	std::set<int> selected = get_multi_selected_objects(), targets{};

	bool contained = true;
	auto const* const head_ptr = *exedit.ObjectArray_ptr;
	for (; j <= R; j++) {
		int const idx = exedit.SortedObject[j] - head_ptr;
		targets.insert(idx);
		if (selected.erase(idx) == 0) contained = false;
	}
	if (!contained) selected.insert_range(targets);

	// write to the memory.
	set_multi_selected_objects(selected);

	// redraw the timeline.
	::InvalidateRect(exedit.fp->hwnd, nullptr, FALSE);

	return false;
}

// clicks::select_all
bool expt::clicks::select_all(int x, int y, modkeys mkeys)
{
	if (!is_editing()) return false;

	auto const* const head_ptr = *exedit.ObjectArray_ptr;
	int const prev_count = std::exchange(*exedit.SelectedObjectNum_ptr, 0);
	auto const* layer_settings = &exedit.LayerSettings[tl::constants::num_layers * *exedit.current_scene];
	for (int l = 0; l < tl::constants::num_layers; l++) {
		// skip locked layers.
		if (has_flag_or(layer_settings[l].flag, ExEdit::LayerSetting::Flag::Locked)) continue;

		for (int j = exedit.SortedObjectLayerBeginIndex[l], R = exedit.SortedObjectLayerEndIndex[l];
			j <= R; j++) {
			exedit.SelectedObjectIndex[(*exedit.SelectedObjectNum_ptr)++]
				= exedit.SortedObject[j] - head_ptr;
		}
	}
	if (*exedit.SelectedObjectNum_ptr > 0) {
		if (prev_count == *exedit.SelectedObjectNum_ptr)
			// if all objects were already selected, deselect them all.
			exedit.deselect_all_objects();
		else ::InvalidateRect(exedit.fp->hwnd, nullptr, FALSE);
	}

	return false;
}

// clicks::squeeze_left
bool expt::clicks::squeeze_left(int x, int y, modkeys mkeys)
{
	if (!is_editing()) return false;

	// moves all object on the clicked point and the right
	// to left so there will be no space on the left.

	// get the current frame and layer.
	int const
		frame = tl::point_to_frame(x),
		layer = tl::point_to_layer(y);

	// ignore if the layer is locked.
	if (has_flag_or(exedit.LayerSettings[layer + tl::constants::num_layers * *exedit.current_scene].flag,
		ExEdit::LayerSetting::Flag::Locked))
		return false;

	// find the object position in the layer.
	int const
		L = exedit.SortedObjectLayerBeginIndex[layer],
		R = exedit.SortedObjectLayerEndIndex[layer];
	if (L > R) return false; // no objects on the layer.
	int i = tl::find_left_sorted_index(frame, layer);
	if (i >= 0 && exedit.SortedObject[i]->frame_end < frame) i++;
	if (i > R) return false; // no object on the right.

	// determine the left and right boundaries.
	int j = i < 0 ? L : i;
	int const
		l = i <= L ? 0 : exedit.SortedObject[i - 1]->frame_end + 1,
		r = exedit.SortedObject[j]->frame_begin,
		move_len = r - l;
	if (move_len <= 0) return false; // no need to move.

	// move the objects on the right by move_len.
	int const dlg_obj_idx = *exedit.SettingDialogObjectIndex;
	bool should_update_dialog = false;
	exedit.nextundo();
	for (auto const* const head_ptr = *exedit.ObjectArray_ptr;
		j <= R; j++) {
		auto* const obj = exedit.SortedObject[j];
		int const index = obj - head_ptr;

		exedit.setundo(index, 0x08);
		obj->frame_begin -= move_len;
		obj->frame_end -= move_len;

		if (index == dlg_obj_idx) should_update_dialog = true;
	}

	// redraw the timeline.
	::InvalidateRect(exedit.fp->hwnd, nullptr, FALSE);

	// update the dialog if the moved object is selected.
	if (should_update_dialog) {
		ForceKeyState k{ VK_CONTROL, true }; // otherwise objects are deselected.
		update_setting_dialog(dlg_obj_idx);
	}
	return true;
}

// clicks::squeeze_right
bool expt::clicks::squeeze_right(int x, int y, modkeys mkeys)
{
	if (!is_editing()) return false;

	// moves all object on the clicked point and the left
	// to right so there will be no space on the right.

	// get the current frame and layer.
	int const
		frame = tl::point_to_frame(x),
		layer = tl::point_to_layer(y);

	// ignore if the layer is locked.
	if (has_flag_or(exedit.LayerSettings[layer + tl::constants::num_layers * *exedit.current_scene].flag,
		ExEdit::LayerSetting::Flag::Locked))
		return false;

	// find the object position in the layer.
	int const
		L = exedit.SortedObjectLayerBeginIndex[layer],
		R = exedit.SortedObjectLayerEndIndex[layer];
	if (L > R) return false; // no objects on the layer.
	int const i = tl::find_left_sorted_index(frame, layer);
	if (i < 0) return false; // no object on the left.

	// determine the left and right boundaries.
	int j = i;
	int const
		l = exedit.SortedObject[i]->frame_end + 1,
		r = i >= R ? *exedit.curr_scene_len : exedit.SortedObject[i + 1]->frame_begin,
		move_len = r - l;
	if (move_len <= 0) return false; // no need to move.

	// move the objects on the right by move_len.
	int const dlg_obj_idx = *exedit.SettingDialogObjectIndex;
	bool should_update_dialog = false;
	exedit.nextundo();
	for (auto const* const head_ptr = *exedit.ObjectArray_ptr;
		j >= L; j--) {
		auto* const obj = exedit.SortedObject[j];
		int const index = obj - head_ptr;

		exedit.setundo(index, 0x08);
		obj->frame_begin += move_len;
		obj->frame_end += move_len;

		if (index == dlg_obj_idx) should_update_dialog = true;
	}

	// redraw the timeline.
	::InvalidateRect(exedit.fp->hwnd, nullptr, FALSE);

	// update the dialog if the moved object is selected.
	if (should_update_dialog) {
		ForceKeyState k{ VK_CONTROL, true }; // otherwise objects are deselected.
		update_setting_dialog(dlg_obj_idx);
	}
	return true;
}

// clicks::toggle_active
bool expt::clicks::toggle_active(int x, int y, modkeys mkeys)
{
	if (!is_editing()) return false;

	// find the object at the clicked position.
	int const idx_obj = exedit.obj_from_point(x, y);
	if (idx_obj < 0) return false; // no object at the clicked position.

	// determine the objects to toggle.
	auto* const objects = *exedit.ObjectArray_ptr;
	std::set<int32_t> targets{};
	{
		std::set<int32_t> selected = get_multi_selected_objects();
		selected.insert(idx_obj);

		// collect the leading objects.
		for (auto idx : selected) {
			int const idx_head = objects[idx].index_midpt_leader;
			targets.insert(idx_head < 0 ? idx : idx_head);
		}
	}
	
	// see whether the clicked object is active.
	constexpr auto flag_active = ExEdit::Object::FilterStatus::Active;
	bool flagging;
	{
		int const idx_head = objects[idx_obj].index_midpt_leader;
		flagging = !has_flag_or(objects[idx_head < 0 ? idx_obj : idx_head]
			.filter_status[0], flag_active);
	}

	// see whether a target object is on the setting dialog.
	int idx_dlg = *exedit.SettingDialogObjectIndex;
	if (idx_dlg >= 0) {
		int const idx_head = objects[idx_dlg].index_midpt_leader;
		if (!targets.contains(idx_head < 0 ? idx_dlg : idx_head))
			idx_dlg = -1;
	}

	// prepare the undo buffer.
	exedit.nextundo();

	// toggle the states of the first filter for each object.
	for (auto idx : targets) {
		auto& status = objects[idx].filter_status[0];
		if (flagging ^ has_flag_or(status, flag_active)) {
			exedit.setundo(idx, 0x01);
			status ^= flag_active;
		}
	}

	// redraw the timeline.
	::InvalidateRect(exedit.fp->hwnd, nullptr, FALSE);

	// update the setting dialog if the toggled object is selected.
	if (idx_dlg >= 0) {
		ForceKeyState k{ VK_CONTROL, true }; // otherwise objects are deselected.
		update_setting_dialog(idx_dlg);
	}

	return true; // requires updates to the main window.
}


////////////////////////////////
// ホイール．
////////////////////////////////
// wheels::scroll_h
bool expt::wheels::scroll_h(int screen_x, int screen_y, int delta, modkeys mkeys)
{
	ForceKeyState k{
		VK_CONTROL, false,
		VK_SHIFT, false,
		VK_MENU, false,
	};
	return internal::call_next_proc(WM_MOUSEWHEEL,
		delta << 16,
		point_to_wp(screen_x, screen_y));
}

// wheels::scroll_v
bool expt::wheels::scroll_v(int screen_x, int screen_y, int delta, modkeys mkeys)
{
	ForceKeyState k{
		VK_CONTROL, false,
		VK_SHIFT, false,
		VK_MENU, true,
	};
	return internal::call_next_proc(WM_MOUSEWHEEL,
		delta << 16,
		point_to_wp(screen_x, screen_y));
}

// wheels::zoom_h
bool expt::wheels::zoom_h(int screen_x, int screen_y, int delta, modkeys mkeys)
{
	ForceKeyState k{
		VK_CONTROL, true,
		VK_SHIFT, false,
		VK_MENU, false,
	};
	return internal::call_next_proc(WM_MOUSEWHEEL,
		(delta << 16) | keys_to_wp(modkeys::ctrl),
		point_to_wp(screen_x, screen_y));
}

// wheels::zoom_v
bool expt::wheels::zoom_v(int screen_x, int screen_y, int delta, modkeys mkeys)
{
	lr::set_layer_size(lr::get_layer_size_delayed() + (delta > 0 ? +1 : -1), false);
	return false;
}

// wheels::move_frame_1
bool expt::wheels::move_frame_1(int screen_x, int screen_y, int delta, modkeys mkeys)
{
	if (!is_editing()) return false;
	return move_frame(*exedit.curr_edit_frame + (delta > 0 ? -1 : +1));
}

// wheels::move_frame_len
bool expt::wheels::move_frame_len(int screen_x, int screen_y, int delta, modkeys mkeys)
{
	if (!is_editing()) return false;
	return wa::on_menu_command(enhanced_tl::this_fp->hwnd,
		delta > 0 ? wa::menu::step_len_left : wa::menu::step_len_right, *exedit.editp);
}

// wheels::step_midpt_layer
bool expt::wheels::step_midpt_layer(int screen_x, int screen_y, int delta, modkeys mkeys)
{
	if (!is_editing()) return false;
	return step_layer(screen_x, screen_y, delta, false);
}

// wheels::step_obj_layer
bool expt::wheels::step_obj_layer(int screen_x, int screen_y, int delta, modkeys mkeys)
{
	if (!is_editing()) return false;
	return step_layer(screen_x, screen_y, delta, true);
}

// wheels::step_midpt_scene
bool expt::wheels::step_midpt_scene(int screen_x, int screen_y, int delta, modkeys mkeys)
{
	if (!is_editing()) return false;
	return wa::on_menu_command(enhanced_tl::this_fp->hwnd,
		delta > 0 ? wa::menu::step_midpt_left_all : wa::menu::step_midpt_right_all, *exedit.editp);
}

// wheels::step_obj_scene
bool expt::wheels::step_obj_scene(int screen_x, int screen_y, int delta, modkeys mkeys)
{
	if (!is_editing()) return false;
	return wa::on_menu_command(enhanced_tl::this_fp->hwnd,
		delta > 0 ? wa::menu::step_obj_left_all : wa::menu::step_obj_right_all, *exedit.editp);
}

// wheels::step_bpm_grid
bool expt::wheels::step_bpm_grid(int screen_x, int screen_y, int delta, modkeys mkeys)
{
	if (!is_editing()) return false;

	namespace bpm = enhanced_tl::BPM_Grid;

	// get the current frame.
	int const frame = *exedit.curr_edit_frame;

	// find the nearby grid line.
	auto const [beat_numer, beat_denom] = select_num_beats_BPM(mkeys);
	bpm::BPM_Grid const grid{ beat_numer, beat_denom };
	int const dest = delta > 0 ?
		grid.pos_from_beat(grid.beat_from_pos(frame - 1)) : // to left.
		grid.pos_from_beat(grid.beat_from_pos(frame) + 1); // to right.

	return dest != frame && move_frame(dest);
}

// wheels::change_scene
bool expt::wheels::change_scene(int screen_x, int screen_y, int delta, modkeys mkeys)
{
	if (!is_editing()) return false;
	return wa::on_menu_command(enhanced_tl::this_fp->hwnd,
		delta > 0 ? wa::menu::change_scene_prev : wa::menu::change_scene_next, *exedit.editp);
}
