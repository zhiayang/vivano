// args.h
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <span>
#include <utility>
#include <optional>
#include <string_view>

#include "util.h"


namespace vvn
{
	namespace args
	{
		using Arg = std::pair<std::string_view, std::string_view>;

		bool check(std::span<std::string_view> args, const Arg& arg);
		std::optional<std::string_view> checkValidArgs(std::span<std::string_view> args, const util::hashset<Arg>& known);

		static constexpr Arg HELP           = { "-h", "--help" };
		static constexpr Arg FORCE_BUILD    = { "-f", "--force" };
		static constexpr Arg USE_STALE      = { "-s", "--stale" };
		static constexpr Arg ALL            = { "-a", "--all" };
		static constexpr Arg IPS            = { "-i", "--ips" };
	}

	static constexpr std::string_view CMD_HELP          = "help";
	static constexpr std::string_view CMD_MANUAL        = "man";
	static constexpr std::string_view CMD_VERSION       = "version";
	static constexpr std::string_view CMD_INIT          = "init";
	static constexpr std::string_view CMD_CLEAN         = "clean";
	static constexpr std::string_view CMD_CHECK         = "check";
	static constexpr std::string_view CMD_BUILD         = "build";
	static constexpr std::string_view CMD_SYNTH         = "synth";
	static constexpr std::string_view CMD_IMPL          = "impl";
	static constexpr std::string_view CMD_BITSREAM      = "bitstream";

	static constexpr std::string_view CMD_IP            = "ip";
	static constexpr std::string_view CMD_IP_EDIT       = "edit";
	static constexpr std::string_view CMD_IP_LIST       = "list";
	static constexpr std::string_view CMD_IP_BUILD      = "build";
	static constexpr std::string_view CMD_IP_CLEAN      = "clean";
	static constexpr std::string_view CMD_IP_CREATE     = "create";
	static constexpr std::string_view CMD_IP_DELETE     = "delete";
}
