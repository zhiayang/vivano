// vivado.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <thread>
#include <chrono>
#include <optional>

#include "util.h"
#include "vivano.h"
#include "vivado.h"
#include "progressbar.h"
#include "zprocpipe.h"

namespace vvn
{
	static constexpr std::string_view PROMPT_STRING = "@PROMPT@";
	static constexpr std::string_view PROMPT_STRING_NEWLINE = "@PROMPT@\n";

	static zpp::Process spawn_vivado(stdfs::path vivado_path, stdfs::path working_dir, std::vector<std::string> args)
	{
		if(args.empty())
			args = { "-mode", "tcl", "-notrace", "-nolog",  "-nojournal" };

		std::string vivado {};
		if(vivado_path.empty())
		{
			vivado = "vivado";
		}
		else
		{
			vivado_path = vivado_path / "bin" / "vivado";
			if(not stdfs::exists(vivado_path))
				vvn::error_and_exit("could not find vivado binary at '{}'", vivado_path.string());

			vivado = vivado_path.string();
		}

		auto proc = zpp::runProcess(vivado, args, working_dir);
		if(not proc.first.has_value())
			vvn::error_and_exit("failed to launch vivado: {}", proc.second);

		return std::move(*proc.first);
	}

	void Vivado::relaunchWithArgs(const std::vector<std::string>& args, const std::optional<stdfs::path>& working_dir)
	{
		auto cwd = working_dir.has_value() ? *working_dir : stdfs::current_path();

		m_working_dir = cwd;
		m_process.terminate();
		m_process = spawn_vivado(m_vivado_path, cwd, args);
	}

	void Vivado::send_prompt_marker()
	{
		m_process.sendLine(zpr::sprint("puts \"{}\"", PROMPT_STRING));
	}

	bool Vivado::partExists(const std::string& part) const
	{
		return m_parts_list.find(part) != m_parts_list.end();
	}

	void Vivado::close(bool quiet)
	{
		if(not m_process.isAlive())
			return;

		if(not quiet)
			vvn::log("waiting for vivado to close");

		m_process.sendLine("exit");

		using namespace std::chrono_literals;
		constexpr auto EXIT_TIMEOUT = 3s;

		auto start = std::chrono::steady_clock::now();
		while(m_process.isAlive())
		{
			std::this_thread::sleep_for(250ms);
			if(auto now = std::chrono::steady_clock::now(); now - start > EXIT_TIMEOUT)
			{
				m_process.terminateAll();
				m_process.wait();
				break;
			}
		}
	}

	Vivado::~Vivado()
	{
		this->close();
	}

	bool Vivado::alive() const
	{
		return m_process.isAlive();
	}

	void Vivado::forceClose()
	{
		m_process.terminateAll();
	}

	Vivado::Vivado(stdfs::path vivado_path, const MsgConfig& msg_config) : Vivado(vivado_path, msg_config, stdfs::current_path()) {}

	Vivado::Vivado(stdfs::path vivado_path, const MsgConfig& msg_config, stdfs::path working_dir)
		: m_msg_config(&msg_config), m_process(spawn_vivado(std::move(vivado_path), std::move(working_dir), {}))
	{
		m_working_dir = working_dir;

		using namespace std::chrono_literals;
		vvn::log("starting vivado...");
		auto timer = util::Timer();

		// wait for the prompt
		this->send_prompt_marker();
		this->waitForPrompt();

		constexpr std::string_view MARKER = "****** Vivado ";

		// split the current buffer into lines
		auto lines = util::splitString(m_output_buffer, '\n');
		if(lines.size() < 2 || lines[1].find(MARKER) != 0)
			vvn::error_and_exit("unexpected vivado output!\ngot:\n{}", m_output_buffer);

		lines[1].remove_prefix(MARKER.size());
		vvn::log("version: {}", lines[1]);

		{
			auto tmp = this->runCommand("puts [join [get_parts] \"\\n\"]");
			auto parts = util::splitString(tmp.content, '\n');
			for(auto part : parts)
				m_parts_list.emplace(part);
		}
		vvn::log("loaded {} parts in {}", m_parts_list.size(), timer.print());
	}

