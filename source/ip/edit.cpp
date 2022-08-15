// edit.cpp
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

	Failable<std::string> editUsingGui(const Project& proj, std::string_view ip_name)
	{
		const IpInstance* ip_ptr = nullptr;
		if(ip_ptr = proj.getIpWithName(ip_name), ip_ptr == nullptr)
			return ErrFmt("ip '{}' was not found in the project. try 'ip list'", ip_name);

		const auto& ip = *ip_ptr;
		std::string new_property_cmd {};

		auto e = run_gui_and_wait_for_journal(proj, /* ip_project: */ true, [&](auto& v) -> Failable<std::string> {
			// experimentally, it's faster to source the tcl script and make a new instance
			// rather than importing the xci file... lame
			auto _ = MsgConfigIpSevPusher(proj.getMsgConfig());
			vvn::log("loading ip '{}' into temporary project", ip.name);

			// make the ip/xci directory so vivado won't complain
			auto rel_path = stdfs::relative(proj.getIpOutputsLocation(), proj.getProjectLocation());
			stdfs::create_directories(v.workingDirectory() / rel_path);

			if(v.streamCommand("source \"{}\"", ip.tcl.string()).has_errors())
				return ErrFmt("failed to load ip '{}'", ip.name);

			return Ok();
		}, [&](const auto& lines) -> bool {
				for(auto it = lines.rbegin(); it != lines.rend(); ++it)
				{
					// TODO: this doesn't work
					if(it->starts_with("set_property"))
					{
						new_property_cmd = std::string(*it);
						return true;
					}
				}
				return false;
			});

		if(e.is_err())
			return Err(e.error());

		if(not stdfs::exists(ip.tcl))
			return ErrFmt("ip tcl file '{}' disappeared", ip.tcl.string());

		auto contents = util::readEntireFile(ip.tcl.string());
		auto lines = util::splitString(contents, '\n');

		if(auto x = parse_ip_name_from_property_cmd(new_property_cmd); x != ip.name)
			return ErrFmt("expected ip '{}', found ip '{}' instead!", ip.name, x);

		std::string new_contents {};

		for(size_t i = 0; i < lines.size(); i++)
		{
			// don't overwrite the old command ><
			if(lines[i] == SET_IP_PROPERTIES_CMD_END_MARKER)
			{
				new_contents.append(zpr::sprint("{}\n{}\n", new_property_cmd,
					SET_IP_PROPERTIES_CMD_END_MARKER));
			}
			else
			{
				new_contents.append(lines[i]);
				new_contents.append("\n");
			}
		}

		// rewrite the tcl file
		auto f = fopen(ip.tcl.c_str(), "wb");
		fwrite(new_contents.c_str(), 1, new_contents.size(), f);
		fclose(f);

		// yeet the existing build products
		return cleanIpProducts(proj, ip_name);
	}
}
