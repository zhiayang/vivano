// prettyprinter.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <vector>
#include <string>
#include <sstream>
#include <string_view>


#if defined(_WIN32)

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#else

#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#endif

namespace util
{
	static std::vector<std::string_view> split_words(const std::string& s)
	{
		std::vector<std::string_view> ret;

		size_t word_start = 0;
		for(size_t i = 0; i < s.size(); i++)
		{
			if(s[i] == ' ')
			{
				ret.push_back(std::string_view(s.c_str() + word_start, i - word_start));
				word_start = i + 1;
			}
			else if(s[i] == '-')
			{
				ret.push_back(std::string_view(s.c_str() + word_start, i - word_start + 1));
				word_start = i + 1;
			}
		}

		ret.push_back(std::string_view(s.c_str() + word_start));
		return ret;
	}

	size_t getTerminalWidth()
	{
		#ifdef _WIN32
			CONSOLE_SCREEN_BUFFER_INFO csbi;
			GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
			return csbi.srWindow.Right - csbi.srWindow.Left + 1;
		#else

			#ifdef _MSC_VER
			#else
				#pragma GCC diagnostic push
				#pragma GCC diagnostic ignored "-Wold-style-cast"
			#endif

			struct winsize w;
			ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
			return w.ws_col;

			#ifdef _MSC_VER
			#else
				#pragma GCC diagnostic pop
			#endif

		#endif
	}


	std::string prettyFormatTextBlock(const std::string& block, const char* leftMargin, const char* rightMargin,
		bool no_margin_on_first_line)
	{
		auto tw = getTerminalWidth();
		tw = std::min(tw, tw - strlen(leftMargin) - strlen(rightMargin));

		size_t lines = 1;

		size_t remaining = tw;

		// sighs.
		auto ss = std::stringstream();
		if(not no_margin_on_first_line)
			ss << leftMargin;

		// each "line" is actually a paragraph. we want to be nice, so pad on the right by a few spaces
		// and hyphenate split words.

		// first split into words
		auto words = split_words(block);
		for(const auto& word : words)
		{
			auto len = word.size();
			if(remaining >= len)
			{
				ss << word << (word.back() != '-' ? " " : "");

				remaining -= len;

				if(remaining > 0)
				{
					remaining -= 1;
				}
				else
				{
					ss << "\n" << leftMargin;
					lines++;

					remaining = tw;
				}
			}
			else if(remaining < 3 || len < 5)
			{
				// for anything less than 5 chars, put it on the next line -- don't hyphenate.
				ss << "\n" << leftMargin << word << (word.back() != '-' ? " " : "");

				remaining = tw - (len + 1);
				lines++;
			}
			else
			{
				auto thisline = remaining - 2;

				// if we end up making a fragment 3 letters or shorter,
				// push it to the next line instead.
				if(std::min(word.size(), thisline ) <= 3)
				{
					thisline = 0;
					ss << "\n" << leftMargin << word << (word.back() != '-' ? " " : "");
				}
				else
				{
					// split it.
					ss << word.substr(0, thisline) << "-" << "\n";
					ss << leftMargin << word.substr(thisline) << " ";
				}

				remaining = tw - word.substr(thisline).size();

				lines++;
			}
		}

		return ss.str();
	}
}
