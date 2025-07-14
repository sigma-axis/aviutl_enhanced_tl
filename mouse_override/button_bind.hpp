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

#include "../modkeys.hpp"
#include "../inifile_op.hpp"


////////////////////////////////
// マウスボタンの設定項目．
////////////////////////////////
namespace enhanced_tl::mouse_override::button_bind
{
	template<class LoaderT, class T>
	concept loader = requires (T t, int32_t n)
	{
		{ LoaderT::Save(t) } -> std::convertible_to<int32_t>;
		{ LoaderT::Load(n) } -> std::convertible_to<T>;
	};
	template<class T>
	struct simple_loader {
		static constexpr int32_t Save(T t) noexcept { return static_cast<int32_t>(t); }
		static constexpr T Load(int32_t n) noexcept { return static_cast<T>(n); }
	};

	/// @brief represents a binding for a mouse button action with different modifier keys.
	/// @tparam T the type of the action, must be convertible to and from int32_t.
	/// @tparam LoaderT the type used to convert values between `T` and `int32_t`.
	template<class T, loader<T> LoaderT = simple_loader<T>>
	struct binding {
		using action_type = T;
		using loader_type = LoaderT;
		using modkeys = sigma_lib::modifier_keys::modkeys;

		action_type neutral, ctrl, shift, ctrl_shift,
			alt, ctrl_alt, shift_alt, three;

		/// @brief selects the action based on the modifier keys.
		/// @param deduced this type.
		/// @param mkeys the modifier keys to select the action for.
		/// @return a reference to the selected action.
		constexpr auto& operator[](this auto& self, modkeys mkeys)
		{
			switch (mkeys) {
			default:
			case modkeys::none: return self.neutral;
			case modkeys::ctrl: return self.ctrl;
			case modkeys::shift: return self.shift;
			case modkeys::alt: return self.alt;
			case modkeys::ctrl | modkeys::shift: return self.ctrl_shift;
			case modkeys::ctrl | modkeys::alt: return self.ctrl_alt;
			case modkeys::shift | modkeys::alt: return self.shift_alt;
			case modkeys::ctrl | modkeys::shift | modkeys::alt: return self.three;
			}
		}

		/// @brief loads the actions for each modifier key from an ini file.
		/// @param based_on the fallback value for each action if it is not specified.
		/// @param ini_file the path to the ini file to load from.
		/// @param section the section in the ini file, that is of the form `[section]`.
		/// @param key_head the head of the key in ini file. each key will be of the form `key_head<modifiers>`.
		void load(binding const& based_on, char const* ini_file, char const* section, char const* key_head)
		{
			constexpr auto len_tail = std::bit_ceil(2
				+ std::string_view{ (modkeys::ctrl | modkeys::shift | modkeys::alt).canon_name() }.length());
			std::string key = key_head;
			size_t const len_head = key.size();
			key.reserve(len_head + len_tail);

			// load the neutral action first.
			if (len_head == 0) key = modkeys{}.canon_name();
			neutral		= load_one(based_on.neutral,	ini_file, section, key.c_str());

			// load the actions for each modifier key. default to the neutral action if not specified.
			set_key(key, modkeys::ctrl, len_head);
			ctrl		= load_one(based_on.ctrl,		ini_file, section, key.c_str());
			set_key(key, modkeys::shift, len_head);
			shift		= load_one(based_on.shift,		ini_file, section, key.c_str());
			set_key(key, modkeys::ctrl | modkeys::shift, len_head);
			ctrl_shift = load_one(based_on.ctrl_shift,	ini_file, section, key.c_str());

			set_key(key, modkeys::alt, len_head);
			alt			= load_one(based_on.alt,		ini_file, section, key.c_str());
			set_key(key, modkeys::ctrl | modkeys::alt, len_head);
			ctrl_alt	= load_one(based_on.ctrl_alt,	ini_file, section, key.c_str());
			set_key(key, modkeys::shift | modkeys::alt, len_head);
			shift_alt	= load_one(based_on.shift_alt,	ini_file, section, key.c_str());

			set_key(key, modkeys::ctrl | modkeys::shift | modkeys::alt, len_head);
			three		= load_one(based_on.three,		ini_file, section, key.c_str());
		}

		/// @brief normalizes the actions by checking if they are valid.
		/// fallback for neutral action is specified, others are set to neutral if invalid.
		/// @param is_valid a callable of the form `bool(action_type)` that checks if the action is valid.
		/// @param fallback the set of actions to use as fallbacks.
		constexpr void normalize(auto&& is_valid, binding const& fallback)
		{
			if (!is_valid(neutral))		neutral		= fallback.neutral;
			if (!is_valid(ctrl))		ctrl		= fallback.ctrl;
			if (!is_valid(shift))		shift		= fallback.shift;
			if (!is_valid(ctrl_shift))	ctrl_shift	= fallback.ctrl_shift;
			if (!is_valid(alt))			alt			= fallback.alt;
			if (!is_valid(ctrl_alt))	ctrl_alt	= fallback.ctrl_alt;
			if (!is_valid(shift_alt))	shift_alt	= fallback.shift_alt;
			if (!is_valid(three))		three		= fallback.three;
		}

		static constexpr binding uniform(action_type uniform_action) {
			return {
				.neutral	= uniform_action,
				.ctrl		= uniform_action,
				.shift		= uniform_action,
				.ctrl_shift	= uniform_action,
				.alt		= uniform_action,
				.ctrl_alt	= uniform_action,
				.shift_alt	= uniform_action,
				.three		= uniform_action,
			};
		}

	private:
		static constexpr void set_key(std::string& key, modkeys mkeys, size_t pos)
		{
			if (pos == 0) key = mkeys.canon_name();
			else {
				key.erase(pos);
				key.append("<").append(mkeys.canon_name()).append(">");
			}
		}

		/// @brief loads a single action from the ini file.
		static action_type load_one(action_type def, char const* ini_file, char const* section, char const* key)
		{
			using namespace sigma_lib::inifile;
			return loader_type::Load(read_int(loader_type::Save(def), ini_file, section, key));
		}
	};
}
