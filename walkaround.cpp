/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using byte = uint8_t;
#include <exedit.hpp>

#include "inifile_op.hpp"
#include "key_states.hpp"
#include "timeline.hpp"
#include "bpm_grid.hpp"

#include "enhanced_tl.hpp"
#include "walkaround.hpp"


#define NS_BEGIN(...) namespace __VA_ARGS__ {
#define NS_END }
NS_BEGIN()

////////////////////////////////
// 編集点移動のショートカット．
////////////////////////////////
namespace timeline = enhanced_tl::timeline;
namespace tlc = timeline::constants;
namespace BPM_Grid = enhanced_tl::BPM_Grid;
using namespace enhanced_tl::walkaround;

template<HWND*& phwnd, UINT mes_scroll>
inline static void set_scroll_pos(int pos)
{
	auto const scr_bar = *phwnd;
	if (scr_bar == nullptr) return;

	SCROLLINFO const si{ .cbSize = sizeof(si), .fMask = SIF_POS, .nPos = pos };
	::SetScrollInfo(scr_bar, SB_CTL, &si, true);
	exedit.fp->func_WndProc(exedit.fp->hwnd, mes_scroll,
		(pos << 16) | SB_THUMBTRACK, reinterpret_cast<LPARAM>(scr_bar),
		*exedit.editp, exedit.fp);
}
auto& set_h_scroll_pos = set_scroll_pos<exedit.timeline_h_scroll_bar, WM_HSCROLL>;
auto& set_v_scroll_pos = set_scroll_pos<exedit.timeline_v_scroll_bar, WM_VSCROLL>;

static bool scroll_absolute(int dir, AviUtl::EditHandle* editp);

inline static bool step_boundary(bool to_left, bool entire_scene, bool skip_midpt, AviUtl::EditHandle* editp)
{
	/* handles:
	{ menu::step_midpt_left,		    "左の中間点(レイヤー)" },
	{ menu::step_midpt_right,		    "右の中間点(レイヤー)" },
	{ menu::step_obj_left,			    "左のオブジェクト境界(レイヤー)" },
	{ menu::step_obj_right,			    "右のオブジェクト境界(レイヤー)" },
	{ menu::step_midpt_left_all,	    "左の中間点(シーン)" },
	{ menu::step_midpt_right_all,	    "右の中間点(シーン)" },
	{ menu::step_obj_left_all,		    "左のオブジェクト境界(シーン)" },
	{ menu::step_obj_right_all,		    "右のオブジェクト境界(シーン)" },
	*/

	// get the current status.
	int const
		pos = *exedit.curr_edit_frame,
		len = *exedit.curr_scene_len;

	// determine the new position.
	int new_pos;
	if (entire_scene || *exedit.SettingDialogObjectIndex < 0) {
		// search from the entire scene.
		auto const* const layer_settings = &exedit.LayerSettings[*exedit.current_scene * timeline::constants::num_layers];
		if (to_left) {
			new_pos = 0;
			for (int layer = 0; layer < tlc::num_layers; layer++) {
				if (!settings.skip_hidden_layers || timeline::is_visible(layer_settings[layer]))
					new_pos = std::max(new_pos, timeline::find_adjacent_left(pos, layer, skip_midpt, settings.skip_inactive_objects));
			}
		}
		else {
			new_pos = len - 1;
			for (int layer = 0; layer < tlc::num_layers; layer++) {
				if (!settings.skip_hidden_layers || timeline::is_visible(layer_settings[layer]))
					new_pos = std::min(new_pos, timeline::find_adjacent_right(pos, layer, skip_midpt, settings.skip_inactive_objects, len));
			}
		}
	}
	else {
		// search from the layer where the currently selected object lies on.
		int const layer = (*exedit.ObjectArray_ptr)[*exedit.SettingDialogObjectIndex].layer_set;
		new_pos = to_left ?
			timeline::find_adjacent_left(pos, layer, skip_midpt, settings.skip_inactive_objects) :
			timeline::find_adjacent_right(pos, layer, skip_midpt, settings.skip_inactive_objects, len);
	}
	if (pos == new_pos) return false; // no need to move.
	return move_frame(new_pos, editp);
}

