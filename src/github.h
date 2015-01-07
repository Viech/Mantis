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

#ifndef GITHUBQUERY_H
#define GITHUBQUERY_H

#include <iostream>

#include "common.h"

class GitHubQuery
{
public:

	GitHubQuery(std::string owner, std::string repo, bool useColor);

	std::string linkIssue(int issue, bool verbose);
	std::string linkCommit(std::string hash, bool verbose);

private:

	static size_t curlWriteToStream(char *input, size_t size, size_t count, void *ostringstreamPtr);
	bool exists(std::string category, std::string resource);

	std::string owner;
	std::string repo;

	bool useColor;

};

#endif // GITHUBQUERY_H
