// util.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <cctype>
#include <cstdio>
#include <cassert>

#include <cstdlib>
#include <unistd.h>

#include <span>
#include <chrono>
#include <vector>
#include <utility>
#include <fstream>
#include <sstream>
#include <charconv>
#include <algorithm>
#include <filesystem>
#include <string_view>

#include "args.h"
#include "util.h"
#include "vivano.h"

namespace stdfs = std::filesystem;

namespace util
{
	std::string readEntireFile(std::string_view path)
	{
		auto f = std::ifstream(std::string(path), std::ios::in | std::ios::binary);
		std::stringstream ss {};
		ss << f.rdbuf();

		return ss.str();
	}

	std::string_view trim(std::string_view sv)
	{
		while(sv.size() > 0 && sv.find_first_of(" \t") == 0)
			sv.remove_prefix(1);

		while(sv.size() > 0 && sv.find_last_of(" \t") == sv.size() - 1)
			sv.remove_suffix(1);

		return sv;
	}

	std::vector<std::string_view> splitString(std::string_view str, char delim)
	{
		std::vector<std::string_view> ret {};

		while(true)
		{
			size_t ln = str.find(delim);

			if(ln != std::string_view::npos)
			{
				ret.emplace_back(str.data(), ln);
				str.remove_prefix(ln + 1);
			}
			else
			{
				break;
			}
		}

		// account for the case when there's no trailing newline, and we still have some stuff stuck in the view.
		if(!str.empty())
			ret.emplace_back(str.data(), str.length());

		return ret;
	}



	/*
		Same semantics as `find_files`, but it returns files matching the given extension.
	*/
	std::vector<stdfs::path> find_files_ext(const stdfs::path& dir, std::string_view ext)
	{
		return find_files(dir, [&ext](auto ent) -> bool {
			return ent.path().extension() == ext;
		});
	}

	/*
		Same semantics as `find_files_recursively`, but it returns files matching the given extension.
	*/
	std::vector<stdfs::path> find_files_ext_recursively(const stdfs::path& dir, std::string_view ext)
	{
		return find_files_recursively(dir, [&ext](auto ent) -> bool {
			return ent.path().extension() == ext;
		});
	}


	std::optional<int> parseInt(std::string_view sv)
	{
		int x = 0;
		if(std::from_chars(sv.begin(), sv.end(), x).ec == std::errc{})
			return x;
		else
			return std::nullopt;
	}

	std::string lowercase(std::string_view sv)
	{
		auto ret = std::string(sv);
		for(size_t i = 0; i < ret.size(); i++)
			ret[i] = static_cast<char>(std::tolower(sv[i]));

		return ret;
	}
}



#include <pwd.h>

stdfs::path util::getHomeFolder()
{
	#if defined(_WIN32)
		#error "not support";
	#else
		auto home = std::getenv("HOME");
		if(home != nullptr)
			return stdfs::path(home);

		struct passwd* pw = getpwuid(getuid());
		return stdfs::path(pw->pw_dir);
	#endif
}




[[noreturn]] void zst::error_and_exit(const char* s, size_t n)
{
	zpr::fprintln(stderr, "{}", std::string_view(s, n));
	abort();
}


static int g_log_indent = 0;

void vvn::logIndent()
{
	g_log_indent++;
}
void vvn::logUnindent()
{
	g_log_indent--;
	assert(g_log_indent >= 0);
}
int vvn::getLogIndent()
{
	return g_log_indent;
}

static bool is_tty()
{
	// if we're not printing to a tty, don't output colours. don't be
	// "one of those" programs.
	#if defined(_WIN32)
		return GetFileType(GetStdHandle(STD_OUTPUT_HANDLE));
	#else
		return isatty(STDOUT_FILENO);
	#endif
}

std::string util::colourise(std::string_view sv, int severity)
{
	if(not is_tty())
		return std::string(sv);

	constexpr std::string_view COLOUR_INFO  = "\x1b[30;1m";
	constexpr std::string_view COLOUR_LOG   = "\x1b[94;1m";
	constexpr std::string_view COLOUR_WARN  = "\x1b[1m\x1b[33m";
	constexpr std::string_view COLOUR_CRIT  = "\x1b[1m\x1b[31m";
	constexpr std::string_view COLOUR_ERROR = "\x1b[1m\x1b[37m\x1b[101m";
	constexpr std::string_view COLOUR_RESET = "\x1b[0m";

	std::string_view colour {};
	switch(severity)
	{
		case 0: colour = COLOUR_INFO; break;
		case 1: colour = COLOUR_LOG; break;
		case 2: colour = COLOUR_WARN; break;
		case 3: colour = COLOUR_CRIT; break;


		case 4: [[fallthrough]];
		default:
			colour = COLOUR_ERROR;
			break;
	}

	return zpr::sprint("{}{}{}", colour, sv, COLOUR_RESET);
}
