// bitstream.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

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
	bool Project::should_rewrite_bitstream(Vivado& vivado, bool allow_stale) const
	{
		if(this->should_reimplement(vivado, allow_stale))
			return true;

		auto bit_file = this->get_bitstream_name();
		if(not stdfs::exists(bit_file))
			return true;

		auto bit_time = stdfs::last_write_time(bit_file);
		auto impl_dcp = m_build_folder / m_implemented_dcp_name;
		auto synth_dcp = m_build_folder / m_synthesised_dcp_name;

		return stdfs::last_write_time(impl_dcp) > bit_time
			|| stdfs::last_write_time(synth_dcp) > bit_time;
	}


	zst::Result<bool, std::string> Project::writeBitstream(Vivado& vivado, std::span<std::string_view> args, bool use_dcp) const
	{
		if(auto a = args::checkValidArgs(args, { args::FORCE_BUILD, args::USE_STALE }); a.has_value())
			return ErrFmt("unsupported option '{}', try '--help'", *a);

		auto allow_stale = args::check(args, args::USE_STALE);
		auto force_build = args::check(args, args::FORCE_BUILD);

		if(not this->should_rewrite_bitstream(vivado, allow_stale) && not force_build)
		{
			vvn::log("bitstream up to date");
			return Ok(true);
		}

		zpr::println("");
		vvn::log("writing bitstream");

		auto timer = util::Timer();
		auto _ = vvn::LogIndenter();

		if(use_dcp)
		{
			this->reload_project(vivado);

			auto impl_dcp = m_build_folder / m_implemented_dcp_name;
			vvn::log("opening implementation checkpoint '{}'", impl_dcp.string());
			if(not stdfs::exists(impl_dcp))
			{
				vvn::error("implementation dcp file does not exist (run implementation first?)");
				return ErrFmt("could not read implementation dcp");
			}
			else if(vivado.streamCommand("open_checkpoint \"{}\"", impl_dcp.string()).has_errors())
			{
				return ErrFmt("could not read implementation dcp");
			}
		}

		if(vivado.streamCommand("write_bitstream -force \"{}\"", this->get_bitstream_name().string()).has_errors())
			return ErrFmt("failed to write bitstream");

		vvn::log("bitstream written to '{}' in {}", this->get_bitstream_name().string(), timer.print());
		return Ok(false);
	}
}