	CommandOutput Vivado::run_command(const std::string& cmd)
	{
		m_output_buffer.clear();

		m_process.sendLine(cmd);
		this->send_prompt_marker();
		this->waitForPrompt();

		assert(m_output_buffer.ends_with(PROMPT_STRING_NEWLINE));
		m_output_buffer.erase(m_output_buffer.size() - PROMPT_STRING_NEWLINE.size());

		return parseOutput(std::move(m_output_buffer), *m_msg_config);
	}

	void Vivado::run_command_async(const std::string& cmd)
	{
		m_output_buffer.clear();

		// make a thread and detach it.
		m_process.sendLine(cmd);
		this->send_prompt_marker();

		auto t = std::thread([this]() {
			std::string stdout;
			std::string stderr;
			while(true)
			{
				this->m_process.pollOutput(stdout, stderr);
				stderr.clear();

				if(stdout.ends_with(PROMPT_STRING_NEWLINE))
					break;
			}
		});

		t.detach();
	}



	static std::optional<std::string_view> consume_one_line(const std::string& str, size_t& start_idx)
	{
		auto sv = std::string_view(str).substr(start_idx);
		if(auto idx = sv.find('\n'); idx != std::string_view::npos)
		{
			start_idx += idx + 1;
			return sv.substr(0, idx);
		}
		else
		{
			return std::nullopt;
		}
	}

	CommandOutput Vivado::stream_command(const std::string& cmd)
	{
		using namespace std::chrono_literals;

		m_output_buffer.clear();

		CommandOutput cmd_out {};

		m_process.sendLine(cmd);
		this->send_prompt_marker();

		std::string stdout;
		std::string stderr;
		size_t stdout_skip_idx = 0;
		size_t stderr_skip_idx = 0;

		auto pbar = util::ProgressBar(static_cast<size_t>(2 * (1 + getLogIndent())), 30);

		auto start = std::chrono::steady_clock::now();
		auto last_pbar_update = start;

		while(true)
		{
			bool redraw_pbar = false;
			bool did_read = m_process.pollOutput(stdout, stderr, /* milliseconds: */ 50);

			if(did_read)
			{
				bool parsed = false;
				do {
					parsed = false;

					if(auto line = consume_one_line(stdout, stdout_skip_idx); line.has_value())
					{
						parsed = true;
						if(auto m = parseMessageIntoCmdOutput(cmd_out, *line, *m_msg_config); m.has_value())
							redraw_pbar |= m->print(*m_msg_config);
					}

					if(auto line = consume_one_line(stderr, stderr_skip_idx); line.has_value())
					{
						parsed = true;
						if(auto m = parseMessageIntoCmdOutput(cmd_out, *line, *m_msg_config); m.has_value())
							redraw_pbar |= m->print(*m_msg_config);
					}
				} while(parsed);
			}

			auto now = std::chrono::steady_clock::now();
			auto show_progress = (now - start) > 1000ms;
			if(now - start > 5000ms)
				pbar.showTime();

			if(show_progress && now - last_pbar_update >= util::ProgressBar::DEFAULT_INTERVAL)
			{
				last_pbar_update = now;
				redraw_pbar = true;
				pbar.update();
			}

			if(show_progress && redraw_pbar)
				pbar.draw();

			if(stdout.ends_with(PROMPT_STRING_NEWLINE))
				break;
		}

		pbar.clear();

		cmd_out.content = std::move(stdout);
		cmd_out.stderr_content = std::move(stderr);
		return cmd_out;
	}

	void Vivado::waitForPrompt()
	{
		while(true)
		{
			if(this->isCommandDone())
				return;

			using namespace std::chrono_literals;
			std::this_thread::sleep_for(50ms);
		}
	}

	bool Vivado::isCommandDone()
	{
		m_process.readStdout(m_output_buffer);
		return m_output_buffer.substr(0, m_output_buffer.size() - 1).ends_with(PROMPT_STRING);
	}

	void Vivado::closeProject()
	{
		this->runCommand("close_project");
		m_added_constraints.clear();
	}

	bool Vivado::haveConstraintFile(const std::string& xdc) const
	{
		return m_added_constraints.contains(xdc);
	}

	CommandOutput Vivado::addConstraintFile(const std::string& xdc)
	{
		m_added_constraints.insert(xdc);
		return this->streamCommand("read_xdc \"{}\"", xdc);
	}

	void Vivado::setMsgConfig(const MsgConfig& msg_cfg)
	{
		m_msg_config = &msg_cfg;
	}

	const MsgConfig& Vivado::getMsgConfig() const
	{
		return *m_msg_config;
	}

}
