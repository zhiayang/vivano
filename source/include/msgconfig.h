// msgconfig.h
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>
#include <vector>
#include <filesystem>

#include <zpr.h>

#include "util.h"

namespace vvn
{
	struct MsgConfig
	{
		int min_severity = 0;

		int min_ip_severity = 0;
		bool print_message_ids = false;

		std::filesystem::path project_path;

		util::hashmap<std::string, int> severity_overrides;
		util::hashset<std::string> suppressions;

		mutable int ip_nesting_depth = 0;
	};

	struct MsgConfigIpSevPusher
	{
		MsgConfigIpSevPusher(const MsgConfig& msg_cfg) : m_msg_cfg(msg_cfg)
		{
			m_msg_cfg.ip_nesting_depth++;
		}

		~MsgConfigIpSevPusher()
		{
			m_msg_cfg.ip_nesting_depth--;
		}

	private:
		const MsgConfig& m_msg_cfg;
	};
}