inline static bool step_into_obj(bool skip_midpt, AviUtl::EditHandle* editp)
{
	/* handles:
	{ menu::step_into_sel_midpt,	    "現在位置を選択中間点区間に移動" },
	{ menu::step_into_sel_obj,		    "現在位置を選択オブジェクトに移動" },
	*/

	// get the current status.
	int const idx = *exedit.SettingDialogObjectIndex;
	if (idx < 0) return false;
	int const
		pos = *exedit.curr_edit_frame,
		len = *exedit.curr_scene_len;

	auto const* const obj = &(*exedit.ObjectArray_ptr)[idx];

	// move the position into the interval.
	int new_pos = pos;
	if (int pt = skip_midpt ? timeline::chain_begin(obj) : obj->frame_begin;
		pos < pt) new_pos = pt;
	else if (pt = skip_midpt ? timeline::chain_end(obj) : obj->frame_end;
		pt < pos) new_pos = pt;

	if (pos == new_pos) return false; // no need to move.
	return move_frame(new_pos, editp);
}

inline static bool step_into_view(AviUtl::EditHandle* editp)
{
	/* handles:
	{ menu::step_into_view,			    "現在位置をTL表示範囲内に移動" },
	*/

	// get the current status.
	int const
		pos = *exedit.curr_edit_frame,
		len = *exedit.curr_scene_len;

	int const
		scr_pos = *exedit.timeline_h_scroll_pos,
		page = *exedit.timeline_width_in_frames,
		margin = timeline::horiz_scroll_margin();

	// determine the destination.
	int new_pos = pos;
	if (scr_pos == 0 && pos <= margin); // do nothing.
	else if (2 * margin >= page)
		new_pos = scr_pos + page / 2;
	else new_pos = std::clamp(pos, scr_pos + margin, scr_pos + page - margin);

	new_pos = std::clamp(new_pos, 0, len - 1);

	if (pos == new_pos) return false; // no need to move.
	return move_frame(new_pos, editp);
}

inline static bool step_length(bool to_left, bool by_page, AviUtl::EditHandle* editp)
{
	/* handles:
	{ menu::step_len_left,			    "左へ一定量移動" },
	{ menu::step_len_right,			    "右へ一定量移動" },
	{ menu::step_page_left,			    "左へ1ページ分移動" },
	{ menu::step_page_right,		    "右へ1ページ分移動" },
	*/

	// get the current status.
	int const
		pos = *exedit.curr_edit_frame,
		len = *exedit.curr_scene_len;

	// determine the distance.
	int dist = by_page ?
		*exedit.timeline_width_in_frames - timeline::horiz_scroll_margin() :
		static_cast<int>(timeline::horiz_scroll_size() * static_cast<int64_t>(settings.step_amount_time) / settings.config_rate_denom);
	if (to_left) dist *= -1;

	// determine the new position.
	int new_pos = std::clamp(pos + dist, 0, len - 1);

	if (pos == new_pos) return false; // no need to move.
	return move_frame(new_pos, editp);
}

inline static bool step_bpm_grid(bool to_left, int beats_numer, int beats_denom, AviUtl::EditHandle* editp)
{
	/* handles:
	{ menu::step_bpm_measure_left,	    "左の小節線に移動(BPM)" },
	{ menu::step_bpm_measure_right,	    "右の小節線に移動(BPM)" },
	{ menu::step_bpm_beat_left,		    "左の拍数位置に移動(BPM)" },
	{ menu::step_bpm_beat_right,	    "右の拍数位置に移動(BPM)" },
	{ menu::step_bpm_quarter_left,	    "左の1/4拍位置に移動(BPM)" },
	{ menu::step_bpm_quarter_right,	    "右の1/4拍位置に移動(BPM)" },
	{ menu::step_bpm_nth_left,		    "左の1/N拍位置に移動(BPM)" },
	{ menu::step_bpm_nth_right,		    "右の1/N拍位置に移動(BPM)" },
	*/

	// get the current status.
	int const
		pos = *exedit.curr_edit_frame,
		len = *exedit.curr_scene_len;

	// determine the new position.
	BPM_Grid::BPM_Grid const grid{ beats_numer, beats_denom };
	int new_pos = to_left ?
		grid.pos_from_beat(grid.beat_from_pos(pos - 1)) :
		grid.pos_from_beat(grid.beat_from_pos(pos) + 1);

	new_pos = std::clamp(new_pos, 0, len - 1);

	if (pos == new_pos) return false; // no need to move.
	return move_frame(new_pos, editp);
}

