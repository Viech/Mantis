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

#ifndef COMMON_H
#define COMMON_H

#define UNUSED(x) (void)(x)

#define MIN(a,b) ( a < b ? a : b )
#define MAX(a,b) ( a > b ? a : b )

#define COMMAND  " >  "
#define ANSWER   " <  "
#define INCOMING "[<] "
#define OUTGOING "[>] "
#define NOTICE   "[ ] "
#define ERROR    "[!] Error: "
#define FATAL    "[X] Fatal: "

#define COND_ON            (this->useColor ?
#define COND_OFF           : "")
#define C(COLOR)           COND_ON "[COLOR=" COLOR "]" COND_OFF
#define C_OFF              COND_ON "[/COLOR]"          COND_OFF
#define B_ON               COND_ON "[B]"               COND_OFF
#define B_OFF              COND_ON "[/B]"              COND_OFF
#define U_ON               COND_ON "[U]"               COND_OFF
#define U_OFF              COND_ON "[/U]"              COND_OFF

#endif // COMMON_H
