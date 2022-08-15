// delete.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include "ip.h"
#include "util.h"
#include "vivado.h"
#include "vivano.h"
#include "project.h"

using zst::Result;
using zst::Failable;

namespace vvn::ip
{
	Failable<std::string> deleteIp(const Project& proj, std::string_view ip_name)
	{
		auto ip = proj.getIpWithName(ip_name);
		if(ip == nullptr)
			return ErrFmt("ip '{}' does not exist; try 'ip list'", ip_name);

		vvn::log("deleting ip '{}'", ip_name);

		if(stdfs::exists(ip->tcl))
		{
			zpr::println("{}- {}", indentStr(1), ip->tcl.string());
			stdfs::remove(ip->tcl);
		}

		if(auto p = ip->xci.parent_path(); stdfs::exists(p))
		{
			zpr::println("{}- {}", indentStr(1), p.string());
			stdfs::remove_all(p);
		}

		return Ok();
	}
}
