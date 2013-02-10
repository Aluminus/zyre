/*  =========================================================================
    zre_msg.c

    Generated codec implementation for zre_msg
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
#include "../include/zre_msg.hpp"

//  Structure of our class

struct zre_msg_data_t {
    zframe_t *address;          //  Address of peer if any
    int id;                     //  zre_msg message ID
    byte *needle;               //  Read/write pointer for serialization
    byte *ceiling;              //  Valid upper limit for read pointer
    uint16_t sequence;
    char *ipaddress;
    uint16_t mailbox;
    zlist_t *groups;
    byte status;
    zhash_t *headers;
    size_t headers_bytes;       //  Size of dictionary content
    zframe_t *content;
    char *group;
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
//  Create a new zre_msg

zre_msg::zre_msg (int id)
{
    myData = new zre_msg_data_t;
    myData->id = id;
	myData->address = nullptr;
	myData->ceiling = nullptr;
	myData->content = nullptr;
	myData->group = nullptr;
	myData->groups = nullptr;
	myData->headers = nullptr;
	myData->ipaddress = nullptr;
	myData->needle = nullptr;
}


//  --------------------------------------------------------------------------
//  Destroy the zre_msg

zre_msg::~zre_msg ()
{
    //  Free class properties
    zframe_destroy (&myData->address);
    free (myData->ipaddress);
    if (myData->groups)
        zlist_destroy (&myData->groups);
    zhash_destroy (&myData->headers);
    zframe_destroy (&myData->content);
    free (myData->group);

    //  Free object itself
    delete myData;
}


//  --------------------------------------------------------------------------
//  Receive and parse a zre_msg from the socket. Returns new object or
//  NULL if error. Will block if there's no message waiting.

zre_msg *
zre_msg::recv (void *input)
{
    assert (input);
    auto shell = new zre_msg (0);
	auto myData = shell->myData;
    zframe_t *frame = NULL;
    size_t string_size;
    size_t list_size;
    size_t hash_size;

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
        if (signature == (0xAAA0 | 1))
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
        case ZRE_MSG_HELLO:
            GET_NUMBER2 (myData->sequence);
            free (myData->ipaddress);
            GET_STRING (myData->ipaddress);
            GET_NUMBER2 (myData->mailbox);
            GET_NUMBER1 (list_size);
            myData->groups = zlist_new ();
            zlist_autofree (myData->groups);
            while (list_size--) {
                char *string;
                GET_STRING (string);
                zlist_append (myData->groups, string);
                free (string);
            }
            GET_NUMBER1 (myData->status);
            GET_NUMBER1 (hash_size);
            myData->headers = zhash_new ();
            zhash_autofree (myData->headers);
            while (hash_size--) {
                char *string;
                GET_STRING (string);
                char *value = strchr (string, '=');
                if (value)
                    *value++ = 0;
                zhash_insert (myData->headers, string, value);
                free (string);
            }
            break;

        case ZRE_MSG_WHISPER:
            GET_NUMBER2 (myData->sequence);
            //  Get next frame, leave current untouched
            if (!zsocket_rcvmore (input))
                goto malformed;
            myData->content = zframe_recv (input);
            break;

        case ZRE_MSG_SHOUT:
            GET_NUMBER2 (myData->sequence);
            free (myData->group);
            GET_STRING (myData->group);
            //  Get next frame, leave current untouched
            if (!zsocket_rcvmore (input))
                goto malformed;
            myData->content = zframe_recv (input);
            break;

        case ZRE_MSG_JOIN:
            GET_NUMBER2 (myData->sequence);
            free (myData->group);
            GET_STRING (myData->group);
            GET_NUMBER1 (myData->status);
            break;

        case ZRE_MSG_LEAVE:
            GET_NUMBER2 (myData->sequence);
            free (myData->group);
            GET_STRING (myData->group);
            GET_NUMBER1 (myData->status);
            break;

        case ZRE_MSG_PING:
            GET_NUMBER2 (myData->sequence);
            break;

        case ZRE_MSG_PING_OK:
            GET_NUMBER2 (myData->sequence);
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

//  Count size of key=value pair
int
zre_msg::s_headers_count (const char *key, void *item, void *argument)
{
    auto shell = (zre_msg *) argument;
    shell->myData->headers_bytes += strlen (key) + 1 + strlen ((char *) item) + 1;
    return 0;
}

//  Serialize headers key=value pair
int
zre_msg::s_headers_write (const char *key, void *item, void *argument)
{
    auto shell = (zre_msg *) argument;
	auto myData = shell->myData;
    char string [STRING_MAX + 1];
    snprintf (string, STRING_MAX, "%s=%s", key, (char *) item);
    size_t string_size;
    PUT_STRING (string);
    return 0;
}


//  --------------------------------------------------------------------------
//  Send the zre_msg to the socket, and destroy it
//  Returns 0 if OK, else -1

int
zre_msg::send (void *output)
{
    assert (output);

    //  Calculate size of serialized data
    size_t frame_size = 2 + 1;          //  Signature and message ID
    switch (myData->id) {
        case ZRE_MSG_HELLO:
            //  sequence is a 2-byte integer
            frame_size += 2;
            //  ipaddress is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (myData->ipaddress)
                frame_size += strlen (myData->ipaddress);
            //  mailbox is a 2-byte integer
            frame_size += 2;
            //  groups is an array of strings
            frame_size++;       //  Size is one octet
            if (myData->groups) {
                //  Add up size of list contents
                char *groups = (char *) zlist_first (myData->groups);
                while (groups) {
                    frame_size += 1 + strlen (groups);
                    groups = (char *) zlist_next (myData->groups);
                }
            }
            //  status is a 1-byte integer
            frame_size += 1;
            //  headers is an array of key=value strings
            frame_size++;       //  Size is one octet
            if (myData->headers) {
                myData->headers_bytes = 0;
                //  Add up size of dictionary contents
                zhash_foreach (myData->headers, s_headers_count, this);
            }
            frame_size += myData->headers_bytes;
            break;
            
        case ZRE_MSG_WHISPER:
            //  sequence is a 2-byte integer
            frame_size += 2;
            break;
            
        case ZRE_MSG_SHOUT:
            //  sequence is a 2-byte integer
            frame_size += 2;
            //  group is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (myData->group)
                frame_size += strlen (myData->group);
            break;
            
        case ZRE_MSG_JOIN:
            //  sequence is a 2-byte integer
            frame_size += 2;
            //  group is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (myData->group)
                frame_size += strlen (myData->group);
            //  status is a 1-byte integer
            frame_size += 1;
            break;
            
        case ZRE_MSG_LEAVE:
            //  sequence is a 2-byte integer
            frame_size += 2;
            //  group is a string with 1-byte length
            frame_size++;       //  Size is one octet
            if (myData->group)
                frame_size += strlen (myData->group);
            //  status is a 1-byte integer
            frame_size += 1;
            break;
            
        case ZRE_MSG_PING:
            //  sequence is a 2-byte integer
            frame_size += 2;
            break;
            
        case ZRE_MSG_PING_OK:
            //  sequence is a 2-byte integer
            frame_size += 2;
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
    PUT_NUMBER2 (0xAAA0 | 1);
    PUT_NUMBER1 (myData->id);

    switch (myData->id) {
        case ZRE_MSG_HELLO:
            PUT_NUMBER2 (myData->sequence);
            if (myData->ipaddress) {
                PUT_STRING (myData->ipaddress);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            PUT_NUMBER2 (myData->mailbox);
            if (myData->groups != NULL) {
                PUT_NUMBER1 (zlist_size (myData->groups));
                char *groups = (char *) zlist_first (myData->groups);
                while (groups) {
                    PUT_STRING (groups);
                    groups = (char *) zlist_next (myData->groups);
                }
            }
            else
                PUT_NUMBER1 (0);    //  Empty string array
            PUT_NUMBER1 (myData->status);
            if (myData->headers != NULL) {
                PUT_NUMBER1 (zhash_size (myData->headers));
                zhash_foreach (myData->headers, s_headers_write, this);
            }
            else
                PUT_NUMBER1 (0);    //  Empty dictionary
            break;
            
        case ZRE_MSG_WHISPER:
            PUT_NUMBER2 (myData->sequence);
            frame_flags = ZFRAME_MORE;
            break;
            
        case ZRE_MSG_SHOUT:
            PUT_NUMBER2 (myData->sequence);
            if (myData->group) {
                PUT_STRING (myData->group);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            frame_flags = ZFRAME_MORE;
            break;
            
        case ZRE_MSG_JOIN:
            PUT_NUMBER2 (myData->sequence);
            if (myData->group) {
                PUT_STRING (myData->group);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            PUT_NUMBER1 (myData->status);
            break;
            
        case ZRE_MSG_LEAVE:
            PUT_NUMBER2 (myData->sequence);
            if (myData->group) {
                PUT_STRING (myData->group);
            }
            else
                PUT_NUMBER1 (0);    //  Empty string
            PUT_NUMBER1 (myData->status);
            break;
            
        case ZRE_MSG_PING:
            PUT_NUMBER2 (myData->sequence);
            break;
            
        case ZRE_MSG_PING_OK:
            PUT_NUMBER2 (myData->sequence);
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
        case ZRE_MSG_WHISPER:
            //  If content isn't set, send an empty frame
            if (!myData->content)
                myData->content = zframe_new (nullptr, 0);
            if (zframe_send (&myData->content, output, 0)) {
                zframe_destroy (&frame);
                delete this;
                return -1;
            }
            break;
        case ZRE_MSG_SHOUT:
            //  If content isn't set, send an empty frame
            if (!myData->content)
                myData->content = zframe_new (nullptr, 0);
            if (zframe_send (&myData->content, output, 0)) {
                zframe_destroy (&frame);
                delete this;
                return -1;
            }
            break;
    }
    //  Destroy zre_msg object
    delete this;
    return 0;
}


//  --------------------------------------------------------------------------
//  Send the HELLO to the socket in one step

int
zre_msg::send_hello (
    void *output,
    uint16_t sequence,
    char *ipaddress,
    uint16_t mailbox,
    zlist_t *groups,
    byte status,
    zhash_t *headers)
{
    auto self = new zre_msg (ZRE_MSG_HELLO);
    self->sequence_set(sequence);
    self->ipaddress_set(ipaddress);
    self->mailbox_set(mailbox);
    self->groups_set(zlist_dup (groups));
    self->status_set(status);
    self->headers_set(zhash_dup (headers));
	return self->send(output);
}


//  --------------------------------------------------------------------------
//  Send the WHISPER to the socket in one step

int
zre_msg::send_whisper (
    void *output,
    uint16_t sequence,
    zframe_t *content)
{
    auto self = new zre_msg (ZRE_MSG_WHISPER);
    self->sequence_set(sequence);
    self->content_set(zframe_dup (content));
	return self->send(output);
}


//  --------------------------------------------------------------------------
//  Send the SHOUT to the socket in one step

int
zre_msg::send_shout (
    void *output,
    uint16_t sequence,
    char *group,
    zframe_t *content)
{
    auto self = new zre_msg (ZRE_MSG_SHOUT);
    self->sequence_set(sequence);
    self->group_set(group);
    self->content_set(zframe_dup (content));
    return self->send(output);
}


//  --------------------------------------------------------------------------
//  Send the JOIN to the socket in one step

int
zre_msg::send_join (
    void *output,
    uint16_t sequence,
    char *group,
    byte status)
{
    auto self = new zre_msg (ZRE_MSG_JOIN);
    self->sequence_set(sequence);
    self->group_set(group);
    self->status_set(status);
	return self->send(output);
}


//  --------------------------------------------------------------------------
//  Send the LEAVE to the socket in one step

int
zre_msg::send_leave (
    void *output,
    uint16_t sequence,
    char *group,
    byte status)
{
    auto self = new zre_msg (ZRE_MSG_LEAVE);
    self->sequence_set(sequence);
    self->group_set(group);
    self->status_set(status);
	return self->send(output);
}


//  --------------------------------------------------------------------------
//  Send the PING to the socket in one step

int
zre_msg::send_ping (
    void *output,
    uint16_t sequence)
{
    auto self = new zre_msg (ZRE_MSG_PING);
    self->sequence_set(sequence);
	return self->send(output);
}


//  --------------------------------------------------------------------------
//  Send the PING_OK to the socket in one step

int
zre_msg::send_ping_ok (
    void *output,
    uint16_t sequence)
{
    auto self = new zre_msg (ZRE_MSG_PING_OK);
    self->sequence_set(sequence);
	return self->send(output);
}


//  --------------------------------------------------------------------------
//  Duplicate the zre_msg message

zre_msg *
zre_msg::dup ()
{
    auto shell = new zre_msg (myData->id);
	auto copy = shell->myData;
    if (myData->address)
        copy->address = zframe_dup (myData->address);
    switch (myData->id) {
        case ZRE_MSG_HELLO:
            copy->sequence = myData->sequence;
            copy->ipaddress = strdup (myData->ipaddress);
            copy->mailbox = myData->mailbox;
            copy->groups = zlist_copy (myData->groups);
            copy->status = myData->status;
            copy->headers = zhash_dup (myData->headers);
            break;

        case ZRE_MSG_WHISPER:
            copy->sequence = myData->sequence;
            copy->content = zframe_dup (myData->content);
            break;

        case ZRE_MSG_SHOUT:
            copy->sequence = myData->sequence;
            copy->group = strdup (myData->group);
            copy->content = zframe_dup (myData->content);
            break;

        case ZRE_MSG_JOIN:
            copy->sequence = myData->sequence;
            copy->group = strdup (myData->group);
            copy->status = myData->status;
            break;

        case ZRE_MSG_LEAVE:
            copy->sequence = myData->sequence;
            copy->group = strdup (myData->group);
            copy->status = myData->status;
            break;

        case ZRE_MSG_PING:
            copy->sequence = myData->sequence;
            break;

        case ZRE_MSG_PING_OK:
            copy->sequence = myData->sequence;
            break;

    }
    return shell;
}


//  Dump headers key=value pair to stdout
static int
s_headers_dump (const char *key, void *item, void *argument)
{
    auto self = (zre_msg *) argument;
    printf ("        %s=%s\n", key, (char *) item);
    return 0;
}


//  --------------------------------------------------------------------------
//  Print contents of message to stdout

void
zre_msg::dump ()
{
    switch (myData->id) {
        case ZRE_MSG_HELLO:
            puts ("HELLO:");
            printf ("    sequence=%ld\n", (long) myData->sequence);
            if (myData->ipaddress)
                printf ("    ipaddress='%s'\n", myData->ipaddress);
            else
                printf ("    ipaddress=\n");
            printf ("    mailbox=%ld\n", (long) myData->mailbox);
            printf ("    groups={");
            if (myData->groups) {
                char *groups = (char *) zlist_first (myData->groups);
                while (groups) {
                    printf (" '%s'", groups);
                    groups = (char *) zlist_next (myData->groups);
                }
            }
            printf (" }\n");
            printf ("    status=%ld\n", (long) myData->status);
            printf ("    headers={\n");
            if (myData->headers)
                zhash_foreach (myData->headers, s_headers_dump, this);
            printf ("    }\n");
            break;
            
        case ZRE_MSG_WHISPER:
            puts ("WHISPER:");
            printf ("    sequence=%ld\n", (long) myData->sequence);
            printf ("    content={\n");
            if (myData->content) {
                size_t size = zframe_size (myData->content);
                byte *data = zframe_data (myData->content);
                printf ("        size=%td\n", zframe_size (myData->content));
                if (size > 32)
                    size = 32;
                int content_index;
                for (content_index = 0; content_index < size; content_index++) {
                    if (content_index && (content_index % 4 == 0))
                        printf ("-");
                    printf ("%02X", data [content_index]);
                }
            }
            printf ("    }\n");
            break;
            
        case ZRE_MSG_SHOUT:
            puts ("SHOUT:");
            printf ("    sequence=%ld\n", (long) myData->sequence);
            if (myData->group)
                printf ("    group='%s'\n", myData->group);
            else
                printf ("    group=\n");
            printf ("    content={\n");
            if (myData->content) {
                size_t size = zframe_size (myData->content);
                byte *data = zframe_data (myData->content);
                printf ("        size=%td\n", zframe_size (myData->content));
                if (size > 32)
                    size = 32;
                int content_index;
                for (content_index = 0; content_index < size; content_index++) {
                    if (content_index && (content_index % 4 == 0))
                        printf ("-");
                    printf ("%02X", data [content_index]);
                }
            }
            printf ("    }\n");
            break;
            
        case ZRE_MSG_JOIN:
            puts ("JOIN:");
            printf ("    sequence=%ld\n", (long) myData->sequence);
            if (myData->group)
                printf ("    group='%s'\n", myData->group);
            else
                printf ("    group=\n");
            printf ("    status=%ld\n", (long) myData->status);
            break;
            
        case ZRE_MSG_LEAVE:
            puts ("LEAVE:");
            printf ("    sequence=%ld\n", (long) myData->sequence);
            if (myData->group)
                printf ("    group='%s'\n", myData->group);
            else
                printf ("    group=\n");
            printf ("    status=%ld\n", (long) myData->status);
            break;
            
        case ZRE_MSG_PING:
            puts ("PING:");
            printf ("    sequence=%ld\n", (long) myData->sequence);
            break;
            
        case ZRE_MSG_PING_OK:
            puts ("PING_OK:");
            printf ("    sequence=%ld\n", (long) myData->sequence);
            break;
            
    }
}


//  --------------------------------------------------------------------------
//  Get/set the message address

zframe_t *
zre_msg::address_get ()
{
    return myData->address;
}

void
zre_msg::address_set (zframe_t *address)
{
    if (myData->address)
        zframe_destroy (&myData->address);
    myData->address = zframe_dup (address);
}


//  --------------------------------------------------------------------------
//  Get/set the zre_msg id

int
zre_msg::id_get ()
{
    return myData->id;
}

void
zre_msg::id_set (int id)
{
    myData->id = id;
}

//  --------------------------------------------------------------------------
//  Return a printable command string

char *
zre_msg::command_get ()
{
    switch (myData->id) {
        case ZRE_MSG_HELLO:
            return ("HELLO");
            break;
        case ZRE_MSG_WHISPER:
            return ("WHISPER");
            break;
        case ZRE_MSG_SHOUT:
            return ("SHOUT");
            break;
        case ZRE_MSG_JOIN:
            return ("JOIN");
            break;
        case ZRE_MSG_LEAVE:
            return ("LEAVE");
            break;
        case ZRE_MSG_PING:
            return ("PING");
            break;
        case ZRE_MSG_PING_OK:
            return ("PING_OK");
            break;
    }
    return "?";
}

//  --------------------------------------------------------------------------
//  Get/set the sequence field

uint16_t
zre_msg::sequence_get ()
{
    return myData->sequence;
}

void
zre_msg::sequence_set (uint16_t sequence)
{
    myData->sequence = sequence;
}


//  --------------------------------------------------------------------------
//  Get/set the ipaddress field

char *
zre_msg::ipaddress_get ()
{
    return myData->ipaddress;
}

void
zre_msg::ipaddress_set (char *format, ...)
{
    //  Format into newly allocated string
    va_list argptr;
    va_start (argptr, format);
    free (myData->ipaddress);
    myData->ipaddress = (char *) malloc (STRING_MAX + 1);
    assert (myData->ipaddress);
    vsnprintf (myData->ipaddress, STRING_MAX, format, argptr);
    va_end (argptr);
}


//  --------------------------------------------------------------------------
//  Get/set the mailbox field

uint16_t
zre_msg::mailbox_get ()
{
    return myData->mailbox;
}

void
zre_msg::mailbox_set (uint16_t mailbox)
{
    myData->mailbox = mailbox;
}


//  --------------------------------------------------------------------------
//  Get/set the groups field

zlist_t *
zre_msg::groups_get ()
{
    return myData->groups;
}

//  Greedy function, takes ownership of groups; if you don't want that
//  then use zlist_dup() to pass a copy of groups

void
zre_msg::groups_set (zlist_t *groups)
{
    zlist_destroy (&myData->groups);
    myData->groups = groups;
}

//  --------------------------------------------------------------------------
//  Iterate through the groups field, and append a groups value

char *
zre_msg::groups_first ()
{
    if (myData->groups)
        return (char *) (zlist_first (myData->groups));
    else
        return nullptr;
}

char *
zre_msg::groups_next ()
{
    if (myData->groups)
        return (char *) (zlist_next (myData->groups));
    else
        return nullptr;
}

void
zre_msg::groups_append (char *format, ...)
{
    //  Format into newly allocated string
    va_list argptr;
    va_start (argptr, format);
    char *string = (char *) malloc (STRING_MAX + 1);
    assert (string);
    vsnprintf (string, STRING_MAX, format, argptr);
    va_end (argptr);
    
    //  Attach string to list
    if (!myData->groups) {
        myData->groups = zlist_new ();
        zlist_autofree (myData->groups);
    }
    zlist_append (myData->groups, string);
    free (string);
}

size_t
zre_msg::groups_size ()
{
    return zlist_size (myData->groups);
}


//  --------------------------------------------------------------------------
//  Get/set the status field

byte
zre_msg::status_get ()
{
    return myData->status;
}

void
zre_msg::status_set (byte status)
{
    myData->status = status;
}


//  --------------------------------------------------------------------------
//  Get/set the headers field

zhash_t *
zre_msg::headers_get ()
{
    return myData->headers;
}

//  Greedy function, takes ownership of headers; if you don't want that
//  then use zhash_dup() to pass a copy of headers

void
zre_msg::headers_set (zhash_t *headers)
{
    zhash_destroy (&myData->headers);
    myData->headers = headers;
}

//  --------------------------------------------------------------------------
//  Get/set a value in the headers dictionary

char *
zre_msg::headers_string (char *key, char *default_value)
{
    char *value = nullptr;
    if (myData->headers)
        value = (char *) (zhash_lookup (myData->headers, key));
    if (!value)
        value = default_value;

    return value;
}

uint64_t
zre_msg::headers_number (char *key, uint64_t default_value)
{
    uint64_t value = default_value;
    char *string = nullptr;
    if (myData->headers)
        string = (char *) (zhash_lookup (myData->headers, key));
    if (string)
        value = atol (string);

    return value;
}

void
zre_msg::headers_insert (char *key, char *format, ...)
{
    //  Format string into buffer
    va_list argptr;
    va_start (argptr, format);
    char *string = (char *) malloc (STRING_MAX + 1);
    assert (string);
    vsnprintf (string, STRING_MAX, format, argptr);
    va_end (argptr);

    //  Store string in hash table
    if (!myData->headers) {
        myData->headers = zhash_new ();
        zhash_autofree (myData->headers);
    }
    zhash_update (myData->headers, key, string);
    free (string);
}

size_t
zre_msg::headers_size ()
{
    return zhash_size (myData->headers);
}


//  --------------------------------------------------------------------------
//  Get/set the content field

zframe_t *
zre_msg::content_get ()
{
    return myData->content;
}

//  Takes ownership of supplied frame
void
zre_msg::content_set (zframe_t *frame)
{
    if (myData->content)
        zframe_destroy (&myData->content);
    myData->content = frame;
}

//  --------------------------------------------------------------------------
//  Get/set the group field

char *
zre_msg::group_get ()
{
    return myData->group;
}

void
zre_msg::group_set (char *format, ...)
{
    //  Format into newly allocated string
    va_list argptr;
    va_start (argptr, format);
    free (myData->group);
    myData->group = (char *) malloc (STRING_MAX + 1);
    assert (myData->group);
    vsnprintf (myData->group, STRING_MAX, format, argptr);
    va_end (argptr);
}



//  --------------------------------------------------------------------------
//  Selftest

int
zre_msg_test (bool verbose)
{
    printf (" * zre_msg: ");

    //  Simple create/destroy test
    auto self = new zre_msg (0);
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

    self = new zre_msg (ZRE_MSG_HELLO);
    self->sequence_set(123);
    self->ipaddress_set("Life is short but Now lasts for ever");
    self->mailbox_set(123);
    self->groups_append("Name: %s", "Brutus");
    self->groups_append("Age: %d", 43);
    self->status_set(123);
    self->headers_insert("Name", "Brutus");
    self->headers_insert("Age", "%d", 43);
	self->send(output);
    
    self = zre_msg::recv (input);
    assert (self);
	assert (self->sequence_get() == 123);
	assert (streq (self->ipaddress_get(), "Life is short but Now lasts for ever"));
	assert (self->mailbox_get() == 123);
	assert (self->groups_size() == 2);
    assert (streq (self->groups_first(), "Name: Brutus"));
    assert (streq (self->groups_next(), "Age: 43"));
	assert (self->status_get() == 123);
    assert (self->headers_size() == 2);
    assert (streq (self->headers_string("Name", "?"), "Brutus"));
    assert (self->headers_number("Age", 0) == 43);
    delete self;

    self = new zre_msg (ZRE_MSG_WHISPER);
    self->sequence_set(123);
    self->content_set(zframe_new ("Captcha Diem", 12));
	self->send(output);
    
    self = zre_msg::recv (input);
    assert (self);
    assert (self->sequence_get() == 123);
	assert (zframe_streq (self->content_get(), "Captcha Diem"));
    delete self;

    self = new zre_msg (ZRE_MSG_SHOUT);
    self->sequence_set(123);
    self->group_set("Life is short but Now lasts for ever");
    self->content_set(zframe_new ("Captcha Diem", 12));
	self->send(output);
    
    self = zre_msg::recv (input);
    assert (self);
	assert (self->sequence_get() == 123);
	assert (streq (self->group_get(), "Life is short but Now lasts for ever"));
	assert (zframe_streq (self->content_get(), "Captcha Diem"));
    delete self;

    self = new zre_msg (ZRE_MSG_JOIN);
    self->sequence_set(123);
    self->group_set("Life is short but Now lasts for ever");
    self->status_set(123);
	self->send(output);
    
    self = zre_msg::recv (input);
    assert (self);
	assert (self->sequence_get() == 123);
	assert (streq (self->group_get(), "Life is short but Now lasts for ever"));
	assert (self->status_get() == 123);
    delete self;

    self = new zre_msg (ZRE_MSG_LEAVE);
    self->sequence_set(123);
    self->group_set("Life is short but Now lasts for ever");
    self->status_set(123);
	self->send(output);
    
    self = zre_msg::recv (input);
    assert (self);
	assert (self->sequence_get() == 123);
	assert (streq (self->group_get(), "Life is short but Now lasts for ever"));
	assert (self->status_get() == 123);
    delete self;

    self = new zre_msg (ZRE_MSG_PING);
    self->sequence_set(123);
	self->send(output);
    
    self = zre_msg::recv (input);
    assert (self);
	assert (self->sequence_get() == 123);
    delete self;

    self = new zre_msg (ZRE_MSG_PING_OK);
    self->sequence_set(123);
	self->send(output);
    
    self = zre_msg::recv (input);
    assert (self);
	assert (self->sequence_get() == 123);
    delete self;

    zctx_destroy (&ctx);
    printf ("OK\n");
    return 0;
}
