// project.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <cstdio>
#include <filesystem>
#include <optional>

#include <picojson.h>
namespace pj = picojson;

#include "util.h"
#include "vivano.h"
#include "vivado.h"
#include "project.h"

using zst::Ok;
using zst::Err;
using zst::ErrFmt;
using zst::Result;

namespace defaults
{
	constexpr std::string_view BUILD_LOCATION   = "build";

	constexpr std::string_view SOURCES_LOCATION = "sources";
	constexpr std::string_view HDL_SUBDIR       = "hdl";
	constexpr std::string_view SIM_SUBDIR       = "sim";
	constexpr std::string_view XDC_SUBDIR       = "constraints";

	constexpr std::string_view IP_LOCATION      = "ip";
	constexpr std::string_view IP_OUTPUT_SUBDIR = "outputs";

	constexpr std::string_view BD_LOCATION      = "bd";
	constexpr std::string_view BD_OUTPUT_SUBDIR = "outputs";

	constexpr std::string_view SYNTHESISED_DCP  = "synthesised.dcp";
	constexpr std::string_view IMPLEMENTED_DCP  = "implemented.dcp";

	constexpr int MIN_MESSAGE_SEVERITY          = 0;
	constexpr int MIN_IP_MESSAGE_SEVERITY       = 2;
	constexpr bool PRINT_MESSAGE_IDS            = true;

	constexpr std::pair<std::string_view, int> MSG_SEVERITY_CHANGES[] = {
		{ "HDL 9-806", vvn::Message::ERROR },       // syntax error
		{ "Opt 31-80", vvn::Message::ERROR },       // multi-driver net
		{ "Route 35-14", vvn::Message::ERROR },     // multi-driver net
		{ "AVAL-46", vvn::Message::ERROR },         // MMCM or PPL VCO freq out of range
		{ "IP_Flow 19-3664", vvn::Message::ERROR }, // IP file not found
		{ "HDL 9-1314", vvn::Message::ERROR },      // formal port/generic not declared
		{ "HDL 9-3136", vvn::Message::ERROR },      // undeclared variable
		{ "HDL 9-3242", vvn::Message::ERROR },      // invalid port map
		{ "HDL 9-3500", vvn::Message::ERROR },      // formal port has no value

		{ "Physopt 32-619", vvn::Message::LOG },    // estimated timing summary
      	{ "Route 35-57", vvn::Message::LOG }        // estimated timing summary
	};

	constexpr std::string_view MSG_SUPPRESSIONS[] = {
	};
}

namespace vvn
{
	static Result<std::optional<std::string>, std::string> read_string(const pj::object& dict, const std::string& key)
	{
		if(auto it = dict.find(key); it != dict.end())
		{
			if(it->second.is_str())
				return Ok(it->second.as_str());
			else
				return ErrFmt("expected string value for key '{}'", key);
		}
		else
		{
			return Ok(std::nullopt);
		}
	}

	static Result<std::string, std::string> read_string(const pj::object& dict, const std::string& key,
		std::string_view default_value)
	{
		auto foo = read_string(dict, key);
		if(not foo.ok())
			return Err(foo.error());
		else if(foo->has_value())
			return Ok(foo->value());
		else
			return Ok(std::string(default_value));
	}

#if 0
	static Result<int64_t, std::string> read_int(const pj::object& dict, const std::string& key,
		int64_t default_value)
	{
		if(auto it = dict.find(key); it != dict.end())
		{
			if(it->second.is_int())
				return Ok(it->second.as_int());
			else
				return ErrFmt("expected integer value for key '{}'", key);
		}
		else
		{
			return Ok(default_value);
		}
	}
#endif

	static Result<bool, std::string> read_boolean(const pj::object& dict, const std::string& key,
		bool default_value)
	{
		if(auto it = dict.find(key); it != dict.end())
		{
			if(it->second.is_bool())
				return Ok(it->second.as_bool());
			else
				return ErrFmt("expected boolean value for key '{}'", key);
		}
		else
		{
			return Ok(default_value);
		}
	}

