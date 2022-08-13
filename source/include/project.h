// project.h
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <cstdint>
#include <cstddef>

#include <span>
#include <vector>
#include <string>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include <zst.h>

#include "util.h"
#include "msgconfig.h"

namespace stdfs = std::filesystem;

namespace vvn
{
	static constexpr const char* PROJECT_JSON_FILENAME = "vivano-project.json";

	static constexpr const char* PROJECT_SYNTHESISED_DCP_NAME = "synthesised.dcp";
	static constexpr const char* PROJECT_IMPLEMENTED_DCP_NAME = "implemented.dcp";

	struct ProjectConfig
	{
		std::string part_name;
		std::string project_name;
		std::string top_module;

		stdfs::path location;
		stdfs::path build_folder;

		std::string synthesised_dcp_name;
		std::string implemented_dcp_name;

		int min_message_severity;
		bool print_message_ids;

		struct {
			stdfs::path location;
			std::string hdl_subdir;
			std::string xdc_subdir;
			std::string sim_subdir;

			std::vector<std::string> impl_only_xdcs;
			std::vector<std::string> synth_only_xdcs;

			bool auto_find_sources;

		} sources_config;

		struct {
			stdfs::path location;
			std::string xci_subdir;

			bool auto_find_sources;

		} ip_config;

		MsgConfig messages_config;
	};

	void createProject(std::span<std::string_view> args);

	zst::Result<ProjectConfig, std::string> parseProjectJson(std::string_view json_path);
	zst::Result<void, std::string> writeDefaultProjectJson(const std::string& part, const std::string& proj);

	struct Vivado;

	struct Project
	{
		Project(const ProjectConfig& config);

		zst::Result<void, std::string> setup(Vivado& vivado) const;
		zst::Result<void, std::string> clean(std::span<std::string_view> args) const;
		zst::Result<void, std::string> buildAll(Vivado& vivado, std::span<std::string_view> args) const;
		// zst::Result<void, std::string> elaborate(Vivado& vivado) const;

		zst::Result<bool, std::string> synthesise(Vivado& vivado, std::span<std::string_view> args) const;

		auto implement(Vivado& vivado, std::span<std::string_view> args) const
		{
			return this->implement(vivado, std::move(args), /* use_prev: */ false);
		}

		auto writeBitstream(Vivado& vivado, std::span<std::string_view> args) const
		{
			return this->writeBitstream(vivado, std::move(args), /* use_prev: */ false);
		}


		struct IpInst
		{
			std::string name;
			stdfs::path tcl;
			stdfs::path xci;
		};

		const MsgConfig& getMsgConfig() const { return m_msg_config; }

		const std::string& getPartName() const { return m_part_name; }
		const std::string& getProjectName() const { return m_project_name; }

		stdfs::path getProjectLocation() const { return m_location; }
		stdfs::path getIpLocation() const { return m_ip_folder; }
		stdfs::path getIpOutputsLocation() const { return m_xci_folder; }
		stdfs::path getBuildFolder() const { return m_build_folder; }

	private:
		zst::Result<bool, std::string> implement(Vivado& vivado, std::span<std::string_view> args, bool use_prev) const;
		zst::Result<bool, std::string> writeBitstream(Vivado& vivado, std::span<std::string_view> args, bool use_prev) const;

		zst::Result<void, std::string> reload_project(Vivado& vivado) const;
		zst::Result<void, std::string> read_files(Vivado& vivado) const;

		bool should_resynthesise(Vivado& vivado) const;
		bool should_reimplement(Vivado& vivado, bool allow_stale) const;
		bool should_rewrite_bitstream(Vivado& vivado, bool allow_stale) const;

		stdfs::path get_bitstream_name() const;

		std::string m_project_name;
		std::string m_part_name;
		std::string m_top_module;

		stdfs::path m_location;
		stdfs::path m_build_folder;
		stdfs::path m_ip_folder;
		stdfs::path m_xci_folder;

		std::string m_synthesised_dcp_name;
		std::string m_implemented_dcp_name;

		MsgConfig m_msg_config;

		// in-memory state
		std::vector<std::string> m_vhdl_sources;
		std::vector<std::string> m_verilog_sources;
		std::vector<std::string> m_systemverilog_sources;

		std::vector<std::string> m_sim_vhdl_sources;
		std::vector<std::string> m_sim_verilog_sources;
		std::vector<std::string> m_sim_systemverilog_sources;

		std::vector<std::string> m_synth_constraints;
		std::vector<std::string> m_impl_constraints;

		std::vector<IpInst> m_ip_instances;
	};

}
