/*  =========================================================================
    zre_group - group known to this node

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

struct zre_group_data_t {
    char *name;                 //  Group name
    zhash_t *peers;             //  Peers in group
};


//  ---------------------------------------------------------------------
//  Construct new group object

zre_group::zre_group (char *name, zhash_t *container)
{
    myData = new zre_group_data_t;
    myData->name = _strdup (name);
    myData->peers = zhash_new ();
    
    //  Insert into container if requested
    if (container) {
        zhash_insert (container, name, this);
        zhash_freefn (container, name, [] (void *argument)
		//  Callback when we remove group from container
		{
			zre_group *group = (zre_group *) argument;
			delete group;
		});
    }
}


//  ---------------------------------------------------------------------
//  Destroy group object

zre_group::~zre_group ()
{
    zhash_destroy (&myData->peers);
    delete[] myData->name;
    delete myData;
}


//  ---------------------------------------------------------------------
//  Add peer to group
//  Ignore duplicate joins

void
zre_group::join (zre_peer *peer)
{
    assert (peer);
    zhash_insert (myData->peers, peer->identity(), peer);
    peer->status_set(peer->status() + 1);
}


//  ---------------------------------------------------------------------
//  Remove peer from group

void
zre_group::leave (zre_peer *peer)
{
    assert (peer);
	zhash_delete (myData->peers, peer->identity());
	peer->status_set(peer->status() + 1);
}


static int
s_peer_send (const char *key, void *item, void *argument)
{
    zre_peer *peer = (zre_peer *) item;
    auto msg = ((zre_msg *) argument)->dup();
	peer->send(&msg);
    return 0;
}

//  ---------------------------------------------------------------------
//  Send message to all peers in group

void
zre_group::send (zre_msg **msg_p)
{
    zhash_foreach (myData->peers, s_peer_send, *msg_p);
    delete (*msg_p);
}
