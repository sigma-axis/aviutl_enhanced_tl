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

using byte = uint8_t;
#include <exedit/layer.hpp>

#include "../modkeys.hpp"
#include "mouse_actions.hpp"


////////////////////////////////
// レイヤー周りのマウス操作．
////////////////////////////////
namespace enhanced_tl::mouse_override::layers
{
	namespace drags
	{
		namespace detail
		{
			using LayerFlag = ExEdit::LayerSetting::Flag;
			struct flag_drag : drag_state {
			protected:
				bool can_continue() const override;
				bool on_mouse_down_core(modkeys mkeys) override;
				bool on_mouse_move_core(modkeys mkeys) override;
				bool on_mouse_up_core(modkeys mkeys) override;
				bool on_mouse_cancel_core(bool release) override;
				bool handle_key_messages_core(bool& ret, UINT message, WPARAM wparam, LPARAM lparam) override { return true; }
				virtual LayerFlag flag() const = 0;
				virtual bool redraw() const { return false; }
			};
		}
		/// @brief shows or hides the layers within the dragged range.
		inline struct Show_Hide : detail::flag_drag {
		protected:
			detail::LayerFlag flag() const override { return detail::LayerFlag::UnDisp; }
			bool redraw() const override { return true; }
		} show_hide;
		inline struct Lock_Unlock : detail::flag_drag {
		protected:
			detail::LayerFlag flag() const override { return detail::LayerFlag::Locked; }
		} lock_unlock;
		inline struct Link_Coord : detail::flag_drag {
		protected:
			detail::LayerFlag flag() const override { return detail::LayerFlag::CoordLink; }
		} link_coord;
		inline struct Mask_Above : detail::flag_drag {
		protected:
			detail::LayerFlag flag() const override { return detail::LayerFlag::Clip; }
			bool redraw() const override { return true; }
		} mask_above;
		inline struct Select_All : drag_state {
		protected:
			bool can_continue() const override;
			bool on_mouse_down_core(modkeys mkeys) override;
			bool on_mouse_move_core(modkeys mkeys) override;
			bool on_mouse_up_core(modkeys mkeys) override;
			bool on_mouse_cancel_core(bool release) override;
		} select_all;
		inline struct Drag_Move : drag_state {
		protected:
			bool can_continue() const override;
			bool on_mouse_down_core(modkeys mkeys) override;
			bool on_mouse_move_core(modkeys mkeys) override;
			bool on_mouse_up_core(modkeys mkeys) override;
			bool on_mouse_cancel_core(bool release) override;
		} drag_move;
	}

	namespace clicks
	{
		click_func
			rename,
			toggle_others,
			insert,
			remove;
	}

	namespace wheels
	{
		wheel_func
			zoom_h;
	}
}
