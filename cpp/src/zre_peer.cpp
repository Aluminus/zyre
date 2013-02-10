/*  =========================================================================
    zre_peer - one of our peers in a ZRE network

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

struct zre_peer_data_t {
    zctx_t *ctx;                //  CZMQ context
    void *mailbox;              //  Socket through to peer
    char *identity;             //  Identity string
    char *endpoint;             //  Endpoint connected to
    uint64_t evasive_at;        //  Peer is being evasive
    uint64_t expired_at;        //  Peer has expired by now
    bool connected;             //  Peer will send messages
    bool ready;                 //  Peer has said Hello to us
    byte status;                //  Our status counter
    uint16_t sent_sequence;     //  Outgoing message sequence
    uint16_t want_sequence;     //  Incoming message sequence
    zhash_t *headers;           //  Peer headers
};


//  ---------------------------------------------------------------------
//  Construct new peer object

zre_peer::zre_peer (char *identity, zhash_t *container, zctx_t *ctx)
{
    myData = (zre_peer_data_t *) zmalloc (sizeof (zre_peer_data_t));
    myData->ctx = ctx;
    myData->identity = _strdup (identity);
    myData->ready = false;
    myData->connected = false;
    myData->sent_sequence = 0;
    myData->want_sequence = 0;
    
    //  Insert into container if requested
    if (container) {
        zhash_insert (container, identity, this);
        zhash_freefn (container, identity, [] (void *argument)
		//  Callback when we remove peer from container
		{
			zre_peer *peer = (zre_peer *) argument;
			delete(peer);
		});
    }
}


//  ---------------------------------------------------------------------
//  Destroy peer object

zre_peer::~zre_peer ()
{
    disconnect ();
    zhash_destroy (&myData->headers);
    free (myData->identity);
    free (myData);
}


//  ---------------------------------------------------------------------
//  Connect peer mailbox
//  Configures mailbox and connects to peer's router endpoint

void
zre_peer::connect (char *reply_to, char *endpoint)
{
    assert (!myData->connected);

    //  Create new outgoing socket (drop any messages in transit)
    myData->mailbox = zsocket_new (myData->ctx, ZMQ_DEALER);
    //  Null if shutting down
    if (myData->mailbox) {
        //  Set our caller 'From' identity so that receiving node knows
        //  who each message came from.
        zsocket_set_identity (myData->mailbox, reply_to);

        //  Set a high-water mark that allows for reasonable activity
        zsocket_set_sndhwm (myData->mailbox, PEER_EXPIRED * 100);

        //  Send messages immediately or return EAGAIN
        zsocket_set_sndtimeo (myData->mailbox, 0);

        //  Connect through to peer node
        zsocket_connect (myData->mailbox, "tcp://%s", endpoint);
        myData->endpoint = _strdup (endpoint);
        myData->connected = true;
        myData->ready = false;
    }
}


//  ---------------------------------------------------------------------
//  Disconnect peer mailbox
//  No more messages will be sent to peer until connected again

void
zre_peer::disconnect ()
{
    //  If connected, destroy socket and drop all pending messages
    if (myData->connected) {
        zsocket_destroy (myData->ctx, myData->mailbox);
        free (myData->endpoint);
        myData->mailbox = NULL;
        myData->endpoint = NULL;
        myData->connected = false;
    }
}


//  ---------------------------------------------------------------------
//  Send message to peer

int
zre_peer::send (zre_msg **msg_p)
{
    if (myData->connected) {
        (*msg_p)->sequence_set(++(myData->sent_sequence));
		if ((*msg_p)->send(myData->mailbox) && errno == EAGAIN) {
            disconnect ();
            return -1;
        }
    }
    else
        delete (*msg_p);
    
    return 0;
}


//  ---------------------------------------------------------------------
//  Return peer connected status

bool
zre_peer::connected ()
{
    return myData->connected;
}


//  ---------------------------------------------------------------------
//  Return peer identity string

char *
zre_peer::identity ()
{
    return myData->identity;
}


//  ---------------------------------------------------------------------
//  Return peer connection endpoint

char *
zre_peer::endpoint ()
{
    if (myData->connected)
        return myData->endpoint;
    else
        return "";
}


//  ---------------------------------------------------------------------
//  Register activity at peer

void
zre_peer::refresh ()
{
    myData->evasive_at = zclock_time () + PEER_EVASIVE;
    myData->expired_at = zclock_time () + PEER_EXPIRED;
}


//  ---------------------------------------------------------------------
//  Return peer future evasive time

int64_t
zre_peer::evasive_at ()
{
    return myData->evasive_at;
}


//  ---------------------------------------------------------------------
//  Return peer future expired time

int64_t
zre_peer::expired_at ()
{
    return myData->expired_at;
}


//  ---------------------------------------------------------------------
//  Return peer cycle
//  This gives us a state change count for the peer, which we can
//  check against its claimed status, to detect message loss.

byte
zre_peer::status ()
{
    return myData->status;
}


//  ---------------------------------------------------------------------
//  Set peer status

void
zre_peer::status_set (byte status)
{
    myData->status = status;
}


//  ---------------------------------------------------------------------
//  Return peer ready state

byte
zre_peer::ready ()
{
    return myData->ready;
}


//  ---------------------------------------------------------------------
//  Set peer ready

void
zre_peer::ready_set (bool ready)
{
    myData->ready = ready;
}


//  ---------------------------------------------------------------------
//  Get peer header value

char *
zre_peer::header (char *key, char *default_value)
{
    char *value = NULL;
    if (myData->headers)
        value = (char *) (zhash_lookup (myData->headers, key));
    if (!value)
        value = default_value;

    return value;
}


//  ---------------------------------------------------------------------
//  Set peer headers from provided dictionary

void
zre_peer::headers_set (zhash_t *headers)
{
    zhash_destroy (&myData->headers);
    myData->headers = zhash_dup (headers);
}


//  ---------------------------------------------------------------------
//  Check peer message sequence

bool
zre_peer::check_message (zre_msg *msg)
{
    assert (msg);
	uint16_t recd_sequence = msg->sequence_get();

    bool valid = (++(myData->want_sequence) == recd_sequence);
    if (!valid)
        --(myData->want_sequence);    //  Rollback

    return valid;
}
