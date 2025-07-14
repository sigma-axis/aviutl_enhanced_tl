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

#include "../modkeys.hpp"
#include "mouse_actions.hpp"


////////////////////////////////
// TLマウス操作の実装．
////////////////////////////////
namespace enhanced_tl::mouse_override::timeline
{
	namespace drags
	{
		/// @brief simulates L drag on an object or blank area, accepts modifier keys during the drag.
		inline struct L : drag_state {
		protected:
			bool on_mouse_down_core(modkeys mkeys) override;
			bool on_mouse_move_core(modkeys mkeys) override;
			bool on_mouse_up_core(modkeys mkeys) override;
			bool on_mouse_cancel_core(bool release) override;
		} l;

		/// @brief simulates L drag on an object without modifier keys.
		inline struct Obj_L : drag_state {
		protected:
			bool can_continue() const override;
			bool on_mouse_down_core(modkeys mkeys) override;
			bool on_mouse_move_core(modkeys mkeys) override;
			bool on_mouse_up_core(modkeys mkeys) override;
			bool on_mouse_cancel_core(bool release) override;
			bool handle_key_messages_core(bool& ret, UINT message, WPARAM wparam, LPARAM lparam) override;
		} obj_l;

		/// @brief simulates Ctrl+L drag on an object (selecting or moving objects).
		inline struct Obj_Ctrl_L : drag_state {
		protected:
			bool can_continue() const override;
			bool on_mouse_down_core(modkeys mkeys) override;
			bool on_mouse_move_core(modkeys mkeys) override;
			bool on_mouse_up_core(modkeys mkeys) override;
			bool on_mouse_cancel_core(bool release) override;
			bool handle_key_messages_core(bool& ret, UINT message, WPARAM wparam, LPARAM lparam) override;
		} obj_ctrl_l;

		/// @brief simulates Shift+L drag on an object (variant form of midpt drag).
		inline struct Obj_Shift_L : drag_state {
		protected:
			bool can_continue() const override;
			bool on_mouse_down_core(modkeys mkeys) override;
			bool on_mouse_move_core(modkeys mkeys) override;
			bool on_mouse_up_core(modkeys mkeys) override;
			bool on_mouse_cancel_core(bool release) override;
			bool handle_key_messages_core(bool& ret, UINT message, WPARAM wparam, LPARAM lparam) override;
		} obj_shift_l;

		/// @brief simulates L drag on background (moves the current frame).
		inline struct Bk_L : drag_state {
		protected:
			bool can_continue() const override;
			bool on_mouse_down_core(modkeys mkeys) override;
			bool on_mouse_move_core(modkeys mkeys) override;
			bool on_mouse_up_core(modkeys mkeys) override;
			bool on_mouse_cancel_core(bool release) override;
			bool handle_key_messages_core(bool& ret, UINT message, WPARAM wparam, LPARAM lparam) override;
		} bk_l;

		/// @brief simulates Ctrl+L drag on background (range selection).
		inline struct Bk_Ctrl_L : drag_state {
		protected:
			bool can_continue() const override;
			bool on_mouse_down_core(modkeys mkeys) override;
			bool on_mouse_move_core(modkeys mkeys) override;
			bool on_mouse_up_core(modkeys mkeys) override;
			bool on_mouse_cancel_core(bool release) override;
			bool handle_key_messages_core(bool& ret, UINT message, WPARAM wparam, LPARAM lparam) override;
		} bk_ctrl_l;

		/// @brief simulates Alt+L drag (drag scroll).
		inline struct Alt_L : drag_state {
		protected:
			bool on_mouse_down_core(modkeys mkeys) override;
			bool on_mouse_move_core(modkeys mkeys) override;
			bool on_mouse_up_core(modkeys mkeys) override;
			bool on_mouse_cancel_core(bool release) override;
		} alt_l;

		/// @brief "bi-directional zoom" drag.
		inline struct Zoom_Bi : drag_state {
		protected:
			bool can_continue() const override;
			bool on_mouse_down_core(modkeys mkeys) override;
			bool on_mouse_move_core(modkeys mkeys) override;
			bool on_mouse_up_core(modkeys mkeys) override;
			bool on_mouse_cancel_core(bool release) override;
			bool handle_key_messages_core(bool& ret, UINT message, WPARAM wparam, LPARAM lparam) override { return true; }
		} zoom_bi;

		/// @brief snaps the current frame to end-points or mid-points of objects.
		inline struct Step_Bound : drag_state {
		protected:
			bool on_mouse_down_core(modkeys mkeys) override;
			bool on_mouse_move_core(modkeys mkeys) override;
			bool on_mouse_up_core(modkeys mkeys) override;
			bool on_mouse_cancel_core(bool release) override;
			bool handle_key_messages_core(bool& ret, UINT message, WPARAM wparam, LPARAM lparam) override { return true; }
		} step_bound;

		/// @brief snaps the current frame to BPM grid.
		inline struct Step_BPM : drag_state {
		protected:
			bool on_mouse_down_core(modkeys mkeys) override;
			bool on_mouse_move_core(modkeys mkeys) override;
			bool on_mouse_up_core(modkeys mkeys) override;
			bool on_mouse_cancel_core(bool release) override;
			bool handle_key_messages_core(bool& ret, UINT message, WPARAM wparam, LPARAM lparam) override { return true; }
		} step_bpm;
	}

	namespace clicks
	{
		click_func
			obj_ctrl_shift_l,
			obj_l_dbl,
			rclick,
			toggle_midpt,
			select_line_left,
			select_line_right,
			select_all,
			squeeze_left,
			squeeze_right,
			toggle_active;
	}

	namespace wheels
	{
		wheel_func
			scroll_h,
			scroll_v,
			zoom_h,
			zoom_v,
			move_frame_1,
			move_frame_len,
			step_midpt_layer,
			step_obj_layer,
			step_midpt_scene,
			step_obj_scene,
			step_bpm_grid,
			change_scene;
	}
}
