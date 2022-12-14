// clean.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <filesystem>

#include "ip.h"
#include "args.h"
#include "help.h"
#include "vivano.h"
#include "vivado.h"
#include "project.h"

using zst::Result;
namespace stdfs = std::filesystem;

namespace vvn
{
	Result<void, std::string> Project::clean(std::span<std::string_view> args) const
	{
		if(auto a = args::checkValidArgs(args, { args::ALL, args::IPS }); a.has_value())
			return ErrFmt("invalid clean flag '{}', try '--help'", *a);

		bool clean_dcps = false;
		bool clean_ips = false;

		if(args.empty() || args::check(args, args::ALL))
			clean_dcps = true;

		if(args::check(args, args::IPS) || args::check(args, args::ALL))
			clean_ips = true;

		// always yeet this
		if(auto xil = m_location / ".Xil"; stdfs::exists(xil) && stdfs::is_directory(xil))
			stdfs::remove_all(xil);

		if(clean_dcps)
		{
			vvn::log("cleaning build products");

			// just clean the checkpoints
			if(auto dcp = m_build_folder / m_synthesised_dcp_name; stdfs::exists(dcp))
			{
				zpr::println("{}- {}", indentStr(1), stdfs::relative(dcp.string(), m_location).string());
				stdfs::remove(dcp);
			}

			if(auto dcp = m_build_folder / m_implemented_dcp_name; stdfs::exists(dcp))
			{
				zpr::println("{}- {}", indentStr(1), stdfs::relative(dcp.string(), m_location).string());
				stdfs::remove(dcp);
			}

			if(auto bit = this->get_bitstream_name(); stdfs::exists(bit))
			{
				zpr::println("{}- {}", indentStr(1), stdfs::relative(bit.string(), m_location).string());
				stdfs::remove(bit);
			}
		}

		if(clean_ips)
		{
			vvn::log("cleaning IP products");
			for(auto& ip : m_ip_instances)
				ip::cleanIpProducts(*this, ip.name);

			// yeet the cache
			if(auto cache = m_location / ".cache"; stdfs::exists(cache) && stdfs::is_directory(cache))
			{
				vvn::log("clearing IP cache");
				stdfs::remove_all(cache);
			}
		}

		return Ok();
	}
}
