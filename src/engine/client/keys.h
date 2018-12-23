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
// File name:   keys.h
// Version:     v1.01
// Created:
// Compilers:   Visual Studio 2017, gcc 7.3.0
// Description:
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#ifndef __KEYS_H__
#define __KEYS_H__

#ifndef __KEYCODES_H__
#include <GUI/keycodes.h>
#endif

typedef struct
{
    bool        down;
    S32             repeats;	// if > 1, it is autorepeating
    UTF8*           binding;
    S32             hash;
} qkey_t;

extern bool key_overstrikeMode;
extern qkey_t   keys[MAX_KEYS];

// NOTE TTimo the declaration of field_t and Field_Clear is now in qcommon/qcommon.h

void            Field_KeyDownEvent( field_t* edit, S32 key );
void            Field_CharEvent( field_t* edit, S32 ch );
void            Field_Draw( field_t* edit, S32 x, S32 y, bool showCursor, bool noColorEscape, F32 alpha );
void            Field_BigDraw( field_t* edit, S32 x, S32 y, bool showCursor, bool noColorEscape );

extern field_t  g_consoleField;
extern field_t  chatField;
extern S32      anykeydown;
extern bool chat_team;
extern bool chat_buddy;
// Dushan
extern bool chat_irc;

void            Key_WriteBindings( fileHandle_t f );
void            Key_SetBinding( S32 keynum, StringEntry binding );
void            Key_GetBindingByString( StringEntry binding, S32* key1, S32* key2 );
UTF8*           Key_GetBinding( S32 keynum );
bool			Key_IsDown( S32 keynum );
bool			Key_GetOverstrikeMode( void );
void            Key_SetOverstrikeMode( bool state );
void            Key_ClearStates( void );
S32             Key_GetKey( StringEntry binding );

#endif // !__KEYS_H__
