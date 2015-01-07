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

#include <ctime>
#include <sstream>
#include <ctime>

#include "calendar.h"

using namespace std;

Calendar::timepoint_t Calendar::nextDate(const timepoint_t date, recurrence_t recurrence)
{
	time_t time = chrono::system_clock::to_time_t(date);
	tm     utc  = *gmtime(&time);
	int    year = utc.tm_year + 1900;
	bool   leap = (year % 4 == 0 && (year % 400 == 0 || year % 100 != 0));

	switch (recurrence)
	{
		case ONCE:
			return date;

		case MINUTELY:
			return date + chrono::minutes(1);

		case HOURLY:
			return date + chrono::hours(1);

		case DAILY:
			return date + chrono::hours(24);

		case WEEKLY:
			return date + chrono::hours(24 * 7);

		case FIRSTOF:
		{
			timepoint_t d = date;
			time_t      t;
			tm          u;

			do
			{
				d += chrono::hours(24 * 7);
				t  = chrono::system_clock::to_time_t(d);
				u  = *gmtime(&t);
			}
			while (u.tm_mon == utc.tm_mon);

			return d;
		}

		case MONTHLY:
			switch (utc.tm_mon + 1)
			{
				case 2:
					if (leap)
					{
						return date + chrono::hours(24 * 29);
					}
					else
					{
						return date + chrono::hours(24 * 28);
					}

				case 4:
				case 6:
				case 9:
				case 11:
					return date + chrono::hours(24 * 30);

				default:
					return date + chrono::hours(24 * 31);
			}

		case YEARLY:
	        if (leap)
			{
				return date + chrono::hours(24 * 366);
			}
			else
			{
				return date + chrono::hours(24 * 365);
			}

		default:
			cout << ERROR << "Calendar::nextEvent called with unknown recurrence value.";
			return date;
	}
}

string Calendar::recurrenceToString(recurrence_t recurrence)
{
	switch(recurrence)
	{
		case ONCE:     return "once";
		case MINUTELY: return "every minute";
		case DAILY:    return "daily";
		case WEEKLY:   return "weekly";
		case FIRSTOF:  return "monthly at a fixed weekday";
		case MONTHLY:  return "monthly";
		case YEARLY:   return "once a year";
		default:       return "";
	}
}

Calendar::recurrence_t Calendar::stringToRecurrence(const string str)
{
	if (str.find("o")  == 0) return ONCE;
	if (str.find("mi") == 0) return MINUTELY;
	if (str.find("d")  == 0) return DAILY;
	if (str.find("w")  == 0) return WEEKLY;
	if (str.find("f")  == 0) return FIRSTOF;
	if (str.find("mo") == 0) return MONTHLY;
	if (str.find("y")  == 0) return YEARLY;
	else
	{
		cout << ERROR << "Failed to parse recurrence value, using 'once'." << endl;
		return ONCE;
	}
}

string Calendar::dateToString(const timepoint_t date)
{
	char buf[64];
	const char *weekday[] = {
	    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

	time_t time = chrono::system_clock::to_time_t(date);
	tm     utc  = *gmtime(&time);

	snprintf(buf, sizeof(buf), "%s, %04u-%02u-%02u, %02u:%02u UTC", weekday[utc.tm_wday],
	         utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min);

	return string(buf);
}

string Calendar::dateOffsetToString(const timepoint_t date)
{
	ostringstream stream;
	bool          future = (date >= chrono::system_clock::now());
	duration_t    offset;
	int           div, lastDiv, seconds, minutes, hours, days, weeks;
	int           tokens = 0;

	if (future)
	{
		offset = chrono::duration_cast<chrono::seconds>(date - chrono::system_clock::now());
	}
	else
	{
		offset = chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - date);
	}

	// round up one second
	// TODO: Round towards nearest
	offset += chrono::seconds(1);

	// seconds
	div     = 60;
	seconds = (offset.count() % div);
	offset -= chrono::seconds(seconds);

	// minutes
	lastDiv = div;
	div    *= 60;
	minutes = (offset.count() % div) / lastDiv;
	offset -= chrono::minutes(minutes);

	// hours
	lastDiv = div;
	div    *= 24;
	hours   = (offset.count() % div) / lastDiv;
	offset -= chrono::hours(hours);

	// days
	lastDiv = div;
	div    *= 7;
	days    = (offset.count() % div) / lastDiv;
	offset -= chrono::hours(days * 24);

	// weeks
	weeks   = abs(offset.count()) / div;

	if (future)
	{
		stream << "in ";
	}

	if (weeks > 0)
	{
		stream << weeks << " weeks";
		tokens++;
	}

	if (days > 0)
	{
		stream << (tokens ? ", " : "") << days    << " day"    << (days    == 1 ? "" : "s");
		tokens++;
	}

	if (tokens < 2 && hours > 0)
	{
		stream << (tokens ? ", " : "") << hours   << " hour"   << (hours   == 1 ? "" : "s");
		tokens++;
	}

	if (tokens < 2 && minutes > 0)
	{
		stream << (tokens ? ", " : "") << minutes << " minute" << (minutes == 1 ? "" : "s");
		tokens++;
	}

	if (( tokens < 2 && seconds > 0) || tokens == 0)
	{
		stream << (tokens ? ", " : "") << seconds << " second" << (seconds == 1 ? "" : "s");
	}

	if (!future)
	{
		stream << " ago";
	}

	return stream.str();
}

