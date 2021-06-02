// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2021, The Qwertycoin Group.
//
// This file is part of Qwertycoin.
//
// Qwertycoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Qwertycoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Qwertycoin.  If not, see <http://www.gnu.org/licenses/>.

#include <Common/CommandLine.h>
#include <Common/Util.h>
#include <CryptoNoteCore/CoreConfig.h>

namespace CryptoNote {

CoreConfig::CoreConfig()
{
    configFolder = Tools::getDefaultDataDirectory();
}

void CoreConfig::init(const boost::program_options::variables_map &options)
{
    if (options.count(command_line::arg_data_dir.name) != 0
        && (!options[command_line::arg_data_dir.name].defaulted()
            || configFolder == Tools::getDefaultDataDirectory())
        ) {
        configFolder = command_line::get_arg(options, command_line::arg_data_dir);
        configFolderDefaulted = options[command_line::arg_data_dir.name].defaulted();
    }

	if (options.count(command_line::arg_db_type.name) != 0 &&
		(!options[command_line::arg_db_type.name].defaulted() ||
		cDBType == Tools::getDefaultDBType())) {
		cDBType = command_line::get_arg(options, command_line::arg_db_type);
		bDBTypeDefaulted = options[command_line::arg_db_type.name].defaulted();
	}

	if (options.count(command_line::arg_db_sync_mode.name) != 0 &&
		(!options[command_line::arg_db_sync_mode.name].defaulted() ||
		cDBSyncMode == Tools::getDefaultDBSyncMode())) {
		cDBSyncMode = command_line::get_arg(options, command_line::arg_db_sync_mode);
		bDBSyncModeDefaulted = options[command_line::arg_db_sync_mode.name].defaulted();
	}
}

void CoreConfig::initOptions(boost::program_options::options_description &desc)
{
}

} // namespace CryptoNote
