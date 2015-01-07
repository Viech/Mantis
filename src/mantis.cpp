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

#include <iostream>
#include <sstream>
#include <list>

#include "config.h"
#include "ircclient.h"
#include "unvquery.h"
#include "github.h"

using namespace std;

int main(int argc, char **argv)
{
	MantisConfig      *config;
	list<IRCClient *> ircClients;

	// load configuration
	{
		string configFile;

		if (argc > 1)
		{
			configFile = argv[1];
		}
		else
		{
			cerr << "Usage: " << argv[0] << " <config>" << endl;
			return -1;
		}

		try
		{
			config = new MantisConfig(configFile);
		}
		catch (libconfig::FileIOException)
		{
			cerr << FATAL << "Failed to parse " << configFile << "." << endl;
			return -2;
		}
		catch (libconfig::ParseException e)
		{
			cerr << FATAL << e.getError() << " on line " << e.getLine() << " of " << e.getFile() << "." << endl;
			return -3;
		}
	}

	const libconfig::Setting &cfgRoot = config->getRoot();
	const libconfig::Setting &globals = cfgRoot["globals"];

	// global config
	bool useColor = (bool)globals["usecolor"];

	// start irc clients
	{
		const libconfig::Setting &cfg     = cfgRoot["irc"];
		const libconfig::Setting &servers = cfg["servers"];

		for (int serverNum = 0; serverNum < servers.getLength(); serverNum++)
		{
			const libconfig::Setting &server   = servers[serverNum];
			const libconfig::Setting &channels = server["channels"];

			string host     = server["host"];
			string password = server["password"];
			string nick     = server["nick"];
			int    port     = server["port"];

			IRCClient   *ircClient   = new IRCClient(host, port, password, nick);
			GitHubQuery *gitHubQuery;
			UnvQuery    *unvQuery;
			Calendar    *calendar;

			// init unvanquished query module
			{
				const libconfig::Setting &cfg = cfgRoot["unvquery"];

				string master   = cfg["master"];
				int    port     = cfg["port"];
				int    protocol = cfg["protocol"];

				try
				{
					unvQuery = new UnvQuery(master, port, protocol, useColor);
				}
				catch (int error)
				{
					return error - 100;
				}
			}

			// init calendar module
			{
				const libconfig::Setting &cfg    = cfgRoot["calendar"];
				const libconfig::Setting &events = cfg["events"];

				calendar = new Calendar(useColor);

				for (int eventNum = 0; eventNum < events.getLength(); eventNum++)
				{
					const libconfig::Setting &event = events[eventNum];

					int    start       = event["start"];
					int    warnTime    = event["warnTime"];
					int    warnCount   = event["warnCount"];
					string period      = event["period"];
					string description = event["description"];

					ostringstream stream;

					stream << start << " " << warnTime << " " << warnCount << " " << period << " " << description;

					calendar->addEvent(stream.str());
				}
			}

			// init GitHub query module
			{
				const libconfig::Setting &cfg = cfgRoot["github"];

				string owner = cfg["owner"];
				string repo  = cfg["repository"];

				gitHubQuery = new GitHubQuery(owner, repo, useColor);
			}

			// add modules
			ircClient->addUnvQuery(unvQuery);
			ircClient->addCalendar(calendar);
			ircClient->addGitHubQuery(gitHubQuery);

			// join channels
			for (int chanNum = 0; chanNum < channels.getLength(); chanNum++)
			{
				const libconfig::Setting &channel = channels[chanNum];

				string name     = channel["name"];
				string password = channel["password"];

				int broadcastFlags;

				if (channel.exists("noBroadcasts") && (bool)channel["noBroadcasts"])
				{
					broadcastFlags = BROADCAST_NONE;
				}
				else
				{
					 broadcastFlags = BROADCAST_ALL;

					if (channel.exists("noPlayerPeekBroadcasts") && (bool)channel["noPlayerPeekBroadcasts"])
					{
						broadcastFlags &= ~BROADCAST_PLAYERPEEK;
					}

					if (channel.exists("noEventBroadcasts") && (bool)channel["noEventBroadcasts"])
					{
						broadcastFlags &= ~BROADCAST_EVENT;
					}
				}

				ircClient->join(name, password, broadcastFlags);
			}

			ircClients.push_back(ircClient);
		}
	}

	// wait for irc clients to terminate
	for (IRCClient *ircClient : ircClients)
	{
		ircClient->threadJoin();
	}

	return 0;
}
