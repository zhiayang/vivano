// synth.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include "ip.h"
#include "util.h"
#include "vivado.h"
#include "vivano.h"
#include "project.h"

using zst::Result;
using zst::Failable;

namespace vvn
{
	bool IpInstance::shouldRegenerate() const
	{
		return (not stdfs::exists(this->xci))
			|| (stdfs::last_write_time(this->tcl) > stdfs::last_write_time(this->xci));
	}

	bool IpInstance::shouldResynthesise() const
	{
		// to see whether we need to re-synthesise the IP, check whether:
		// (a) the TCL script is newer than either the XCI or the DCP
		// (b) the XCI is newer than the DCP

		auto dcp_file = this->xci;
		dcp_file.replace_extension(".dcp");

		return this->shouldRegenerate()
			|| (not this->is_global && (not stdfs::exists(dcp_file)
				|| stdfs::last_write_time(this->xci) > stdfs::last_write_time(dcp_file)
			));
	}
}


namespace vvn::ip
{
	static Result<void, std::string> regenerate_ip_instance(Vivado& vivado, const IpInstance& ip, const MsgConfig& msg_cfg)
	{
		auto _ = vvn::LogIndenter();
		auto timer = util::Timer();
		auto uwu = MsgConfigIpSevPusher(msg_cfg);

		vvn::log("regenerating ip '{}'", ip.name);
		if(not stdfs::exists(ip.xci.parent_path().parent_path()))
			stdfs::create_directories(ip.xci.parent_path().parent_path());

		if(stdfs::exists(ip.xci.parent_path()))
			stdfs::remove_all(ip.xci.parent_path());

		auto a = vivado.streamCommand("source {}", ip.tcl.string());
		if(a.has_errors())
			return ErrFmt("failed to run '{}'", ip.tcl.string());

		auto b = vivado.streamCommand("set_property GENERATE_SYNTH_CHECKPOINT {} [get_files {}]",
			ip.is_global ? "false" : "true", ip.xci.filename().string());

		if(b.has_errors())
			return ErrFmt("failed to set '{}' as {}", ip.name, ip.is_global ? "global" : "out-of-context");

		auto __ = vvn::LogIndenter();
		vvn::log("finished in {}; suppressed {} info(s), {} warning(s)", timer.print(),
			a.infos.size(), a.warnings.size());

		return Ok();
	}

	static Result<void, std::string> synthesise_ip_instance(Vivado& vivado, const IpInstance& ip, const MsgConfig& msg_cfg)
	{
		auto _ = vvn::LogIndenter();
		auto timer = util::Timer();
		auto uwu = MsgConfigIpSevPusher(msg_cfg);

		if(not ip.is_global)
		{
			vvn::log("rebuilding ip '{}'", ip.name);
			auto b = vivado.streamCommand("synth_ip [get_ips {}]", ip.name);
			if(b.has_errors())
				return ErrFmt("synthesis of '{}' failed", ip.name);

			auto __ = vvn::LogIndenter();
			vvn::log("finished in {}; suppressed {} info(s), {} warning(s)", timer.print(),
				b.infos.size(), b.warnings.size());
		}

		return Ok();
	}


	static Failable<std::string> build_one_ip(Vivado& vivado, const Project& proj, const IpInstance& ip)
	{
		auto _ = vvn::LogIndenter();
		zpr::println("{}+ {}{}", vvn::indentStr(), ip.is_global ? "(global) " : "", ip.name);


		if(ip.shouldRegenerate())
		{
			if(auto e = regenerate_ip_instance(vivado, ip, proj.getMsgConfig()); e.is_err())
				return Err(e.error());
		}
		else
		{
			// if we had to regenerate the IP, then `create_ip` already puts it in
			// the current project, so there's no need to re-read it (in fact, we can't)
			if(vivado.streamCommand("read_ip \"{}\"", ip.xci.string()).has_errors())
				return ErrFmt("failed to read ip '{}'", ip.name);

			// make sure that the out-of-context property in the XCI and in our project agree
			auto foo = util::lowercase(vivado.runCommand("puts -nonewline [format \"%s\""
				" [get_property GENERATE_SYNTH_CHECKPOINT [get_files {}]]]",
				ip.xci.filename().string()).content);

			// it's OOC by default, so if it's empty assume OOC.
			auto is_ooc = (foo == "true") || (foo == "1") || foo.empty();
			if((is_ooc && ip.is_global) || (not is_ooc && not ip.is_global))
			{
				vivado.runCommand("set_property GENERATE_SYNTH_CHECKPOINT {} [get_files {}]",
					ip.is_global ? "FALSE" : "TRUE", ip.xci.string());
			}
		}

		if(ip.shouldResynthesise())
		{
			if(auto e = synthesise_ip_instance(vivado, ip, proj.getMsgConfig()); e.is_err())
				return Err(e.error());
		}
		else if(ip.is_global)
		{
			if(vivado.streamCommand("generate_target all [get_ips {}]", ip.name).has_errors())
				return ErrFmt("failed to generate targets for '{}'", ip.name);
		}

		return Ok();
	}

	Failable<std::string> synthesiseIpProducts(const Project& proj, const util::hashset<std::string_view>& ip_names)
	{
		vvn::log("synthesising ips");
		auto vivado = proj.launchVivado();
		if(auto e = proj.setup(vivado); e.is_err())
			return e;

		if(ip_names.empty())
			return synthesiseIpProducts(vivado, proj);

		std::vector<const IpInstance*> ip_insts {};
		for(auto& ip : proj.getIpInstances())
		{
			if(ip_names.contains(ip.name))
				ip_insts.push_back(&ip);
		}

		for(auto ip : ip_insts)
		{
			if(auto e = build_one_ip(vivado, proj, *ip); e.is_err())
				return Err(e.error());
		}

		return Ok();
	}

	zst::Failable<std::string> synthesiseIpProducts(Vivado& vivado, const Project& proj)
	{
		vvn::log("synthesising ips");
		for(const auto& ip : proj.getIpInstances())
		{
			if(auto e = build_one_ip(vivado, proj, ip); e.is_err())
				return Err(e.error());
		}

		return Ok();
	}
}
