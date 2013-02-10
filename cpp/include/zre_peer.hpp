/*  =========================================================================
    zre_peer - ZRE network peer

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

#ifndef __ZRE_PEER_H_INCLUDED__
#define __ZRE_PEER_H_INCLUDED__

struct  zre_peer_data_t;	// Opaque storage for peer data

class zre_peer
{
public: 

//  Constructor
	zre_peer (char *identity, zhash_t *container, zctx_t *ctx);

//  Destructor

    ~zre_peer ();

//  Connect peer mailbox
void
    connect (char *reply_to, char *endpoint);

//  Connect peer mailbox
void
    disconnect ();

//  Return peer connected status
bool
    connected ();

//  Return peer connection endpoint
char *
    endpoint ();

//  Send message to peer
int
    send (zre_msg **msg_p);

//  Return peer identity string
char *
    identity ();
    
//  Register activity at peer
void
    refresh ();
    
//  Return peer future evasive time
int64_t
    evasive_at ();

//  Return peer future expired time
int64_t
    expired_at ();

//  Return peer status
byte
    status ();

//  Set peer status
void
    status_set (byte status);

//  Return peer ready state
byte
    ready ();
    
//  Set peer ready
void
    ready_set (bool ready);

//  Get peer header value
char *
    header (char *key, char *default_value);

//  Set peer headers from provided dictionary
void
    headers_set (zhash_t *headers);

//  Check peer message sequence
bool
    check_message (zre_msg *msg);

private:
	zre_peer_data_t* myData;
};

#endif
