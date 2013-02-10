/*  =========================================================================
    zre_uuid - UUID support class

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
#if !defined (__WINDOWS__)
#   include <uuid/uuid.h>
#endif

//  Structure of our class

struct zre_uuid_data_t {
    byte uuid [ZRE_UUID_LEN];           //  Binary UUID
    char str [ZRE_UUID_LEN * 2 + 1];    //  Printable UUID
};


//  --------------------------------------------------------------------------
//  Constructor

zre_uuid::zre_uuid ()
{
    myData = new zre_uuid_data_t;
#if defined (__WINDOWS__)
    UUID uuid;
    assert (sizeof (uuid) == ZRE_UUID_LEN);
    UuidCreate (&uuid);
    set((byte *) &uuid);
#else
    uuid_t uuid;
    assert (sizeof (uuid) == ZRE_UUID_LEN);
    uuid_generate (uuid);
    set((byte *) uuid);
#endif
}


//  -----------------------------------------------------------------
//  Destructor

zre_uuid::~zre_uuid ()
{
    delete myData;
}


//  -----------------------------------------------------------------
//  Set UUID to new supplied value

void
zre_uuid::set (byte *source)
{
    memcpy (myData->uuid, source, ZRE_UUID_LEN);
    char hex_char [] = "0123456789ABCDEF";
    int byte_nbr;
    for (byte_nbr = 0; byte_nbr < ZRE_UUID_LEN; byte_nbr++) {
        uint val = (myData->uuid) [byte_nbr];
        myData->str [byte_nbr * 2 + 0] = hex_char [val >> 4];
        myData->str [byte_nbr * 2 + 1] = hex_char [val & 15];
    }
    myData->str [ZRE_UUID_LEN * 2] = 0;
}


//  -----------------------------------------------------------------
//  Returns UUID as string

char *
zre_uuid::str ()
{
    return myData->str;
}


//  -----------------------------------------------------------------
//  Store UUID blob in target array

void
zre_uuid::cpy (byte *target)
{
    memcpy (target, myData->uuid, ZRE_UUID_LEN);
}


//  -----------------------------------------------------------------
//  Check if UUID is same as supplied value

bool
zre_uuid::eq (byte *compare)
{
    return (memcmp (myData->uuid, compare, ZRE_UUID_LEN) == 0);
}


//  -----------------------------------------------------------------
//  Check if UUID is different from supplied value

bool
zre_uuid::neq (byte *compare)
{
    return (memcmp (myData->uuid, compare, ZRE_UUID_LEN) != 0);
}


//  --------------------------------------------------------------------------
//  Selftest

int
zre_uuid_test (bool verbose)
{
    printf (" * zre_uuid: ");

    //  Simple create/destroy test
    auto self = new zre_uuid ();
    assert (self);
	assert (strlen (self->str()) == 32);
    delete self;

    printf ("OK\n");
    return 0;
}
