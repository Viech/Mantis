/*
====================================================================
Copyright 2013-2014 Maximilian Stahlberg

This file is part of Mantis.

Mantis is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mantis is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mantis.  If not, see <http://www.gnu.org/licenses/>.
====================================================================
*/

#ifndef CONFIGPARSER_H
#define CONFIGPARSER_H

#include <iostream>

#include "libconfig.h++"

/**
 * @brief A simple wrapper to libconfig.
 */
class MantisConfig
{
public:

	/**
	 * @param path Path to config file.
	 */
	MantisConfig(std::string path);

	/**
	 * @brief Reread the config file.
	 */
	void reparse();

	/**
	 * @brief Get the root configuration node.
	 */
	libconfig::Setting &getRoot() const;

private:

	/**
	 * @brief Config parser.
	 */
	libconfig::Config *parser;

	/**
	 * @brief Path to config file.
	 */
	std::string path;
};

#endif // CONFIGPARSER_H
