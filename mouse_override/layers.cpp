/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <cmath>
#include <set>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using byte = uint8_t;
#include <exedit.hpp>

#include "../modkeys.hpp"
#include "../timeline.hpp"
#include "../walkaround.hpp"
#include "mouse_actions.hpp"

#include "../enhanced_tl.hpp"
#include "../mouse_override.hpp"

#include "layers.hpp"


#define NS_BEGIN(...) namespace __VA_ARGS__ {
#define NS_END }
NS_BEGIN()

////////////////////////////////
// レイヤー周りのマウス操作．
////////////////////////////////
using namespace enhanced_tl::mouse_override::layers;
namespace internal = enhanced_tl::mouse_override::internal;
namespace tl = enhanced_tl::timeline;
namespace wa = enhanced_tl::walkaround;

static constexpr bool key_changed(LPARAM l) {
	return ((l ^ (l >> 1)) >> 30) == 0;
}
namespace layer_commands
{
	enum id : uint32_t {
		show_hide		= 1025,
		lock_unlock		= 1026,
		insert			= 1032,
		remove			= 1033,
		rename			= 1056,
		link_coord		= 1062,
		mask_above		= 1065,
		toggle_others	= 1075,
	};
}

static void set_undo_obj(int idx_obj) { exedit.setundo(idx_obj, 0x08); }
static void set_undo_layer(int idx_layer) { exedit.setundo(idx_layer, 0x10); }

static constinit class ScrollTimer {
	using drag_state = enhanced_tl::mouse_override::drag_state;
	using modkeys = sigma_lib::modifier_keys::modkeys;

	uintptr_t timer_uid() const { return reinterpret_cast<uintptr_t>(this); }
	auto kill_internal()
	{
		::KillTimer(enhanced_tl::this_fp->hwnd, timer_uid());
		return std::exchange(callback, nullptr);
	}
	static void CALLBACK on_timer(HWND hwnd, UINT, UINT_PTR uid, DWORD)
	{
		if (hwnd != enhanced_tl::this_fp->hwnd ||
			uid != scroll_timer.timer_uid()) return;

		// stop the timer from repeating. check if the drag is still active.
		auto const callback = scroll_timer.kill_internal();
		if (callback == nullptr || !callback->active()) return;

		// send a "fake" message to the drag manager.
		POINT pt; ::GetCursorPos(&pt);
		::ScreenToClient(drag_state::target, &pt);
		if (drag_state::on_mouse_move(pt.x, pt.y, curr_modkeys()))
			// update the main screen if necessary.
			update_current_frame();
	}

	drag_state* callback = nullptr;
	int next_scroll_tick = 0;

#pragma warning(suppress : 28159) // 32 bit is enough.
	static int get_tick() { return ::GetTickCount(); }

public:
	/// @brief whether the auto-scrolls should be performed.
	operator bool() const { return get_tick() - next_scroll_tick >= 0; }
	/// @brief whether the auto-scrolls should be suspended.
	bool operator!() const { return !operator bool(); }
	void set(drag_state* caller)
	{
		using enhanced_tl::mouse_override::settings;

		callback = caller;
		next_scroll_tick = get_tick() + settings.layer.auto_scroll_delay_ms;
		::SetTimer(enhanced_tl::this_fp->hwnd, timer_uid(),
			settings.layer.auto_scroll_delay_ms, on_timer);
	}
	void kill()
	{
		if (callback != nullptr) kill_internal();
		next_scroll_tick = get_tick();
	}
} scroll_timer{};

static bool layer_command_action(int y, layer_commands::id id)
{
	auto const l = tl::point_to_layer(y);
	if (auto const v = *exedit.timeline_v_scroll_pos;
		v > l || l >= v + *exedit.timeline_height_in_layers) return false;

	*exedit.last_clicked_point_y = tl::point_from_layer(l);
	return internal::call_next_proc(WM_COMMAND, id, 0) != FALSE;
}

static constinit uint32_t prev_undo_state{};
static constinit int32_t prev_scene{};

// variables for versatile purposes. roles change depending on the drags.
static constinit int layer_start{}, layer_prev{}, layer_top{}, layer_btm{};
static constinit bool flag_value{};

