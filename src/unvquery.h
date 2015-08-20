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

#ifndef UNVINFO_H
#define UNVINFO_H

#include <netdb.h>
#include <iostream>

#include "common.h"

// Maximum number of servers to query
// TODO: Remove 'empty' from GETSERVERSQUERY so this is always sufficient
#define MAX_SERVERS 128

// Maximum number of players on a server
#define MAX_PLAYERS 64

// Timeout for queries in seconds
#define TIMEOUT_S   2

// Minimum query periods for printActiveServers
#define PRINTACTIVESERVERS_LISTPERIOD   120
#define PRINTACTIVESERVERS_STATUSPERIOD 10

// Minimum query periods for checkPeekActivity
#define CHECKPEEKACTIVITY_LISTPERIOD    120
#define CHECKPEEKACTIVITY_STATUSPERIOD  10

// Query and response constants
#define PREFIX             "\xff\xff\xff\xff"
#define GETSERVERSQUERY    PREFIX "getservers %d full empty"
#define GETSERVERSRESPONSE PREFIX "getserversResponse"
#define GETSTATUSQUERY     PREFIX "getstatus"
#define GETSTATUSRESPONSE  PREFIX "statusResponse\n"

// Error message substrings
#define PARSINGSTATUS      "Parsing status report: "

class UnvQuery
{
public:

	/**
	 * @param master   Hostname or address of master server
	 * @param port     Port of master server
	 * @param protocol Protocol number of game servers
	 * @param useColor Whether to use BB style codes in responses
	 */
	UnvQuery(std::string master, unsigned short port, unsigned short protocol, bool useColor);

	/**
	 * @param useColor Whether to use BB style codes in responses
	 */
	void           setUseColor(bool useColor);

	/**
	 * @return Whether BB style codes are used in responses
	 */
	bool           usesColor();

	/**
	 * @brief Refresh server list and server status for every known server.
	 * @param minServersQueryPeriod Don't refresh server list more frequently than this.
	 * @param minStatusQueryPeriod  Don't refresh server status more frequently than this.
	 * @return
	 */
	bool           refresh(time_t minServersQueryPeriod, time_t minStatusQueryPeriod);

	/**
	 * @brief Refresh server list and server status for every known server.
	 * @return
	 */
	bool           refresh();

	/**
	 * @brief Refresh server list.
	 * @param minPeriod Don't refresh server list more frequently than this.
	 * @return
	 */
	bool           refreshServerList(time_t minPeriod);

	/**
	 * @brief Refresh server list.
	 * @return
	 */
	bool           refreshServerList();

	/**
	 * @brief Refresh server status for every known server.
	 * @param minPeriod Don't refresh server status more frequently than this.
	 * @return
	 */
	bool           refreshServerStatus(time_t minPeriod);

	/**
	 * @brief Refresh server status for every known server.
	 * @return
	 */
	bool           refreshServerStatus();

	/**
	 * @return Number of responsive game servers.
	 */
	int            numberResponsiveServers();

	std::string    printServerLine(int serverNum);

	/**
	 * @brief Prints a list of servers with players on a team on them.
	 * @return The list as a newline seperated string.
	 */
	std::string    printActiveServers();

	std::string    checkPeekActivity(time_t period, int minPlayers);

private:

	typedef enum team_e
	{
		FREE_SLOT,

		TEAM_SPEC,

		TEAM_1,
		TEAM_2,

		NUM_TEAMS
	}
	team_t;

	typedef struct serverStatus_s
	{
		// IP address and port in human readable format
		char   addr[32];

		// Name of the server
		char   name[128];

		// Name of the map being played
		char   map[128];

		// Team of a client
		team_t clientTeam[MAX_PLAYERS];

		// Whether a client is a bot
		bool   isBot[MAX_PLAYERS];

		// Number of Clients/Players/Bots of a team
		int    numClientSlots;
		int    numClients[NUM_TEAMS];
		int    numPlayers[NUM_TEAMS];
		int    numBots   [NUM_TEAMS];
	}
	serverStatus_t;

	// parameters
	bool           useColor;

	// network
	int            masterSock;
	int            serverSock;
	sockaddr_in    masterAddr;
	char           getServersQuery[128];

	// rate limiting
	time_t         lastServerListQuery;
	time_t         lastServerStatusQuery;
	bool           serverListQuerySuccessful;
	bool           serverStatusQuerySuccessful;

	// server info
	int            numKnown, numResponsive;
	uint32_t       servers[MAX_SERVERS];
	uint16_t       ports[MAX_SERVERS];
	serverStatus_t status[MAX_SERVERS];

	// peek activity data
	typedef struct  peekActivity_e
	{
		time_t lastSeen;
		time_t lastInformed;
	}
	peekActivity_t;

	peekActivity_t peekActivityData[MAX_PLAYERS + 1];

	// parsers
	bool parseStatusResponse(int infoNum, const char *address, const char *response, size_t responseLen);
	int  parseStatusResponseField(int infoNum, const char *field, size_t maxLen);
	void parseStatusResponseKeyValue(int statusNum, const char *key, const char *value);
	void analyzeClientData(int statusNum);

	// helpers
	static void    stripColors(char *dst, const char *src, size_t maxChars);
};

#endif // UNVINFO_H
