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

#include <czmq.h>
#include "../include/zre_internal.hpp"


//  ---------------------------------------------------------------------
//  Structure of our class

struct zre_log_data_t {
    zctx_t *ctx;                //  CZMQ context
    void *publisher;            //  Socket to send to
    uint16_t nodeid;            //  Own correlation ID
};


//  ---------------------------------------------------------------------
//  Construct new log object

zre_log::zre_log (char *endpoint)
{
    myData = new zre_log_data_t;
    myData->ctx = zctx_new ();
    myData->publisher = zsocket_new (myData->ctx, ZMQ_PUB);
    //  Modified Bernstein hashing function
    while (*endpoint)
        myData->nodeid = 33 * myData->nodeid ^ *endpoint++;
}


//  ---------------------------------------------------------------------
//  Destroy log object

zre_log::~zre_log ()
{
    zctx_destroy (&myData->ctx);
    delete myData;
}


//  ---------------------------------------------------------------------
//  Connect log to remote endpoint

void
zre_log::connect (char *endpoint)
{
    zsocket_connect (myData->publisher, endpoint);
}


//  ---------------------------------------------------------------------
//  Record one log event

void
zre_log::info (int event, char *peer, char *format, ...)
{
    uint16_t peerid = 0;
    while (peer && *peer)
        peerid = 33 * peerid ^ *peer++;

    //  Format body if any
    char body [256];
    if (format) {
        va_list argptr;
        va_start (argptr, format);
        vsnprintf (body, 255, format, argptr);
        va_end (argptr);
    }
    else
        *body = 0;
    
    zre_log_msg::send_log (myData->publisher, ZRE_LOG_MSG_LEVEL_INFO, 
        event, myData->nodeid, peerid, time (NULL), body);
}
