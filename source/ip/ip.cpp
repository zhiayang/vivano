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
    edit            edit an existing IP instance
    list            list IP instances in the project

These commands launch the Vivado GUI; create or customise the
IP using the IP customisation dialog. Viavdo will automatically
close when the IP operation is completed.
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
		else if(args[0] == CMD_IP_LIST)
		{
			puts("list of ips:");
			return print_ip_list(proj);
		}
		else
		{
			puts(help_str);
			return ErrFmt("unknown ip subcommand '{}'", args[0]);
		}

		return Ok();
	}






	Failable<std::string> run_gui_and_wait_for_journal(const Project& proj, bool ip_project,
		std::function<Failable<std::string> (Vivado&)> run_setup,
		std::function<bool (std::span<std::string_view>)> callback)
	{
		constexpr auto fake_proj = "xx-temporary-project";
		auto d1 = util::Defer([&]() {
			if(stdfs::exists(fake_proj))
				stdfs::remove_all(fake_proj);
		});

		auto vivado = proj.launchVivado();
		auto journal_name = "xx-vivado-journal.jou";

		// use project flow so we can put it in its own folder, and yeet the whole thing at once
		{
			// make it 3 deep...
			stdfs::create_directory(fake_proj);
			auto foo = zpr::sprint("create_project {} -force -part {} {} {}/{}", ip_project ? "-ip" : "",
				proj.getPartName(), fake_proj, fake_proj, fake_proj);

			if(vivado.streamCommand(foo.c_str()).has_errors())
				return ErrFmt("error creating temporary project");

			// run the setup in a fresh instance of vivado in the fake directory
			vivado.close(/* quiet: */ true);

			vvn::log("running pre-setup");
			vivado.relaunchWithArgs({
				"-mode", "tcl",
				"-nolog", "-nojournal",
				zpr::sprint("{}/{}.xpr", fake_proj, fake_proj)
			}, stdfs::path(fake_proj));

			if(auto e = run_setup(vivado); e.is_err())
				return Err(e.error());

			// close it again
			vivado.close(/* quiet: */ true);

			// touch the file
			fclose(fopen((stdfs::path(fake_proj) / journal_name).c_str(), "wb"));

			// open it again
			vvn::log("starting gui");
			vivado.relaunchWithArgs({
				"-mode", "gui",
				"-nolog", "-appjournal",
				"-journal", journal_name,
				zpr::sprint("{}/{}.xpr", fake_proj, fake_proj)
			}, stdfs::path(fake_proj));
		}

		auto pbar = util::ProgressBar(static_cast<size_t>(2 * (1 + getLogIndent())), 30);

		std::string cmds {};
		std::vector<std::string_view> lines {};

		auto last_update = std::chrono::steady_clock::now();
		auto journal_file = fopen((stdfs::path(fake_proj) / journal_name).c_str(), "rb");
		if(journal_file == nullptr)
			return ErrFmt("couldn't open journal: {} ({})", strerror(errno), errno);

		auto d3 = util::Defer([&]() {
			fclose(journal_file);
			if(auto j = stdfs::path(fake_proj) / journal_name; stdfs::exists(j))
				stdfs::remove(j);
		});

		vvn::log("waiting for user action");

		while(true)
		{
			static constexpr auto REFRESH_INTERVAL = util::ProgressBar::DEFAULT_INTERVAL / 2;

			char buf[4096] {};
			auto current_ofs = static_cast<size_t>(ftell(journal_file));
			auto did_read = fread(&buf[0], 1, 4096, journal_file);

			if(did_read > 0)
			{
				cmds.append(&buf[0], did_read);
				auto lines = util::splitString(cmds, '\n');

				if(lines.size() > 0 && callback(lines))
					break;
			}
			else if(feof(journal_file))
			{
				fseek(journal_file, static_cast<long>(current_ofs + did_read), SEEK_SET);
				std::this_thread::sleep_for(REFRESH_INTERVAL);
			}

			using namespace std::chrono_literals;
			if(auto now = std::chrono::steady_clock::now(); now - last_update > util::ProgressBar::DEFAULT_INTERVAL)
			{
				last_update = now;
				pbar.update();
				pbar.draw();
			}

			if(not vivado.alive())
			{
				vvn::warn("vivado exited unexpectedly, cancelling");
				return Ok();
			}
		}

		pbar.clear();
		return Ok();
	}
}
