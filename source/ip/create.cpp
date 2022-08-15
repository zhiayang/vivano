// create.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <cerrno>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <chrono>
#include <thread>
#include <vector>
#include <functional>
#include <filesystem>
#include <string_view>

#include "ip.h"
#include "args.h"
#include "util.h"
#include "help.h"
#include "vivano.h"
#include "vivado.h"
#include "project.h"
#include "msgconfig.h"
#include "progressbar.h"

#if defined(_WIN32)
#else
   #include <sys/types.h>
   #include <sys/stat.h>
#endif

#include <zprocpipe.h>

namespace zpp = zprocpipe;

namespace vvn::ip
{
	using zst::Result;
	using zst::Failable;

	using namespace std::chrono_literals;

	Failable<std::string> createUsingGui(const Project& proj)
	{
		std::string create_xci_cmd;
		std::string customise_ip_cmd;

		auto e = run_gui_and_wait_for_journal(proj, /* ip_project: */ true, [](auto&) {
			return Ok();
		}, [&](const auto& lines) -> bool {
				int state = 0;
				for(auto it = lines.rbegin(); it != lines.rend(); ++it)
				{
					// we probably don't want to wait for generate_target
					if(state == 1 && it->starts_with("create_ip"))
					{
						create_xci_cmd = std::string(*it);
						state = 2;
						break;
					}
					else if(it->starts_with("set_property"))
					{
						customise_ip_cmd = std::string(*it);
						state = 1;
					}
				}

				return (state == 2);
			});

		if(e.is_err())
			return Err(e.error());

		create_xci_cmd = rewrite_module_directory(proj, create_xci_cmd);
		auto ip_name = parse_module_name(create_xci_cmd);
		vvn::log("created ip '{}'", ip_name);

		auto tcl_file = proj.getIpLocation() / zpr::sprint("{}.tcl", ip_name);
		if(stdfs::exists(tcl_file))
			return ErrFmt("file '{}.tcl' already exists, not overwriting", tcl_file.string());

		auto f = fopen(tcl_file.c_str(), "wb");
		fprintf(f, "# DO NOT MANUALLY EDIT THIS FILE\n");
		fprintf(f, "%s\n", CREATE_IP_CMD_START_MARKER);
		fprintf(f, "%s\n", create_xci_cmd.c_str());
		fprintf(f, "%s\n", CREATE_IP_CMD_END_MARKER);
		fprintf(f, "%s\n", SET_IP_PROPERTIES_CMD_START_MARKER);
		fprintf(f, "%s\n", customise_ip_cmd.c_str());
		fprintf(f, "%s\n", SET_IP_PROPERTIES_CMD_END_MARKER);
		fclose(f);

		vvn::log("created tcl script '{}'", tcl_file.string());
		return Ok();
	}


	std::string parse_module_name(const std::string& create_ip_cmd)
	{
		auto words = util::splitString(create_ip_cmd, ' ');
		for(size_t i = 0; i < words.size(); i++)
		{
			if(words[i] == "-module_name")
			{
				assert(i + 1 < words.size());
				return std::string(words[i + 1]);
			}
		}

		vvn::error_and_exit("failed to parse IP module name");
	}

	std::string parse_ip_name_from_property_cmd(std::string_view property_cmd)
	{
		if(not property_cmd.ends_with("]"))
			vvn::error_and_exit("failed to parse IP name");

		property_cmd.remove_suffix(1);
		auto i = property_cmd.find("[get_ips ");
		if(i == std::string::npos)
			vvn::error_and_exit("failed to parse IP name");

		property_cmd.remove_prefix(i);
		property_cmd.remove_prefix(strlen("[get_ips "));
		return std::string(property_cmd);
	}

	std::string rewrite_module_directory(const Project& proj, const std::string& create_ip_cmd)
	{
		std::string ret {};

		auto words = util::splitString(create_ip_cmd, ' ');
		for(size_t i = 0; i < words.size();)
		{
			ret += words[i];
			ret += " ";

			if(words[i] == "-dir")
			{
				assert(i + 1 < words.size());
				while(++i < words.size())
				{
					if(words[i].starts_with('-'))
						break;
				}

				auto xci_dir = proj.getIpLocation() / proj.getIpOutputsLocation();
				ret += zpr::sprint("{{{}}} ", stdfs::relative(xci_dir, proj.getProjectLocation()).string());
			}
			else
			{
				i++;
			}
		}

		return ret;
	}
}