// helper function for scrolling operations.
static int find_layer_and_scroll(int client_y, enhanced_tl::mouse_override::drag_state* caller)
{
	// determine the layer where the mouse cursor is on.
	int layer_curr = tl::point_to_layer(client_y);

	// check if the layer is beyond the limit.
	int scroll_delta = 0;
	auto const top = *exedit.timeline_v_scroll_pos;
	if (layer_curr < top) {
		// above the scroll range. limit to the top.
		layer_curr = top;
		scroll_delta = -1;
	}
	else if (auto const btm = top + *exedit.timeline_height_in_layers - 1;
		layer_curr > btm) {
		// below the scroll range. limit to the bottom.
		layer_curr = btm;
		scroll_delta = +1;
	}

	// scroll the layer if necessary.
	if (scroll_delta != 0 && scroll_timer) {
		wa::set_layer_scroll(top + scroll_delta, *exedit.editp); // force scroll by one.
		scroll_timer.set(caller); // set cool down and schedule.
		layer_curr += scroll_delta;
	}

	return layer_curr;
}

static bool add_selection(int layer, std::set<int32_t>& sel)
{
	// avoid selecting objects in locked layers.
	auto* const layer_settings = exedit.LayerSettings + tl::constants::num_layers * *exedit.current_scene;
	if (has_flag_or(layer_settings[layer].flag, ExEdit::LayerSetting::Flag::Locked))
		return false;

	bool ret = false;
	int const L = exedit.SortedObjectLayerBeginIndex[layer], R = exedit.SortedObjectLayerEndIndex[layer];
	for (int j = L; j <= R; j++)
		ret |= sel.insert(exedit.SortedObject[j] - *exedit.ObjectArray_ptr).second;
	return ret;
}
static bool remove_selection(int layer, std::set<int32_t>& sel)
{
	bool ret = false;
	int const L = exedit.SortedObjectLayerBeginIndex[layer], R = exedit.SortedObjectLayerEndIndex[layer];
	for (int j = L; j <= R; j++)
		ret |= sel.erase(exedit.SortedObject[j] - *exedit.ObjectArray_ptr) != 0;
	return ret;
}
NS_END


////////////////////////////////
// Exported functions.
////////////////////////////////
namespace expt = enhanced_tl::mouse_override::layers;

////////////////////////////////
// ドラッグ．
////////////////////////////////
// base class for the following drags to toggle flags.
// - drags::Show_Hide
//   「レイヤーの表示」の切り替え．
// - drags::Lock_Unlock
//   「レイヤーのロック」の切り替え．
// - drags::Link_Coord
//   「座標のリンク」の切り替え．
// - drags::Mask_Above
//   「上のオブジェクトでクリッピング」の切り替え．
bool expt::drags::detail::flag_drag::can_continue() const {
	return is_editing()
		&& (!active() || (
			prev_undo_state == *exedit.undo_id_ptr &&
			prev_scene == *exedit.current_scene));
}
bool expt::drags::detail::flag_drag::on_mouse_down_core(modkeys mkeys)
{
	layer_start = layer_prev = tl::point_to_layer(pt_start.y);
	auto* const layer_settings = exedit.LayerSettings + tl::constants::num_layers * *exedit.current_scene;
	auto& f = layer_settings[layer_start].flag;
	flag_value = !has_flag_or(f, flag()); // whether to set the flag.

	// prepare undo buffer.
	exedit.nextundo();
	set_undo_layer(layer_start);
	prev_undo_state = *exedit.undo_id_ptr;
	prev_scene = *exedit.current_scene;

	// modify the first flag.
	f ^= flag();

	scroll_timer.kill(); // reset the timer.
	::SetCapture(exedit.fp->hwnd);

	// redraw the entire timeline.
	::InvalidateRect(exedit.fp->hwnd, nullptr, FALSE);
	return redraw();
}
bool expt::drags::detail::flag_drag::on_mouse_move_core(modkeys mkeys)
{
	int const layer_curr = find_layer_and_scroll(pt_curr.y, this);
	auto* const layer_settings = exedit.LayerSettings + tl::constants::num_layers * *exedit.current_scene;

	// identify the dragged range.
	int from, until;
	if (layer_prev == layer_curr) return false;
	else if (layer_prev < layer_curr) { from = layer_prev + 1; until = layer_curr + 1; }
	else { from = layer_curr; until = layer_prev; }
	layer_prev = layer_curr;

	// modify the layer flags.
	bool modified = false;
	for (int l = from; l < until; l++) {
		auto& f = layer_settings[l].flag;
		if (has_flag_or(f, flag()) ^ flag_value) {
			modified = true;
			set_undo_layer(l);
			f ^= flag();
		}
	}

	if (modified) {
		// redraw the entire timeline.
		::InvalidateRect(exedit.fp->hwnd, nullptr, FALSE);
		return redraw();
	}
	return false;
}
bool expt::drags::detail::flag_drag::on_mouse_up_core(modkeys mkeys)
{
	scroll_timer.kill();
	::ReleaseCapture();
	return false;
}
bool expt::drags::detail::flag_drag::on_mouse_cancel_core(bool release)
{
	scroll_timer.kill();
	if (release) ::ReleaseCapture();
	return false;
}

