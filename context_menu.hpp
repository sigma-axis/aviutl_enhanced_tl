/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <cstdint>
#include <tuple>
#include <memory>
#include <string>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "timeline.hpp"


////////////////////////////////
// 右クリックメニュー追加．
////////////////////////////////
struct AviUtl::EditHandle;
namespace enhanced_tl::context_menu
{
	inline constinit struct Settings {
		constexpr static auto num_levels = enhanced_tl::timeline::constants::num_zoom_levels;
		std::unique_ptr<std::wstring> zoom_menus[num_levels]{};
		bool zoom_gauge = true, scene_button = true;

		void load(char const* ini_file);
		constexpr bool is_enabled() const { return zoom_gauge || scene_button; }
	} settings;

	/// @brief starts or terminates the functinality.
	/// @param hwnd the handle to the window of this plugin.
	/// @param initializing `true` when hooking, `false` when unhooking.
	/// @return `true` when hook/unhook was necessary and successfully done, `false` otherwise.
	bool setup(HWND hwnd, bool initializing);

	namespace menu
	{
		// iterating menu items without statically allocating
		// buffer for each menu item and narrow string of its title.
		struct title_wrapper {
			std::string title;
			constexpr operator char const* () const { return title.c_str(); }
		};
		struct iterator {
			int32_t idx;
			std::pair<int32_t, title_wrapper> operator*() const;
			constexpr iterator& operator++() {
				while (++idx < Settings::num_levels && settings.zoom_menus[idx] == nullptr);
				return *this;
			}
			constexpr bool operator !=(iterator const& right) const { return idx != right.idx; }
		};
		struct enumerator {
			constexpr iterator begin() const { return ++iterator{ -1 }; }
			constexpr iterator end() const { return { Settings::num_levels }; }
		};
	}
	constexpr menu::enumerator menu_items{};

	/// @brief handles menu commands.
	/// @param hwnd the handle to the window of this plugin.
	/// @param menu_id the id of the menu command defined in `menu_items`.
	/// @param editp the edit handle.
	/// @return `true` to redraw the window, `false` otherwise.
	bool on_menu_command(HWND hwnd, int32_t menu_id, AviUtl::EditHandle* editp);
}
