////////////////////////////////////////////////////////////////////////////////////////
// Copyright(C) 2018 Dusan Jocic <dusanjocic@msn.com>
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
// File name:   serverGame.h
// Version:     v1.01
// Created:
// Compilers:   Visual Studio 2017, gcc 7.3.0
// Description:
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#ifndef __SERVERGAME_H__
#define __SERVERGAME_H__

// taken from cl_main.cpp
#define MAX_RCON_MESSAGE 1024

//
// idServerGameSystemLocal
//
class idServerGameSystemLocal : public idServerGameSystem
{
public:
    virtual void ShutdownGameProgs( void );
    virtual bool GameCommand( void );
    virtual void LocateGameData( sharedEntity_t* gEnts, S32 numGEntities, S32 sizeofGEntity_t, playerState_t* clients, S32 sizeofGameClient );
    virtual void GameDropClient( S32 clientNum, StringEntry reason, S32 length );
    virtual void GameSendServerCommand( S32 clientNum, StringEntry text );
    virtual bool EntityContact( const vec3_t mins, const vec3_t maxs, const sharedEntity_t* gEnt, traceType_t type );
    virtual void SetBrushModel( sharedEntity_t* ent, StringEntry name );
    virtual bool inPVS( const vec3_t p1, const vec3_t p2 );
    virtual bool inPVSIgnorePortals( const vec3_t p1, const vec3_t p2 );
    virtual void GetServerinfo( UTF8* buffer, S32 bufferSize );
    virtual void AdjustAreaPortalState( sharedEntity_t* ent, bool open );
    virtual void UpdateSharedConfig( U32 port, StringEntry rconpass );
    virtual void GetUsercmd( S32 clientNum, usercmd_t* cmd );
    virtual bool GetTag( S32 clientNum, S32 tagFileNumber, UTF8* tagname, orientation_t* _or );
    virtual bool GetEntityToken( UTF8* buffer, S32 bufferSize );
    virtual void BotGetUserCommand( S32 clientNum, usercmd_t* ucmd );
    virtual void PhysicsSetGravity( const idVec3& gravity );
    virtual idTraceModel* AllocTraceModel( void );
    virtual void ResetPhysics( void );
    virtual bool GameIsSinglePlayer( void );
    virtual sharedEntity_t* GentityNum( S32 num );
    virtual svEntity_t* SvEntityForGentity( sharedEntity_t* gEnt );
    virtual void InitGameProgs( void );
    virtual bool GameIsCoop( void );
    virtual sharedEntity_t* GEntityForSvEntity( svEntity_t* svEnt );
    virtual void RestartGameProgs( void );
    virtual playerState_t* GameClientNum( S32 num );
public:
    idServerGameSystemLocal();
    ~idServerGameSystemLocal();
    
    static void GameError( StringEntry string );
    static void GamePrint( StringEntry string );
    static S32 NumForGentity( sharedEntity_t* ent );
    static void InitExportTable( void );
    static void InitGameModule( bool restart );
    
};

extern idServerGameSystemLocal serverGameSystemLocal;

#endif //!__SERVERGAME_H__
