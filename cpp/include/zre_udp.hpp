/*  =========================================================================
    zre_udp - UDP management class

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

#ifndef __ZRE_UDP_H_INCLUDED__
#define __ZRE_UDP_H_INCLUDED__

#ifdef __WINDOWS__
#define ZRE_INVALID_SOCKET INVALID_SOCKET
#else
#define ZRE_INVALID_SOCKET -1
#endif

struct zre_udp_data_t;

class zre_udp
{
public:

//  Constructor
    zre_udp (int port_nbr);

//  Destructor
    ~zre_udp ();

//  Returns UDP socket handle
int
    handle ();

//  Send message using UDP broadcast
void
    send (byte *buffer, size_t length);

//  Receive message from UDP broadcast
ssize_t
    recv (byte *buffer, size_t length);

//  Return our own IP address as printable string
char *
    host ();

//  Return IP address of peer that sent last message
char *
    from ();

private:
	zre_udp_data_t* myData;
   
};

#endif
