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

#include <sstream>
#include <regex>
#include <algorithm>
#include <curl/curl.h>

#include "github.h"

using namespace std;

GitHubQuery::GitHubQuery(string owner, string repo, bool useColor)
	: owner(owner), repo(repo), useColor(useColor)
{}

string GitHubQuery::linkIssue(int issue, bool verbose)
{
	ostringstream stream;
	ostringstream stringIssue;

	if (issue <= 0 || issue == INT_MAX)
		return verbose ? "Invalid issue number." : "";

	stringIssue << issue;

	if (!exists("issues", stringIssue.str()))
		return verbose ? "Issue doesn't exist." : "";

	stream << "Issue " << B_ON << "#" << issue << B_OFF << ": https://github.com/" << owner << "/"
	       << repo << "/issues/" << issue;

	return stream.str();
}

string GitHubQuery::linkCommit(string hash, bool verbose)
{
	ostringstream stream;
	string        shortHash;
	regex         commitRE("[0-9a-fA-F]{7,40}");

	if (!regex_match(hash.begin(), hash.end(), commitRE))
		return verbose ? "Invalid commit hash." : "";

	std::transform(hash.begin(), hash.end(), hash.begin(), ::tolower);
	shortHash = hash.substr(0, 7);

	if (!exists("commits", hash))
		return verbose ? "Commit doesn't exist." : "";

	stream << "Commit " << B_ON << shortHash << B_OFF << ": https://github.com/" << owner << "/"
	       << repo << "/commit/" << shortHash;

	return stream.str();
}

size_t GitHubQuery::curlWriteToStream(char *input, size_t size, size_t count, void *ostringstreamPtr)
{
	if (ostringstreamPtr)
	{
		ostringstream &oss    = *static_cast<ostringstream*>(ostringstreamPtr);
		streamsize    written = size * count;
		if (oss.write(input, written)) return written;
	}

	return 0;
}

bool GitHubQuery::exists(string category, string resource)
{
	ostringstream stream, response;
	CURL          *curl = curl_easy_init();

	if (!curl) return false;

	stream << "https://api.github.com/repos/" << owner << "/" << repo << "/" << category << "/"
	       << resource;

	curl_easy_setopt(curl, CURLOPT_URL, stream.str().c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &curlWriteToStream);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "mantisbot");

	if(curl_easy_perform(curl) != CURLE_OK) return false;

	curl_easy_cleanup(curl);

	return (response.str().find("Not Found") == string::npos);
}
