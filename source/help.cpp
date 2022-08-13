// help.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <cstdio>
#include <zpr.h>

#include "util.h"
#include "help.h"

namespace vvn::help
{
	void showCommandList()
	{
		zpr::println(R"(
usage: vvn [command] [arguments...]

Commands:
    help            display this menu
    init            initialise a new empty project (non-interactive only)
    exit            exit vivano (interactive only)
    clean           clean all existing build products
    check           check source files for syntax errors only
    build           build the entire project (synth, impl, bitstream)
    elab            perform RTL elaboration
    synth           perform synthesis
    impl            perform implementation
    bitstream       write the bitstream
    create-ip       open Vivado to instantiate IP via the GUI
)");
	}

	void showCleanHelp()
	{
		zpr::println(R"(
usage: vvn clean [options]

Options:
    -a, --all       Clean all build products, including IP products
    -i, --ips       Clean only IP build products

If no options are specified, only design checkpoints and bitstreams are cleaned.
)");
	}

	void showCreateIpHelp()
	{
		zpr::println(R"(
usage: vvn create-ip

Launches the Vivado GUI; go to the IP catalog and instantiate and customise an IP.
Once the IP has been customised (click 'OK'), the GUI will close automatically, and
the customised IP will be added to the project.
)");
	}

	void showInitHelp()
	{
		zpr::println(R"(
usage: vvn init <part_name> [project_name]

Initialise a new project using the given 'part_name', and
optionally a 'project_name' (defaults to the current folder name).
)");
	}

	void showBuildHelp()
	{
		zpr::println(R"(
usage: vvn build [options]

Options:
    -h, --help      show this help
    -s, --stale     allow using stale design checkpoints, even if they are older than sources
    -f, --force     force a rebuild even if sources are up to date
)");
	}

	void showSynthHelp()
	{
		zpr::println(R"(
usage: vvn synth [options]

Options:
    -h, --help      show this help
    -f, --force     force a rebuild even if sources are up to date
)");
	}

	void showImplHelp()
	{
		zpr::println(R"(
usage: vvn impl [options]

Options:
    -h, --help      show this help
    -s, --stale     allow using stale design checkpoints, even if they are older than sources
    -f, --force     force a rebuild even if sources are up to date
)");
	}

	void showBitstreamHelp()
	{
		zpr::println(R"(
usage: vvn bitstream [options]

Options:
    -h, --help      show this help
    -s, --stale     allow using stale design checkpoints, even if they are older than sources
    -f, --force     force a rebuild even if sources are up to date
)");
	}

	void showManual()
	{
		auto pretty = [](const std::string& s) {
			puts(util::prettyFormatTextBlock(s, "  ", "      ").c_str());
			puts("");
		};

		puts("Overview");
		pretty("Vivano is a build tool for managing and building Xilinx Vivado projects. "
		"Use 'vvn help' to show the list of subcommands available. Generally, compilation proceeds as "
		"usual: synthesis, implementation, and bitstream generation.");

		pretty("Since Vivado is operated in non-project mode, intermediate results are written to design "
		"checkpoint (dcp) files, which can be found under 'build/' by default. These files can also be "
		"opened in Vivado to inspect the build at that stage.");

		puts("Folder structure");
		puts("  project_dir/");
		puts("   |- vivano-project.json");
		puts("   |* build/");
		puts("       |- dcp files");
		puts("       |- bitstream");
		puts("   |* ip/");
		puts("       |- ip regeneration tcls");
		puts("       |* xci/");
		puts("           |* ip build products");
		puts("   |* sources/");
		puts("       |* hdl/");
		puts("           |- vhdl files");
		puts("           |- verilog files");
		puts("       |* constraints/");
		puts("           |- xdc files");
	}
}
