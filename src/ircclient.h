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

#ifndef IRCCLIENT_H
#define IRCCLIENT_H

#include <thread>
#include <set>

#include <libircclient/libircclient.h>

#include "common.h"
#include "unvquery.h"
#include "calendar.h"
#include "github.h"

#define ASYNC_WORKER_PERIOD_MS 1000
#define RECONNECT_DELAY_S      5

#define HANDLER_NUMERIC (irc_session_t *session, \
                         unsigned int  event, \
						 const char    *origin, \
						 const char    **params, \
						 unsigned int  count)

#define HANDLER         (irc_session_t *session, \
						 const char    *event, \
						 const char    *origin, \
						 const char    **params, \
						 unsigned int  count)

#define HANDLER_NOWARN UNUSED(session); UNUSED(event); UNUSED(origin); UNUSED(params); UNUSED(count);

// Response texts
#define RESP_MISSINGMODULE "Can't do that, don't have the necessary module."
#define RESP_NOPLAYERS     "No one is playing. :("
#define RESP_NOEVENTS      "No upcoming events."

// Broadcast flags
#define BROADCAST_ALL        -1
#define BROADCAST_NONE       0
#define BROADCAST_EVENT      0b0001
#define BROADCAST_PLAYERPEEK 0b0010

#define ERRCHK(cmd)      if(cmd) printIRCSessionError()
#define CMDOK            cout << ANSWER << "OK." << endl

class IRCClient
{
public:

	/**
	 * @param host     Server hostname
	 * @param port     Server post
	 * @param password Server password
	 * @param nick     Prefered nickname
	 */
	IRCClient(std::string host, unsigned short port, std::string password, std::string nick);

	/**
	 * @brief Adds a UnvQuery instance that is used to provide server browser functionality.
	 * @param instance A UnvQuery instance
	 */
	void addUnvQuery(UnvQuery *instance);

	/**
	 * @brief Adds a Calendar isntance that is used to announce relevant dates.
	 * @param instance A Calendar instance
	 */
	void addCalendar(Calendar *instance);

	/**
	 * @brief Adds a GitHubQuery isntance that is used to lookup commits and issues.
	 * @param instance A GitHubQuery instance
	 */
	void addGitHubQuery(GitHubQuery *instance);

	/**
	 * @brief Adds a channel, it will be automatically (re)joined.
	 * @param channel  Channel name
	 * @param password Channel password
	 */
	void join(std::string channel, std::string password, int broadcastFlags);

	/**
	 * @brief Removes a channel and trys to leave it.
	 * @param channel Channel name
	 */
	void leave(std::string channel);

	/**
	 * @brief Send a text to a channel. Supports multiline.
	 * @param channel Channel name
	 * @param text    Text
	 */
	void msg(std::string channel, std::string text);

	/**
	 * @brief Send a text to all channels. Supports multiline.
	 * @param text Text
	 */
	void broadcast(std::string text, int flags);

	/**
	 * @brief Reconnects to the IRC network.
	 * @param reason Quit message
	 */
	void reconnect(std::string reason);

	/**
	 * @brief Disconnects from the IRC network.
	 * @param reason Quit message
	 */
	void quit(std::string reason);

	/**
	 * @brief Joins the main loop thread.
	 */
	void threadJoin();

	/**
	 * @brief Receives commands on standard input in a loop.
	 */
	void commandLoop();

private:

	typedef struct channel_s
	{
		std::string name;
		std::string password;
		bool        joined;
		int         broadcastFlags;
	} channel_t;

	// system
	irc_callbacks_t callbacks;
	irc_session_t   *session;
	std::thread     *eventWorker;
	std::thread     *asyncWorker;
	bool            asyncWorkerRun;
	bool            asyncWorkerRunning;
	bool            shallReconnect;

	// modules and tools
	UnvQuery        *unvQuery;
	Calendar        *calendar;
	GitHubQuery     *gitHubQuery;

	// context data
	std::string     host;
	std::string     password;
	std::string     preferedNick;
	std::string     currentNick;
	unsigned short  port;
	bool            connected;
	std::set<channel_t *> channels;

	// retrieves the class instance from the C library's session "object"
	static IRCClient *instanceFromSession(irc_session_t *session);

	// event handlers
	static void handleNumeric HANDLER_NUMERIC;
	static void handleConnect HANDLER;
	static void handleJoin    HANDLER;
	static void handleChannel HANDLER;
	static void handleKick    HANDLER;

	// user commands
	void cmdList(std::string channel);
	void cmdTop(std::string channel);
	void cmdEvents(std::string channel);
	void cmdIssue(std::string channel, int issue, bool verbose);
	void cmdCommit(std::string channel, std::string commit, bool verbose);

	// helpers
	void printIRCSessionError();
	int  connectToServer();
	void takeNextNick();
	void mainLoop();
	void internalJoin(std::string name, std::string password);
	void asyncWork();
	void startAsyncWorker();
	void stopAsyncWorker();
};

#endif // IRCCLIENT_H
