// create_ip.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <cerrno>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>
#include <string_view>

#include "args.h"
#include "util.h"
#include "help.h"
#include "vivano.h"
#include "vivado.h"
#include "project.h"
#include "progressbar.h"

#if defined(_WIN32)
#else
   #include <sys/types.h>
   #include <sys/stat.h>
#endif

#include <zprocpipe.h>

namespace zpp = zprocpipe;

namespace vvn
{
	static std::string parse_module_name(const std::string& create_ip_cmd)
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

	static std::string rewrite_module_directory(const Project& proj, const std::string& create_ip_cmd)
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

	zst::Result<void, std::string> createIpWithVivadoGUI(const Project& proj, std::span<std::string_view> args)
	{
		using namespace std::chrono_literals;

		if(auto a = args::checkValidArgs(args, {}); a.has_value())
			return ErrFmt("unsupported option '{}', try '--help'", *a);

		if(args::check(args, args::HELP))
		{
			help::showCreateIpHelp();
			return Ok();
		}

		auto vivado = proj.launchVivado();

		constexpr auto fake_proj = "temporary-ip-project";
		auto d1 = util::Defer([&]() {
			if(stdfs::exists(fake_proj))
				stdfs::remove_all(fake_proj);
		});

		auto journal_name = "vivado-aoeu.jou";
		// use project flow so we can put it in its own folder, and yeet the whole thing at once
		{
			// make it 3 deep...
			stdfs::create_directory(fake_proj);
			auto foo = zpr::sprint("create_project -ip -force -part {} {} {}/{}", proj.getPartName(),
				fake_proj, fake_proj, fake_proj);

			if(vivado.streamCommand(foo.c_str()).has_errors())
				return ErrFmt("error creating temporary project");

			// touch the file
			fclose(fopen((stdfs::path(fake_proj) / journal_name).c_str(), "wb"));

			// then, restart vivado and just launch it in GUI mode with the path to the project.
			// there are apparently some issues; open_project followed by start_gui doesn't want to actually
			// open the project properly in the gui.
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

		vvn::log("waiting for ip customisation");

		std::string create_xci_cmd;
		std::string customise_ip_cmd;

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

				if(lines.size() > 0)
				{
					bool flag = false;
					for(auto it = lines.rbegin(); it != lines.rend(); ++it)
					{
						if(it->starts_with("generate_target"))
							flag = true;
						else if(flag && it->starts_with("create_ip"))
							create_xci_cmd = std::string(*it);
						else if(flag && it->starts_with("set_property"))
							customise_ip_cmd = std::string(*it);
					}

					if(flag)
						break;
				}
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
				vvn::warn("vivado exited before printing IP commands, cancelling");
				return Ok();
			}
		}

		pbar.clear();

		std::this_thread::sleep_for(1s);
		vivado.forceClose();

		create_xci_cmd = rewrite_module_directory(proj, create_xci_cmd);
		auto ip_name = parse_module_name(create_xci_cmd);
		vvn::log("created ip '{}'", ip_name);

		auto tcl_file = proj.getIpLocation() / zpr::sprint("{}.tcl", ip_name);
		if(stdfs::exists(tcl_file))
			return ErrFmt("file '{}.tcl' already exists, not overwriting", tcl_file.string());

		auto f = fopen(tcl_file.c_str(), "wb");
		fprintf(f, "%s\n", create_xci_cmd.c_str());
		fprintf(f, "%s\n", customise_ip_cmd.c_str());
		fclose(f);

		vvn::log("created tcl script '{}'", tcl_file.string());
		return Ok();
	}
}
