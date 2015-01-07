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

#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <regex>

#include <libircclient/libircclient.h>
#include <libircclient/libirc_rfcnumeric.h>

#include "ircclient.h"

using namespace std;

IRCClient *IRCClient::instanceFromSession(irc_session_t *session)
{
	return (IRCClient *)irc_get_ctx(session);
}

void IRCClient::handleNumeric HANDLER_NUMERIC
{
	HANDLER_NOWARN

	IRCClient *instance = IRCClient::instanceFromSession(session);

	// print errors
	if ( event > 400 )
	{
		string error;

		for ( unsigned int i = 0; i < count; i++ )
		{
			if ( i > 0 )
			{
				error += " ";
			}

			error += params[i];
		}

		cout << ERROR << event << ": " << ( origin ? origin : "?" ) << " " << error << endl;
	}

	switch ( event )
	{
		case LIBIRC_RFC_ERR_NICKNAMEINUSE:
			instance->takeNextNick();
			break;
	}
}

void IRCClient::handleConnect HANDLER
{
	HANDLER_NOWARN

	IRCClient *instance = IRCClient::instanceFromSession(session);

	instance->connected = true;

	cout << INCOMING << "Connected to " << origin << "." << endl;

	// start asynchronous worker
	instance->startAsyncWorker();

	// rejoin all channels
	for(channel_t *channel : instance->channels)
	{
		instance->internalJoin(channel->name, channel->password);
	}
}

void IRCClient::handleJoin HANDLER
{
	HANDLER_NOWARN

	IRCClient *instance = IRCClient::instanceFromSession(session);

	const char *channel = params[0];

	if ( origin == instance->currentNick )
	{
		cout << INCOMING << "Joined " << channel << "." << endl;

		for(channel_t *cursor : instance->channels)
		{
			if(cursor->name == channel)
			{
				cursor->joined = true;
			}
		}
	}
}

void IRCClient::handleKick HANDLER
{
	HANDLER_NOWARN

	IRCClient *instance = IRCClient::instanceFromSession(session);

	const char *channel = params[0];
	const char *victim = params[1];

	if ( victim == instance->currentNick )
	{
		cout << INCOMING << "Kicked from " << channel << " by " << origin << "." << endl;

		for(channel_t *cursor : instance->channels)
		{
			if(cursor->name == channel)
			{
				cursor->joined = false;

				// try to rejoin once
				instance->internalJoin(cursor->name, cursor->password);
			}
		}
	}
}

void IRCClient::handleChannel HANDLER
{
	HANDLER_NOWARN

	IRCClient *instance = IRCClient::instanceFromSession(session);

	const char *channel = params[0];
	const char *msg     = params[1];

	istringstream stream(msg);

	if (msg[0] == '!')
	{
		string cmd;
		stream >> cmd;

		if (!cmd.compare("!list"))
		{
			instance->cmdList(string(channel));
		}
		else if (!cmd.compare("!top"))
		{
			instance->cmdTop(string(channel));
		}
		else if (!cmd.compare("!events"))
		{
			instance->cmdEvents(string(channel));
		}
		else if (!cmd.compare("!issue"))
		{
			int issue;
			stream >> issue;
			thread(&IRCClient::cmdIssue, instance, string(channel), issue, true).detach();
		}
		else if (!cmd.compare("!commit"))
		{
			string hash;
			stream >> hash;
			thread(&IRCClient::cmdCommit, instance, string(channel), hash, true).detach();
		}
	}
	/*
	else
	{
		regex issueRE("#[0-9]+");
		regex commitRE("[0-9a-fA-F]{7,40}");

		// issues
		{
			cregex_iterator begin(msg, msg + strlen(msg), issueRE), end;
			set<int>        issues;
			int             lineNum = 0;

			for (cregex_iterator match = begin; match != end; match++)
				issues.insert(atoi(match->str().c_str() + 1));

			for (int issue : issues)
			{
				if (lineNum++ == 3) break;
				thread(&IRCClient::cmdIssue, instance, string(channel), issue, false).detach();
			}
		}

		// commits
		{
			cregex_iterator begin(msg, msg + strlen(msg), commitRE), end;
			set<string>     hashes;
			int             lineNum = 0;

			for (cregex_iterator match = begin; match != end; match++)
				hashes.insert(match->str());

			for (string hash : hashes)
			{
				if (lineNum++ == 3) break;
				thread(&IRCClient::cmdCommit, instance, string(channel), hash, false).detach();
			}
		}
	}
	*/
}

void IRCClient::printIRCSessionError()
{
	cout << ERROR << irc_strerror(irc_errno(this->session)) << endl;
}

