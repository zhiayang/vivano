// output.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <cctype>
#include <cassert>

#include <string>
#include <optional>
#include <algorithm>
#include <string_view>

#include "util.h"
#include "vivado.h"
#include "vivano.h"
#include "project.h"

namespace vvn
{
	static std::optional<Message> parse_message(std::string_view sv, const MsgConfig& msg_cfg)
	{
		// note: vivado doesn't print "log" messages, that's our own invention
		Message msg {};
		if(sv.starts_with("INFO: "))
			sv.remove_prefix(6), msg.severity = Message::INFO;
		else if(sv.starts_with("WARNING: "))
			sv.remove_prefix(9), msg.severity = Message::WARNING;
		else if(sv.starts_with("CRITICAL WARNING: "))
			sv.remove_prefix(18), msg.severity = Message::CRIT_WARNING;
		else if(sv.starts_with("ERROR: "))
			sv.remove_prefix(7), msg.severity = Message::ERROR;
		else
			return std::nullopt;

		assert(sv.size() > 0 && sv[0] == '[');
		sv.remove_prefix(1);

		auto i = sv.find(']');
		assert(i != std::string_view::npos);

		msg.code = sv.substr(0, i);
		sv.remove_prefix(2 + msg.code.size());

		// idk how to check for this properly, so this is a little scuffed.
		if(sv.back() == ']')
		{
			// scan for the '['
			auto open_loc = sv.find_last_of('[');
			if(open_loc == std::string_view::npos)
				goto cancel;

			auto location = sv.substr(open_loc + 1, sv.size() - open_loc - 2);
			if(location.empty())
				goto cancel;

			Message::Loc loc;

			// get the line number. extra check is needed because of drive letter for windows
			if(auto k = location.find_last_of(':'); k != std::string_view::npos && std::isdigit(location[k + 1]))
			{
				loc.path = location.substr(0, k);
				if(auto i = util::parseInt(location.substr(k + 1)); i.has_value())
					loc.line = *i;
				else
					loc.line = 1;
			}
			else
			{
				loc.path = location;
				loc.line = 1;
			}

			msg.location = std::move(loc);
			msg.message = sv.substr(0, open_loc);
			goto success;
		}

	cancel:
		msg.message = sv;

	success:
		if(auto it = msg_cfg.severity_overrides.find(msg.code); it != msg_cfg.severity_overrides.end())
			msg.severity = it->second;

		return msg;
	}


	bool Message::print(const MsgConfig& msg_cfg) const
	{
		if(this->severity < (msg_cfg.ip_nesting_depth > 0 ? msg_cfg.min_ip_severity : msg_cfg.min_severity))
			return false;
		else if(msg_cfg.suppressions.find(this->code) != msg_cfg.suppressions.end())
			return false;

		static constexpr std::string_view kinds[] = {
			"[info]", "[log]", "[warn]", "[crit]", "[error]"
		};

		static constexpr std::string_view spaces[] = {
			" ", "  ", " ", " ", ""
		};

		auto tmp = std::string(this->message);
		if(msg_cfg.print_message_ids)
			tmp += zpr::sprint(" (id: {})", this->code);

		std::string error_location;
		if(auto loc = this->location; this->location.has_value())
		{
			error_location = zpr::sprint("{}:{}: ", stdfs::relative(loc->path, msg_cfg.project_path).string(),
				loc->line);
		}

		zpr::println("{}{}{} {}{}", indentStr(1),
			util::colourise(kinds[this->severity], this->severity),
			spaces[this->severity],
			error_location,
			tmp
			// util::prettyFormatTextBlock(tmp, "            ", "   ", /* skip-left-margin-on-first-line: */ true)
		);

		return true;
	}

	std::optional<Message> parseMessageIntoCmdOutput(CommandOutput& cmd_out, std::string_view line, const MsgConfig& msg_cfg)
	{
		if(auto m = parse_message(line, msg_cfg); m.has_value())
		{
			cmd_out.all_messages.push_back(*m);
			if(m->severity == Message::INFO)
				cmd_out.infos.push_back(*m);
			else if(m->severity == Message::LOG)
				cmd_out.logs.push_back(*m);
			else if(m->severity == Message::WARNING)
				cmd_out.warnings.push_back(*m);
			else if(m->severity == Message::CRIT_WARNING)
				cmd_out.critical_warnings.push_back(*m);
			else if(m->severity == Message::ERROR)
				cmd_out.errors.push_back(*m);

			return *m;
		}
		else
		{
			return std::nullopt;
		}
	}

	CommandOutput parseOutput(std::string output, const MsgConfig& msg_cfg)
	{
		CommandOutput ret {};
		ret.content = std::move(output);

		auto lines = util::splitString(ret.content, '\n');
		for(auto& line : lines)
			parseMessageIntoCmdOutput(ret, line, msg_cfg);

		return ret;
	}


	const CommandOutput& CommandOutput::print(const MsgConfig& msg_cfg) const
	{
		for(auto& msg : this->all_messages)
			msg.print(msg_cfg);

		return *this;
	}
}
