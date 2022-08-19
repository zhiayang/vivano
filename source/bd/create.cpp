// create.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include "bd.h"
#include "util.h"
#include "args.h"
#include "vivado.h"
#include "vivano.h"
#include "project.h"

using zst::Failable;

namespace vvn::bd
{
	Failable<std::string> createUsingGui(const Project& proj, std::string_view bd_name)
	{
		vvn::log("creating block design '{}'", bd_name);
		auto _ = LogIndenter();

		if(proj.getBdWithName(bd_name) != nullptr)
			return ErrFmt("block design '{}' already exists", bd_name);

		if(not stdfs::exists(proj.getBdOutputsLocation()))
			stdfs::create_directories(proj.getBdOutputsLocation());

		auto tcl = fopen((proj.getBdLocation() / zpr::sprint("{}.tcl", bd_name)).c_str(), "wb");
		auto d0 = util::Defer([&tcl]() {
			fclose(tcl);
		});

		{
			zpr::fprintln(tcl, "# DO NOT MANUALLY EDIT THIS FILE");
			zpr::fprintln(tcl, "create_bd_design {} -dir bd/", bd_name);
			zpr::fprintln(tcl, "open_bd_design {}", bd_name);
		}

		constexpr auto fake_proj = "xx-temporary-project";
		auto d1 = util::Defer([&]() {
			// if(stdfs::exists(fake_proj))
			// 	stdfs::remove_all(fake_proj);
		});

		{
			// make it 3 deep...
			stdfs::create_directory(fake_proj);
			auto vivado = proj.launchVivado({}, fake_proj, /* source_scripts: */ false, /* run_init: */ true);
			auto foo = zpr::sprint("create_project -force -part {} {} {}", proj.getPartName(), fake_proj, fake_proj);

			if(vivado.streamCommand(foo.c_str()).has_errors())
				return ErrFmt("error creating temporary project");

			// close it again
			vivado.close(/* quiet: */ true);
		}

		auto fake_path = stdfs::path(fake_proj);
		auto journal_path = fake_path / "xx-vivado-journal.jou";

		auto tmp_script_name = "xx-tmp-init.tcl";
		auto tmp_script = fopen((fake_path / tmp_script_name).c_str(), "wb");
		if(tmp_script == nullptr)
			return ErrFmt("failed to create temporary file: {}", strerror(errno));

		auto d2 = util::Defer([&]() {
			stdfs::remove(fake_path / tmp_script_name);
		});


		// write stuff to the temporary script -- create the bd, add the files, then open it.
		// if we start vivado in gui mode but source the script, then the script is run after
		// the gui opens, which means "open_bd_design" will actually open it in the gui.
		zpr::fprintln(tmp_script, "set_part {}", proj.getPartName());
		zpr::fprintln(tmp_script, "create_bd_design {} -dir \"{}/bd/\"", bd_name, fake_proj);

		// we are adding files so that we can use RTL things in the IP integrator
		// TODO: add files so we can use RTL things in the IP integrator

		// now launch one instance to get the gui
		{
			zpr::fprintln(tmp_script, "open_bd_design {}", bd_name);
			fclose(tmp_script);

			auto v = proj.launchVivado({
				"-nolog", "-appjournal",
				"-journal", journal_path.filename().string(),
				"-source", tmp_script_name,
				"-mode", "gui"
			}, fake_path, /* source_scripts: */ false, /* run_init: */ false);

			vvn::log("starting gui");
			vvn::log("close the block design to finish editing");

			auto e = waitForJournalOnGui(proj, v, journal_path, [](auto lines) -> bool {
				return (lines.size() > 0 && lines.back().starts_with("close_bd_design"));
			});

			if(e.is_err())
				return e;
		}

		// launch the next instance to write the stupid tcl script to disk
		{
			auto v = proj.launchVivado({}, fake_path, /* source_scripts: */ false, /* run_init: */ false);
			vvn::log("exporting block design");

			v.streamCommand("set_part {}", proj.getPartName());

			auto bd_file = (fake_path / "bd" / bd_name / zpr::sprint("{}.bd", bd_name));
			if(v.streamCommand("read_bd {}", bd_file.string()).has_errors())
				return ErrFmt("failed to read block design");

			if(v.streamCommand("open_bd_design [get_files {}]", bd_file.string()).has_errors())
				return ErrFmt("failed to open block design");

			// don't yeet even if it fails validation
			vvn::log("validating block design");
			v.streamCommand("validate_bd_design");

			if(v.streamCommand("write_bd_tcl xx-export-bd.tcl").has_errors())
				return ErrFmt("failed to export block design tcl");

			v.close();
		}

		// ok, now we get the exported script
		auto exported_tcl = util::readEntireFile((fake_path / "xx-export-bd.tcl").string());
		auto lines = util::splitString(exported_tcl, '\n');

		// the actual block design commands start after 'current_bd_instance $parentObj'
		// and end before '# Restore current instance'

		bool flag = false;
		for(auto line : lines)
		{
			line = util::trim(line);
			if(line.starts_with("current_bd_instance $parentObj"))
			{
				flag = true;
			}
			else if(line.starts_with("# Restore current instance"))
			{
				break;
			}
			else if(flag)
			{
				fwrite(line.data(), 1, line.size(), tcl);
				fputs("\n", tcl);
			}
		}

		fprintf(tcl, "close_bd_design\n");

		vvn::log("wrote {}.tcl", (proj.getBdLocation() / bd_name).string());
		return Ok();
	}
}