	static Result<std::vector<std::string>, std::string> read_string_array(const pj::object& dict, const std::string& key)
	{
		if(auto it = dict.find(key); it != dict.end())
		{
			std::vector<std::string> ret {};
			if(it->second.is_arr())
			{
				for(const auto& val : it->second.as_arr())
				{
					if(val.is_str())
						ret.push_back(val.as_str());
					else
						return ErrFmt("expected string value for elements of array '{}'", key);
				}

				return Ok(std::move(ret));
			}
			else
			{
				return ErrFmt("expected array value for key '{}'", key);
			}
		}
		else
		{
			return Ok(std::vector<std::string>{});
		}
	}


	static Result<void, std::string> parseSourcesJson(ProjectConfig& project, const pj::object& dict)
	{
		auto& sources = project.sources_config;
		if(auto foo = dict.find("sources"); foo == dict.end())
		{
			sources.location = project.location / defaults::SOURCES_LOCATION;
			sources.hdl_subdir = defaults::HDL_SUBDIR;
			sources.xdc_subdir = defaults::XDC_SUBDIR;
			sources.sim_subdir = defaults::SIM_SUBDIR;
			sources.auto_find_sources = true;
		}
		else
		{
			auto& obj = foo->second.as_obj();
			{
				auto foo = read_string(obj, "location", defaults::SOURCES_LOCATION);
				if(foo.is_err())
					return Err(foo.error());
				sources.location = project.location / foo.unwrap();
			}

			{
				auto foo = read_string(obj, "hdl_subdir", defaults::HDL_SUBDIR);
				if(foo.is_err())
					return Err(foo.error());
				sources.hdl_subdir = foo.unwrap();
			}

			{
				auto foo = read_string(obj, "xdc_subdir", defaults::XDC_SUBDIR);
				if(foo.is_err())
					return Err(foo.error());
				sources.xdc_subdir = foo.unwrap();
			}
			{
				auto foo = read_string(obj, "sim_subdir", defaults::SIM_SUBDIR);
				if(foo.is_err())
					return Err(foo.error());
				sources.sim_subdir = foo.unwrap();
			}
			{
				auto foo = read_boolean(obj, "auto_find_sources", true);
				if(foo.is_err())
					return Err(foo.error());
				sources.auto_find_sources = foo.unwrap();
			}
			{
				auto foo = read_string_array(obj, "synth_only_constraints");
				if(foo.is_err())
					return Err(foo.error());
				sources.synth_only_xdcs = foo.unwrap();
			}
			{
				auto foo = read_string_array(obj, "impl_only_constraints");
				if(foo.is_err())
					return Err(foo.error());
				sources.impl_only_xdcs = foo.unwrap();
			}
			{
				auto foo = read_string_array(obj, "tcls");
				if(foo.is_err())
					return Err(foo.error());
				sources.tcl_scripts = foo.unwrap();
			}
		}

		return Ok();
	}

	static Result<void, std::string> parseIpJson(ProjectConfig& project, const pj::object& dict)
	{
		auto& ip = project.ip_config;
		if(auto foo = dict.find("ip"); foo == dict.end())
		{
			ip.location = project.location / defaults::IP_LOCATION;
			ip.auto_find_sources = true;
			ip.output_subdir = defaults::IP_OUTPUT_SUBDIR;
		}
		else
		{
			auto& obj = foo->second.as_obj();
			{
				auto foo = read_string(obj, "location", defaults::IP_LOCATION);
				if(foo.is_err())
					return Err(foo.error());
				ip.location = project.location / foo.unwrap();
			}

			{
				auto foo = read_string(obj, "output_subdir", defaults::IP_OUTPUT_SUBDIR);
				if(foo.is_err())
					return Err(foo.error());
				ip.output_subdir = foo.unwrap();
			}

			{
				auto foo = read_boolean(obj, "auto_find_sources", true);
				if(foo.is_err())
					return Err(foo.error());
				ip.auto_find_sources = foo.unwrap();
			}

			{
				auto foo = read_string_array(obj, "global_ips");
				if(foo.is_err())
					return Err(foo.error());
				ip.global_ips.insert(foo->begin(), foo->end());
			}
		}

		return Ok();
	}

