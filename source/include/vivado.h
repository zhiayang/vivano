// vivado.h
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <cstddef>

#include <span>
#include <string>
#include <optional>
#include <functional>
#include <string_view>
#include <unordered_set>

#include <zpr.h>
#include <zst.h>
#include <zprocpipe.h>

#include "msgconfig.h"

namespace zpp = zprocpipe;

namespace vvn
{
	struct Message
	{
		static constexpr int INFO           = 0;
		static constexpr int LOG            = 1;
		static constexpr int WARNING        = 2;
		static constexpr int CRIT_WARNING   = 3;
		static constexpr int ERROR          = 4;

		int severity;
		std::string_view code;
		std::string_view message;

		struct Loc
		{
			std::string_view path;
			int line;
		};
		std::optional<Loc> location;

		bool print(const MsgConfig& msg_cfg) const;
	};

	struct CommandOutput
	{
		std::string content;
		std::string stderr_content;

		std::vector<Message> all_messages;

		std::vector<Message> infos;
		std::vector<Message> logs;
		std::vector<Message> warnings;
		std::vector<Message> critical_warnings;
		std::vector<Message> errors;

		const CommandOutput& print(const MsgConfig& msg_cfg) const;
		bool has_errors() const { return errors.size() > 0; }
	};

	CommandOutput parseOutput(std::string output, const MsgConfig& msg_cfg);
	std::optional<Message> parseMessageIntoCmdOutput(CommandOutput& cmd_out, std::string_view line, const MsgConfig& msg_cfg);

	struct Vivado
	{
		~Vivado();
		Vivado(stdfs::path vivado_path, const MsgConfig& msg_config);
		Vivado(stdfs::path vivado_path, const MsgConfig& msg_config, stdfs::path working_dir);
		Vivado(stdfs::path vivado_path, const MsgConfig& msg_config, const std::vector<std::string>& args,
			stdfs::path working_dir, bool run_init);

		Vivado(Vivado&&) = default;
		Vivado& operator= (Vivado&&) = default;

		template <typename... Args>
		CommandOutput runCommand(const char* fmt, Args&&... args)
		{
			return this->run_command(zpr::sprint(fmt, static_cast<Args&&>(args)...));
		}

		template <typename... Args>
		CommandOutput streamCommand(const char* fmt, Args&&... args)
		{
			return this->stream_command(zpr::sprint(fmt, static_cast<Args&&>(args)...));
		}

		template <typename... Args>
		void runCommandAsync(const char* fmt, Args&&... args)
		{
			return this->run_command_async(zpr::sprint(fmt, static_cast<Args&&>(args)...));
		}

		void closeProject();
		bool haveConstraintFile(const std::string& xdc) const;
		CommandOutput addConstraintFile(const std::string& xdc);

		void setMsgConfig(const MsgConfig& msg_cfg);
		const MsgConfig& getMsgConfig() const;

		void waitForPrompt();
		bool isCommandDone();

		bool partExists(const std::string& part) const;

		bool alive() const;
		void close(bool quiet = false);
		void forceClose();
		void relaunchWithArgs(const std::vector<std::string>& args,
			const std::optional<stdfs::path>& working_dir = std::nullopt);

		stdfs::path workingDirectory() const { return m_working_dir; }

	private:
		const MsgConfig* m_msg_config = nullptr;

		stdfs::path m_vivado_path;
		stdfs::path m_working_dir;

		zpp::Process m_process;
		std::string m_output_buffer;
		util::hashset<std::string> m_parts_list;
		util::hashset<std::string> m_added_constraints;

		void send_prompt_marker();
		void run_command_async(const std::string& cmd);
		CommandOutput run_command(const std::string& cmd);
		CommandOutput stream_command(const std::string& cmd);
	};

	struct Project;

	// this one starts a new vivado instance with a new project
	zst::Failable<std::string> runGuiAndWaitForJournal(const Project& proj, bool ip_project,
		std::function<zst::Failable<std::string> (Vivado&)> run_setup,
		std::function<bool (std::span<std::string_view>)> callback);

	// this one uses an existing vivado instance, and assumes that the gui is already open
	// and vivado was launched with the correct journal option.
	zst::Failable<std::string> waitForJournalOnGui(const Project& proj, Vivado& vivado,
		stdfs::path journal, std::function<bool (std::span<std::string_view>)> callback);

}
