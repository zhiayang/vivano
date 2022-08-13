// init.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <string>
#include <optional>
#include <filesystem>

#include "args.h"
#include "help.h"
#include "vivado.h"
#include "vivano.h"
#include "project.h"
#include "msgconfig.h"

namespace stdfs = std::filesystem;

namespace vvn
{
	static void create_project(std::string part_name, std::optional<std::string> proj_name_)
	{
		// check that the json doesn't exist
		if(stdfs::exists(PROJECT_JSON_FILENAME))
			vvn::error_and_exit("existing project file '{}' exists, refusing to overwrite", PROJECT_JSON_FILENAME);

		auto proj_name = (proj_name_.has_value()
			? *proj_name_
			: stdfs::current_path().filename().string()
		);

		zpr::println("creating project: '{}' using part '{}'",
			proj_name, part_name);

		zpr::println("checking that '{}' is a valid part", part_name);

		auto vivado = vvn::Vivado(MsgConfig{});
		if(not vivado.partExists(part_name))
			vvn::error_and_exit("part '{}' does not exist (check licenses?)", part_name);

		vvn::log("writing project to '{}'", PROJECT_JSON_FILENAME);
		writeDefaultProjectJson(part_name, proj_name);
	}

	void createProject(std::span<std::string_view> args)
	{
		if(args::check(args, args::HELP) || args.size() < 2)
		{
			vvn::help::showInitHelp();
			return;
		}

		auto part_name = std::string(args[1]);
		std::optional<std::string> proj_name_opt {};
		if(args.size() > 2)
			proj_name_opt = std::string(args[2]);

		create_project(std::move(part_name), std::move(proj_name_opt));
	}
}