	static Result<void, std::string> parseBdJson(ProjectConfig& project, const pj::object& dict)
	{
		auto& bd = project.bd_config;
		if(auto foo = dict.find("bd"); foo == dict.end())
		{
			bd.location = project.location / defaults::BD_LOCATION;
			bd.output_subdir = defaults::BD_OUTPUT_SUBDIR;
			bd.auto_find_sources = true;
		}
		else
		{
			auto& obj = foo->second.as_obj();
			{
				auto foo = read_string(obj, "location", defaults::BD_LOCATION);
				if(foo.is_err())
					return Err(foo.error());
				bd.location = project.location / foo.unwrap();
			}

			{
				auto foo = read_string(obj, "output_subdir", defaults::BD_OUTPUT_SUBDIR);
				if(foo.is_err())
					return Err(foo.error());
				bd.output_subdir = foo.unwrap();
			}

			{
				auto foo = read_boolean(obj, "auto_find_sources", true);
				if(foo.is_err())
					return Err(foo.error());
				bd.auto_find_sources = foo.unwrap();
			}
		}

		return Ok();
	}



	static Result<void, std::string> parseMessagesJson(ProjectConfig& project, const pj::object& dict)
	{
		auto& msg = project.messages_config;
		util::hashset<std::string> force_show_msgs {};

		if(auto foo = dict.find("messages"); foo != dict.end())
		{
			if(not foo->second.is_obj())
				return ErrFmt("expected object for key 'messages'");

			auto& msg_top = foo->second.as_obj();

			auto parse_severity = [](const std::string_view& key, const pj::value& val) -> Result<int, std::string> {
				if(val.is_int())
				{
					auto i = static_cast<int>(val.as_int());
					if(Message::INFO <= i && i <= Message::ERROR)
					{
						return Ok(i);
					}
					else
					{
						return ErrFmt("expected integer between {} and {} for '{}'",
							Message::INFO, Message::ERROR, key);
					}
				}
				else if(val.is_str())
				{
					auto& s = val.as_str();
					if(s == "info" || s == "INFO")
						return Ok(Message::INFO);
					else if(s == "log" || s == "LOG")
						return Ok(Message::LOG);
					else if(s == "warn" || s == "WARN" || s == "warning" || s == "WARNING")
						return Ok(Message::WARNING);
					else if(s == "crit" || s == "CRIT" || s == "critical warning" || s == "CRITICAL WARNING")
						return Ok(Message::CRIT_WARNING);
					else if(s == "error" || s == "ERROR")
						return Ok(Message::ERROR);
					else
						return ErrFmt("invalid severity '{}'", s);
				}
				else
				{
					return ErrFmt("expected string or integer for key '{}'", key);
				}
			};

			auto aoeu = [&](const char* key, int* num, int def) -> Result<void, std::string> {
				if(auto x = msg_top.find(key); x != msg_top.end())
				{
					if(auto s = parse_severity(key, x->second); s.is_err())
						return Err(s.error());
					else
						*num = s.unwrap();
				}
				else
				{
					*num = def;
				}
				return Ok();
			};

			if(auto e = aoeu("min_print_severity", &msg.min_severity, defaults::MIN_MESSAGE_SEVERITY); e.is_err())
				return Err(e.error());

			if(auto e = aoeu("min_ip_print_severity", &msg.min_ip_severity, defaults::MIN_IP_MESSAGE_SEVERITY); e.is_err())
				return Err(e.error());

			if(auto x = read_boolean(msg_top, "print_message_ids", defaults::PRINT_MESSAGE_IDS); x.is_err())
				return Err(x.error());
			else
				msg.print_message_ids = x.unwrap();


			if(auto c = msg_top.find("change"); c != msg_top.end())
			{
				if(not c->second.is_obj())
					return ErrFmt("expected object for key 'change'");

				auto& foo = c->second.as_obj();
				for(auto& [ id, sev ] : foo)
				{
					auto s = parse_severity(id, sev);
					if(s.is_err())
						return ErrFmt("expected integer values for new severities in 'change' object");

					msg.severity_overrides[id] = s.unwrap();
				}
			}

			if(auto s = msg_top.find("suppress"); s != msg_top.end())
			{
				if(not s->second.is_arr())
					return ErrFmt("expected array for key 'suppress'");

				auto& foo = s->second.as_arr();
				for(auto& id : foo)
				{
					if(not id.is_str())
						return ErrFmt("expected string values for message ids in 'suppress' object");

					msg.suppressions.insert(id.as_str());
				}
			}

			if(auto s = msg_top.find("show"); s != msg_top.end())
			{
				if(not s->second.is_arr())
					return ErrFmt("expected array for key 'show'");

				auto& foo = s->second.as_arr();
				for(auto& id : foo)
				{
					if(not id.is_str())
						return ErrFmt("expected string values for message ids in 'show' object");

					force_show_msgs.insert(id.as_str());
				}
			}
		}


		for(auto& x : defaults::MSG_SEVERITY_CHANGES)
		{
			if(msg.severity_overrides.find(x.first) == msg.severity_overrides.end())
				msg.severity_overrides[std::string(x.first)] = x.second;
		}

		for(auto& x : defaults::MSG_SUPPRESSIONS)
		{
			if(force_show_msgs.find(x) == force_show_msgs.end())
				msg.suppressions.emplace(x);
		}

		return Ok();
	}




