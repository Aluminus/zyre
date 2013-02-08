/*  =========================================================================
    zre_node - node on a ZRE network

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

#ifndef __ZRE_NODE_H_INCLUDED__
#define __ZRE_NODE_H_INCLUDED__

#include <memory>

//  Optional global context for zre_node instances
//  Used for large-scale testing simulation only
ZRE_EXPORT extern zctx_t *zre_global_ctx;

//  Optional temp directory; set by caller if needed
ZRE_EXPORT extern char *zre_global_tmpdir;

struct zre_node_data_t;	// Opaque node data type

class zre_node
{
public:

//  Constructor
ZRE_EXPORT zre_node ();

//  Destructor
ZRE_EXPORT ~zre_node ();

//  Set node tracing on or off
ZRE_EXPORT void
    verbose_set (bool verbose);

//  Join a group
ZRE_EXPORT int
    join (const char *group);
    
//  Leave a group
ZRE_EXPORT int
    leave (const char *group);

//  Receive next message from node
ZRE_EXPORT zmsg_t *
    recv ();

//  Send message to single peer; peer ID is first frame in message
ZRE_EXPORT int
    whisper (zmsg_t **msg_p);
    
//  Send message to a group of peers
ZRE_EXPORT int
    shout (zmsg_t **msg_p);
    
//  Return node handle, for polling
ZRE_EXPORT void *
    handle () const;

//  Set node header value
ZRE_EXPORT void
    header_set (char *name, char *format, ...);

//  Publish file under some logical name
//  Physical name is the actual file location
ZRE_EXPORT void
    publish (char *logical, char *physical);

//  Retract published file 
ZRE_EXPORT void
    retract (char *logical);

private:
	zre_node_data_t* myData;
};

#endif
