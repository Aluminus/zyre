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

#ifndef __ZRE_UUID_H_INCLUDED__
#define __ZRE_UUID_H_INCLUDED__

#define ZRE_UUID_LEN    16

struct zre_uuid_data_t;

class zre_uuid
{
public:

//  Constructor
    zre_uuid ();

//  Destructor
    ~zre_uuid ();

//  Returns UUID as string
char *
    str ();

//  Set UUID to new supplied value 
void
    set (byte *source);
    
//  Store UUID blob in target array
void
    cpy (byte *target);

//  Check if UUID is same as supplied value
bool
    eq (byte *compare);

//  Check if UUID is different from supplied value
bool
    neq (byte *compare);

private:
	zre_uuid_data_t* myData;
};

//  Self test of this class
ZRE_EXPORT int
    zre_uuid_test (bool verbose);

#endif
