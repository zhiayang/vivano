// synth.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <filesystem>

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
	static bool should_regenerate_ip(const Project::IpInst& ip)
	{
		return (not stdfs::exists(ip.xci))
			|| (stdfs::last_write_time(ip.tcl) > stdfs::last_write_time(ip.xci));
	}

	static bool should_rebuild_ip(const Project::IpInst& ip)
	{
		// all IPs are synthesised out of context. to see whether we need
		// to re-synthesise the IP, check whether:
		// (a) the TCL script is newer than either the XCI or the DCP
		// (b) the XCI is newer than the DCP

		auto dcp_file = ip.xci;
		dcp_file.replace_extension(".dcp");

		return should_regenerate_ip(ip)
			|| (not stdfs::exists(dcp_file))
			|| (stdfs::last_write_time(ip.xci) > stdfs::last_write_time(dcp_file));
	}

	static Result<void, std::string> regenerate_ip_instance(Vivado& vivado, const Project::IpInst& ip, const MsgConfig& msg_cfg)
	{
		auto uwu = MsgConfigIpSevPusher(msg_cfg);

		vvn::log("regenerating ip '{}'", ip.name);
		if(not stdfs::exists(ip.xci.parent_path().parent_path()))
			stdfs::create_directories(ip.xci.parent_path().parent_path());

		if(stdfs::exists(ip.xci.parent_path()))
			stdfs::remove_all(ip.xci.parent_path());

		auto a = vivado.streamCommand("source {}", ip.tcl.string());
		if(a.has_errors())
			return ErrFmt("failed to run '{}'", ip.tcl.string());

		auto _ = vvn::LogIndenter();
		vvn::log("suppressed {} info(s), {} warning(s)", a.infos.size(), a.warnings.size());

		return Ok();
	}

	static Result<void, std::string> synthesise_ip_instance(Vivado& vivado, const Project::IpInst& ip, const MsgConfig& msg_cfg)
	{
		auto uwu = MsgConfigIpSevPusher(msg_cfg);

		vvn::log("rebuilding ip '{}'", ip.name);
		auto b = vivado.streamCommand("synth_ip [get_ips {}]", ip.name);
		if(b.has_errors())
			return ErrFmt("synthesis of '{}' failed", ip.name);

		auto _ = vvn::LogIndenter();
		vvn::log("suppressed {} info(s), {} warning(s)", b.infos.size(), b.warnings.size());

		return Ok();
	}


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
			return should_regenerate_ip(ip) || should_rebuild_ip(ip);
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

		if(args::checkArgument(args, args::HELP))
		{
			help::showSynthHelp();
			return Ok(false);
		}

		auto force_build = args::checkArgument(args, args::FORCE_BUILD);
		if(not this->should_resynthesise(vivado) && not force_build)
		{
			vvn::log("synthesis up to date");
			return Ok(false);
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

		// synthesise any IP designs as out of context
		std::vector<const Project::IpInst*> rebuild_ips {};

		vvn::log("loading ips");
		for(auto& ip : m_ip_instances)
		{
			zpr::println("{}+ {} ({})", vvn::indentStr(1), ip.name, stdfs::relative(ip.xci, m_location).string());
			if(should_regenerate_ip(ip))
			{
				if(regenerate_ip_instance(vivado, ip, m_msg_config).is_err())
					return ErrFmt("failed to regenerate ip '{}'", ip.name);
			}
			else
			{
				// if we had to regenerate the IP, then `create_ip` already puts it in
				// the current project, so there's no need to re-read it (in fact, we can't)
				if(vivado.streamCommand("read_ip \"{}\"", ip.xci.string()).has_errors())
					return ErrFmt("failed to read ip '{}'", ip.name);
			}

			if(should_rebuild_ip(ip))
				rebuild_ips.push_back(&ip);
		}

		for(auto* ip : rebuild_ips)
		{
			if(auto e = synthesise_ip_instance(vivado, *ip, m_msg_config); e.is_err())
				return Err(e.error());
		}

		// synthesise
		vvn::log("running synth_design");
		if(vivado.streamCommand("synth_design -top {} -verbose -assert", m_top_module).has_errors())
			return ErrFmt("synthesis failed");

		auto dcp_file = m_build_folder / m_synthesised_dcp_name;
		vvn::log("writing checkpoint '{}'", dcp_file.string());
		if(vivado.streamCommand("write_checkpoint -force \"{}\"", dcp_file.string()).has_errors())
			return ErrFmt("failed to write post-synthesis checkpoint");

		vvn::log("synthesis finished in {}", timer.print());
		return Ok(true);
	}
}
