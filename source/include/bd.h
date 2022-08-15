// bd.h
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <span>
#include <string>
#include <string_view>

#include <zst.h>

namespace vvn
{
	struct Project;
}

namespace vvn::bd
{
	zst::Failable<std::string> createUsingGui(const Project& proj, std::string_view bd_name);

	zst::Failable<std::string> runBdCommand(const Project& proj, std::span<std::string_view> args);
}
