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

#ifndef CALENDAR_H
#define CALENDAR_H

#include <iostream>
#include <chrono>
#include <set>

#include "common.h"

class Calendar
{
public:

	typedef enum recurrence_e
	{
		ONCE,
		MINUTELY,
		HOURLY,
		DAILY,
		WEEKLY,
		FIRSTOF,
		MONTHLY,
		YEARLY
	} recurrence_t;

	typedef std::chrono::time_point<std::chrono::system_clock> timepoint_t;
	typedef std::chrono::duration<int>                         duration_t;

	Calendar(bool useColor);

	bool addEvent(std::string input);

	bool addEvent(timepoint_t date, duration_t warn, unsigned int warnCount, recurrence_t recurrence,
	              std::string description);

	int  numEvents();

	std::string listEvents();

	std::string pumpEvents();

private:

	typedef struct date_s
	{
		std::string  description;
		timepoint_t  date;
		duration_t   warn;
		duration_t   currentWarn;
		unsigned int warnCount;
		unsigned int currentWarnCount;
		recurrence_t recurrence;
	} event_t;

	std::set<event_t *> events;
	bool                useColor;

	static std::string  recurrenceToString(recurrence_t recurrence);
	static recurrence_t stringToRecurrence(const std::string str);
	static std::string  dateToString(const timepoint_t date);
	std::string         dateOffsetToString(const timepoint_t date);
	static timepoint_t  nextDate(const timepoint_t date, recurrence_t recurrence);
};

#endif // CALENDAR_H
