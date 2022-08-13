// impl.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <filesystem>

#include "args.h"
#include "help.h"
#include "vivano.h"
#include "vivado.h"
#include "project.h"

using zst::Result;
namespace stdfs = std::filesystem;

namespace vvn
{
	bool Project::should_reimplement(Vivado& vivado, bool allow_stale) const
	{
		if(not allow_stale && this->should_resynthesise(vivado))
			return true;

		auto dcp_file = m_build_folder / m_implemented_dcp_name;
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

		return stdfs::last_write_time(m_build_folder / m_synthesised_dcp_name) > dcp_time
			|| check_files(m_impl_constraints);
	}

	Result<bool, std::string> Project::implement(Vivado& vivado, std::span<std::string_view> args, bool from_prev) const
	{
		if(auto a = args::checkValidArgs(args, { args::FORCE_BUILD, args::USE_STALE }); a.has_value())
			return ErrFmt("unsupported option '{}', try '--help'", *a);

		if(args::check(args, args::HELP))
		{
			help::showImplHelp();
			return Ok(false);
		}

		auto allow_stale = args::check(args, args::USE_STALE);
		auto force_build = args::check(args, args::FORCE_BUILD);

		if(not this->should_reimplement(vivado, allow_stale) && not force_build)
		{
			vvn::log("implementation up to date");
			return Ok(false);
		}

		zpr::println("");
		vvn::log("performing implementation");

		auto timer = util::Timer();
		auto _ = vvn::LogIndenter();

		if(not from_prev)
		{
			this->reload_project(vivado);

			auto synthesis_dcp = m_build_folder / m_synthesised_dcp_name;
			vvn::log("opening synthesis checkpoint '{}'", synthesis_dcp.string());
			if(not stdfs::exists(synthesis_dcp))
			{
				vvn::error("synthesis dcp file does not exist (run synthesis first?)");
				return ErrFmt("could not read synthesis dcp");
			}
			else if(vivado.streamCommand("open_checkpoint \"{}\"", synthesis_dcp.string()).has_errors())
			{
				return ErrFmt("could not read synthesis dcp");
			}

			this->read_files(vivado);
		}

		if(m_impl_constraints.size() > 0)
		{
			vvn::log("reading constraints");
			auto _ = vvn::LogIndenter();

			for(auto& xdc : m_impl_constraints)
			{
				if(not vivado.haveConstraintFile(xdc))
				{
					zpr::println("{}+ {}", vvn::indentStr(), xdc);
					if(vivado.addConstraintFile(xdc).has_errors())
						return ErrFmt("failed to read '{}'", xdc);
				}
			}
		}

		// implement
		vvn::log("running opt_design");
		if(vivado.streamCommand("opt_design").has_errors())
			return ErrFmt("opt_design failed");

		vvn::log("running place_design");
		if(vivado.streamCommand("place_design").has_errors())
			return ErrFmt("place_design failed");

		vvn::log("running route_design");
		if(vivado.streamCommand("route_design").has_errors())
			return ErrFmt("route_design failed");

		auto dcp_file = m_build_folder / m_implemented_dcp_name;
		vvn::log("writing checkpoint '{}'", dcp_file.string());

		if(vivado.streamCommand("write_checkpoint -force \"{}\"", dcp_file.string()).has_errors())
			return ErrFmt("failed to write post-implementation checkpoint");

		vvn::log("implementation finished in {}", timer.print());
		return Ok(true);
	}
}