// drags::Select_All
// レイヤー上のオブジェクト選択．
bool expt::drags::Select_All::can_continue() const {
	return is_editing()
		&& (!active() || (
			prev_undo_state == *exedit.undo_id_ptr &&
			prev_scene == *exedit.current_scene));
}
bool expt::drags::Select_All::on_mouse_down_core(modkeys mkeys)
{
	layer_start = layer_prev = tl::point_to_layer(pt_start.y);
	auto sel = get_multi_selected_objects();

	// see whether all objects in the layer is selected.
	flag_value = true; // empty layer should turn to "selecting" mode.
	for (int j = exedit.SortedObjectLayerBeginIndex[layer_start], r = exedit.SortedObjectLayerEndIndex[layer_start];
		j <= r; j++) {
		if (sel.contains(exedit.SortedObject[j] - *exedit.ObjectArray_ptr)) flag_value = false;
		else {
			flag_value = true;
			break;
		}
	}

	// record the undo stage and the current scene.
	prev_undo_state = *exedit.undo_id_ptr;
	prev_scene = *exedit.current_scene;

	// select or deselect the objects on the layer.
	bool const modified = flag_value ?
		add_selection(layer_start, sel) :
		remove_selection(layer_start, sel);

	if (modified) {
		set_multi_selected_objects(sel);

		// redraw the entire timeline.
		::InvalidateRect(exedit.fp->hwnd, nullptr, FALSE);
	}

	scroll_timer.kill(); // reset the timer.
	::SetCapture(exedit.fp->hwnd);
	return false;
}
bool expt::drags::Select_All::on_mouse_move_core(modkeys mkeys)
{
	int const layer_curr = find_layer_and_scroll(pt_curr.y, this);

	// identify the dragged range.
	int from, until;
	if (layer_prev == layer_curr) return false;
	else if (layer_prev < layer_curr) { from = layer_prev + 1; until = layer_curr + 1; }
	else { from = layer_curr; until = layer_prev; }
	layer_prev = layer_curr;

	// select or deselect the objects in the range.
	auto sel = get_multi_selected_objects();
	bool modified = false;
	if (flag_value) {
		for (int l = from; l < until; l++)
			modified |= add_selection(l, sel);
	}
	else {
		for (int l = from; l < until; l++)
			modified |= remove_selection(l, sel);
	}

	if (modified) {
		set_multi_selected_objects(sel);

		// redraw the entire timeline.
		::InvalidateRect(exedit.fp->hwnd, nullptr, FALSE);
	}

	return false;
}
bool expt::drags::Select_All::on_mouse_up_core(modkeys mkeys)
{
	scroll_timer.kill();
	::ReleaseCapture();
	return false;
}
bool expt::drags::Select_All::on_mouse_cancel_core(bool release)
{
	scroll_timer.kill();
	if (release) ::ReleaseCapture();
	return false;
}

