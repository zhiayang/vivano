// util.h
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <cstddef>

#include <chrono>
#include <vector>
#include <string>
#include <optional>
#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace stdfs = std::filesystem;

namespace util
{
	// https://en.cppreference.com/w/cpp/container/unordered_map/find
	// stupid language
	struct hasher
	{
		using is_transparent = void;
		using H = std::hash<std::string_view>;

		size_t operator() (const char* str) const        { return H{}(str); }
		size_t operator() (std::string_view str) const   { return H{}(str); }
		size_t operator() (const std::string& str) const { return H{}(str); }

		template <typename A, typename B>
		size_t operator() (const std::pair<A, B>& p) const
		{
			return std::hash<A>{}(p.first) ^ std::hash<B>{}(p.second);
		}
	};

	template <typename K, typename V>
	using hashmap = std::unordered_map<K, V, hasher, std::equal_to<>>;

	template <typename T>
	using hashset = std::unordered_set<T, hasher, std::equal_to<>>;
}

namespace util
{
	std::string prettyPrintTime(std::chrono::steady_clock::duration dur);

	struct Timer
	{
		Timer();

		std::string print() const;
		std::chrono::steady_clock::duration measure() const;

		std::chrono::steady_clock::time_point start_time;
	};

	template <typename T>
	struct Defer
	{
		Defer(const T& lambda) : m_lambda(lambda) {}
		~Defer() { m_lambda(); }

	private:
		const T& m_lambda;
	};
}

namespace util
{
	std::string colourise(std::string_view sv, int severity);
	std::string prettyFormatTextBlock(const std::string& block, const char* leftMargin, const char* rightMargin,
		bool no_margin_on_first_line = false);

	stdfs::path getHomeFolder();
	size_t getTerminalWidth();

	std::string lowercase(std::string_view sv);

	std::optional<int> parseInt(std::string_view sv);

	std::string readEntireFile(std::string_view path);
	std::vector<std::string_view> splitString(std::string_view str, char delim);

	std::vector<stdfs::path> find_files_ext(const stdfs::path& dir, std::string_view ext);
	std::vector<stdfs::path> find_files_ext_recursively(const stdfs::path& dir, std::string_view ext);

	template <typename Iter, typename Predicate>
	inline void find_files_helper(std::vector<stdfs::path>& list, const stdfs::path& dir, Predicate&& pred)
	{
		// i guess this is not an error...?
		if(!stdfs::is_directory(dir))
			return;

		auto iter = Iter(dir);
		for(auto& ent : iter)
		{
			if((ent.is_regular_file() || ent.is_symlink()) && pred(ent))
				list.push_back(ent.path());
		}
	}

	/*
		Search for files in the given directory (non-recursively), returning a list of paths
		that match the given predicate. Note that the predicate should accept a `stdfs::directory_entry`,
		*NOT* a `stdfs::path`.
	*/
	template <typename Predicate>
	inline std::vector<stdfs::path> find_files(const stdfs::path& dir,
		Predicate&& pred)
	{
		std::vector<stdfs::path> ret {};
		find_files_helper<stdfs::directory_iterator>(ret, dir, pred);
		return ret;
	}

	/*
		Same as `find_files`, but recursively traverses directories.
	*/
	template <typename Predicate>
	inline std::vector<stdfs::path> find_files_recursively(const stdfs::path& dir,
		Predicate&& pred)
	{
		std::vector<stdfs::path> ret {};
		find_files_helper<stdfs::recursive_directory_iterator>(ret, dir, pred);
		return ret;
	}

	/*
		Same as `find_files`, but in multiple (distinct) directories at once.
	*/
	template <typename Predicate>
	inline std::vector<stdfs::path> find_files(const std::vector<stdfs::path>& dirs,
		Predicate&& pred)
	{
		std::vector<stdfs::path> ret {};
		for(auto& dir : dirs)
			find_files_helper<stdfs::directory_iterator>(ret, dir, pred);
		return ret;
	}

	/*
		Same as `find_files_recursively`, but in multiple (distinct) directories at once.
	*/
	template <typename Predicate>
	inline std::vector<stdfs::path> find_files_recursively(const std::vector<stdfs::path>& dirs,
		Predicate&& pred)
	{
		std::vector<stdfs::path> ret {};
		for(auto& dir : dirs)
			find_files_helper<stdfs::recursive_directory_iterator>(ret, dir, pred);
		return ret;
	}
}