int IRCClient::connectToServer()
{
	cout << OUTGOING << "Connecting to " << this->host << ":" << this->port << "..." << endl;

	const char *host = this->host.c_str();
	const char *pass = this->password.empty() ? NULL : this->password.c_str();
	const char *nick = this->preferedNick.c_str();

	return irc_connect(this->session, host, this->port, pass, nick, nick, nick);
}

void IRCClient::takeNextNick()
{
	string oldNick = this->currentNick;

	this->currentNick += "_";

	cout << OUTGOING << "Changing nick: " << oldNick << " -> " << this->currentNick << endl;

	irc_cmd_nick(this->session, this->currentNick.c_str());
}

IRCClient::IRCClient(string host, unsigned short port, string password, string nick)
{
	// context
	this->preferedNick = nick;
	this->currentNick  = nick;
	this->host         = host;
	this->port         = port;
	this->password     = password;
	this->connected    = false;
	this->channels     = set<channel_t *>();

	// system
	this->shallReconnect = true;

	// event handlers
	memset(&this->callbacks, 0, sizeof(this->callbacks));
	this->callbacks.event_connect = &handleConnect;
	this->callbacks.event_numeric = &handleNumeric;
	this->callbacks.event_join    = &handleJoin;
	this->callbacks.event_channel = &handleChannel;
	this->callbacks.event_kick    = &handleKick;

	// background worker
	this->asyncWorkerRun     = false;
	this->asyncWorkerRunning = false;

	// modules
	this->unvQuery = NULL;
	this->calendar = NULL;

	// start main loop worker
	this->eventWorker = new thread(&IRCClient::mainLoop, this);
}

void IRCClient::mainLoop()
{
	while (true)
	{
		// create a session
		this->session = irc_create_session(&this->callbacks);

		if (!this->session)
		{
			cerr << FATAL << "Failed to initialize IRC session." << endl;
			break;
		}

		// set session options
		irc_option_set(this->session, LIBIRC_OPTION_STRIPNICKS);
		irc_set_ctx(this->session, this);

		// connect
		if (connectToServer() != 0)
		{
			cerr << FATAL << "Failed to initialize IRC connection." << endl;
			break;
		}

		// start event handler loop, will exit when disconnected
		irc_run(this->session);

		cout << INCOMING << "Disconnected: " << irc_strerror(irc_errno(this->session)) << "." << endl;

		if (shallReconnect)
		{
			// stop asynchronous worker
			this->stopAsyncWorker();

			// reset context
			this->connected   = false;
			this->currentNick = this->preferedNick;

			// mark all channels as not joined
			for(channel_t *channel : this->channels)
			{
				channel->joined = false;
			}

			// destroy session
			irc_destroy_session(this->session);

			// wait a while before reconnecting
			if (RECONNECT_DELAY_S > 0)
			{
				cout << NOTICE << "Reconnecting in " << RECONNECT_DELAY_S << " seconds..." << endl;
				this_thread::sleep_for(chrono::seconds(RECONNECT_DELAY_S));

				if (this->asyncWorkerRunning)
				{
					cout << NOTICE << "Asynchronous worker still running, waiting for termination..." << endl;
				}
			}

			// make sure the worker is dead
			if (this->asyncWorker && this->asyncWorker->joinable())
			{
				this->asyncWorker->join();
			}
		}
		else
		{
			break;
		}
	}

	// clean up
	this->stopAsyncWorker();

	if (this->asyncWorker->joinable())
	{
		this->asyncWorker->join();
	}

	if (this->session)
	{
		irc_destroy_session(this->session);
	}
}

void IRCClient::threadJoin()
{
	eventWorker->join();
}

void IRCClient::commandLoop()
{
	string command;

	cout << NOTICE << "Receiving commands on console." << endl;

	while (true)
	{
		getline(cin, command);

		cout << COMMAND << command << endl;

		if (command == "quit")
		{
			this->quit("Shut down via console.");

			break;
		}
		else if (command == "reconnect")
		{
			this->reconnect("Reconnect via console.");
		}
		else if (command == "help")
		{
			cout << ANSWER << "Available commands: help, reconnect, quit." << endl;
		}
	}

	this->threadJoin();
}

void IRCClient::internalJoin(string name, string password)
{
	cout << OUTGOING << "Joining " << name << "..." << endl;

	ERRCHK(irc_cmd_join(this->session, name.c_str(), password.empty() ? NULL : password.c_str()));
}

