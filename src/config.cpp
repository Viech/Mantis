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

#include "libconfig.h++"

#include "config.h"

using namespace std;
using namespace libconfig;

MantisConfig::MantisConfig(string path)
{
	this->parser = new Config();
	this->path   = path;

	this->reparse();
}

void MantisConfig::reparse()
{
	this->parser->readFile(path.c_str());
}

libconfig::Setting &MantisConfig::getRoot() const
{
	return this->parser->getRoot();
}
