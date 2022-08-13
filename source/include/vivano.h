// vivano.h
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstddef>

#include <string>
#include <vector>
#include <filesystem>
#include <string_view>

#include <zpr.h>
#include <zst.h>

using zst::Ok;
using zst::Err;
using zst::ErrFmt;

namespace util
{
	std::string colourise(std::string_view sv, int severity);
}

namespace vvn
{
	void logIndent();
	void logUnindent();

	struct LogIndenter
	{
		LogIndenter() { logIndent(); }
		~LogIndenter() { logUnindent(); }
	};

	int getLogIndent();

	inline auto indentStr(int extra = 0)
	{
		return zpr::w((extra + getLogIndent()) * 2)("");
	}

	template <typename... Args>
	void error(const char* fmt, Args&&... args)
	{
		zpr::fprintln(stderr, "{}{} {}", indentStr(), util::colourise("[vvn-err]", 3),
			zpr::fwd(fmt, static_cast<Args&&>(args)...));
	}

	template <typename... Args>
	[[noreturn]] void error_and_exit(const char* fmt, Args&&... args)
	{
		zpr::fprintln(stderr, "{}{} {}", indentStr(), util::colourise("[vvn-err]", 3),
			zpr::fwd(fmt, static_cast<Args&&>(args)...));
		exit(1);
	}

	template <typename... Args>
	void log(const char* fmt, Args&&... args)
	{
		zpr::println("{}{} {}", indentStr(), util::colourise("[vvn-log]", 0),
			zpr::fwd(fmt, static_cast<Args&&>(args)...));
	}

	template <typename... Args>
	void warn(const char* fmt, Args&&... args)
	{
		zpr::println("{}{} {}", indentStr(), util::colourise("[vvn-wrn]", 2),
			zpr::fwd(fmt, static_cast<Args&&>(args)...));
	}
}
