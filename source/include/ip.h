// ip.h
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <span>
#include <string>
#include <functional>
#include <string_view>

#include <zpr.h>
#include <zst.h>

#include "util.h"

namespace vvn
{
	struct Vivado;
	struct Project;

	struct IpInstance;
}

namespace vvn::ip
{
	zst::Failable<std::string> createUsingGui(const Project& proj);
	zst::Failable<std::string> editUsingGui(const Project& proj, std::string_view ip_name);

	zst::Failable<std::string> deleteIp(const Project& proj, std::string_view ip_name);
	zst::Failable<std::string> cleanIpProducts(const Project& proj, std::string_view ip_name);

	zst::Failable<std::string> synthesiseIpProducts(const Project& proj, const util::hashset<std::string_view>& ip_names);
	zst::Failable<std::string> synthesiseIpProducts(Vivado& vivado, const Project& proj);

	zst::Failable<std::string> runIpCommand(const Project& proj, std::span<std::string_view> args);

	static constexpr const char* CREATE_IP_CMD_START_MARKER         = "# CREATE_IP_CMD_START";
	static constexpr const char* CREATE_IP_CMD_END_MARKER           = "# CREATE_IP_CMD_END";
	static constexpr const char* SET_IP_PROPERTIES_CMD_START_MARKER = "# SET_IP_PROPERTIES_CMD_START";
	static constexpr const char* SET_IP_PROPERTIES_CMD_END_MARKER   = "# SET_IP_PROPERTIES_CMD_END";

	std::string parse_module_name(const std::string& create_ip_cmd);
	std::string parse_ip_name_from_property_cmd(std::string_view property_cmd);
	std::string rewrite_module_directory(const Project& proj, const std::string& create_ip_cmd);
}
