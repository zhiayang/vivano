// project.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <filesystem>

#include "util.h"
#include "vivado.h"
#include "vivano.h"
#include "project.h"

namespace stdfs = std::filesystem;

namespace vvn
{
	static std::vector<std::string> find_files_by_extension(const stdfs::path& base, const stdfs::path& folder,
		std::string_view extension)
	{
		std::vector<std::string> files {};
		for(auto& p : util::find_files_ext_recursively(folder, extension))
			files.push_back(stdfs::relative(p, base).string());

		return files;
	}

	stdfs::path Project::get_bitstream_name() const
	{
		return m_build_folder / zpr::sprint("{}.bit", m_project_name);
	}

	Vivado Project::launchVivado(bool source_scripts) const
	{
		auto vivado = Vivado(m_vivado_dir, m_msg_config, m_location);
		if(source_scripts)
		{
			for(auto& tcl : m_tcl_scripts)
			{
				auto tmp = stdfs::relative(tcl, m_location);
				if(vivado.streamCommand("source \"{}\"", tmp.string()).has_errors())
					vvn::error("failed to source tcl script '{}'", tmp.string());
			}
		}
		return vivado;
	}

	Vivado Project::launchVivado(const std::vector<std::string>& args, stdfs::path working_dir,
		bool source_scripts, bool run_init) const
	{
		auto vivado = Vivado(m_vivado_dir, m_msg_config, args, working_dir, run_init);
		if(source_scripts)
		{
			for(auto& tcl : m_tcl_scripts)
			{
				auto tmp = stdfs::relative(tcl, m_location);
				if(vivado.streamCommand("source \"{}\"", tmp.string()).has_errors())
					vvn::error("failed to source tcl script '{}'", tmp.string());
			}
		}
		return vivado;
	}

	Project::Project(const ProjectConfig& config)
	{
		m_project_name = config.project_name;
		m_part_name = config.part_name;
		m_top_module = config.top_module;

		vvn::log("loaded project '{}'", m_project_name);

		m_location = config.location;
		m_build_folder = config.build_folder;

		m_ip_folder = config.ip_config.location;
		m_xci_folder = m_ip_folder / config.ip_config.xci_subdir;

		m_bd_folder = config.bd_config.location;
		m_bd_output_folder = m_bd_folder / config.bd_config.bd_subdir;

		m_msg_config = config.messages_config;
		m_msg_config.project_path = m_location;
		m_synthesised_dcp_name = config.synthesised_dcp_name;
		m_implemented_dcp_name = config.implemented_dcp_name;

		m_vivado_dir = config.vivado_installation_dir;
		if(auto s = m_vivado_dir.string(); s.starts_with("~/") || s.starts_with("~\\"))
			m_vivado_dir = stdfs::canonical(util::getHomeFolder() / s.substr(2));

		for(auto& tcl : config.sources_config.tcl_scripts)
			m_tcl_scripts.push_back(m_location / tcl);

		// source files and constraints
		if(config.sources_config.auto_find_sources)
		{
			const auto& root_dir = m_location;
			const auto& src_dir = config.sources_config.location;
			{
				const auto& hdl_dir = src_dir / config.sources_config.hdl_subdir;
				auto vhds = find_files_by_extension(root_dir, hdl_dir, ".vhd");
				auto vhdls = find_files_by_extension(root_dir, hdl_dir, ".vhdl");

				m_vhdl_sources.insert(m_vhdl_sources.end(), vhds.begin(), vhds.end());
				m_vhdl_sources.insert(m_vhdl_sources.end(), vhdls.begin(), vhdls.end());

				m_verilog_sources = find_files_by_extension(root_dir, hdl_dir, ".v");
				m_systemverilog_sources = find_files_by_extension(root_dir, hdl_dir, ".sv");
			}

			{
				constexpr auto vec_contains = [&](const std::vector<std::string>& xs, const std::string& x) -> bool {
					return std::find(xs.begin(), xs.end(), x) != xs.end();
				};

				const auto& xdc_dir = src_dir / config.sources_config.xdc_subdir;
				for(auto& src : find_files_by_extension(root_dir, xdc_dir, ".xdc"))
				{
					auto rel_path = stdfs::relative(src, xdc_dir).string();

					// impl if it's not synth_only, and synth if it's not impl_only
					bool is_synth_only_xdc = vec_contains(config.sources_config.synth_only_xdcs, rel_path);
					bool is_impl_only_xdc = vec_contains(config.sources_config.impl_only_xdcs, rel_path);

					if(is_synth_only_xdc)
						m_synth_constraints.push_back(src);

					if(is_impl_only_xdc)
						m_impl_constraints.push_back(src);

					if(not is_synth_only_xdc && not is_impl_only_xdc)
					{
						m_synth_constraints.push_back(src);
						m_impl_constraints.push_back(src);
					}
				}
			}
		}
		else
		{
			vvn::error_and_exit("unsupported");
		}


		// ip
		if(config.ip_config.auto_find_sources)
		{
			for(auto& tcl : util::find_files_ext(m_ip_folder, ".tcl"))
			{
				IpInstance inst {};
				inst.tcl = tcl;
				inst.name = tcl.stem().string();
				inst.xci = m_xci_folder
					/ inst.name
					/ stdfs::path(inst.name).replace_extension(".xci");

				inst.is_global = config.ip_config.global_ips.contains(inst.name);
				m_ip_instances.push_back(std::move(inst));
			}

			// sort the IPs because why not
			std::sort(m_ip_instances.begin(), m_ip_instances.end(), [](auto& a, auto& b) {
				if(a.is_global == b.is_global)
					return a.name < b.name;

				return b.is_global;
			});
		}
		else
		{
			vvn::error_and_exit("unsupported");
		}

		// bd
		if(config.bd_config.auto_find_sources)
		{
			for(auto& tcl : util::find_files_ext(m_bd_folder, ".tcl"))
			{
				BdInstance inst {};
				inst.tcl = tcl;
				inst.name = tcl.stem().string();
				inst.bd = m_bd_output_folder
					/ inst.name
					/ stdfs::path(inst.name).replace_extension(".bd");

				m_bd_instances.push_back(std::move(inst));
			}

		}
		else
		{
			vvn::error_and_exit("unsupported");
		}
	}

	const IpInstance* Project::getIpWithName(std::string_view name) const
	{
		for(auto& ip : m_ip_instances)
		{
			if(ip.name == name)
				return &ip;
		}
		return nullptr;
	}

	const BdInstance* Project::getBdWithName(std::string_view name) const
	{
		for(auto& bd : m_bd_instances)
		{
			if(bd.name == name)
				return &bd;
		}
		return nullptr;
	}
}