	Result<ProjectConfig, std::string> parseProjectJson(std::string_view json_path)
	{
		if(not stdfs::exists(json_path))
			return ErrFmt("'{}' does not exist", json_path);

		auto config_contents = util::readEntireFile(json_path);
		pj::value config_json {};

		std::string err;
		pj::parse(config_json, config_contents.begin(), config_contents.end(), &err);
		if(not err.empty())
			return ErrFmt("parse error: {}", err);

		auto& json_top = config_json.as_obj();

		ProjectConfig proj {};
		if(auto part = read_string(json_top, "part"); not part.ok() or not part->has_value())
			return ErrFmt("required field 'part' is missing or invalid");
		else
			proj.part_name = *part.unwrap();

		if(auto top = read_string(json_top, "top_module"); not top.ok() or not top->has_value())
			return ErrFmt("required field 'top_module' is missing or invalid");
		else
			proj.top_module = *top.unwrap();

		// project name: default is the folder that json_path is contained in
		if(auto proj_name = read_string(json_top, "name"); not proj_name.ok())
			return ErrFmt("'name' field is invalid");
		else if(not proj_name->has_value())
			proj.project_name = stdfs::canonical(json_path).parent_path().filename().string();
		else
			proj.project_name = *proj_name.unwrap();

		proj.location = stdfs::canonical(json_path).parent_path();

		if(auto build_dir = read_string(json_top, "build_dir", defaults::BUILD_LOCATION); build_dir.is_err())
			return Err(build_dir.error());
		else
			proj.build_folder = stdfs::relative(build_dir.unwrap(), proj.location);


		if(auto synth_dcp = read_string(json_top, "synthesised_dcp_name", defaults::SYNTHESISED_DCP); synth_dcp.is_err())
			return Err(synth_dcp.error());
		else
			proj.synthesised_dcp_name = synth_dcp.unwrap();

		if(auto impl_dcp = read_string(json_top, "implemented_dcp_name", defaults::IMPLEMENTED_DCP); impl_dcp.is_err())
			return Err(impl_dcp.error());
		else
			proj.implemented_dcp_name = impl_dcp.unwrap();

		if(auto vivado_dir = read_string(json_top, "vivado_install_dir", ""); vivado_dir.is_err())
			return Err(vivado_dir.error());
		else if(not vivado_dir->empty())
			proj.vivado_installation_dir = vivado_dir.unwrap();

		if(auto x = parseSourcesJson(proj, json_top); x.is_err())
			return Err(x.error());

		if(auto x = parseIpJson(proj, json_top); x.is_err())
			return Err(x.error());

		if(auto x = parseBdJson(proj, json_top); x.is_err())
			return Err(x.error());

		if(auto x = parseMessagesJson(proj, json_top); x.is_err())
			return Err(x.error());

		// parse the install dir, if it exists.
		{
			auto install_file = proj.location / VIVADO_INSTALL_DIR_FILENAME;
			if(stdfs::exists(install_file))
			{
				auto file = util::readEntireFile(install_file.string());
				auto tmp = util::splitString(file, '\n')[0];
				vvn::log("using vivado installation at '{}'", tmp);
				proj.vivado_installation_dir = tmp;
			}
		}



		return Ok(std::move(proj));
	}

