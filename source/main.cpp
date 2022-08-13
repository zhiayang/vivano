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

static constexpr std::string_view VERSION = "0.1.0";


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
		vvn::createProject(args);
		exit(0);
	}

	// if we are asking for help, don't bother to launch vivado
	if(vvn::args::checkArgument(args, vvn::args::HELP))
	{
		using namespace vvn;
		using namespace vvn::help;
		if(command == CMD_INIT)
			showInitHelp();
		else if(command == CMD_BUILD)
			showBuildHelp();
		else if(command == CMD_SYNTH)
			showSynthHelp();
		else if(command == CMD_IMPL)
			showImplHelp();
		else if(command == CMD_BITSREAM)
			showBitstreamHelp();
		else if(command == CMD_CLEAN)
			showCleanHelp();

		exit(0);
	}

	constexpr auto one_of = [&](std::string_view sv, const auto& aa) -> bool {
		return std::find(aa.begin(), aa.end(), sv) != aa.end();
	};

	auto supported_commands = std::vector {
		vvn::CMD_CREATE_IP,
		vvn::CMD_BUILD,
		vvn::CMD_SYNTH,
		vvn::CMD_IMPL,
		vvn::CMD_BITSREAM,
		vvn::CMD_CLEAN
	};

	if(not one_of(command, supported_commands))
	{
		zpr::println("unsupported command '{}'", command);
		vvn::help::showCommandList();
		exit(1);
	}

	// try to read a vivano-project.json
	auto config_or_err = vvn::parseProjectJson(vvn::PROJECT_JSON_FILENAME);
	if(not config_or_err.ok())
		vvn::error_and_exit("failed to read project json: {}", config_or_err.error());

	auto project = vvn::Project(config_or_err.unwrap());
	if(command == vvn::CMD_CLEAN)
	{
		auto e = project.clean(args);
		if(e.is_err())
			zpr::fprintln(stderr, "\nfailed: {}", e.error());

		exit(e.is_err() ? 1 : 0);
	}
	else if(command == vvn::CMD_CREATE_IP)
	{
		vvn::createIpWithVivadoGUI(project, args);
		exit(0);
	}

	auto vivado = vvn::Vivado(project.getMsgConfig());

	zst::Result<void, std::string> result {};
	if(command == vvn::CMD_BUILD)
	{
		result = project.setup(vivado).flatmap([&]() {
			return project.buildAll(vivado, args);
		});
	}
	else if(command == vvn::CMD_SYNTH)
	{
		result = project.setup(vivado).flatmap([&]() {
			return project.synthesise(vivado, args).remove_value();
		});
	}
	else if(command == vvn::CMD_IMPL)
	{
		result = project.setup(vivado).flatmap([&]() {
			return project.implement(vivado, args).remove_value();
		});
	}
	else if(command == vvn::CMD_BITSREAM)
	{
		result = project.setup(vivado).flatmap([&]() {
			return project.writeBitstream(vivado, args).remove_value();
		});
	}

	if(result.is_err())
	{
		zpr::fprintln(stderr, "\naborting: {}", result.error());
		exit(1);
	}
}










