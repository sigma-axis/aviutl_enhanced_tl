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


////////////////////////////////
// 編集点移動のショートカット．
////////////////////////////////
struct AviUtl::EditHandle;
namespace enhanced_tl::walkaround
{
	inline constinit struct Settings {
		constexpr static int config_rate_denom = 1000; // スクロール量などの比率指定の分母．

		bool skip_inactive_objects = false;
		bool skip_hidden_layers = false;
		bool suppress_shift = true;
		uint8_t BPM_nth = 3;
		int32_t scroll_amount_layer = 1;
		int32_t scroll_amount_time = config_rate_denom;
		int32_t step_amount_time = config_rate_denom;

		void load(char const* ini_file);
		constexpr bool is_enabled() const { return true; }
	} settings;

	namespace menu
	{
		enum : int32_t {
			step_obj_left,
			step_obj_left_all,
			step_obj_right,
			step_obj_right_all,

			step_midpt_left,
			step_midpt_left_all,
			step_midpt_right,
			step_midpt_right_all,

			step_len_left,
			step_len_right,
			step_page_left,
			step_page_right,

			step_bpm_measure_left,
			step_bpm_measure_right,
			step_bpm_beat_left,
			step_bpm_beat_right,
			step_bpm_quarter_left,
			step_bpm_quarter_right,
			step_bpm_nth_left,
			step_bpm_nth_right,

			step_into_sel_obj,
			step_into_sel_midpt,
			step_into_view,

			scroll_left,
			scroll_right,
			scroll_page_left,
			scroll_page_right,
			scroll_to_current,
			scroll_left_most,
			scroll_right_most,
			scroll_up,
			scroll_down,

			select_upper_obj,
			select_lower_obj,

			bpm_move_origin_left,
			bpm_move_origin_right,
			bpm_move_origin_current,
			bpm_fit_bar_to_current,

			change_scene_prev,
			change_scene_next,
		};
		struct item {
			int32_t id; char const* title;
		};
	}
	constexpr menu::item menu_items[] = {
		{ menu::step_midpt_left,		    "左の中間点(レイヤー)" },
		{ menu::step_midpt_right,		    "右の中間点(レイヤー)" },
		{ menu::step_obj_left,			    "左のオブジェクト境界(レイヤー)" },
		{ menu::step_obj_right,			    "右のオブジェクト境界(レイヤー)" },
		{ menu::step_midpt_left_all,	    "左の中間点(シーン)" },
		{ menu::step_midpt_right_all,	    "右の中間点(シーン)" },
		{ menu::step_obj_left_all,		    "左のオブジェクト境界(シーン)" },
		{ menu::step_obj_right_all,		    "右のオブジェクト境界(シーン)" },
		{ menu::step_into_sel_midpt,	    "現在位置を選択中間点区間に移動" },
		{ menu::step_into_sel_obj,		    "現在位置を選択オブジェクトに移動" },
		{ menu::step_into_view,			    "現在位置をTL表示範囲内に移動" },
		{ menu::step_len_left,			    "左へ一定量移動" },
		{ menu::step_len_right,			    "右へ一定量移動" },
		{ menu::step_page_left,			    "左へ1ページ分移動" },
		{ menu::step_page_right,		    "右へ1ページ分移動" },
		{ menu::step_bpm_measure_left,	    "左の小節線に移動(BPM)" },
		{ menu::step_bpm_measure_right,	    "右の小節線に移動(BPM)" },
		{ menu::step_bpm_beat_left,		    "左の拍数位置に移動(BPM)" },
		{ menu::step_bpm_beat_right,	    "右の拍数位置に移動(BPM)" },
		{ menu::step_bpm_quarter_left,	    "左の1/4拍位置に移動(BPM)" },
		{ menu::step_bpm_quarter_right,	    "右の1/4拍位置に移動(BPM)" },
		{ menu::step_bpm_nth_left,		    "左の1/N拍位置に移動(BPM)" },
		{ menu::step_bpm_nth_right,		    "右の1/N拍位置に移動(BPM)" },
		{ menu::bpm_move_origin_left,	    "グリッドを左に移動(BPM)" },
		{ menu::bpm_move_origin_right,	    "グリッドを右に移動(BPM)" },
		{ menu::bpm_move_origin_current,    "基準フレームを現在位置に(BPM)" },
		{ menu::bpm_fit_bar_to_current,		"最寄りの小節線を現在位置に(BPM)" },
		{ menu::scroll_left,			    "TLスクロール(左)" },
		{ menu::scroll_right,			    "TLスクロール(右)" },
		{ menu::scroll_page_left,		    "TLスクロール(左ページ)" },
		{ menu::scroll_page_right,		    "TLスクロール(右ページ)" },
		{ menu::scroll_to_current,		    "TLスクロール(現在位置まで)" },
		{ menu::scroll_left_most,		    "TLスクロール(開始位置まで)" },
		{ menu::scroll_right_most,		    "TLスクロール(終了位置まで)" },
		{ menu::scroll_up,				    "TLスクロール(上)" },
		{ menu::scroll_down,			    "TLスクロール(下)" },
		{ menu::change_scene_prev,			"前のシーンに切り替え" },
		{ menu::change_scene_next,			"次のシーンに切り替え" },
		{ menu::select_upper_obj,		    "現在フレームのオブジェクトへ移動(上)" },
		{ menu::select_lower_obj,		    "現在フレームのオブジェクトへ移動(下)" },
	};

	/// @brief handles menu commands.
	/// @param hwnd the handle to the window of this plugin.
	/// @param menu_id the id of the menu command defined in `menu_items`.
	/// @param editp the edit handle.
	/// @return `true` to redraw the window, `false` otherwise.
	bool on_menu_command(HWND hwnd, int32_t menu_id, AviUtl::EditHandle* editp);

	/// @brief moves the current frame and adjusts the scroll position.
	/// @param pos the position of the frame to move to.
	/// @param editp the edit handle.
	/// @return `true` to redraw the window, `false` otherwise.
	bool move_frame(int pos, AviUtl::EditHandle* editp);

	/// @brief sets the scroll position of the timeline.
	/// @param pos the position to set the scroll to.
	/// @param editp not used.
	void set_frame_scroll(int pos, AviUtl::EditHandle* editp);

	/// @brief sets the scroll position of the layer.
	/// @param pos the position to set the scroll to.
	/// @param editp not used.
	void set_layer_scroll(int pos, AviUtl::EditHandle* editp);

	/// @brief sets the currently editing scene.
	/// @param idx the index of the scene to set as current.
	/// @param editp the edit handle.
	/// @return `true` if the scene was changed, `false` otherwise.
	bool set_scene_index(int idx, AviUtl::EditHandle* editp);
}
