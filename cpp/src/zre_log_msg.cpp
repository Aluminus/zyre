/*  =========================================================================
    zre_log_msg.c

    Generated codec implementation for zre_log_msg
    -------------------------------------------------------------------------
    Copyright (c) 1991-2012 iMatix Corporation -- http://www.imatix.com     
    Copyright other contributors as noted in the AUTHORS file.              
                                                                            
    This file is part of Zyre, an open-source framework for proximity-based 
    peer-to-peer applications -- See http://zyre.org.                       
                                                                            
    This is free software; you can redistribute it and/or modify it under   
    the terms of the GNU Lesser General Public License as published by the  
    Free Software Foundation; either version 3 of the License, or (at your  
    option) any later version.                                              
                                                                            
    This software is distributed in the hope that it will be useful, but    
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTA-   
    BILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General  
    Public License for more details.                                        
                                                                            
    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see http://www.gnu.org/licenses/.      
    =========================================================================
*/

#include <czmq.h>
#include "../include/zre_log_msg.hpp"

//  Structure of our class

struct zre_log_msg_data_t {
    zframe_t *address;          //  Address of peer if any
    int id;                     //  zre_log_msg message ID
    byte *needle;               //  Read/write pointer for serialization
    byte *ceiling;              //  Valid upper limit for read pointer
    byte level;
    byte event;
    uint16_t node;
    uint16_t peer;
    uint64_t time;
    char *data;
};

//  --------------------------------------------------------------------------
//  Network data encoding macros

//  Strings are encoded with 1-byte length
#define STRING_MAX  255

//  Put a block to the frame
#define PUT_BLOCK(host,size) { \
    memcpy (myData->needle, (host), size); \
    myData->needle += size; \
    }

//  Get a block from the frame
#define GET_BLOCK(host,size) { \
    if (myData->needle + size > myData->ceiling) \
        goto malformed; \
    memcpy ((host), myData->needle, size); \
    myData->needle += size; \
    }

//  Put a 1-byte number to the frame
#define PUT_NUMBER1(host) { \
    *(byte *) myData->needle = (host); \
    myData->needle++; \
    }

//  Put a 2-byte number to the frame
#define PUT_NUMBER2(host) { \
    myData->needle [0] = (byte) (((host) >> 8)  & 255); \
    myData->needle [1] = (byte) (((host))       & 255); \
    myData->needle += 2; \
    }

//  Put a 4-byte number to the frame
#define PUT_NUMBER4(host) { \
    myData->needle [0] = (byte) (((host) >> 24) & 255); \
    myData->needle [1] = (byte) (((host) >> 16) & 255); \
    myData->needle [2] = (byte) (((host) >> 8)  & 255); \
    myData->needle [3] = (byte) (((host))       & 255); \
    myData->needle += 4; \
    }

//  Put a 8-byte number to the frame
#define PUT_NUMBER8(host) { \
    myData->needle [0] = (byte) (((host) >> 56) & 255); \
    myData->needle [1] = (byte) (((host) >> 48) & 255); \
    myData->needle [2] = (byte) (((host) >> 40) & 255); \
    myData->needle [3] = (byte) (((host) >> 32) & 255); \
    myData->needle [4] = (byte) (((host) >> 24) & 255); \
    myData->needle [5] = (byte) (((host) >> 16) & 255); \
    myData->needle [6] = (byte) (((host) >> 8)  & 255); \
    myData->needle [7] = (byte) (((host))       & 255); \
    myData->needle += 8; \
    }

//  Get a 1-byte number from the frame
#define GET_NUMBER1(host) { \
    if (myData->needle + 1 > myData->ceiling) \
        goto malformed; \
    (host) = *(byte *) myData->needle; \
    myData->needle++; \
    }

//  Get a 2-byte number from the frame
#define GET_NUMBER2(host) { \
    if (myData->needle + 2 > myData->ceiling) \
        goto malformed; \
    (host) = ((uint16_t) (myData->needle [0]) << 8) \
           +  (uint16_t) (myData->needle [1]); \
    myData->needle += 2; \
    }

