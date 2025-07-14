/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <cstdint>
#include <concepts>
#include <string>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "color_abgr.hpp"


////////////////////////////////
// ツールチップ表示．
////////////////////////////////
namespace enhanced_tl::tooltip
{
	inline constinit struct Settings {
		uint16_t delay = 340, duration = 10000;
		bool animation = true;
		int8_t offset_x = 0, offset_y = 24;
		sigma_lib::W32::GDI::Color text_color{ -1 };

		struct {
			bool enabled	= true;
		} scene_button;

		struct {
			bool enabled	= true;
		} zoom_gauge;

		struct {
			bool enabled	= true;
			bool frame_1_origin	= true;
		} ruler;

		struct {
			bool enabled	= true;
			bool centralize = false;
		} layer_name;

		struct {
			bool enabled	= true;
			int8_t max_filters = 4;
		} object_info;

		struct {
			bool enabled = true;
		} drag_info;

		void load(char const* ini_file);
		constexpr bool is_enabled() const {
			return scene_button.enabled || zoom_gauge.enabled || 
				ruler.enabled || layer_name.enabled ||
				object_info.enabled || drag_info.enabled;
		}
	} settings;

	/// @brief starts or terminates the functinality.
	/// @param hwnd the handle to the window of this plugin.
	/// @param initializing `true` when hooking, `false` when unhooking.
	/// @return `true` when hook/unhook was necessary and successfully done, `false` otherwise.
	bool setup(HWND hwnd, bool initializing);
}
