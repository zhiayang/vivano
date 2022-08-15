// timer.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <chrono>

#include <zpr.h>
#include "util.h"

namespace util
{
	Timer::Timer() : start_time(std::chrono::steady_clock::now())
	{
	}

	std::string prettyPrintTime(std::chrono::steady_clock::duration dur)
	{
		namespace stdc = std::chrono;
		using namespace std::chrono_literals;

		auto time = stdc::duration_cast<stdc::seconds>(dur).count();
		std::string ret;

		if(time >= 60 * 60)
		{
			ret += zpr::sprint("{}h ", time / (60 * 60));
			time %= 60 * 60;
		}

		if(time >= 60)
		{
			ret += zpr::sprint("{}m ", time / 60);
			time %= 60;
		}

		if(time > 0)
		{
			ret += zpr::sprint("{}s", time);
		}
		else if(ret.empty())
		{
			auto ms = stdc::duration_cast<stdc::milliseconds>(dur).count();
			ret = zpr::sprint("{}ms", ms);
		}

		return ret;
	}

	std::string Timer::print() const
	{
		return prettyPrintTime(this->measure());
	}

	std::chrono::steady_clock::duration Timer::measure() const
	{
		return std::chrono::steady_clock::now() - this->start_time;
	}

}
