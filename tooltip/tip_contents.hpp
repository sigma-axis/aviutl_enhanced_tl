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
#include <CommCtrl.h>

#include "../tooltip.hpp"


////////////////////////////////
// ツールチップ表示．
////////////////////////////////
namespace enhanced_tl::tooltip
{
	struct tip_content {
		static inline HWND tip = nullptr;
		static void setup(bool initializing);
		void setup();
		static void relay(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
		uintptr_t uid() const { return reinterpret_cast<uintptr_t>(this); }
		static tip_content* from_uid(uintptr_t uid) { return reinterpret_cast<tip_content*>(uid); }

		static bool is_active();
		bool active() const;

		virtual LRESULT on_show() { return 0; }
		virtual LRESULT on_draw(NMTTCUSTOMDRAW const& info) { return CDRF_DODEFAULT; }
		virtual void on_get_dispinfo(NMTTDISPINFOA& info) {}

		// returns true when tracking is active.
		static bool on_mouse_move(int x, int y, bool leaving);
		void on_resize();

	protected:
		virtual void setup_core(TTTOOLINFOW& ti) {}
		virtual RECT get_rect() = 0;
		virtual void update(int x, int y) {};
		virtual bool should_deactivate(int x, int y);
	};

	struct tip_content_semitracking : tip_content {
		void on_get_dispinfo(NMTTDISPINFOA& info) override final;
	protected:
		virtual void on_get_dispinfo_core(NMTTDISPINFOA& info) = 0;
		virtual void on_activate() {}
	};

	struct tip_content_tracking : tip_content {
		void activate(int x, int y, bool active);
	protected:
		void setup_core(TTTOOLINFOW& ti) override;
		// returns false if cannot activate / deactivate.
		virtual bool on_activate(int x, int y, bool active) { return true; }
	};

	namespace internal
	{
		constexpr int pad_right_extra = 2, pad_bottom_extra = 1;
		SIZE measure_text(std::wstring const& text, HWND hwnd);

		inline auto centisec_converter()
		{
			AviUtl::FileInfo fi;
			exedit.fp->exfunc->get_file_info(*exedit.editp, &fi);
			return [fps_numer = fi.video_rate, fps_denom = fi.video_scale](int frames) {
				return static_cast<int>(100LL * frames * fps_denom / fps_numer);
			};
		}
	}
}