// drags::Drag_Move
// レイヤーをドラッグ移動．
bool expt::drags::Drag_Move::can_continue() const {
	return is_editing()
		&& (!active() || (
			prev_undo_state == *exedit.undo_id_ptr &&
			prev_scene == *exedit.current_scene));
}
bool expt::drags::Drag_Move::on_mouse_down_core(modkeys mkeys)
{
	layer_start = layer_prev = tl::point_to_layer(pt_start.y);
	flag_value = false; // whether any layers were moved.

	// record the undo stage and the current scene.
	prev_undo_state = *exedit.undo_id_ptr;
	prev_scene = *exedit.current_scene;

	// nothing to do here at mouse down.

	scroll_timer.kill(); // reset the timer.
	::SetCapture(exedit.fp->hwnd);
	::SetCursor(::LoadCursorW(nullptr, reinterpret_cast<wchar_t const*>(IDC_SIZENS)));
	return false;
}
bool expt::drags::Drag_Move::on_mouse_move_core(modkeys mkeys)
{
	int const layer_curr = find_layer_and_scroll(pt_curr.y, this);
	auto* const layer_settings = exedit.LayerSettings + tl::constants::num_layers * *exedit.current_scene;

	// identify the dragged range.
	int from = layer_prev, until = layer_curr, delta;
	if (layer_prev == layer_curr) return false;
	else if (layer_prev < layer_curr) delta = +1;
	else delta = -1;
	until += delta;
	layer_prev = layer_curr;

	if (!flag_value) {
		// prepare undo buffer.
		flag_value = true;
		exedit.nextundo();
		prev_undo_state = *exedit.undo_id_ptr;

		// initialize the range where "undo buffer was pushed".
		layer_top = tl::constants::num_layers;
		layer_btm = -1;
	}

	// push undo for each object and layer setting in the range.
	for (int l = from; l != until; l += delta) {
		if (layer_top <= l && l <= layer_btm) continue; // skip the marked range.

		// each object on the layer.
		int const L = exedit.SortedObjectLayerBeginIndex[l], R = exedit.SortedObjectLayerEndIndex[l];
		for (int j = L; j <= R; j++)
			set_undo_obj(exedit.SortedObject[j] - *exedit.ObjectArray_ptr);

		// layer setting.
		set_undo_layer(l);
	}

	// update the marked range.
	layer_top = std::min(layer_top, delta > 0 ? from : until - delta);
	layer_btm = std::max(layer_btm, delta < 0 ? from : until - delta);

	// move each object and layer setting by one layer up or down.
	bool modified = false;
	{
		int const L = exedit.SortedObjectLayerBeginIndex[from], R = exedit.SortedObjectLayerEndIndex[from];
		modified |= L <= R;
		for (int j = L; j <= R; j++) {
			auto* obj = exedit.SortedObject[j];
			obj->layer_disp = obj->layer_set = layer_curr;
		}
	}
	auto const settings_src = layer_settings[from];
	for (int l = from + delta; l != until; l += delta) {
		int const L = exedit.SortedObjectLayerBeginIndex[l], R = exedit.SortedObjectLayerEndIndex[l];
		modified |= L <= R;
		for (int j = L; j <= R; j++) {
			auto* obj = exedit.SortedObject[j];
			obj->layer_disp = obj->layer_set = l - delta;
		}
		layer_settings[l - delta] = layer_settings[l];
	}
	layer_settings[layer_curr] = settings_src;

	// re-construct internal object tables.
	if (modified) exedit.update_object_tables();

	// redraw the entire timeline. (cannot skip as layer settings might have changed.)
	::InvalidateRect(exedit.fp->hwnd, nullptr, FALSE);
	return false; // don't update the main window now.
}
bool expt::drags::Drag_Move::on_mouse_up_core(modkeys mkeys)
{
	scroll_timer.kill();
	::ReleaseCapture();
	return true; // update the main window at the end of the drag.
}
bool expt::drags::Drag_Move::on_mouse_cancel_core(bool release)
{
	scroll_timer.kill();
	if (release) ::ReleaseCapture();
	return true;
}

////////////////////////////////
// クリック．
////////////////////////////////
// clicks::rename
// 「レイヤー名を変更」
bool expt::clicks::rename(int x, int y, modkeys mkeys)
{
	return layer_command_action(y, layer_commands::rename);
}

// clicks::toggle_others
// 「他のレイヤーを全表示/非表示」
bool expt::clicks::toggle_others(int x, int y, modkeys mkeys)
{
	// original behavior is:
	// - if all other layers are visible, hide them all.
	// - if any of other layers is hidden, show them all.
	// - for any cases above, show the selected layer.
	// especcially, the visibility of the current layer does not affect.
	return layer_command_action(y, layer_commands::toggle_others);
}

// clicks::insert
// 「レイヤーの挿入」
bool expt::clicks::insert(int x, int y, modkeys mkeys)
{
	return layer_command_action(y, layer_commands::insert);
}

// clicks::remove
// 「レイヤーの削除」
bool expt::clicks::remove(int x, int y, modkeys mkeys)
{
	return layer_command_action(y, layer_commands::remove);
}

////////////////////////////////
// ホイール．
////////////////////////////////
// wheels::zoom_h
bool expt::wheels::zoom_h(int screen_x, int screen_y, int delta, modkeys mkeys)
{
	// discarding screen_x and screen_y, it is outside the area.

	// apply the zoom.
	exedit.set_timeline_zoom(
		*exedit.curr_timeline_zoom_level + (delta > 0 ? +1 : -1),
		internal::zoom_center_frame(settings.layer.zoom_center_wheel));
	return false;
}
