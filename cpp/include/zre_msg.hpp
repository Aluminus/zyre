/*  =========================================================================
    zre_msg.hpp
    
    Generated codec header for zre_msg
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

#ifndef __ZRE_MSG_H_INCLUDED__
#define __ZRE_MSG_H_INCLUDED__

#include <cstdint>
#include "platform.hpp"

/*  These are the zre_msg messages
    HELLO - Greet a peer so it can connect back to us
        sequence      number 2
        ipaddress     string
        mailbox       number 2
        groups        strings
        status        number 1
        headers       dictionary
    WHISPER - Send a message to a peer
        sequence      number 2
        content       frame
    SHOUT - Send a message to a group
        sequence      number 2
        group         string
        content       frame
    JOIN - Join a group
        sequence      number 2
        group         string
        status        number 1
    LEAVE - Leave a group
        sequence      number 2
        group         string
        status        number 1
    PING - Ping a peer that has gone silent
        sequence      number 2
    PING_OK - Reply to a peer's ping
        sequence      number 2
*/

#define ZRE_MSG_VERSION                     1

#define ZRE_MSG_HELLO                       1
#define ZRE_MSG_WHISPER                     2
#define ZRE_MSG_SHOUT                       3
#define ZRE_MSG_JOIN                        4
#define ZRE_MSG_LEAVE                       5
#define ZRE_MSG_PING                        6
#define ZRE_MSG_PING_OK                     7

//  Opaque class structure
struct zre_msg_data_t;

class zre_msg
{
public:

//  Create a new zre_msg
    zre_msg (int id);

//  Destroy the zre_msg
    ~zre_msg ();

//  Receive and parse a zre_msg from the input
static zre_msg *
    recv (void *input);

//  Send the zre_msg to the output, and destroy it
int
    send (void *output);

//  Send the HELLO to the output in one step
static int
    send_hello (void *output,
        uint16_t sequence,
        char *ipaddress,
        uint16_t mailbox,
        zlist_t *groups,
        byte status,
        zhash_t *headers);
    
//  Send the WHISPER to the output in one step
static int
    send_whisper (void *output,
        uint16_t sequence,
        zframe_t *content);
    
//  Send the SHOUT to the output in one step
static int
    send_shout (void *output,
        uint16_t sequence,
        char *group,
        zframe_t *content);
    
//  Send the JOIN to the output in one step
static int
    send_join (void *output,
        uint16_t sequence,
        char *group,
        byte status);
    
//  Send the LEAVE to the output in one step
static int
    send_leave (void *output,
        uint16_t sequence,
        char *group,
        byte status);
    
//  Send the PING to the output in one step
static int
    send_ping (void *output,
        uint16_t sequence);
    
//  Send the PING_OK to the output in one step
static int
    send_ping_ok (void *output,
        uint16_t sequence);
    
//  Duplicate the zre_msg message
zre_msg *
    dup ();

//  Print contents of message to stdout
void
    dump ();

//  Get/set the message address
zframe_t *
    address_get ();
void
    address_set (zframe_t *address);

//  Get the zre_msg id and printable command
int
    id_get ();
void
    id_set (int id);
char *
    command_get ();

//  Get/set the sequence field
uint16_t
    sequence_get ();
void
    sequence_set (uint16_t sequence);

//  Get/set the ipaddress field
char *
    ipaddress_get ();
void
    ipaddress_set (char *format, ...);

//  Get/set the mailbox field
uint16_t
    mailbox_get ();
void
    mailbox_set (uint16_t mailbox);

//  Get/set the groups field
zlist_t *
    groups_get ();
void
    groups_set (zlist_t *groups);

//  Iterate through the groups field, and append a groups value
char *
    groups_first ();
char *
    groups_next ();
void
    groups_append (char *format, ...);
size_t
    groups_size ();

//  Get/set the status field
byte
    status_get ();
void
    status_set (byte status);

//  Get/set the headers field
zhash_t *
    headers_get ();
void
    headers_set (zhash_t *headers);
    
//  Get/set a value in the headers dictionary
char *
    headers_string (char *key, char *default_value);
uint64_t
    headers_number (char *key, uint64_t default_value);
void
    headers_insert (char *key, char *format, ...);
size_t
    headers_size ();

//  Get/set the content field
zframe_t *
    content_get ();
void
    content_set (zframe_t *frame);

//  Get/set the group field
char *
    group_get ();
void
    group_set (char *format, ...);

//  Count size of key=value pair
static int
s_headers_count (const char *key, void *item, void *argument);

//  Serialize headers key=value pair
static int
s_headers_write (const char *key, void *item, void *argument);

private:
	zre_msg_data_t* myData;
};

//  Self test of this class
ZRE_EXPORT int
    zre_msg_test (bool verbose);
  

#endif
