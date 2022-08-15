// check.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include "args.h"
#include "vivado.h"
#include "vivano.h"
#include "project.h"

using zst::Ok;
using zst::Err;
using zst::ErrFmt;
using zst::Result;

namespace vvn
{
	Result<void, std::string> Project::check(Vivado& vivado, std::span<std::string_view> args) const
	{
		if(auto a = args::checkValidArgs(args, { }); a.has_value())
			return ErrFmt("unsupported option '{}', try '--help'", *a);

		this->read_files(vivado);

		vvn::log("running check_syntax");
		if(vivado.streamCommand("check_syntax").has_errors())
			return ErrFmt("one or more files had syntax errors");

		auto _ = vvn::LogIndenter();
		vvn::log("no issues found");

		return Ok();
	}
}
