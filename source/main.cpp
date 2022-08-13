// main.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <cassert>

#include <string>
#include <algorithm>
#include <string_view>
#include <initializer_list>

#include <zpr.h>
#include <zprocpipe.h>

#include "args.h"
#include "util.h"
#include "help.h"
#include "vivano.h"
#include "vivado.h"
#include "project.h"

using zst::Result;
static constexpr std::string_view VERSION = "0.1.0";

static Result<void, std::string> run_subcommand(vvn::Project& project, std::string_view command, std::span<std::string_view> args)
{
	using namespace vvn;
	if(command == vvn::CMD_CLEAN)
	{
		if(args::check(args, args::HELP))
			help::showCleanHelp(), exit(0);

		return project.clean(args);
	}
	else if(command == vvn::CMD_CREATE_IP)
	{
		if(args::check(args, args::HELP))
			help::showCreateIpHelp(), exit(0);

		return vvn::createIpWithVivadoGUI(project, args);
	}
	else if(command == vvn::CMD_BUILD)
	{
		if(args::check(args, args::HELP))
			help::showBuildHelp(), exit(0);

		auto vivado = project.launchVivado();
		return project.setup(vivado).flatmap([&]() {
			return project.buildAll(vivado, args);
		});
	}
	else if(command == vvn::CMD_SYNTH)
	{
		if(args::check(args, args::HELP))
			help::showSynthHelp(), exit(0);

		auto vivado = project.launchVivado();
		return project.setup(vivado).flatmap([&]() {
			return project.synthesise(vivado, args).remove_value();
		});
	}
	else if(command == vvn::CMD_IMPL)
	{
		if(args::check(args, args::HELP))
			help::showImplHelp(), exit(0);

		auto vivado = project.launchVivado();
		return project.setup(vivado).flatmap([&]() {
			return project.implement(vivado, args).remove_value();
		});
	}
	else if(command == vvn::CMD_BITSREAM)
	{
		if(args::check(args, args::HELP))
			help::showBitstreamHelp(), exit(0);

		auto vivado = project.launchVivado();
		return project.setup(vivado).flatmap([&]() {
			return project.writeBitstream(vivado, args).remove_value();
		});
	}
	else
	{
		vvn::help::showCommandList();
		return ErrFmt("unsupported command '{}'", command);
	}
}


int main(int argc, char** argv)
{
	std::vector<std::string_view> args {};
	std::string_view command = argc > 1 ? argv[1] : "";
	for(int i = 2; i < argc; i++)
		args.push_back(argv[i]);

	if(command.empty())
	{
		zpr::println("TODO: interactive mode not supported yet");
		exit(0);
	}
	else if(command == vvn::CMD_VERSION)
	{
		zpr::println("vivano build tool   version {}", VERSION);
	}
	else if(command == vvn::CMD_HELP)
	{
		zpr::println("vivano build tool   version {}", VERSION);
		vvn::help::showCommandList();
		zpr::println(util::prettyFormatTextBlock(
			"If 'command' is not provided, then vivano runs in interactive mode, and a single Vivado session is active "
			"until vivano exits. Otherwise, a new Vivado instance is spawned (and then terminated) for every vivano command.",
			"", "   "));
		exit(0);
	}
	else if(command == vvn::CMD_MANUAL)
	{
		vvn::help::showManual();
		exit(0);
	}
	else if(command == vvn::CMD_INIT)
	{
		if(vvn::args::check(args, vvn::args::HELP))
			vvn::help::showInitHelp(), exit(0);

		vvn::createProject(args);
		exit(0);
	}

	// try to read a vivano-project.json
	auto config_or_err = vvn::parseProjectJson(vvn::PROJECT_JSON_FILENAME);
	if(not config_or_err.ok())
		vvn::error_and_exit("failed to read project json: {}", config_or_err.error());

	auto project = vvn::Project(config_or_err.unwrap());
	auto result = run_subcommand(project, command, args);

	if(result.is_err())
		zpr::fprintln(stderr, "\nerrors were encountered: {}", result.error());

	exit(result.is_err() ? 1 : 0);
}









