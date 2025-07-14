/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <cstdint>
#include <string>
#include <memory>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "modkeys.hpp"

#include "../timeline.hpp"
#include "mouse_override/button_bind.hpp"


////////////////////////////////
// 拡張編集マウス操作の換装．
////////////////////////////////
namespace enhanced_tl::mouse_override
{
	inline constinit struct Settings {
		template<class T>
		using binding = enhanced_tl::mouse_override::button_bind::binding<T>;
		template<typename T>
		static constexpr auto uniform(T action) { return binding<T>::uniform(action); }
		using modkeys = sigma_lib::modifier_keys::modkeys;
		enum class tl_zoom_center : uint8_t {
			curr_frame		= 0,
			window_left		= 1,
			window_center	= 2,
			window_right	= 3,
			mouse			= 4,
		};

		struct Timeline {
			struct Drag{
				enum class action : uint8_t {
					none		=   0,
					l			=   1,
					obj_l		=   2,
					obj_ctrl_l	=   3,
					obj_shift_l	=   4,
					bk_l		=   5,
					bk_ctrl_l	=   6,
					alt_l		=   7,
					zoom_bi		= 100,
					step_bound	= 101,
					step_bpm	= 102,

					bypass = 255,
				};

				binding<action> L, R, M, X1, X2, L_and_R;
			};

			struct Click{
				enum class action : uint8_t {
					none				=   0,
					obj_ctrl_shift_l	=   1,
					obj_l_dbl			=   2,
					rclick				=   3,
					toggle_midpt		= 100,
					select_line_left	= 102,
					select_line_right	= 103,
					select_all			= 106,
					squeeze_left		= 110,
					squeeze_right		= 111,
					toggle_active		= 112,

					bypass = 255,
				};
				binding<action> L, R, M, X1, X2;
			};

			struct Wheel{
				enum class action : uint8_t {
					none = 0,
					scroll_h_p			=   1,	scroll_h_n			=   2,
					scroll_v_p			=   3,	scroll_v_n			=   4,
					zoom_h_p			=   5,	zoom_h_n			=   6,
					zoom_v_p			= 101,	zoom_v_n			= 102,
					move_one_p			= 103,	move_one_n			= 104,
					move_len_p			= 105,	move_len_n			= 106,
					move_midpt_layer_p	= 107,	move_midpt_layer_n	= 108,
					move_obj_layer_p	= 109,	move_obj_layer_n	= 110,
					move_midpt_all_p	= 111,	move_midpt_all_n	= 112,
					move_obj_all_p		= 113,	move_obj_all_n		= 114,
					move_bpm_p			= 115,	move_bpm_n			= 116,
					change_scene_p		= 121,	change_scene_n		= 122,

					bypass = 255,
				};
				binding<action> normal, r_button;
			};

			Drag drag{
				.L			= uniform(Drag::action::l),
				.R			= uniform(Drag::action::none),
				.M			= uniform(Drag::action::none),
				.X1			= uniform(Drag::action::bypass),
				.X2			= uniform(Drag::action::bypass),
				.L_and_R	= uniform(Drag::action::none),
			};
			Drag drag_obj = drag;

			Click click{
				.L	= uniform(Click::action::none),
				.R	= uniform(Click::action::rclick),
				.M	= uniform(Click::action::none),
				.X1	= uniform(Click::action::bypass),
				.X2	= uniform(Click::action::bypass),
			};
			Click click_obj = click;
			Click dbl_click{
				.L	= uniform(Click::action::none),
				.R	= uniform(Click::action::none),
				.M	= uniform(Click::action::none),
				.X1	= uniform(Click::action::bypass),
				.X2	= uniform(Click::action::bypass),
			};
			Click dbl_click_obj = dbl_click;

			Wheel wheel{
				.normal = {
					.neutral	= Wheel::action::scroll_h_p,
					.ctrl		= Wheel::action::zoom_h_p,
					.shift		= Wheel::action::scroll_h_p,
					.ctrl_shift	= Wheel::action::zoom_h_p,
					.alt		= Wheel::action::scroll_v_p,
					.ctrl_alt	= Wheel::action::zoom_h_p,
					.shift_alt	= Wheel::action::scroll_v_p,
					.three		= Wheel::action::zoom_h_p,
				},
				.r_button = uniform(Wheel::action::none),
			};

			bool enabled					= true;
			bool change_cursor				= true;
			bool wheel_vertical_scrollbar	= true;
			tl_zoom_center zoom_center_wheel = tl_zoom_center::mouse;
			tl_zoom_center zoom_center_drag = tl_zoom_center::window_left;
			uint8_t zoom_drag_refine_x		= 8;
			int16_t zoom_drag_length_x		= 64; // if zero, no change along that axis.
			int16_t zoom_drag_length_y		= 16; // if zero, no change along that axis.
			modkeys skip_midpt_key			= modkeys::shift;
			bool skip_midpt_def				= true;
			modkeys skip_inactives_key		= modkeys::none;
			bool skip_inactives_def			= false;
			binding<int8_t> BPM{
				.neutral	= 1,
				.ctrl		= 1,
				.shift		= 4,
				.ctrl_shift	= 1,
				.alt		= 1,
				.ctrl_alt	= 1,
				.shift_alt	= 1,
				.three		= 1,
			};
		} timeline;

		struct Layer {
			struct Drag {
				enum class action : uint8_t {
					none		=   0,
					show_hide	=   1,
					lock_unlock	=   2,
					link_coord	=   3,
					mask_above	=   4,
					select_all	= 100,
					drag_move	= 101,

					bypass = 255,
				};
				binding<action> L, R, M, X1, X2, L_and_R;
			};

			struct Click {
				enum class action : uint8_t {
					none			= 0,
					rclick			= 1,
					rename			= 2,
					toggle_others	= 3,
					insert			= 4,
					remove			= 5,

					bypass = 255,
				};
				binding<action> L, R, M, X1, X2;
			};

			struct Wheel {
				using action = Timeline::Wheel::action;
				binding<action> normal, r_button;
			};

			Drag drag{
				.L = {
					.neutral	= Drag::action::show_hide,
					.ctrl		= Drag::action::select_all,
					.shift		= Drag::action::lock_unlock,
					.ctrl_shift	= Drag::action::show_hide,
					.alt		= Drag::action::show_hide,
					.ctrl_alt	= Drag::action::show_hide,
					.shift_alt	= Drag::action::show_hide,
					.three		= Drag::action::show_hide,
				},
				.R			= uniform(Drag::action::none),
				.M			= uniform(Drag::action::none),
				.X1			= uniform(Drag::action::bypass),
				.X2			= uniform(Drag::action::bypass),
				.L_and_R	= uniform(Drag::action::none),
			};

			Click click{
				.L	= uniform(Click::action::none),
				.R	= uniform(Click::action::rclick),
				.M	= uniform(Click::action::rename),
				.X1	= uniform(Click::action::bypass),
				.X2	= uniform(Click::action::bypass),
			};
			Click dbl_click{
				.L	= uniform(Click::action::toggle_others),
				.R	= uniform(Click::action::none),
				.M	= uniform(Click::action::none),
				.X1	= uniform(Click::action::bypass),
				.X2	= uniform(Click::action::bypass),
			};

			Wheel wheel{
				.normal = {
					.neutral	= Wheel::action::scroll_v_p,
					.ctrl		= Wheel::action::zoom_v_p,
					.shift		= Wheel::action::scroll_v_p,
					.ctrl_shift	= Wheel::action::scroll_v_p,
					.alt		= Wheel::action::scroll_v_p,
					.ctrl_alt	= Wheel::action::scroll_v_p,
					.shift_alt	= Wheel::action::scroll_v_p,
					.three		= Wheel::action::scroll_v_p,
				},
				.r_button = uniform(Wheel::action::none),
			};

			bool enabled					= true;
			int16_t auto_scroll_delay_ms	= 100;
			tl_zoom_center zoom_center_wheel = tl_zoom_center::window_left;
		} layer;

		struct ZoomGauge {
			bool enabled = true;
			int8_t wheel = 1;
			tl_zoom_center zoom_center = tl_zoom_center::curr_frame;
		} zoom_gauge;

		struct SceneButton{
			bool enabled = true;
			int8_t wheel = 0;
		} scene_button;

		void load(char const* ini_file);
		constexpr bool is_enabled() const {
			return timeline.enabled || layer.enabled || zoom_gauge.enabled || scene_button.enabled;
		}
	} settings;

	/// @brief starts or terminates the functinality.
	/// @param hwnd the handle to the window of this plugin.
	/// @param initializing `true` when hooking, `false` when unhooking.
	/// @return `true` when hook/unhook was necessary and successfully done, `false` otherwise.
	bool setup(HWND hwnd, bool initializing);

	/// @brief checks for conflicts with other plugins.
	/// @return `true` if this plugin can still continue (might be restricted), `false` if the conflict is fatal.
	bool check_conflict();

	namespace internal
	{
		BOOL call_next_proc(UINT message, WPARAM wparam, LPARAM lparam);
		int32_t zoom_center_frame(Settings::tl_zoom_center center);
		int32_t zoom_center_frame(Settings::tl_zoom_center center, int client_x);
		int32_t zoom_center_frame(Settings::tl_zoom_center center, int screen_x, int screen_y);
	}

	namespace interop
	{
		inline POINT pt_mouse_down{}; // client coordinate.
		inline int idx_obj_on_mouse = -1;
		inline int former_frame_begin = -1;
		inline int former_frame_end = -1;
		inline int former_layer_disp = -1;
	}
}
