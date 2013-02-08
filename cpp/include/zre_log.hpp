/*  =========================================================================
    zre_log - record log data

    -------------------------------------------------------------------------
    Copyright (c) 1991-2012 iMatix Corporation <www.imatix.com>
    Copyright other contributors as noted in the AUTHORS file.

    This file is part of Zyre, an open-source framework for proximity-based
    peer-to-peer applications -- See http://zyre.org.

    This is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or (at
    your option) any later version.

    This software is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this program. If not, see
    <http://www.gnu.org/licenses/>.
    =========================================================================
*/

#ifndef __ZRE_LOG_H_INCLUDED__
#define __ZRE_LOG_H_INCLUDED__

struct zre_log_data_t;

class zre_log
{
public:

//  Constructor
    zre_log (char *endpoint);

//  Destructor
    ~zre_log ();

//  Connect log to remote endpoint
void
    connect (char *endpoint);

//  Record one log event
void
    info (int event, char *peer, char *format, ...);

private:
	zre_log_data_t* myData;
};

#endif
