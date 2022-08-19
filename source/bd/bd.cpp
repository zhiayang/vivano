// bd.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>

#include "bd.h"
#include "util.h"
#include "args.h"
#include "vivado.h"
#include "vivano.h"
#include "project.h"

using zst::Failable;

namespace vvn::bd
{
	Failable<std::string> runBdCommand(const Project& proj, std::span<std::string_view> args)
	{
		auto help_str = R"(
usage: vvn bd [subcommand] [options]

Subcommands:
    build           build out-of-context products for block designs
    create          create a new block design
    delete          delete a block design
    clean           clean block design output products
    edit            edit an existing block design
    list            list block designs in the project

Creating and editing a block design will launch the Vivado GUI; create or
edit a block design using the IP integrator. Viavdo will automatically close
when the IP operation is completed.
)";

		auto print_bd_list = [](const Project& proj) -> auto {
			auto bds = proj.getBdInstances();
			std::sort(bds.begin(), bds.end(), [](auto& a, auto& b) {
				return a.name < b.name;
			});

			for(auto& bd : bds)
				zpr::println("  * {}", bd.name);

			puts("");
			return Ok();
		};


		if(args.empty())
		{
			puts(help_str);
			return Ok();
		}
		else if(args[0] == CMD_BD_LIST)
		{
			puts("list of block designs:");
			return print_bd_list(proj);
		}
		else if(args[0] == CMD_BD_CREATE)
		{
			if(args.size() != 2 || args::check(args, args::HELP))
			{
				zpr::println(R"(
usage: vvn bd create <name>

Create a new block design with the given name. Opens the IP integrator GUI
for editing the block design. To save changes, close the block design and
Vivado will automatically exit.
				)");
				return Ok();
			}

			return bd::createUsingGui(proj, args[1]);
		}
		else if(args[0] == CMD_BD_DELETE)
		{
			if(args.size() != 2 || args::check(args, args::HELP))
			{
				zpr::println(R"(
usage: vvn bd delete <bd name>

The name of an existing block design is required, which is one of:)");
				return print_bd_list(proj);
			}

			return bd::deleteBlockDesign(proj, args[1]);
		}
		else
		{
			puts(help_str);
			return ErrFmt("unknown bd subcommand '{}'", args[0]);
		}
	}
}
