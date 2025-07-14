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

#include "color_abgr.hpp"


////////////////////////////////
// レイヤー幅の動的変更．
////////////////////////////////
struct AviUtl::EditHandle;
namespace enhanced_tl::layer_resize
{
	inline constinit struct Settings {
		enum class Orientation : uint8_t {
			L2R = 0, R2L = 1, T2B = 2, B2T = 3,
		};
		enum class Scale : uint8_t {
			linear = 0, logarithmic = 1, harmonic = 2,
		};

		struct {
			sigma_lib::W32::GDI::Color
				fill_lt{ 0x80, 0xc0, 0xff },
				fill_rb{ 0x00, 0x80, 0xff },
				back_lt{ 0x00, 0x00, 0x40 },
				back_rb{ 0x00, 0x00, 0xc0 };
		} color;

		struct {
			Orientation orientation = Orientation::T2B;
			uint8_t margin_x = 0, margin_y = 0;
		} layout;

		struct {
			bool show = true;
			int8_t offset_x = 0, offset_y = 24;
			sigma_lib::W32::GDI::Color text_color{ -1 };
		} tooltip;

		struct {
			Scale scale = Scale::logarithmic;
			uint8_t size_min = 15, size_max = 50;
			bool hide_cursor = false;
			int16_t delay_ms = 10;
			int8_t wheel = +1;
		} behavior;

		void load(char const* ini_file);
		void save(char const* ini_file) const;
		constexpr bool is_enabled() const { return true; }
	} settings;

	/// @brief starts or terminates the functinality.
	/// @param hwnd the handle to the window of this plugin.
	/// @param initializing `true` when hooking, `false` when unhooking.
	/// @return `true` when hook/unhook was necessary and successfully done, `false` otherwise.
	bool setup(HWND hwnd, bool initializing);

	namespace menu
	{
		enum : int32_t {
			decr_size,
			incr_size,
			reset_size,
		};
		struct item {
			int32_t id; char const* title;
		};
	}
	constexpr menu::item menu_items[] = {
		{ menu::decr_size,	"レイヤー幅を小さく" },
		{ menu::incr_size,	"レイヤー幅を大きく" },
		{ menu::reset_size,	"レイヤー幅をリセット" },
	};

	/// @brief handles menu commands.
	/// @param hwnd the handle to the window of this plugin.
	/// @param menu_id the id of the menu command defined in `menu_items`.
	/// @param editp the edit handle.
	/// @return `true` to redraw the window, `false` otherwise.
	bool on_menu_command(HWND hwnd, int32_t menu_id, AviUtl::EditHandle* editp);

	/// @brief practically the main window procedure for this plugin window.
	BOOL on_wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

	/// @brief checks for conflicts with other plugins.
	/// @return `true` if this plugin can still continue (might be restricted), `false` if the conflict is fatal.
	bool check_conflict();

	namespace internal
	{
		/// @brief gets the pending-to-change layer size.
		/// @return the layer size that is expected to be set.
		int get_layer_size_delayed();

		/// @brief sets the layer size, but might not change the size immediately.
		/// @param size the new layer size in pixels to set.
		/// @param delay whether to allow delays to change the size.
		void set_layer_size(int size, bool delay);
	}
}
