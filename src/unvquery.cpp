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
#include <cstring>
#include <chrono>
#include <netdb.h>
#include <arpa/inet.h>

#include "unvquery.h"

using namespace std;

void UnvQuery::stripColors(char *dst, const char *src, size_t maxChars)
{
	bool sequence = false;
	unsigned int s, d;

	for (s = 0, d = 0; d < maxChars - 1 && s < strlen(src); s++)
	{
		if (sequence)
		{
			if (src[s] == '^')
			{
				dst[d++] = src[s];
			}

			sequence = false;
		}
		else
		{
			if (src[s] == '^')
			{
				sequence = true;
			}
			else
			{
				dst[d++] = src[s];
			}
		}
	}

	dst[d] = '\0';
}

UnvQuery::UnvQuery(string master, unsigned short port, unsigned short protocol, bool useColor)
{
	hostent     *targetHost;
	sockaddr_in masterLocalAddr;
	sockaddr_in serverLocalAddr;
	timeval     timeout;

	// copy parameters
	this->useColor = useColor;

	// init timers and confitions
	this->lastServerListQuery   = 0;
	this->lastServerStatusQuery = 0;
	this->serverListQuerySuccessful   = false;
	this->serverStatusQuerySuccessful = false;
	memset(&this->peekActivityData, 0, sizeof(this->peekActivityData));

	// build query strings
	snprintf(this->getServersQuery, sizeof(this->getServersQuery), GETSERVERSQUERY, protocol);

	// resolve master address
	targetHost = gethostbyname(master.c_str());

	// assemble master bind address
	memset(&masterLocalAddr, 0, sizeof(masterLocalAddr));
	masterLocalAddr.sin_family = AF_INET;
	masterLocalAddr.sin_port   = 0;
	masterLocalAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	// assemble server bind address
	memset(&serverLocalAddr, 0, sizeof(serverLocalAddr));
	serverLocalAddr.sin_family = AF_INET;
	serverLocalAddr.sin_port   = 0;
	serverLocalAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	// assemble master target address
	memset(&this->masterAddr, 0, sizeof(this->masterAddr));
	this->masterAddr.sin_family = AF_INET;
	this->masterAddr.sin_port   = htons(port);
	memcpy((void *)&this->masterAddr.sin_addr, targetHost->h_addr_list[0], targetHost->h_length);

	// create master socket
	this->masterSock = socket(AF_INET, SOCK_DGRAM, 0);
	if (this->masterSock < 0)
	{
		cerr << FATAL << "Failed to create socket." << endl;
		throw -1;
	}

	// set master socket options
	timeout.tv_sec  = TIMEOUT_S;
	timeout.tv_usec = 0;
	if (setsockopt(this->masterSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
	{
		cerr << FATAL << "Failed to set socket options." << endl;
		throw -2;
	}

	// bind master socket
	if (bind(this->masterSock, (sockaddr *)&masterLocalAddr, sizeof(masterLocalAddr)) < 0)
	{
		cerr << FATAL << "Failed to bind socket to " << port << "." << endl;
		throw -3;
	}

	// create server socket
	this->serverSock = socket(AF_INET, SOCK_DGRAM, 0);
	if (this->serverSock < 0)
	{
		cerr << FATAL << "Failed to create socket." << endl;
		throw -1;
	}

	// set server socket options
	timeout.tv_sec  = TIMEOUT_S;
	timeout.tv_usec = 0;
	if (setsockopt(this->serverSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
	{
		cerr << FATAL << "Failed to set socket options." << endl;
		throw -2;
	}

	// bind server socket
	if (bind(this->serverSock, (sockaddr *)&serverLocalAddr, sizeof(serverLocalAddr)) < 0)
	{
		cerr << FATAL << "Failed to bind socket to " << port << "." << endl;
		throw -3;
	}
}

void UnvQuery::setUseColor(bool useColor)
{
	this->useColor = useColor;
}

bool UnvQuery::usesColor()
{
	return this->useColor;
}

bool UnvQuery::refresh(time_t minListQueryPeriod, time_t minStatusQueryPeriod)
{
	return ( this->refreshServerList(minListQueryPeriod) &&
	         this->refreshServerStatus(minStatusQueryPeriod) );
}

bool UnvQuery::refresh()
{
	return ( this->refreshServerList() &&
	         this->refreshServerStatus() );
}

bool UnvQuery::refreshServerList(time_t minPeriod)
{
	if (this->lastServerListQuery + MAX(minPeriod, TIMEOUT_S + 1) <= time(NULL))
	{
		return refreshServerList();
	}
	else if (!this->serverListQuerySuccessful &&
	         this->lastServerListQuery + TIMEOUT_S < time(NULL))
	{
		return refreshServerList();
	}
	else
	{
		return this->serverListQuerySuccessful;
	}
}

bool UnvQuery::refreshServerStatus(time_t minPeriod)
{
	if (this->lastServerStatusQuery + MAX(minPeriod, TIMEOUT_S + 1) <= time(NULL))
	{
		return refreshServerStatus();
	}
	else if (!this->serverStatusQuerySuccessful &&
	         this->lastServerStatusQuery + TIMEOUT_S < time(NULL))
	{
		return refreshServerStatus();
	}
	else
	{
		return this->serverStatusQuerySuccessful;
	}
}

bool UnvQuery::refreshServerList()
{
	char          response[1024];
	unsigned char ip[4], port[2];
	int           responseLen = 0, position;

	this->lastServerListQuery = time(NULL);

	// send master request
	sendto(this->masterSock, this->getServersQuery, strlen(this->getServersQuery), 0,
	        (sockaddr *)&this->masterAddr, sizeof(this->masterAddr));

	response[0] = '\0';

	// read until we get a fitting response or timeout
	while ( strncmp(response, GETSERVERSRESPONSE, strlen(GETSERVERSRESPONSE)) != 0 )
	{
		responseLen = recv(this->masterSock, response, sizeof(response), 0);

		// check for timeout/error
		if ( responseLen < 0 )
		{
			cout << ERROR << "Failed to query master for servers." << endl;

			this->serverListQuerySuccessful = false;
			return false;
		}
	}

	position = strlen(GETSERVERSRESPONSE);

	// extract servers
	for ( this->numKnown = 0; this->numKnown < MAX_SERVERS; this->numKnown++ )
	{
		if ( position >= responseLen )
		{
			break;
		}

		if ( sscanf(response + position, "\x5c%4c%2c", ip, port) != 2 )
		{
			break;
		}

		position += 7;

		// extract address in NBO
		this->servers[this->numKnown] = htonl((ip[0] << 24) + (ip[1] << 16) + (ip[2] << 8) + ip[3]);
		this->ports[this->numKnown]   = htons((port[0] << 8) + port[1]);
	}

	this->serverListQuerySuccessful = true;
	return true;
}

bool UnvQuery::refreshServerStatus()
{
	sockaddr_in serverAddr;
	char        response[1024], addrStr[32];
	int         responseLen, serverNum;
	socklen_t   serverAddrLen;

	this->lastServerStatusQuery = time(NULL);

	this->numResponsive = 0;

	if (this->numKnown == 0)
	{
		this->serverStatusQuerySuccessful = true;
		return true;
	}

	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;

	// request status from all known servers
	for ( serverNum = 0; serverNum < this->numKnown; serverNum++ )
	{
		// assemble server address
		serverAddr.sin_addr.s_addr = this->servers[serverNum];
		serverAddr.sin_port = this->ports[serverNum];

		// request server status
		sendto(this->serverSock, GETSTATUSQUERY, strlen(GETSTATUSQUERY), 0, (sockaddr *)&serverAddr, sizeof(serverAddr));
	}

	// receive status responses
	for ( serverNum = 0; serverNum < MAX_SERVERS; serverNum++ )
	{
		serverAddrLen = sizeof(serverAddr);
		responseLen = recvfrom(this->serverSock, response, sizeof(response), 0, (sockaddr *)&serverAddr, &serverAddrLen);

		if ( responseLen < 0 )
		{
			// assume timeout
			break;
		}

		// retrieve human readable server address
		snprintf(addrStr, sizeof(addrStr), "%s:%d", inet_ntoa(serverAddr.sin_addr), ntohs(serverAddr.sin_port));

		// parse the response
		if ( parseStatusResponse(this->numResponsive, addrStr, response, responseLen) )
		{
			// analyze data found in client and bot items
			this->analyzeClientData(this->numResponsive++);
		}
	}

	this->serverStatusQuerySuccessful = ( this->numResponsive > 0 );
	return this->serverStatusQuerySuccessful;
}

bool UnvQuery::parseStatusResponse(int infoNum, const char *address, const char *response, size_t responseLen)
{
	serverStatus_t *ss = &this->status[infoNum];

	size_t pos;
	int    fieldLen;

	if ( strncmp(response, GETSTATUSRESPONSE, strlen(GETSTATUSRESPONSE)) != 0 )
	{
		cout << "Quake3Query: Bad getstatus response received from address " << address << "." << endl;
		return false;
	}

	memset(ss, 0, sizeof(serverStatus_t));

	// copy human readable address
	strncpy(ss->addr, address, sizeof(ss->addr));

	pos = strlen(GETSTATUSRESPONSE);

	while ( pos < responseLen && response[pos] == '\\' )
	{
		fieldLen = this->parseStatusResponseField(infoNum, address, response + pos, responseLen - pos);

		if (fieldLen <= 0)
		{
			return false;
		}

		pos += fieldLen;
	}

	return true;
}

int UnvQuery::parseStatusResponseField(int statusNum, const char *address, const char *field, size_t maxLen)
{
	char   key[1024], value[1024];
	size_t pos = 0, keyPos = 0, valuePos = 0;

	// jump over first backslash
	if ( field[pos++] != '\\' )
	{
		cout << NOTICE << PARSINGSTATUS << "Field doesn't start with a backslash in status packet from address " << address << "." << endl;
		return -1;
	}

	// parse key
	while ( true )
	{
		if ( keyPos >= sizeof(key) )
		{
			cout << NOTICE << PARSINGSTATUS << "Key too big to parse in status packet from address " << address << "." << endl;
			return -1;
		}

		if ( pos >= maxLen )
		{
			cout << NOTICE << PARSINGSTATUS << "End of field while parsing key in status packet from address " << address << "." << endl;
			return -1;
		}

		if ( field[pos] == '\\' )
		{
			key[keyPos] = '\0';
			pos++; // jump over backslash seperating key/value
			break;
		}

		key[keyPos++] = field[pos++];
	}

	// sanity check key
	if (keyPos == 0)
	{
		cout << NOTICE << PARSINGSTATUS << "Found empty key in status packet from address " << address << "." << endl;
		return -1;
	}

	// parse value
	while ( true )
	{
		if ( valuePos >= sizeof(value) )
		{
			cout << NOTICE << PARSINGSTATUS << "Value too big to parse in status packet from address " << address << "." << endl;
			return -1;
		}

		if ( pos >= maxLen )
		{
			value[valuePos] = '\0';
			break;
		}

		if (field[pos] == '\\' || field[pos] == '\n')
		{
			value[valuePos] = '\0';
			break;
		}

		value[valuePos++] = field[pos++];
	}

	// sanity check value
	if (valuePos == 0)
	{
		cout << NOTICE << PARSINGSTATUS << "Found empty value for key " << key << " in status packet from address " << address << "." << endl;
		return -1;
	}

	this->parseStatusResponseKeyValue(statusNum, key, value);

	return pos;
}

void UnvQuery::parseStatusResponseKeyValue(int statusNum, const char *key, const char *value)
{
	serverStatus_t *ss = &this->status[statusNum];

	if (strcmp(key, "P") == 0)
	{
		ss->numClientSlots = MIN(strlen(value), MAX_PLAYERS);

		for (int slot = 0; slot < ss->numClientSlots; slot++)
		{
			switch (value[slot])
			{
				case '-':
					ss->clientTeam[slot] = FREE_SLOT;
					break;

				case '0':
					ss->clientTeam[slot] = TEAM_SPEC;
					break;

				case '1':
					ss->clientTeam[slot] = TEAM_1;
					break;

				case '2':
					ss->clientTeam[slot] = TEAM_2;
					break;
			}
		}
	}

	else if (strcmp(key, "B") == 0)
	{
		ss->numClientSlots = MIN(strlen(value), MAX_PLAYERS);

		for (int slot = 0; slot < ss->numClientSlots; slot++)
		{
			switch (value[slot])
			{
				case '-':
					ss->isBot[slot] = false;
					break;

				case 'b':
					ss->isBot[slot] = true;
					break;
			}
		}
	}

	else if (strcmp(key, "sv_hostname") == 0)
	{
		strncpy(ss->name, value, sizeof(ss->name));
	}

	else if (strcmp(key, "mapname") == 0)
	{
		strncpy(ss->map, value, sizeof(ss->map));
	}
}

void UnvQuery::analyzeClientData(int statusNum)
{
	serverStatus_t *ss = &this->status[statusNum];

	int numActivePlayers = 0;

	for (int slot = 0; slot < ss->numClientSlots; slot++)
	{
		team_t team = ss->clientTeam[slot];

		ss->numClients[team]++;

		if (team == FREE_SLOT)
		{
			continue;
		}

		if (ss->isBot[slot])
		{
			ss->numBots[team]++;
		}
		else
		{
			ss->numPlayers[team]++;

			if (team != TEAM_SPEC)
			{
				numActivePlayers++;
			}
		}
	}

	// save timestamp for total number of players
	this->peekActivityData[numActivePlayers].lastSeen = time(NULL);
}

int UnvQuery::numberResponsiveServers()
{
	return this->numResponsive;
}

std::string UnvQuery::printServerLine(int serverNum)
{
	std::ostringstream stream;
	serverStatus_t     *s;
	int                playing;
	char               name[128];

	if (serverNum < 0 || serverNum >= this->numResponsive)
	{
		return "";
	}

	s = &this->status[serverNum];
	playing = s->numPlayers[TEAM_1] + s->numPlayers[TEAM_2];
	UnvQuery::stripColors(name, s->name, sizeof(name));

	// number of players
	if ( playing == 1 )
	stream << C("YELLOW") << "1 person";
	else if ( playing > 1 )
	stream << C("GREEN")  << playing << " people";
	else
	stream << C("RED")    << "No one";
	stream << C_OFF << " (";

	// team 1
	stream << s->numPlayers[TEAM_1];
	if (s->numBots[TEAM_1] > 0)
	stream << "+" << s->numBots[TEAM_1];
	stream << " " << C("RED")    << "A" << C_OFF << ", ";

	// team 2
	stream << s->numPlayers[TEAM_2];
	if (s->numBots[TEAM_2] > 0)
	stream << "+" << s->numBots[TEAM_2];
	stream << " " << C("BLUE")   << "H" << C_OFF << ", ";

	// spectators
	stream << s->numPlayers[TEAM_SPEC] << " " << C("YELLOW") << "S" << C_OFF;

	// location
	stream << ") "
	       << "playing " << B_ON << s->map << B_OFF << " "
	       << "on " << B_ON << name << B_OFF << " "
	       << "- unv://" << s->addr
	       << endl;

	return stream.str();
}

std::string UnvQuery::printActiveServers()
{
	std::ostringstream stream;
	serverStatus_t     *ss;
	int                playing;

	if (!this->refresh(PRINTACTIVESERVERS_LISTPERIOD, PRINTACTIVESERVERS_STATUSPERIOD))
	{
		return "Failed to retrieve server status info.";
	}

	// build a list of all active servers
	for (int serverNum = 0; serverNum < this->numResponsive; serverNum++)
	{
		ss = &this->status[serverNum];

		playing = ss->numPlayers[TEAM_1] + ss->numPlayers[TEAM_2];

		if (playing == 0)
		{
			continue;
		}

		stream << this->printServerLine(serverNum);
	}

	return stream.str();
}

std::string UnvQuery::checkPeekActivity(time_t period, int minPlayers)
{
	int    periodMaxPlayers = 0, currentMaxPlayers = 0, serverPlayers;
	int    currentMaxServerNum = 0;
	time_t *lastInformed;

	if (!this->refresh(CHECKPEEKACTIVITY_LISTPERIOD, CHECKPEEKACTIVITY_STATUSPERIOD))
	{
		if (period == 0)
		{
			// if the period is zero we are supposed to print something this call
			return "Failed to retrieve server status info.";
		}
		else
		{
			return "";
		}
	}

	// get maximum in period
	for (int playerCount = MAX_PLAYERS; playerCount > 0; playerCount--)
	{
		if (this->peekActivityData[playerCount].lastSeen + period > time(NULL))
		{
			periodMaxPlayers = playerCount;
			break;
		}
	}

	// calculate number of players per server and remember maximum
	for (int serverNum = 0; serverNum < this->numResponsive; serverNum++)
	{
		serverPlayers = 0;

		for (int team = TEAM_SPEC + 1; team < NUM_TEAMS; team++)
		{
			serverPlayers += this->status[serverNum].numPlayers[team];
		}

		if (serverPlayers > currentMaxPlayers)
		{
			currentMaxPlayers   = serverPlayers;
			currentMaxServerNum = serverNum;
		}
	}

	lastInformed = &this->peekActivityData[currentMaxPlayers].lastInformed;

	// if the current maximum is the maximum of the period and hasn't been advertised, do so now
	if (currentMaxPlayers >= minPlayers &&
	    currentMaxPlayers >= periodMaxPlayers &&
	    *lastInformed + period < time(NULL))
	{
		*lastInformed = time(NULL);

		if (currentMaxPlayers == 0)
		{
			return "";
		}

		return this->printServerLine(currentMaxServerNum);
	}
	else
	{
		return "";
	}
}
