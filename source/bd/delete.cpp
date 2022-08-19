// delete.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include "bd.h"
#include "util.h"
#include "vivado.h"
#include "vivano.h"
#include "project.h"

using zst::Result;
using zst::Failable;

namespace vvn::bd
{
	Failable<std::string> deleteBlockDesign(const Project& proj, std::string_view bd_name)
	{
		auto bd = proj.getBdWithName(bd_name);
		if(bd == nullptr)
			return ErrFmt("block design '{}' does not exist; try 'bd list'", bd_name);

		vvn::log("deleting block design '{}'", bd_name);

		if(stdfs::exists(bd->tcl))
		{
			zpr::println("{}- {}", indentStr(1), bd->tcl.string());
			stdfs::remove(bd->tcl);
		}

		if(auto p = bd->bd.parent_path(); stdfs::exists(p))
		{
			zpr::println("{}- {}", indentStr(1), p.string());
			stdfs::remove_all(p);
		}

		return Ok();
	}
}
