// build.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include "args.h"
#include "help.h"
#include "util.h"
#include "vivano.h"
#include "vivado.h"
#include "project.h"

using zst::Result;

namespace vvn
{
	Result<void, std::string> Project::reload_project(Vivado& vivado) const
	{
		vivado.closeProject();
		if(vivado.runCommand("set_part $PART").has_errors())
			return ErrFmt("failed to set part");

		return Ok();
	}

	Result<void, std::string> Project::setup(Vivado& vivado) const
	{
		vivado.setMsgConfig(m_msg_config);

		if(not vivado.partExists(m_part_name))
			vvn::error_and_exit("part '{}' does not exist");

		vvn::log("project part: '{}'", m_part_name);
		vivado.runCommand("set PART \"{}\"", m_part_name);

		if(vivado.streamCommand("set_part $PART").has_errors())
			return ErrFmt("error(s) encountered while setting project part");

		return Ok();
	}

	zst::Result<void, std::string> Project::read_files(Vivado& vivado) const
	{
		vvn::log("reading sources");

		// read all the sources into vivado
		for(auto& src : m_vhdl_sources)
		{
			zpr::println("{}+ {}", vvn::indentStr(1), src);

			// TODO: allow changing library for vhdl
			if(vivado.streamCommand("read_vhdl -vhdl2008 -library xil_defaultLib \"{}\"", src).has_errors())
				return ErrFmt("failed to read '{}'", src);
		}

		for(auto& src : m_verilog_sources)
		{
			zpr::println("{}+ {}", vvn::indentStr(1), src);
			if(vivado.streamCommand("read_verilog \"{}\"", src).has_errors())
				return ErrFmt("failed to read '{}'", src);
		}

		for(auto& src : m_systemverilog_sources)
		{
			zpr::println("{}+ {}", vvn::indentStr(1), src);
			if(vivado.streamCommand("read_verilog -sv \"{}\"", src).has_errors())
				return ErrFmt("failed to read '{}'", src);
		}

		return Ok();
	}

	Result<void, std::string> Project::buildAll(Vivado& vivado, std::span<std::string_view> args) const
	{
		if(auto a = args::checkValidArgs(args, { }); a.has_value())
			return ErrFmt("unsupported option '{}', try '--help'", *a);

		if(args::checkArgument(args, args::HELP))
		{
			help::showBuildHelp();
			return Ok();
		}

		auto timer = util::Timer();

		vvn::log("running full build");
		Result<bool, std::string> result = Ok(true);
		{
			auto _ = LogIndenter();

			result = Result<bool, std::string>(Ok(true)).flatmap([&](bool) {
				return this->synthesise(vivado, args);
			}).flatmap([&](bool did_run) {
				return this->implement(vivado, args, did_run);
			}).flatmap([&](bool did_run) {
				return this->writeBitstream(vivado, args, did_run);
			});
		}

		if(result.ok())
		{
			zpr::println("");
			vvn::log("build finished in {}", timer.print());
			return Ok();
		}
		else
		{
			zpr::println("");
			vvn::error("build failed in {}", timer.print());
			return Err(result.error());
		}
	}
}
