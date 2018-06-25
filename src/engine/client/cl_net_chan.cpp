////////////////////////////////////////////////////////////////////////////////////////
// Copyright(C) 1999 - 2010 id Software LLC, a ZeniMax Media company.
// Copyright(C) 2011 - 2018 Dusan Jocic <dusanjocic@msn.com>
//
// This file is part of the OpenWolf GPL Source Code.
// OpenWolf Source Code is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// OpenWolf Source Code is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenWolf Source Code.  If not, see <http://www.gnu.org/licenses/>.
//
// In addition, the OpenWolf Source Code is also subject to certain additional terms.
// You should have received a copy of these additional terms immediately following the
// terms and conditions of the GNU General Public License which accompanied the
// OpenWolf Source Code. If not, please request a copy in writing from id Software
// at the address below.
//
// If you have questions concerning this license or the applicable additional terms,
// you may contact in writing id Software LLC, c/o ZeniMax Media Inc.,
// Suite 120, Rockville, Maryland 20850 USA.
//
// -------------------------------------------------------------------------------------
// File name:   cl_net_chan.cpp
// Version:     v1.00
// Created:
// Compilers:   Visual Studio 2015
// Description:
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#include <OWLIb/precompiled.h>

/*
==============
CL_Netchan_Encode

	// first 12 bytes of the data are always:
	S64 serverId;
	S64 messageAcknowledge;
	S64 reliableAcknowledge;

==============
*/
static void CL_Netchan_Encode( msg_t* msg )
{
    S32             serverId, messageAcknowledge, reliableAcknowledge;
    S32             i, index, srdc, sbit, soob;
    U8            key, *string;
    
    if( msg->cursize <= CL_ENCODE_START )
    {
        return;
    }
    
    srdc = msg->readcount;
    sbit = msg->bit;
    soob = msg->oob;
    
    msg->bit = 0;
    msg->readcount = 0;
    msg->oob = false;
    
    serverId = MSG_ReadLong( msg );
    messageAcknowledge = MSG_ReadLong( msg );
    reliableAcknowledge = MSG_ReadLong( msg );
    
    msg->oob = ( bool )soob;
    msg->bit = sbit;
    msg->readcount = srdc;
    
    string = ( U8* ) clc.serverCommands[reliableAcknowledge & ( MAX_RELIABLE_COMMANDS - 1 )];
    index = 0;
    //
    key = clc.challenge ^ serverId ^ messageAcknowledge;
    for( i = CL_ENCODE_START; i < msg->cursize; i++ )
    {
        // modify the key with the last received now acknowledged server command
        if( !string[index] )
        {
            index = 0;
        }
        if( string[index] > 127 || string[index] == '%' )
        {
            key ^= '.' << ( i & 1 );
        }
        else
        {
            key ^= string[index] << ( i & 1 );
        }
        index++;
        // encode the data with this key
        *( msg->data + i ) = ( *( msg->data + i ) ) ^ key;
    }
}

/*
==============
CL_Netchan_Decode

	// first four bytes of the data are always:
	S64 reliableAcknowledge;

==============
*/
static void CL_Netchan_Decode( msg_t* msg )
{
    S64            reliableAcknowledge, i, index;
    U8            key, *string;
    S32             srdc, sbit;
    bool        soob;
    
    srdc = msg->readcount;
    sbit = msg->bit;
    soob = msg->oob;
    
    msg->oob = false;
    
    reliableAcknowledge = MSG_ReadLong( msg );
    
    msg->oob = soob;
    msg->bit = sbit;
    msg->readcount = srdc;
    
    string = ( U8* ) clc.reliableCommands[reliableAcknowledge & ( MAX_RELIABLE_COMMANDS - 1 )];
    index = 0;
    // xor the client challenge with the netchan sequence number (need something that changes every message)
    key = ( clc.challenge ^ LittleLong( *( U32* )msg->data ) ) & 0xFF;
    for( i = msg->readcount + CL_DECODE_START; i < msg->cursize; i++ )
    {
        // modify the key with the last sent and with this message acknowledged client command
        if( !string[index] )
        {
            index = 0;
        }
        if( string[index] > 127 || string[index] == '%' )
        {
            key ^= '.' << ( i & 1 );
        }
        else
        {
            key ^= string[index] << ( i & 1 );
        }
        index++;
        // decode the data with this key
        *( msg->data + i ) = *( msg->data + i ) ^ key;
    }
}

/*
=================
CL_Netchan_TransmitNextFragment
=================
*/
void CL_Netchan_TransmitNextFragment( netchan_t* chan )
{
    Netchan_TransmitNextFragment( chan );
}

extern bool SV_GameIsSinglePlayer( void );

/*
================
CL_Netchan_Transmit
================
*/
void CL_Netchan_Transmit( netchan_t* chan, msg_t* msg )
{
    MSG_WriteByte( msg, clc_EOF );
    
    if( !SV_GameIsSinglePlayer() )
    {
        CL_Netchan_Encode( msg );
    }
    Netchan_Transmit( chan, msg->cursize, msg->data );
}

extern S32      oldsize;
S32             newsize = 0;

/*
=================
CL_Netchan_Process
=================
*/
bool CL_Netchan_Process( netchan_t* chan, msg_t* msg )
{
    S32             ret;
    
    ret = Netchan_Process( chan, msg );
    if( !ret )
    {
        return false;
    }
    if( !SV_GameIsSinglePlayer() )
    {
        CL_Netchan_Decode( msg );
    }
    newsize += msg->cursize;
    return true;
}
