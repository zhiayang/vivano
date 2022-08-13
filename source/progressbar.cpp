// progressbar.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <cstdio>

#include <zpr.h>

#include "util.h"
#include "progressbar.h"

namespace util
{
	ProgressBar::ProgressBar(size_t left_pad, size_t width) : m_left_pad(left_pad), m_width(width)
	{
		m_start_time = std::chrono::steady_clock::now();
	}

	void ProgressBar::update()
	{
		m_ticks++;
	}

	void ProgressBar::clear() const
	{
		auto width = getTerminalWidth();
		zpr::print("\r{}\r", std::string(width - 1, ' '));
	}

	void ProgressBar::showTime()
	{
		m_show_time = true;
	}

	void ProgressBar::draw() const
	{
		size_t width = 0;
		std::string time_str {};
		if(m_show_time)
		{
			auto now = std::chrono::steady_clock::now();
			time_str = zpr::sprint(":  {-12 }", util::prettyPrintTime(now - m_start_time));
		}

		// just assume that the time is at most 15 chars long
		if(m_width + m_left_pad + 1 + 12 > getTerminalWidth())
			width = getTerminalWidth() - m_left_pad - 1;
		else
			width = m_width;

		// just in case we underflow
		if(width > getTerminalWidth() || width < 10)
		{
			// fallback to a single char | / - \ thing
			constexpr char bars[] = { '-', '\\', '|', '/' };
			zpr::print("\r{}{}\r", std::string(m_left_pad, ' '), bars[m_ticks % 4]);
			fflush(stdout);
		}
		else
		{
			const auto bar_width = (width - 2 - 3);
			auto pos = m_ticks % (2 * bar_width - 2);

			std::string ls;
			std::string rs;

			if(pos >= bar_width)
			{
				pos -= bar_width;
				ls = std::string(bar_width - pos - 2, ' ');
				rs = std::string(pos + 1, ' ');
			}
			else
			{
				auto start = pos % bar_width;

				ls = std::string(start, ' ');
				rs = std::string(width - 2 - 3 - start - 1, ' ');
			}

			zpr::print("\r{}[{}<=>{}]{}\r", std::string(m_left_pad, ' '), ls, rs,
				time_str);
			fflush(stdout);
		}
	}
}