inline static bool move_bpm_grid(int dir, AviUtl::EditHandle* editp)
{
	// dir:
	//    0: to current,
	//   +1: to right,
	//   -1: to left.
	//   +2: fit bar.
	/* handles:
	{ menu::bpm_move_origin_left,	    "グリッドを左に移動(BPM)" },
	{ menu::bpm_move_origin_right,	    "グリッドを右に移動(BPM)" },
	{ menu::bpm_move_origin_current,    "基準フレームを現在位置に(BPM)" },
	{ menu::bpm_fit_bar_to_current,		"最寄りの小節線を現在位置に(BPM)" },
	*/

	// get the current status.
	int const origin = *exedit.timeline_BPM_frame_origin;

	// determine the new origin.
	int new_origin = origin;
	if (dir == 2) {
		// find the nearest two measure bars from the current frame.
		BPM_Grid::BPM_Grid const grid{ *exedit.timeline_BPM_num_beats, 1 };
		int const
			pos = *exedit.curr_edit_frame;
		auto const
			beat = grid.beat_from_pos(pos);
		int const
			l = grid.pos_from_beat(beat), r = grid.pos_from_beat(beat + 1);

		// move the origin to the nearer side.
		new_origin += l + r < 2 * pos ?
			pos - r : // right.
			pos - l; // left.
	}
	else if (dir == 0) new_origin = *exedit.curr_edit_frame;
	else new_origin += dir;

	if (origin == new_origin) return false; // no need to move.

	// apply the new value.
	*exedit.timeline_BPM_frame_origin = new_origin;

	// redraw the grid if it's visible.
	if (*exedit.timeline_BPM_show_grid != 0)
		::InvalidateRect(exedit.fp->hwnd, nullptr, FALSE);

	return false;
}

inline static bool scroll_length(bool to_left, bool by_page, AviUtl::EditHandle* editp)
{
	/* handles:
	{ menu::scroll_left,			    "TLスクロール(左)" },
	{ menu::scroll_right,			    "TLスクロール(右)" },
	{ menu::scroll_page_left,		    "TLスクロール(左ページ)" },
	{ menu::scroll_page_right,		    "TLスクロール(右ページ)" },
	*/

	// get the current status.
	int const pos = *exedit.timeline_h_scroll_pos;

	// determine the distance.
	int dist = by_page ?
		*exedit.timeline_width_in_frames :
		static_cast<int>(timeline::horiz_scroll_size() * static_cast<int64_t>(settings.scroll_amount_time) / settings.config_rate_denom);
	if (to_left) dist *= -1;

	// determine the new position.
	int const new_pos = pos + dist;

	if (pos == new_pos) return false; // no need to move.
	set_h_scroll_pos(new_pos);
	return false;
}

inline static bool scroll_absolute(int dir, AviUtl::EditHandle* editp)
{
	// dir:
	//    0: to current,
	//   +1: to right-most,
	//   -1: to left-most.
	/* handles:
	{ menu::scroll_to_current,		    "TLスクロール(現在位置まで)" },
	{ menu::scroll_left_most,		    "TLスクロール(開始位置まで)" },
	{ menu::scroll_right_most,		    "TLスクロール(終了位置まで)" },
	*/

	// get the current status.
	int const pos = *exedit.timeline_h_scroll_pos;

	// determine the new position.
	int new_pos;
	if (dir == 0) {
		// move near to the current frame.
		int const
			page = *exedit.timeline_width_in_frames,
			margin = timeline::horiz_scroll_margin(),
			curr = *exedit.curr_edit_frame;

		new_pos = page < 2 * margin ? curr - (page / 2) :
			std::clamp(pos, curr - page + margin, curr - margin);
		new_pos = std::min(new_pos, *exedit.curr_scene_len - page + margin);
		new_pos = std::max(new_pos, 0);
	}
	else if (dir == -1) new_pos = 0;
	else new_pos = *exedit.curr_scene_len;

	if (pos == new_pos) return false; // no need to move.
	set_h_scroll_pos(new_pos);
	return false;
}

