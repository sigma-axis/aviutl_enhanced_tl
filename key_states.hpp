/*
The MIT License (MIT)

Copyright (c) 2024-2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <cstdint>
#include <algorithm>
#include <tuple>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

////////////////////////////////
// Windows API 利用の補助関数．
////////////////////////////////
namespace sigma_lib::W32::UI
{
	enum class key_map : uint8_t { id, off, on, inv };

	namespace detail
	{
		constexpr static size_t num_keys = 256;
		static constexpr uint8_t coerce_key(int val) {
			if (val < 0 || val >= num_keys) return 0;
			return static_cast<uint8_t>(val);
		}
		constexpr static uint8_t map_key_state(uint8_t& key, key_map mod)
		{
			uint8_t a, b;
			switch (mod) {
				using enum key_map;
			case off:	a = 0x80, b = 0x80; break;
			case on:	a = 0x80, b = 0x00; break;
			case inv:	a = 0x00, b = 0x80; break;
			case id: default: a = b = 0x00; break;
			}
			return std::exchange(key, (key | a) ^ b);
		}
		constexpr static uint8_t map_key_state(uint8_t& key, bool mod)
		{
			uint8_t a, b;
			if (mod) { a = 0x80; b = 0x00; }
			else { a = 0x80; b = 0x80; }
			return std::exchange(key, (key | a) ^ b);
		}
		constexpr static uint8_t map_key_state(uint8_t& key, uint8_t mod)
		{
			return std::exchange(key, mod);
		}
		constexpr static bool is_key_mod_identity(key_map mod) { return mod == key_map::id; }
		constexpr static bool is_key_mod_identity(bool) { return false; }
		constexpr static bool is_key_mod_identity(uint8_t) { return false; }
	}

	// キー入力認識をちょろまかす補助クラス．
	template<size_t N>
	class ForceKeyState {
		template<size_t i, class... ArgsT>
		using arg_at = std::decay_t<std::tuple_element_t<i, std::tuple<ArgsT...>>>;
		template<size_t i>
		using idx_const = std::integral_constant<size_t, i>;

		template<class... ArgsT>
		consteval static bool check_requirements() {
			constexpr auto check_arg = []<size_t i>(idx_const<i>) {
				return std::convertible_to<arg_at<i, ArgsT...>, int>;
			};
			constexpr auto check_mod = []<size_t i>(idx_const<i>) {
				using mod = arg_at<i, ArgsT...>;
				return std::same_as<mod, bool>
					|| std::same_as<mod, key_map>
					|| std::convertible_to<mod, uint8_t>;
			};

			if (!([]<size_t... I>(std::index_sequence<I...>) {
				if (!(check_arg(idx_const<2 * I>{}) && ...)) return false;
				if (!(check_mod(idx_const<2 * I + 1>{}) && ...)) return false;
				return true;
			}(std::make_index_sequence<N>{}))) return false;

			return true;
		}

		uint8_t key[N], rew[N];

		template<size_t... I>
		ForceKeyState(std::index_sequence<I...>, auto&&... args) noexcept
			: rew{}
		{
			((key[I] =
				!detail::is_key_mod_identity(std::get<2 * I + 1>(std::forward_as_tuple(args...))) ?
				detail::coerce_key(std::get<2 * I>(std::forward_as_tuple(args...))) : 0), ...);
			if (((key[I] == 0) && ...)) return;

			uint8_t state[detail::num_keys]; std::ignore = ::GetKeyboardState(state);
			((key[I] > 0 && (rew[I] = detail::map_key_state(state[key[I]], std::get<2 * I + 1>(std::forward_as_tuple(args...))))), ...);
			::SetKeyboardState(state);
		}

	public:
		template<class... ArgsT>
			requires(sizeof...(ArgsT) == 2 * N && check_requirements<ArgsT...>())
		ForceKeyState(ArgsT const... args) noexcept
			: ForceKeyState{ std::make_index_sequence<N>{}, args... } {}

		ForceKeyState(ForceKeyState const&) = delete;
		ForceKeyState(ForceKeyState&&) = delete;

		~ForceKeyState() noexcept {
			size_t i = 0;
			for (; i < N; i++) { if (key[i] > 0) goto rewind; }
			return;

		rewind:
			// rewind the state.
			uint8_t state[detail::num_keys]; std::ignore = ::GetKeyboardState(state);
			do {
				if (key[i] > 0) state[key[i]] = rew[i];
			} while (++i < N);
			::SetKeyboardState(state);
		}

		template<class... ArgsT>
			requires(N > 1 && sizeof...(ArgsT) == 2 * N && check_requirements<ArgsT...>())
		static void Set(std::remove_cvref_t<ArgsT>... args)
		{
			[&] <size_t... I>(std::index_sequence<I...>) {
				if ((((std::get<2 * I>(std::forward_as_tuple(args...)) =
					!detail::is_key_mod_identity(std::get<2 * I + 1>(std::forward_as_tuple(args...))) ?
					coerce_key(std::get<2 * I>(std::forward_as_tuple(args...))) :
					0) == 0) && ...)) return;

				uint8_t state[detail::num_keys]; std::ignore = ::GetKeyboardState(state);
				((detail::map_key_state(state[
					std::get<2 * I>(std::forward_as_tuple(args...))],
					std::get<2 * I + 1>(std::forward_as_tuple(args...)))), ...);
				::SetKeyboardState(state);
			}(std::make_index_sequence<N>{});
		}
		template<class KeyT, class ModT>
			requires(N == 1 && check_requirements<KeyT, ModT>())
		static uint8_t Set(KeyT key, ModT mod)
		{
			if (detail::is_key_mod_identity(mod) || coerce_key(key) == 0) return ::GetKeyState(key);
			uint8_t state[detail::num_keys]; std::ignore = ::GetKeyboardState(state);
			key = detail::map_key_state(state[coerce_key(key)], mod);
			::SetKeyboardState(state);
			return key;
		}
	};
	template<class... ArgsT>
	ForceKeyState(ArgsT...) -> ForceKeyState<sizeof...(ArgsT) / 2>;
	template<>
	struct ForceKeyState<0> {};

	template<class... ArgsT>
		requires(sizeof...(ArgsT) > 0 && requires(ArgsT... args) { ForceKeyState<sizeof...(ArgsT) / 2>::Set(args...); })
	static auto SetKeyState(ArgsT... args) { return ForceKeyState<sizeof...(ArgsT) / 2>::Set(args...); }
}