Calendar::Calendar(bool useColor)
{
	this->useColor = useColor;

	this->events = set<event_t *>();
}

bool Calendar::addEvent(string input)
{
	istringstream stream(input);

	unsigned int unixTime;
	unsigned int warnSeconds;
	unsigned int warnCount;
	string recurrenceStr;
	string description;

	stream >> unixTime;
	stream >> warnSeconds;
	stream >> warnCount;
	stream >> recurrenceStr;
	getline(stream, description);

	// trim description
	description.erase(0, description.find_first_not_of(" \t"));
	description.erase(description.find_last_not_of(" \t") + 1);

	if (description.empty())
	{
		cout << ERROR << "Failed to parse event." << endl;
		return false;
	}

	timepoint_t  date       = chrono::system_clock::from_time_t((time_t)unixTime);
	duration_t   warn       = chrono::seconds(warnSeconds);
	recurrence_t recurrence = this->stringToRecurrence(recurrenceStr);

	return addEvent(date, warn, warnCount, recurrence, description);
}

bool Calendar::addEvent(timepoint_t date, duration_t warn, unsigned int warnCount,
                        recurrence_t recurrence, std::string description)
{
	timepoint_t now = chrono::system_clock::now();

	// find next recurring date in the future
	if (date <= now)
	{
		if (recurrence != ONCE)
		{
			do
			{
				date = Calendar::nextDate(date, recurrence);
			}
			while(date <= now);
		}
		else
		{
			return false;
		}
	}

	event_t *event = new event_t;

	event->description      = description;
	event->date             = date;
	event->warn             = warn;
	event->currentWarn      = warn;
	event->warnCount        = warnCount;
	event->currentWarnCount = warnCount;
	event->recurrence       = recurrence;

	// decrease warn counter for past warnings
	while (event->currentWarnCount > 0 && event->date - event->currentWarn <= now)
	{
		event->currentWarn /= 2;
		event->currentWarnCount--;
	}

	this->events.insert(event);

	return true;
}

int Calendar::numEvents()
{
	return this->events.size();
}

string Calendar::listEvents()
{
	ostringstream stream;

	for (event_t *event : this->events)
	{
		stream << B_ON << event->description << B_OFF
		       << " happens " << this->recurrenceToString(event->recurrence)
		       << ", next date is " << this->dateToString(event->date)
		       << " (" << this->dateOffsetToString(event->date) << ")." << endl;
	}

	return stream.str();
}

string Calendar::pumpEvents()
{
	ostringstream stream;
	bool          done, past;
	event_t       *event;
	timepoint_t   now = chrono::system_clock::now();

	do
	{
		done = true;
		past = false;

		for (event_t *cursor : this->events)
		{
			event = cursor;

			if (event->date <= now)
			{
				done = false;
				past = true;
				break;
			}
			else if (event->currentWarnCount > 0 && event->date - event->currentWarn <= now)
			{
				done = false;
				past = false;
				break;
			}
		}

		if (done)
		{
			break;
		}

		stream << B_ON << event->description << B_OFF;

		if (past)
		{
			if (chrono::duration_cast<chrono::seconds>(now - event->date).count() >= 60)
			{
				stream << " started " << this->dateOffsetToString(event->date) << ".";
			}
			else
			{
				stream << " starts now.";
			}

			stream << endl;

			if (event->recurrence != ONCE)
			{
				event->date             = this->nextDate(event->date, event->recurrence);
				event->currentWarn      = event->warn;
				event->currentWarnCount = event->warnCount;
			}
			else
			{
				this->events.erase(event);
				//delete event;
			}
		}
		else
		{
			stream << " starts " << this->dateOffsetToString(event->date) << "." << endl;

			event->currentWarn /= 2;
			event->currentWarnCount--;
		}
	}
	while (true);

	return stream.str();
}
