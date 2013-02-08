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

#ifndef __ZRE_GROUP_H_INCLUDED__
#define __ZRE_GROUP_H_INCLUDED__

struct zre_group_data_t;

class zre_group
{
public:

//  Constructor
    zre_group (char *name, zhash_t *container);

//  Destructor
    ~zre_group ();

//  Add peer to group
void
    join (zre_peer *peer);

//  Remove peer from group
void
    leave (zre_peer *peer);

//  Send message to all peers in group
void
    send (zre_msg_t **msg_p);

private:
	zre_group_data_t* myData;
};

#endif