void IRCClient::join(string name, string password, int broadcastFlags)
{
	channel_t *channel;

	channel = new channel_t;
	channel->name           = name;
	channel->password       = password;
	channel->broadcastFlags = broadcastFlags;
	channel->joined         = false;

	this->channels.insert(channel);

	// if we are connected, try to join, otherwise handleConnect will do so
	if (this->connected)
	{
		this->internalJoin(name, password);
	}
}

void IRCClient::leave(std::string name)
{
	channel_t *target = NULL;

	// find channel in set
	for (channel_t *cursor : channels)
	{
		if (cursor->name == name)
		{
			target = cursor;
			break;
		}
	}

	if (target)
	{
		// if we are in the channel, try to leave, otherwise we'll have to wait for a disconnect
		if (this->connected && target->joined)
		{
			cout << OUTGOING << "Leaving " << name << "..." << endl;

			ERRCHK(irc_cmd_part(this->session, name.c_str()));
		}

		delete target;

		this->channels.erase(target);
	}
}

void IRCClient::msg(string target, string text)
{
	char *colored = irc_color_convert_to_mirc(text.c_str());
	istringstream stream(colored);
	string        line;

	if (text.empty())
	{
		return;
	}

	while(getline(stream, line, '\n'))
	{
		irc_cmd_msg(this->session, target.c_str(), line.c_str());
	}

	free(colored);
}

void IRCClient::broadcast(string text, int flags)
{
	for (channel_t *channel : this->channels)
	{
		if (channel->joined && (channel->broadcastFlags & flags))
		{
			this->msg(channel->name, text);
		}
	}
}

void IRCClient::reconnect(string reason)
{
	if (this->connected)
	{
		cout << OUTGOING << "Disconnecting..." << endl;

		ERRCHK(irc_cmd_quit(this->session, reason.c_str()));
	}
}

void IRCClient::quit(string reason)
{
	this->shallReconnect = false;

	this->reconnect(reason);
}

void IRCClient::addUnvQuery(UnvQuery *instance)
{
	this->unvQuery = instance;
}

void IRCClient::addCalendar(Calendar *instance)
{
	this->calendar = instance;
}

void IRCClient::addGitHubQuery(GitHubQuery *instance)
{
	this->gitHubQuery = instance;
}

#define CHECKMODULE(x) \
if(!this->x) \
{ \
	this->msg(channel, RESP_MISSINGMODULE); \
	return; \
}

void IRCClient::cmdList(string channel)
{
	string response;
	CHECKMODULE(unvQuery)
	response = this->unvQuery->printActiveServers();
	if (response.empty()) response = RESP_NOPLAYERS;
	this->msg(channel, response);
}

void IRCClient::cmdTop(string channel)
{
	string response;
	CHECKMODULE(unvQuery)
	response = this->unvQuery->checkPeekActivity(0, 0);
	if (response.empty()) response = RESP_NOPLAYERS;
	this->msg(channel, response);
}

void IRCClient::cmdEvents(string channel)
{
	string response;
	CHECKMODULE(calendar)
	response = this->calendar->listEvents();
	this->msg(channel, response);
}

void IRCClient::cmdIssue(string channel, int issue, bool verbose)
{
	string response;
	CHECKMODULE(gitHubQuery)
	response = this->gitHubQuery->linkIssue(issue, verbose);
	this->msg(channel, response);
}

void IRCClient::cmdCommit(string channel, string hash, bool verbose)
{
	string response;
	CHECKMODULE(gitHubQuery)
	response = this->gitHubQuery->linkCommit(hash, verbose);
	this->msg(channel, response);
}

void IRCClient::asyncWork()
{
	this->asyncWorkerRunning = true;

	cout << NOTICE << "Worker started." << endl;

	while(asyncWorkerRun)
	{
		if(this->unvQuery)
		{
			this->broadcast(this->unvQuery->checkPeekActivity(60 * 60 * 6, 1), BROADCAST_PLAYERPEEK);
		}

		if(this->calendar)
		{
			this->broadcast(this->calendar->pumpEvents(), BROADCAST_EVENT);
		}

		this_thread::sleep_for(chrono::milliseconds(ASYNC_WORKER_PERIOD_MS));
	}

	cout << NOTICE << "Worker stopped." << endl;

	this->asyncWorkerRunning = false;
}

void IRCClient::startAsyncWorker()
{
	if (!this->asyncWorkerRun)
	{
		this->asyncWorkerRun = true;

		this->asyncWorker = new thread(&IRCClient::asyncWork, this);
	}
}

void IRCClient::stopAsyncWorker()
{
	if (this->asyncWorkerRun)
	{
		this->asyncWorkerRun = false;
	}
}