//  Get a 4-byte number from the frame
#define GET_NUMBER4(host) { \
    if (myData->needle + 4 > myData->ceiling) \
        goto malformed; \
    (host) = ((uint32_t) (myData->needle [0]) << 24) \
           + ((uint32_t) (myData->needle [1]) << 16) \
           + ((uint32_t) (myData->needle [2]) << 8) \
           +  (uint32_t) (myData->needle [3]); \
    myData->needle += 4; \
    }

//  Get a 8-byte number from the frame
#define GET_NUMBER8(host) { \
    if (myData->needle + 8 > myData->ceiling) \
        goto malformed; \
    (host) = ((uint64_t) (myData->needle [0]) << 56) \
           + ((uint64_t) (myData->needle [1]) << 48) \
           + ((uint64_t) (myData->needle [2]) << 40) \
           + ((uint64_t) (myData->needle [3]) << 32) \
           + ((uint64_t) (myData->needle [4]) << 24) \
           + ((uint64_t) (myData->needle [5]) << 16) \
           + ((uint64_t) (myData->needle [6]) << 8) \
           +  (uint64_t) (myData->needle [7]); \
    myData->needle += 8; \
    }

//  Put a string to the frame
#define PUT_STRING(host) { \
    string_size = strlen (host); \
    PUT_NUMBER1 (string_size); \
    memcpy (myData->needle, (host), string_size); \
    myData->needle += string_size; \
    }

//  Get a string from the frame
#define GET_STRING(host) { \
    GET_NUMBER1 (string_size); \
    if (myData->needle + string_size > (myData->ceiling)) \
        goto malformed; \
    (host) = (char *) malloc (string_size + 1); \
    memcpy ((host), myData->needle, string_size); \
    (host) [string_size] = 0; \
    myData->needle += string_size; \
    }


//  --------------------------------------------------------------------------
//  Create a new zre_log_msg

zre_log_msg::zre_log_msg (int id)
{
    myData = new zre_log_msg_data_t;
    myData->id = id;
	myData->address = nullptr;
    myData->needle = nullptr;
    myData->ceiling = nullptr;
    myData->data = nullptr;
}


//  --------------------------------------------------------------------------
//  Destroy the zre_log_msg

zre_log_msg::~zre_log_msg ()
{
    //  Free class properties
    zframe_destroy (&myData->address);
    free (myData->data);

    //  Free object itself
    delete myData;
}


//  --------------------------------------------------------------------------
//  Receive and parse a zre_log_msg from the socket. Returns new object or
//  NULL if error. Will block if there's no message waiting.

zre_log_msg *
zre_log_msg::recv (void *input)
{
    assert (input);
    auto shell = new zre_log_msg(0);
	auto myData = shell->myData;
    zframe_t *frame = nullptr;
    size_t string_size;

    //  Read valid message frame from socket; we loop over any
    //  garbage data we might receive from badly-connected peers
    while (true) {
        //  If we're reading from a ROUTER socket, get address
        if (zsockopt_type (input) == ZMQ_ROUTER) {
			zframe_destroy (&myData->address);
            myData->address = zframe_recv (input);
            if (!myData->address)
                goto empty;         //  Interrupted
            if (!zsocket_rcvmore (input))
                goto malformed;
        }
        //  Read and parse command in frame
        frame = zframe_recv (input);
        if (!frame)
            goto empty;             //  Interrupted

        //  Get and check protocol signature
        myData->needle = zframe_data (frame);
        myData->ceiling = myData->needle + zframe_size (frame);
        uint16_t signature;
        GET_NUMBER2 (signature);
        if (signature == (0xAAA0 | 2))
            break;                  //  Valid signature

        //  Protocol assertion, drop message
        while (zsocket_rcvmore (input)) {
            zframe_destroy (&frame);
            frame = zframe_recv (input);
        }
        zframe_destroy (&frame);
    }
    //  Get message id and parse per message type
    GET_NUMBER1 (myData->id);

    switch (myData->id) {
        case ZRE_LOG_MSG_LOG:
            GET_NUMBER1 (myData->level);
            GET_NUMBER1 (myData->event);
            GET_NUMBER2 (myData->node);
            GET_NUMBER2 (myData->peer);
            GET_NUMBER8 (myData->time);
            free (myData->data);
            GET_STRING (myData->data);
            break;

        default:
            goto malformed;
    }
    //  Successful return
    zframe_destroy (&frame);
    return shell;

    //  Error returns
    malformed:
        printf ("E: malformed message '%d'\n", myData->id);
    empty:
        zframe_destroy (&frame);
        delete shell;
        return nullptr;
}