	zst::Result<void, std::string> writeDefaultProjectJson(const std::string& part_name, const std::string& proj_name)
	{
		pj::object json;
		json["part"] = pj::value(part_name);
		json["name"] = pj::value(proj_name);
		json["top_module"] = pj::value("fpga_top");
		json["build_dir"] = pj::value(defaults::BUILD_LOCATION);

		json["synthesised_dcp_name"] = pj::value(defaults::SYNTHESISED_DCP);
		json["implemented_dcp_name"] = pj::value(defaults::IMPLEMENTED_DCP);

		json["sources"] = pj::value(pj::object {
			{ "location", pj::value(defaults::SOURCES_LOCATION) },
			{ "hdl_subdir", pj::value(defaults::HDL_SUBDIR) },
			{ "sim_subdir", pj::value(defaults::SIM_SUBDIR) },
			{ "xdc_subdir", pj::value(defaults::XDC_SUBDIR) },
			{ "auto_find_sources", pj::value(true) },
			{ "impl_only_constraints", pj::value(std::vector<pj::value>{}) },
			{ "synth_only_constraints", pj::value(std::vector<pj::value>{}) }
		});

		json["ip"] = pj::value(pj::object {
			{ "location", pj::value(defaults::IP_LOCATION) },
			{ "output_subdir", pj::value(defaults::IP_OUTPUT_SUBDIR) },
			{ "auto_find_sources", pj::value(true) }
		});

		json["bd"] = pj::value(pj::object {
			{ "location", pj::value(defaults::BD_LOCATION) },
			{ "output_subdir", pj::value(defaults::BD_OUTPUT_SUBDIR) },
			{ "auto_find_sources", pj::value(true) }
		});

		using std::begin;
		using std::end;
		pj::object changes {};
		pj::array suppressed {};
		for(auto& p : defaults::MSG_SEVERITY_CHANGES)
			changes[std::string(p.first)] = pj::value(static_cast<int64_t>(p.second));

		for(auto& p : defaults::MSG_SUPPRESSIONS)
			suppressed.push_back(pj::value(p));

		json["messages"] = pj::value(pj::object {
			{ "min_print_severity", pj::value(static_cast<int64_t>(defaults::MIN_MESSAGE_SEVERITY)) },
			{ "print_message_ids", pj::value(defaults::PRINT_MESSAGE_IDS) },
			{ "change", pj::value(changes) },
			{ "suppress", pj::value(suppressed) }
		});

		// while we're here, create the folders.
		auto sources_path = stdfs::path(defaults::SOURCES_LOCATION);
		auto ip_path = stdfs::path(defaults::IP_LOCATION);
		auto bd_path = stdfs::path(defaults::BD_LOCATION);

		if(auto x = stdfs::path(defaults::BUILD_LOCATION); not stdfs::exists(x))
			stdfs::create_directories(x);
		if(auto x = sources_path / defaults::HDL_SUBDIR; not stdfs::exists(x))
			stdfs::create_directories(x);
		if(auto x = sources_path / defaults::HDL_SUBDIR; not stdfs::exists(x))
			stdfs::create_directories(x);
		if(auto x = sources_path / defaults::XDC_SUBDIR; not stdfs::exists(x))
			stdfs::create_directories(x);
		if(auto x = sources_path / defaults::SIM_SUBDIR; not stdfs::exists(x))
			stdfs::create_directories(x);
		if(auto x = ip_path / defaults::IP_OUTPUT_SUBDIR; not stdfs::exists(x))
			stdfs::create_directories(x);
		if(auto x = bd_path / defaults::BD_OUTPUT_SUBDIR; not stdfs::exists(x))
			stdfs::create_directories(x);

		auto f = fopen(PROJECT_JSON_FILENAME, "wb");
		if(f == nullptr)
			return ErrFmt("error creating '{}': {}", PROJECT_JSON_FILENAME, strerror(errno));

		auto json_str = pj::value(json).serialise(/* prettify: */ true);
		fwrite(json_str.data(), 1, json_str.size(), f);

		fclose(f);

		vvn::log("created '{}'", PROJECT_JSON_FILENAME);
		return Ok();
	}
}
