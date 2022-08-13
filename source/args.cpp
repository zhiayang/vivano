// args.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <span>
#include <algorithm>
#include <string_view>
#include <initializer_list>

#include "args.h"
#include "util.h"

namespace vvn::args
{
	std::optional<std::string_view> checkValidArgs(std::span<std::string_view> args, const util::hashset<Arg>& known)
	{
		util::hashset<std::string_view> knowns { HELP.first, HELP.second };
		for(auto& a : known)
		{
			knowns.insert(a.first);
			knowns.insert(a.second);
		}

		for(auto& arg : args)
		{
			// help is always supported
			if(not knowns.contains(arg))
				return arg;
		}

		return std::nullopt;
	}

	bool checkArgument(std::span<std::string_view> args, const Arg& arg)
	{
		for(auto& a : args)
		{
			if(a == arg.first || a == arg.second)
				return true;
		}

		return false;
	}
}
