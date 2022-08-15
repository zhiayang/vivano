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
		if(proj.getBdWithName(bd_name) != nullptr)
			return ErrFmt("block design '{}' already exists", bd_name);

		if(not stdfs::exists(proj.getBdOutputsLocation()))
			stdfs::create_directories(proj.getBdOutputsLocation());

		auto tcl = fopen((proj.getBdLocation() / zpr::sprint("{}.tcl", bd_name)).c_str(), "wb");
		auto d0 = util::Defer([&tcl]() {
			fclose(tcl);
		});

		{
			auto header = std::string_view("# DO NOT MANUALLY EDIT THIS FILE\n");
			fwrite(header.data(), 1, header.size(), tcl);
		}

		// create the bd first
		{
			auto v = proj.launchVivado();
			if(auto e = proj.setup(v); e.is_err())
				return e;

			auto cmd = zpr::sprint("create_bd_design -dir {} {}",
				stdfs::relative(proj.getBdOutputsLocation(), proj.getProjectLocation()).string(), bd_name);

			if(v.streamCommand("{}", cmd).has_errors())
				return ErrFmt("failed to create block design");

			fwrite(cmd.c_str(), 1, cmd.size(), tcl);
		}

		return Ok();
	}
}
