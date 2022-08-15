// clean.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include "ip.h"
#include "util.h"
#include "vivado.h"
#include "vivano.h"
#include "project.h"

using zst::Failable;

namespace vvn::ip
{
	Failable<std::string> cleanIpProducts(const Project& proj, std::string_view ip_name)
	{
		if(auto ip = proj.getIpWithName(ip_name); ip != nullptr)
		{
			if(stdfs::exists(ip->xci.parent_path()))
			{
				auto foo = stdfs::relative(ip->xci.parent_path(), proj.getProjectLocation());
				zpr::println("{}- {}", indentStr(1), foo.string());
				stdfs::remove_all(ip->xci.parent_path());
			}
			return Ok();
		}
		else
		{
			return ErrFmt("ip '{}' does not exist; try 'ip list'", ip_name);
		}
	}
}