//  --------------------------------------------------------------------------
//  Send the zre_log_msg to the socket, and destroy it
//  Returns 0 if OK, else -1

int
zre_log_msg::send (void *output)
{
    assert (output);

    //  Calculate size of serialized data
    size_t frame_size = 2 + 1;          //  Signature and message ID
    switch (myData->id) {
        case ZRE_LOG_MSG_LOG:
            //  level is a 1-byte integer
            frame_size += 1;
            //  event is a 1-byte integer
            frame_size += 1;
            //  node is a 2-byte integer
            frame_size += 2;
            //  peer is a 2-byte integer
            frame_size += 2;
            //  time is a 8-byte integer
            frame_size += 8;
            //  data is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (myData->data)
                frame_size += strlen (myData->data);
            break;
            
        default:
            printf ("E: bad message type '%d', not sent\n", myData->id);
            //  No recovery, this is a fatal application error
            assert (false);
    }
    //  Now serialize message into the frame
    zframe_t *frame = zframe_new (nullptr, frame_size);
    myData->needle = zframe_data (frame);
    size_t string_size;
    int frame_flags = 0;
    PUT_NUMBER2 (0xAAA0 | 2);
    PUT_NUMBER1 (myData->id);

    switch (myData->id) {
        case ZRE_LOG_MSG_LOG:
            PUT_NUMBER1 (myData->level);
            PUT_NUMBER1 (myData->event);
            PUT_NUMBER2 (myData->node);
            PUT_NUMBER2 (myData->peer);
            PUT_NUMBER8 (myData->time);
            if (myData->data) {
                PUT_STRING (myData->data);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            break;
            
    }
    //  If we're sending to a ROUTER, we send the address first
    if (zsockopt_type (output) == ZMQ_ROUTER) {
        assert (myData->address);
        if (zframe_send (&myData->address, output, ZFRAME_MORE)) {
            zframe_destroy (&frame);
            delete this;
            return -1;
        }
    }
    //  Now send the data frame
    if (zframe_send (&frame, output, frame_flags)) {
        zframe_destroy (&frame);
        delete this;
        return -1;
    }
    
    //  Now send any frame fields, in order
    switch (myData->id) {
		// TODO!
    }
    //  Destroy zre_log_msg object
    delete this;
    return 0;
}


//  --------------------------------------------------------------------------
//  Send the LOG to the socket in one step

int
zre_log_msg::send_log (
    void *output,
    byte level,
    byte event,
    uint16_t node,
    uint16_t peer,
    uint64_t time,
    char *data)
{
    auto self = new zre_log_msg (ZRE_LOG_MSG_LOG);
    self->level_set(level);
    self->event_set(event);
    self->node_set(node);
    self->peer_set(peer);
    self->time_set(time);
    self->data_set(data);
	return self->send(output);
}


//  --------------------------------------------------------------------------
//  Duplicate the zre_log_msg message

zre_log_msg *
zre_log_msg::dup ()
{
    auto shell = new zre_log_msg(myData->id);
	auto copy = shell->myData;
    if (myData->address)
        copy->address = zframe_dup (myData->address);
    switch (myData->id) {
        case ZRE_LOG_MSG_LOG:
            copy->level = myData->level;
            copy->event = myData->event;
            copy->node = myData->node;
            copy->peer = myData->peer;
            copy->time = myData->time;
            copy->data = _strdup (myData->data);
            break;

    }
    return shell;
}



//  --------------------------------------------------------------------------
//  Print contents of message to stdout

void
zre_log_msg::dump ()
{
    switch (myData->id) {
        case ZRE_LOG_MSG_LOG:
            puts ("LOG:");
            printf ("    level=%ld\n", (long) myData->level);
            printf ("    event=%ld\n", (long) myData->event);
            printf ("    node=%ld\n", (long) myData->node);
            printf ("    peer=%ld\n", (long) myData->peer);
            printf ("    time=%ld\n", (long) myData->time);
            if (myData->data)
                printf ("    data='%s'\n", myData->data);
            else
                printf ("    data=\n");
            break;
            
    }
}


//  --------------------------------------------------------------------------
//  Get/set the message address

zframe_t *
zre_log_msg::address_get ()
{
    return myData->address;
}

void
zre_log_msg::address_set (zframe_t *address)
{
    if (myData->address)
        zframe_destroy (&myData->address);
    myData->address = zframe_dup (address);
}


//  --------------------------------------------------------------------------
//  Get/set the zre_log_msg id

int
zre_log_msg::id_get ()
{
    return myData->id;
}

void
zre_log_msg::id_set (int id)
{
    myData->id = id;
}

//  --------------------------------------------------------------------------
//  Return a printable command string

char *
zre_log_msg::command ()
{
    switch (myData->id) {
        case ZRE_LOG_MSG_LOG:
            return ("LOG");
            break;
    }
    return "?";
}

//  --------------------------------------------------------------------------
//  Get/set the level field

byte
zre_log_msg::level_get ()
{
    return myData->level;
}

void
zre_log_msg::level_set (byte level)
{
    myData->level = level;
}


//  --------------------------------------------------------------------------
//  Get/set the event field

byte
zre_log_msg::event_get ()
{
    return myData->event;
}

void
zre_log_msg::event_set (byte event)
{
    myData->event = event;
}


//  --------------------------------------------------------------------------
//  Get/set the node field

uint16_t
zre_log_msg::node_get ()
{
    return myData->node;
}

void
zre_log_msg::node_set (uint16_t node)
{
    myData->node = node;
}


//  --------------------------------------------------------------------------
//  Get/set the peer field

uint16_t
zre_log_msg::peer_get ()
{
    return myData->peer;
}

void
zre_log_msg::peer_set (uint16_t peer)
{
    myData->peer = peer;
}


//  --------------------------------------------------------------------------
//  Get/set the time field

uint64_t
zre_log_msg::time_get ()
{
    return myData->time;
}

void
zre_log_msg::time_set (uint64_t time)
{
    myData->time = time;
}


//  --------------------------------------------------------------------------
//  Get/set the data field

char *
zre_log_msg::data_get ()
{
    return myData->data;
}

void
zre_log_msg::data_set (char *format, ...)
{
    //  Format into newly allocated string
    va_list argptr;
    va_start (argptr, format);
    free (myData->data);
    myData->data = (char *) malloc (STRING_MAX + 1);
    assert (myData->data);
    vsnprintf (myData->data, STRING_MAX, format, argptr);
    va_end (argptr);
}



//  --------------------------------------------------------------------------
//  Selftest

int
zre_log_msg_test (bool verbose)
{
    printf (" * zre_log_msg: ");

    //  Simple create/destroy test
    auto self = new zre_log_msg (0);
    assert (self);
    delete self;

    //  Create pair of sockets we can send through
    zctx_t *ctx = zctx_new ();
    assert (ctx);

    void *output = zsocket_new (ctx, ZMQ_DEALER);
    assert (output);
    zsocket_bind (output, "inproc://selftest");
    void *input = zsocket_new (ctx, ZMQ_ROUTER);
    assert (input);
    zsocket_connect (input, "inproc://selftest");
    
    //  Encode/send/decode and verify each message type

    self = new zre_log_msg (ZRE_LOG_MSG_LOG);
    self->level_set(123);
    self->event_set(123);
    self->node_set(123);
    self->peer_set(123);
    self->time_set(123);
    self->data_set("Life is short but Now lasts for ever");
	self->send(output);
    
	self = zre_log_msg::recv(input);
    assert (self);
	assert (self->level_get() == 123);
	assert (self->event_get() == 123);
	assert (self->node_get() == 123);
	assert (self->peer_get() == 123);
	assert (self->time_get() == 123);
	assert (streq (self->data_get(), "Life is short but Now lasts for ever"));
    delete self;

    zctx_destroy (&ctx);
    printf ("OK\n");
    return 0;
}
