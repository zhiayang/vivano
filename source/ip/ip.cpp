// ip.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <cstdio>
#include <chrono>
#include <thread>
#include <algorithm>

#include "ip.h"
#include "args.h"
#include "util.h"
#include "vivado.h"
#include "vivano.h"
#include "project.h"
#include "progressbar.h"

using zst::Ok;
using zst::Err;
using zst::ErrFmt;
using zst::Result;
using zst::Failable;

namespace vvn::ip
{
	Failable<std::string> runIpCommand(const Project& proj, std::span<std::string_view> args)
	{
		auto help_str = R"(
usage: vvn ip [subcommand] [options]

Subcommands:
    build           build out-of-context products for IPs
    create          create a new IP customisation
    delete          delete an IP instance
    clean           clean IP output products
    edit            edit an existing IP instance
    list            list IP instances in the project

Creating and editing an IP instance will launch the Vivado GUI; create an IP
using the IP catalog, or edit an existing one using the sources list on the
left. Viavdo will automatically close when the IP operation is completed.
)";

		auto print_ip_list = [](const Project& proj) -> auto {
			auto ips = proj.getIpInstances();
			std::sort(ips.begin(), ips.end(), [](auto& a, auto& b) {
				return a.name < b.name;
			});

			for(auto& ip : ips)
				zpr::println("  * {}", ip.name);

			puts("");
			return Ok();
		};


		if(args.empty())
		{
			puts(help_str);
			return Ok();
		}
		else if(args[0] == CMD_IP_LIST)
		{
			puts("list of ips:");
			return print_ip_list(proj);
		}
		else if(args[0] == CMD_IP_CREATE)
		{
			if(args.size() != 1 || args::check(args, args::HELP))
			{
				zpr::println(R"(
usage: vvn ip create

Takes no options, create a new IP instance. Specify the name
of the new IP using the GUI in Vivado.
				)");
				return Ok();
			}

			return ip::createUsingGui(proj);
		}
		else if(args[0] == CMD_IP_BUILD)
		{
			if(args::check(args, args::HELP))
			{
				zpr::println(R"(
usage: vvn ip build [ip_names...]

Regenerates the output products and runs synthesis for out-of-context
IP instances. If no IP names are specified, builds all IPs by default.
				)");
				return Ok();
			}


			if(args.size() > 1)
			{
				util::hashset<std::string_view> selected_ips {};
				for(auto& name : args.subspan(1))
				{
					if(proj.getIpWithName(name) != nullptr)
						selected_ips.insert(name);
					else
						return ErrFmt("ip '{}' does not exist, try 'ip list'", name);
				}

				return ip::synthesiseIpProducts(proj, selected_ips);
			}
			else
			{
				return ip::synthesiseIpProducts(proj, {});
			}
		}
		else if(args[0] == CMD_IP_EDIT)
		{
			if(args.size() != 2 || args::check(args, args::HELP))
			{
				zpr::println(R"(
usage: vvn ip edit <ip name>

The name of an existing IP is required, which is one of:)");

				return print_ip_list(proj);
			}

			return ip::editUsingGui(proj, args[1]);
		}
		else if(args[0] == CMD_IP_DELETE)
		{
			if(args.size() != 2 || args::check(args, args::HELP))
			{
				zpr::println(R"(
usage: vvn ip delete <ip name>

The name of an existing IP is required, which is one of:)");

				return print_ip_list(proj);
			}

			return ip::deleteIp(proj, args[1]);
		}
		else if(args[0] == CMD_IP_CLEAN)
		{
			if(args.size() != 2 || args::check(args, args::HELP))
			{
				zpr::println(R"(
usage: vvn ip clean <ip name>

The name of an existing IP is required, which is one of:)");

				return print_ip_list(proj);
			}

			return ip::cleanIpProducts(proj, args[1]);
		}
		else
		{
			puts(help_str);
			return ErrFmt("unknown ip subcommand '{}'", args[0]);
		}

		return Ok();
	}
}
