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


////////////////////////////////
// ドラッグ状態管理の基礎クラス．
////////////////////////////////
namespace enhanced_tl::mouse_override
{
	using modkeys = sigma_lib::modifier_keys::modkeys;
	enum class drag_status {
		idle = 0,
		entering,	// after the mouse button is down, before it moves a certain amount.
		moving,		// drag is being performed.

		// note:
		// - mouse is captured unless the status is `idle`.
		// - when canceled, mouse capture is released instantly.
		// - click actions occur only when `entering` and buttons are released.
	};

	enum class last_drag_status {
		clicked = 0,
		dragged,
		canceled,
	};

	enum class mouse_button {
		none = 0,
		L = 1 << 0,
		R = 1 << 1,
		M = 1 << 2,
		X1 = 1 << 3,
		X2 = 1 << 4,
		L_and_R = L | R,
	};
	namespace operators
	{
		constexpr mouse_button operator~(mouse_button x) { return static_cast<mouse_button>(~std::to_underlying(x)); }
		constexpr mouse_button& operator&=(mouse_button& x, mouse_button y) { return x = static_cast<mouse_button>(std::to_underlying(x) & std::to_underlying(y)); }
		constexpr mouse_button& operator|=(mouse_button& x, mouse_button y) { return x = static_cast<mouse_button>(std::to_underlying(x) | std::to_underlying(y)); }
		constexpr mouse_button& operator^=(mouse_button& x, mouse_button y) { return x = static_cast<mouse_button>(std::to_underlying(x) ^ std::to_underlying(y)); }
		constexpr mouse_button operator&(mouse_button x, mouse_button y) { mouse_button X = x; return X &= y; }
		constexpr mouse_button operator|(mouse_button x, mouse_button y) { mouse_button X = x; return X |= y; }
		constexpr mouse_button operator^(mouse_button x, mouse_button y) { mouse_button X = x; return X ^= y; }
		constexpr bool operator<=(mouse_button x, mouse_button y) { return (x & ~y) == mouse_button::none; }
		constexpr bool operator<(mouse_button x, mouse_button y) { return (x != y) && (x <= y); }
		constexpr bool operator>=(mouse_button x, mouse_button y) { return y <= x; }
		constexpr bool operator>(mouse_button x, mouse_button y) { return (x != y) && (x >= y); }
	}
	constexpr mouse_button wp_to_buttons(WPARAM wparam)
	{
		using namespace operators;
		using enum mouse_button;
		return
			((wparam & MK_LBUTTON ) != 0 ? L  : none) |
			((wparam & MK_RBUTTON ) != 0 ? R  : none) |
			((wparam & MK_MBUTTON ) != 0 ? M  : none) |
			((wparam & MK_XBUTTON1) != 0 ? X1 : none) |
			((wparam & MK_XBUTTON2) != 0 ? X2 : none);
	}
	constexpr WPARAM buttons_to_wp(mouse_button buttons)
	{
		using namespace operators;
		using enum mouse_button;
		return
			((buttons & L ) != none ? MK_LBUTTON  : 0) |
			((buttons & R ) != none ? MK_RBUTTON  : 0) |
			((buttons & M ) != none ? MK_MBUTTON  : 0) |
			((buttons & X1) != none ? MK_XBUTTON1 : 0) |
			((buttons & X2) != none ? MK_XBUTTON2 : 0);
	}

	struct drag_state {
		static inline HWND target = nullptr;
		static inline drag_status status = drag_status::idle;
		static inline last_drag_status last_status = last_drag_status::clicked;
		static inline mouse_button button{};

		static bool is_active();
		bool active() const;
		static bool is_changing();

	private:
		static inline int
			click_tolerance_x_def{ std::abs(::GetSystemMetrics(SM_CXDRAG)) },
			click_tolerance_y_def{ std::abs(::GetSystemMetrics(SM_CYDRAG)) };

	protected:
		static inline POINT pt_start{}, pt_prev{}, pt_curr{};
		int click_tolerance_x{ click_tolerance_x_def },
			click_tolerance_y{ click_tolerance_y_def };
		virtual bool on_mouse_down_core(modkeys mkeys) = 0; // the mouse should be captured.
		virtual bool on_mouse_move_core(modkeys mkeys) = 0;
		virtual bool on_mouse_up_core(modkeys mkeys) = 0; // the mouse capture should be released.
		virtual bool on_mouse_cancel_core(bool release) = 0;
		// `ret` is initially `false`; do not set it `false` unless intended.
		// `ret` is evaluated only when this function returned `true`.
		virtual bool handle_key_messages_core(bool& ret, UINT message, WPARAM wparam, LPARAM lparam) { return false; }
		virtual bool can_continue() const { return true; }
		static bool fallback_on_down(drag_state& other, modkeys mkeys);
		static bool fallback_on_down();
		static bool fallback_on_move();

	public:
		bool on_mouse_down(HWND hwnd, int x, int y, mouse_button btn, modkeys mkeys);
		static bool on_mouse_move(int x, int y, modkeys mkeys);
		static bool cancel(bool release);
		static bool on_mouse_up_all(int x, int y, modkeys mkeys);
		static void unmark_canceled();
		static bool handle_key_messages(bool& ret, UINT message, WPARAM wparam, LPARAM lparam);
		static void invalidate_click();
	};

	using click_func = bool(int x, int y, sigma_lib::modifier_keys::modkeys mkeys);
	using wheel_func = bool(int screen_x, int screen_y, int delta, sigma_lib::modifier_keys::modkeys mkeys);

	namespace drags
	{
		inline struct no_action : drag_state {
			no_action();

		protected:
			bool on_mouse_down_core(modkeys mkeys) override;
			bool on_mouse_move_core(modkeys mkeys) override;
			bool on_mouse_up_core(modkeys mkeys) override;
			bool on_mouse_cancel_core(bool release) override;
		} no_action{};
	}
	namespace clicks
	{
		click_func no_action;
	}
	namespace wheels
	{
		wheel_func no_action;
	}
}

using enhanced_tl::mouse_override::operators::operator~;
using enhanced_tl::mouse_override::operators::operator&=;
using enhanced_tl::mouse_override::operators::operator|=;
using enhanced_tl::mouse_override::operators::operator^=;
using enhanced_tl::mouse_override::operators::operator&;
using enhanced_tl::mouse_override::operators::operator|;
using enhanced_tl::mouse_override::operators::operator^;
using enhanced_tl::mouse_override::operators::operator<=;
using enhanced_tl::mouse_override::operators::operator<;
using enhanced_tl::mouse_override::operators::operator>=;
using enhanced_tl::mouse_override::operators::operator>;