inline static bool scroll_layer(bool to_up, AviUtl::EditHandle* editp)
{
	/* handles:
	{ menu::scroll_up,				    "TLスクロール(上)" },
	{ menu::scroll_down,			    "TLスクロール(下)" },
	*/

	// get the current status.
	int const pos = *exedit.timeline_v_scroll_pos;

	// determine the new position.
	int const new_pos = std::clamp(
		pos + (to_up ? -1 : +1) * settings.scroll_amount_layer,
		0, tlc::num_layers - 1);

	if (pos == new_pos) return false; // no need to move.
	set_v_scroll_pos(new_pos);
	return false;
}

// 拡張編集の「現在のオブジェクトを選択」コマンドの ID.
constexpr static int exedit_command_select_obj = 1050;
inline static bool select_object(bool to_up, AviUtl::EditHandle* editp)
{
	/* handles:
	{ menu::select_upper_obj,		    "現在フレームのオブジェクトへ移動(上)" },
	{ menu::select_lower_obj,		    "現在フレームのオブジェクトへ移動(下)" },
	*/

	// fake shift key state.
	sigma_lib::W32::UI::ForceKeyState k{ VK_SHIFT, to_up };

	// 拡張編集にコマンドメッセージ送信．
	return exedit.fp->func_WndProc(exedit.fp->hwnd, AviUtl::FilterPlugin::WindowMessage::Command,
		exedit_command_select_obj, 0, editp, exedit.fp) != FALSE;
}

inline static bool set_scene_rel(int32_t delta, AviUtl::EditHandle* editp)
{
	/* handles:
	{ menu::change_scene_prev,			"前のシーンに切り替え" },
	{ menu::change_scene_next,			"次のシーンに切り替え" },
	*/

	return set_scene_index(*exedit.current_scene + delta, editp);
}
NS_END


////////////////////////////////
// exported functions.
////////////////////////////////
namespace expt = enhanced_tl::walkaround;

void expt::Settings::load(char const* ini_file)
{
	using namespace sigma_lib::inifile;
	constexpr auto section = "walkaround";
	constexpr static int
		BPM_nth_min = 2, BPM_nth_max = 128,
		rate_min = 1, rate_max = 16'000;

#define read(func, fld, ...)	fld = read_##func(fld, ini_file, section, #fld __VA_OPT__(,) __VA_ARGS__)

	read(bool,	skip_inactive_objects);
	read(bool,	skip_hidden_layers);
	read(bool,	suppress_shift);
	read(int,	BPM_nth, BPM_nth_min, BPM_nth_max);
	read(int,	scroll_amount_layer, 1, tlc::num_layers - 1);
	read(int,	scroll_amount_time, rate_min, rate_max);
	read(int,	step_amount_time, rate_min, rate_max);

#undef read
}

