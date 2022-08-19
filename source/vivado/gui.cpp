// gui.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <chrono>
#include <thread>

#include "util.h"
#include "vivado.h"
#include "vivano.h"
#include "project.h"
#include "progressbar.h"

using zst::Failable;

namespace vvn
{
	Failable<std::string> waitForJournalOnGui(const Project& proj, Vivado& vivado,
		stdfs::path journal_path, std::function<bool (std::span<std::string_view>)> callback)
	{
		// touch the journal
		fclose(fopen(journal_path.c_str(), "wb"));

		std::string cmds {};
		std::vector<std::string_view> lines {};

		auto last_update = std::chrono::steady_clock::now();
		auto journal_file = fopen(journal_path.c_str(), "rb");
		if(journal_file == nullptr)
			return ErrFmt("couldn't open journal: {} ({})", strerror(errno), errno);

		auto d3 = util::Defer([&]() {
			fclose(journal_file);
			if(stdfs::exists(journal_path))
				stdfs::remove(journal_path);
		});

		vvn::log("waiting for user action");
		auto pbar = util::ProgressBar(static_cast<size_t>(2 * (1 + getLogIndent())), 30);
		pbar.draw();

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
				return ErrFmt("vivado exited prematurely");
			}
		}

		pbar.clear();
		return Ok();
	}

	Failable<std::string> runGuiAndWaitForJournal(const Project& proj, bool ip_project,
		std::function<Failable<std::string> (Vivado&)> run_setup,
		std::function<bool (std::span<std::string_view>)> callback)
	{
		constexpr auto fake_proj = "xx-temporary-project";
		auto d1 = util::Defer([&]() {
			if(stdfs::exists(fake_proj))
				stdfs::remove_all(fake_proj);
		});

		auto journal_name = "xx-vivado-journal.jou";

		// use project flow so we can put it in its own folder, and yeet the whole thing at once
		{
			// make it 3 deep...
			stdfs::create_directory(fake_proj);
			auto vivado = proj.launchVivado({}, fake_proj, /* source_scripts: */ false, /* run_init: */ true);
			auto foo = zpr::sprint("create_project {} -force -part {} {} {}", ip_project ? "-ip" : "",
				proj.getPartName(), fake_proj, fake_proj);

			if(vivado.streamCommand(foo.c_str()).has_errors())
				return ErrFmt("error creating temporary project");
#if 0
			// run the setup in a fresh instance of vivado in the fake directory
			vivado.close(/* quiet: */ true);

			vvn::log("running pre-setup");
			vivado.relaunchWithArgs({
				"-mode", "tcl",
				"-nolog", "-nojournal",
				zpr::sprint("{}/{}.xpr", fake_proj, fake_proj)
			}, stdfs::path(fake_proj));
#endif

			vvn::log("running pre-setup");
			if(auto e = run_setup(vivado); e.is_err())
				return Err(e.error());

			// close it again
			vivado.close(/* quiet: */ true);
		}

		// open it again
		vvn::log("starting gui");

		auto vivado = proj.launchVivado({
			"-mode", "gui",
			"-nolog", "-appjournal",
			"-journal", journal_name,
			zpr::sprint("{}/{}.xpr", fake_proj, fake_proj)
		}, stdfs::path(fake_proj), /* source_scripts: */ false, /* run_init: */ false);

		return waitForJournalOnGui(proj, vivado, stdfs::path(fake_proj) / journal_name, std::move(callback));
	}
}
