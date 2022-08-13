// progressbar.h
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chrono>
#include <cstddef>

namespace util
{
	struct ProgressBar
	{
		ProgressBar(size_t left_pad, size_t width);

		void update();
		void showTime();

		void clear() const;
		void draw() const;

		static inline constexpr auto DEFAULT_INTERVAL = []() {
			using namespace std::chrono_literals;
			return 75ms;
		}();

	private:
		std::chrono::steady_clock::time_point m_start_time;

		bool m_show_time = false;
		size_t m_left_pad = 0;
		size_t m_width = 0;
		size_t m_ticks = 0;
	};
}