bool expt::on_menu_command(HWND hwnd, int32_t menu_id, AviUtl::EditHandle* editp)
{
	if (editp == nullptr ||
		!enhanced_tl::this_fp->exfunc->is_editing(editp) ||
		enhanced_tl::this_fp->exfunc->is_saving(editp)) return false;

	// switch by menu_id.
	switch (menu_id) {
	case menu::step_obj_left:			return step_boundary(true, false, true, editp);
	case menu::step_obj_left_all:		return step_boundary(true, true, true, editp);
	case menu::step_obj_right:			return step_boundary(false, false, true, editp);
	case menu::step_obj_right_all:		return step_boundary(false, true, true, editp);

	case menu::step_midpt_left:			return step_boundary(true, false, false, editp);
	case menu::step_midpt_left_all:		return step_boundary(true, true, false, editp);
	case menu::step_midpt_right:		return step_boundary(false, false, false, editp);
	case menu::step_midpt_right_all:	return step_boundary(false, true, false, editp);

	case menu::step_len_left:			return step_length(true, false, editp);
	case menu::step_len_right:			return step_length(false, false, editp);
	case menu::step_page_left:			return step_length(true, true, editp);
	case menu::step_page_right:			return step_length(false, true, editp);

	case menu::step_bpm_measure_left:	return step_bpm_grid(true, *exedit.timeline_BPM_num_beats, 1, editp);
	case menu::step_bpm_measure_right:	return step_bpm_grid(false, *exedit.timeline_BPM_num_beats, 1, editp);
	case menu::step_bpm_beat_left:		return step_bpm_grid(true, 1, 1, editp);
	case menu::step_bpm_beat_right:		return step_bpm_grid(false, 1, 1, editp);
	case menu::step_bpm_quarter_left:	return step_bpm_grid(true, 1, 4, editp);
	case menu::step_bpm_quarter_right:	return step_bpm_grid(false, 1, 4, editp);
	case menu::step_bpm_nth_left:		return step_bpm_grid(true, 1, settings.BPM_nth, editp);
	case menu::step_bpm_nth_right:		return step_bpm_grid(false, 1, settings.BPM_nth, editp);

	case menu::step_into_sel_obj:		return step_into_obj(true, editp);
	case menu::step_into_sel_midpt:		return step_into_obj(false, editp);
	case menu::step_into_view:			return step_into_view(editp);

	case menu::scroll_left:				return scroll_length(true, false, editp);
	case menu::scroll_right:			return scroll_length(false, false, editp);
	case menu::scroll_page_left:		return scroll_length(true, true, editp);
	case menu::scroll_page_right:		return scroll_length(false, true, editp);

	case menu::scroll_to_current:		return scroll_absolute(0, editp);
	case menu::scroll_left_most:		return scroll_absolute(-1, editp);
	case menu::scroll_right_most:		return scroll_absolute(+1, editp);

	case menu::scroll_up:				return scroll_layer(true, editp);
	case menu::scroll_down:				return scroll_layer(false, editp);

	case menu::select_upper_obj:		return select_object(true, editp);
	case menu::select_lower_obj:		return select_object(false, editp);

	case menu::bpm_move_origin_left:	return move_bpm_grid(-1, editp);
	case menu::bpm_move_origin_right:	return move_bpm_grid(+1, editp);
	case menu::bpm_move_origin_current:	return move_bpm_grid(0, editp);
	case menu::bpm_fit_bar_to_current:	return move_bpm_grid(+2, editp);

	case menu::change_scene_prev:		return set_scene_rel(-1, editp);
	case menu::change_scene_next:		return set_scene_rel(+1, editp);


	[[unlikely]] default: return false;
	}
}

bool expt::move_frame(int pos, AviUtl::EditHandle* editp)
{
	// disable shift key to suppress unintended selection.
	namespace ui = sigma_lib::W32::UI;
	ui::ForceKeyState shift{ VK_SHIFT, settings.suppress_shift ? ui::key_map::off : ui::key_map::id };

	// move the frame position.
	if (int const old_pos = *exedit.curr_edit_frame;
		old_pos == enhanced_tl::this_fp->exfunc->set_frame(editp, pos)) return false; // no move happend.

	// 拡張編集のバグで，「カーソル移動時に自動でスクロール」を設定していても，
	// 「選択オブジェクトの追従」が有効でかつ移動先で新たなオブジェクトが選択された場合，
	// スクロールが追従しないため手動でスクロール．
	if (*exedit.scroll_follows_cursor != 0)
		scroll_absolute(0, editp);

	// true を返して出力画面を更新させる．
	return true;
}

void expt::set_frame_scroll(int pos, AviUtl::EditHandle* editp)
{
	set_h_scroll_pos(pos);
}

void expt::set_layer_scroll(int pos, AviUtl::EditHandle* editp)
{
	set_v_scroll_pos(pos);
}

bool expt::set_scene_index(int idx, AviUtl::EditHandle* editp)
{
	if (!is_editing(editp)) return false;

	idx = std::clamp(idx, 0, tlc::num_scenes - 1);
	if (idx == *exedit.current_scene) return false; // no need to change.

	// change the scene.
	namespace ui = sigma_lib::W32::UI;
	ui::ForceKeyState k{ VK_SHIFT, false }; // suppress shift key.
	exedit.change_disp_scene(idx, exedit.fp, editp);
	return true;
}

