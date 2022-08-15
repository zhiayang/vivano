// synth.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <filesystem>

#include "ip.h"
#include "args.h"
#include "help.h"
#include "vivano.h"
#include "vivado.h"
#include "project.h"
#include "msgconfig.h"

using zst::Result;
namespace stdfs = std::filesystem;

namespace vvn
{
	bool Project::should_resynthesise(Vivado& vivado) const
	{
		(void) vivado;

		auto dcp_file = m_build_folder / m_synthesised_dcp_name;
		if(not stdfs::exists(dcp_file))
			return true;

		auto dcp_time = stdfs::last_write_time(dcp_file);
		auto check_files = [&](const auto& list) -> bool {
			for(const auto& x : list)
			{
				if(stdfs::last_write_time(x) > dcp_time)
					return true;
			}
			return false;
		};

		auto rebuild_ips = std::any_of(m_ip_instances.begin(), m_ip_instances.end(), [](auto& ip) {
			return ip.shouldRegenerate() || ip.shouldResynthesise();
		});

		return rebuild_ips
			|| check_files(m_synth_constraints)
			|| check_files(m_verilog_sources)
			|| check_files(m_vhdl_sources)
			|| check_files(m_systemverilog_sources);
	}


	Result<bool, std::string> Project::synthesise(Vivado& vivado, std::span<std::string_view> args) const
	{
		if(auto a = args::checkValidArgs(args, { args::FORCE_BUILD }); a.has_value())
			return ErrFmt("unsupported option '{}', try '--help'", *a);

		auto force_build = args::check(args, args::FORCE_BUILD);
		if(not this->should_resynthesise(vivado) && not force_build)
		{
			vvn::log("synthesis up to date");
			return Ok(true);
		}

		zpr::println("");
		vvn::log("performing synthesis");

		this->reload_project(vivado);
		this->read_files(vivado);

		auto timer = util::Timer();
		auto _ = vvn::LogIndenter();

		// read synthesis constraints
		if(m_synth_constraints.size() > 0)
		{
			vvn::log("reading constraints");
			auto _ = vvn::LogIndenter();

			for(auto& xdc : m_synth_constraints)
			{
				if(not vivado.haveConstraintFile(xdc))
				{
					zpr::println("{}+ {}", vvn::indentStr(), xdc);
					if(vivado.addConstraintFile(xdc).has_errors())
						return ErrFmt("failed to read '{}'", xdc);
				}
			}
		}

		vvn::log("loading ips");
		if(auto e = ip::synthesiseIpProducts(vivado, *this); e.is_err())
			return Err(e.error());

		// synthesise
		vvn::log("running synth_design");
		if(vivado.streamCommand("synth_design -top {} -verbose -assert", m_top_module).has_errors())
			return ErrFmt("synthesis failed");

		auto dcp_file = m_build_folder / m_synthesised_dcp_name;
		vvn::log("writing checkpoint '{}'", dcp_file.string());
		if(vivado.streamCommand("write_checkpoint -force \"{}\"", dcp_file.string()).has_errors())
			return ErrFmt("failed to write post-synthesis checkpoint");

		vvn::log("synthesis finished in {}", timer.print());
		return Ok(false);
	}
}
