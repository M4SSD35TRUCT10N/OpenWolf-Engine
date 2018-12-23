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
// File name:   serverGame.cpp
// Version:     v1.01
// Created:
// Compilers:   Visual Studio 2017, gcc 7.3.0
// Description: interface to the game dll
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#ifdef DEDICATED
#include <null/null_precompiled.h>
#else
#include <OWLib/precompiled.h>
#endif

botlib_export_t* botlib_export;

idGame* game;
idGame* ( *gameDllEntry )( gameImports_t* gimports );

static gameImports_t exports;

idServerGameSystemLocal serverGameSystemLocal;
idServerGameSystem* serverGameSystem = &serverGameSystemLocal;

/*
===============
idServerGameSystemLocal::idServerGameSystemLocal
===============
*/
idServerGameSystemLocal::idServerGameSystemLocal( void )
{
}

/*
===============
idServerGameSystemLocal::~idServerGameSystemLocal
===============
*/
idServerGameSystemLocal::~idServerGameSystemLocal( void )
{
}

/*
==================
idServerGameSystemLocal::GameError
==================
*/
void idServerGameSystemLocal::GameError( StringEntry string )
{
    Com_Error( ERR_DROP, "%s", string );
}

/*
==================
idServerGameSystemLocal::GamePrint
==================
*/
void idServerGameSystemLocal::GamePrint( StringEntry string )
{
    Com_Printf( "%s", string );
}

// these functions must be used instead of pointer arithmetic, because
// the game allocates gentities with private information after the server shared part
/*
==================
idServerGameSystemLocal::NumForGentity
==================
*/
S32 idServerGameSystemLocal::NumForGentity( sharedEntity_t* ent )
{
    S32 num;
    
    num = ( ( U8* ) ent - ( U8* ) sv.gentities ) / sv.gentitySize;
    
    return num;
}

/*
==================
idServerGameSystemLocal::GentityNum
==================
*/
sharedEntity_t* idServerGameSystemLocal::GentityNum( S32 num )
{
    sharedEntity_t* ent;
    
    ent = ( sharedEntity_t* )( ( U8* ) sv.gentities + sv.gentitySize * ( num ) );
    
    return ent;
}

/*
==================
idServerGameSystemLocal::GentityNum
==================
*/
playerState_t* idServerGameSystemLocal::GameClientNum( S32 num )
{
    playerState_t*  ps;
    
    ps = ( playerState_t* )( ( U8* ) sv.gameClients + sv.gameClientSize * ( num ) );
    
    return ps;
}

/*
==================
idServerGameSystemLocal::SvEntityForGentity
==================
*/
svEntity_t* idServerGameSystemLocal::SvEntityForGentity( sharedEntity_t* gEnt )
{
    if( !gEnt || gEnt->s.number < 0 || gEnt->s.number >= MAX_GENTITIES )
    {
        Com_Error( ERR_DROP, "idServerGameSystemLocal::SvEntityForGentity: bad gEnt" );
    }
    
    return &sv.svEntities[gEnt->s.number];
}

/*
==================
idServerGameSystemLocal::GEntityForSvEntity
==================
*/
sharedEntity_t* idServerGameSystemLocal::GEntityForSvEntity( svEntity_t* svEnt )
{
    S32 num;
    
    num = svEnt - sv.svEntities;
    return serverGameSystemLocal.GentityNum( num );
}

/*
===============
idServerGameSystemLocal::GameSendServerCommand

Sends a command string to a client
===============
*/
void idServerGameSystemLocal::GameSendServerCommand( S32 clientNum, StringEntry text )
{
    if( clientNum == -1 )
    {
        SV_SendServerCommand( NULL, "%s", text );
    }
    else
    {
        if( clientNum < 0 || clientNum >= sv_maxclients->integer )
        {
            return;
        }
        SV_SendServerCommand( svs.clients + clientNum, "%s", text );
    }
}


/*
===============
idServerGameSystemLocal::GameDropClient

Disconnects the client with a message
===============
*/
void idServerGameSystemLocal::GameDropClient( S32 clientNum, StringEntry reason, S32 length )
{
    if( clientNum < 0 || clientNum >= sv_maxclients->integer )
    {
        return;
    }
    
    serverClientLocal.DropClient( svs.clients + clientNum, reason );
    
    if( length )
    {
        serverCcmdsLocal.TempBanNetAddress( svs.clients[clientNum].netchan.remoteAddress, length );
    }
}

/*
=================
idServerGameSystemLocal::SetBrushModel

sets mins and maxs for inline bmodels
=================
*/
void idServerGameSystemLocal::SetBrushModel( sharedEntity_t* ent, StringEntry name )
{
    clipHandle_t h;
    vec3_t mins, maxs;
    
    if( !name )
    {
        Com_Error( ERR_DROP, "idServerGameSystemLocal::SetBrushModel: NULL" );
    }
    
    if( name[0] != '*' )
    {
        Com_Error( ERR_DROP, "idServerGameSystemLocal::SetBrushModel: %s isn't a brush model", name );
    }
    
    ent->s.modelindex = atoi( name + 1 );
    
    h = collisionModelManager->InlineModel( ent->s.modelindex );
    collisionModelManager->ModelBounds( h, mins, maxs );
    VectorCopy( mins, ent->r.mins );
    VectorCopy( maxs, ent->r.maxs );
    ent->r.bmodel = true;
    
    ent->r.contents = -1;		// we don't know exactly what is in the brushes
    
    //SV_LinkEntity( ent );			// FIXME: remove
}

/*
=================
idServerGameSystemLocal::inPVS

Also checks portalareas so that doors block sight
=================
*/
bool idServerGameSystemLocal::inPVS( const vec3_t p1, const vec3_t p2 )
{
    S32 leafnum, cluster, area1, area2;
    U8* mask;
    
    leafnum = collisionModelManager->PointLeafnum( p1 );
    cluster = collisionModelManager->LeafCluster( leafnum );
    area1 = collisionModelManager->LeafArea( leafnum );
    mask = collisionModelManager->ClusterPVS( cluster );
    
    leafnum = collisionModelManager->PointLeafnum( p2 );
    cluster = collisionModelManager->LeafCluster( leafnum );
    area2 = collisionModelManager->LeafArea( leafnum );
    
    if( mask && ( !( mask[cluster >> 3] & ( 1 << ( cluster & 7 ) ) ) ) )
    {
        return false;
    }
    
    if( !collisionModelManager->AreasConnected( area1, area2 ) )
    {
        return false; // a door blocks sight
    }
    
    return true;
}

/*
=================
idServerGameSystemLocal::inPVSIgnorePortals

Does NOT check portalareas
=================
*/
bool idServerGameSystemLocal::inPVSIgnorePortals( const vec3_t p1, const vec3_t p2 )
{
    S32 leafnum, cluster, area1, area2;
    U8* mask;
    
    leafnum = collisionModelManager->PointLeafnum( p1 );
    cluster = collisionModelManager->LeafCluster( leafnum );
    area1 = collisionModelManager->LeafArea( leafnum );
    mask = collisionModelManager->ClusterPVS( cluster );
    
    leafnum = collisionModelManager->PointLeafnum( p2 );
    cluster = collisionModelManager->LeafCluster( leafnum );
    area2 = collisionModelManager->LeafArea( leafnum );
    
    if( mask && ( !( mask[cluster >> 3] & ( 1 << ( cluster & 7 ) ) ) ) )
    {
        return false;
    }
    
    return true;
}

/*
========================
idServerGameSystemLocal::AdjustAreaPortalState
========================
*/
void idServerGameSystemLocal::AdjustAreaPortalState( sharedEntity_t* ent, bool open )
{
    svEntity_t* svEnt;
    
    svEnt = serverGameSystemLocal.SvEntityForGentity( ent );
    
    if( svEnt->areanum2 == -1 )
    {
        return;
    }
    
    collisionModelManager->AdjustAreaPortalState( svEnt->areanum, svEnt->areanum2, open );
}

/*
==================
idServerGameSystemLocal::GameAreaEntities
==================
*/
bool idServerGameSystemLocal::EntityContact( const vec3_t mins, const vec3_t maxs, const sharedEntity_t* gEnt, traceType_t type )
{
    const F32* origin, *angles;
    clipHandle_t ch;
    trace_t trace;
    
    // check for exact collision
    origin = gEnt->r.currentOrigin;
    angles = gEnt->r.currentAngles;
    
    ch = serverWorldSystemLocal.ClipHandleForEntity( gEnt );
    collisionModelManager->TransformedBoxTrace( &trace, vec3_origin, vec3_origin, mins, maxs, ch, -1, origin, angles, type );
    
    return trace.startsolid;
}

/*
===============
idServerGameSystemLocal::GetServerinfo
===============
*/
void idServerGameSystemLocal::GetServerinfo( UTF8* buffer, S32 bufferSize )
{
    if( bufferSize < 1 )
    {
        Com_Error( ERR_DROP, "idServerGameSystemLocal::GetServerinfo: bufferSize == %i", bufferSize );
    }
    
    Q_strncpyz( buffer, cvarSystem->InfoString( CVAR_SERVERINFO | CVAR_SERVERINFO_NOUPDATE ), bufferSize );
}

/*
===============
idServerGameSystemLocal::LocateGameData
===============
*/
void idServerGameSystemLocal::LocateGameData( sharedEntity_t* gEnts, S32 numGEntities, S32 sizeofGEntity_t, playerState_t* clients, S32 sizeofGameClient )
{
    sv.gentities = gEnts;
    sv.gentitySize = sizeofGEntity_t;
    sv.num_entities = numGEntities;
    
    sv.gameClients = clients;
    sv.gameClientSize = sizeofGameClient;
}

/*
===============
idServerGameSystemLocal::GetUsercmd
===============
*/
void idServerGameSystemLocal::GetUsercmd( S32 clientNum, usercmd_t* cmd )
{
    if( clientNum < 0 || clientNum >= sv_maxclients->integer )
    {
        Com_Error( ERR_DROP, "idServerGameSystemLocal::GetUsercmd: bad clientNum:%i", clientNum );
    }
    
    *cmd = svs.clients[clientNum].lastUsercmd;
}

/*
===============
idServerGameSystemLocal::UpdateSharedConfig
===============
*/
void idServerGameSystemLocal::UpdateSharedConfig( U32 port, StringEntry rconpass )
{
    UTF8 message[MAX_RCON_MESSAGE];
    netadr_t to;
    
    message[0] = -1;
    message[1] = -1;
    message[2] = -1;
    message[3] = -1;
    message[4] = 0;
    
    Q_strcat( message, MAX_RCON_MESSAGE, "rcon " );
    
    Q_strcat( message, MAX_RCON_MESSAGE, rconpass );
    
    Q_strcat( message, MAX_RCON_MESSAGE, " !readconfig" );
    
    NET_StringToAdr( "127.0.0.1", &to, NA_UNSPEC );
    to.port = BigShort( port );
    
    NET_SendPacket( NS_SERVER, strlen( message ) + 1, message, to );
}

/*
===============
idServerGameSystemLocal::GetEntityToken
===============
*/
bool idServerGameSystemLocal::GetEntityToken( UTF8* buffer, S32 bufferSize )
{
    StringEntry s;
    
    s = COM_Parse( &sv.entityParsePoint );
    
    Q_strncpyz( buffer, s, bufferSize );
    
    if( !sv.entityParsePoint && !s[0] )
    {
        return false;
    }
    else
    {
        return true;
    }
}

/*
====================
idServerGameSystemLocal::PhysicsSetGravity
====================
*/
void idServerGameSystemLocal::PhysicsSetGravity( const idVec3& gravity )
{
    physicsManager->SetGravity( gravity );
}

/*
====================
idServerGameSystemLocal::AllocTraceModel
====================
*/
idTraceModel* idServerGameSystemLocal::AllocTraceModel( void )
{
    return physicsManager->AllocTraceModel();
}

/*
====================
idServerGameSystemLocal::ResetPhysics
====================
*/
void idServerGameSystemLocal::ResetPhysics( void )
{
    physicsManager->Reset();
}

/*
====================
idServerGameSystemLocal::BotGetUserCommand
====================
*/
void idServerGameSystemLocal::BotGetUserCommand( S32 clientNum, usercmd_t* ucmd )
{
    serverClientLocal.ClientThink( &svs.clients[clientNum], ucmd );
}

/*
====================
idServerGameSystemLocal::InitExportTable
====================
*/
void idServerGameSystemLocal::InitExportTable( void )
{
    exports.Printf = Com_Printf;
    exports.Error = Com_Error;
    exports.Milliseconds = Sys_Milliseconds;
    exports.Argc = Cmd_Argc;
    exports.Argv = Cmd_ArgvBuffer;
    exports.SendConsoleCommand = Cbuf_ExecuteText;
    exports.SetConfigstring = SV_SetConfigstring;
    exports.GetConfigstring = SV_GetConfigstring;
    exports.SetConfigstringRestrictions = SV_SetConfigstringRestrictions;
    exports.SetUserinfo = SV_SetUserinfo;
    exports.GetUserinfo = SV_GetUserinfo;
    exports.RealTime = Com_RealTime;
    exports.Sys_SnapVector = Sys_SnapVector;
    exports.AddCommand = Cmd_AddCommand;
    exports.RemoveCommand = Cmd_RemoveCommand;
    exports.LoadTag = SV_LoadTag;
    
    exports.botlib = botlib_export;
    exports.collisionModelManager = collisionModelManager;
#ifndef DEDICATED
    exports.soundSystem = soundSystem;
#endif
    exports.serverGameSystem = serverGameSystem;
    exports.serverBotSystem = serverBotSystem;
    exports.serverGameSystem = serverGameSystem;
    exports.serverWorldSystem = serverWorldSystem;
    exports.databaseSystem = databaseSystem;
    exports.fileSystem = fileSystem;
    exports.cvarSystem = cvarSystem;
}

/*
===============
idServerGameSystemLocal::ShutdownGameProgs

Called every time a map changes
===============
*/
void idServerGameSystemLocal::ShutdownGameProgs( void )
{
    if( !svs.gameStarted )
    {
        return;
    }
    
    if( !gvm || game == NULL )
    {
        return;
    }
    game->Shutdown( false );
    game = NULL;
    
    Sys_UnloadDll( gvm );
    gvm = NULL;
    if( sv_newGameShlib->string[0] )
    {
        fileSystem->Rename( sv_newGameShlib->string, "game" DLL_EXT );
        cvarSystem->Set( "sv_newGameShlib", "" );
    }
}

/*
==================
idServerGameSystemLocal::InitGameModule

Called for both a full init and a restart
==================
*/
void idServerGameSystemLocal::InitGameModule( bool restart )
{
    S32 i;
    
    // start the entity parsing at the beginning
    sv.entityParsePoint = collisionModelManager->EntityString();
    
    // clear all gentity pointers that might still be set from
    // a previous level
    for( i = 0; i < sv_maxclients->integer; i++ )
    {
        svs.clients[i].gentity = NULL;
    }
    
    // use the current msec count for a random seed
    // init for this gamestate
    game->Init( svs.time, Com_Milliseconds(), restart );
}

/*
===================
idServerGameSystemLocal::RestartGameProgs

Called on a map_restart, but not on a normal map change
===================
*/
void idServerGameSystemLocal::RestartGameProgs( void )
{
    InitExportTable();
    
    if( !gvm )
    {
        svs.gameStarted = false;
        Com_Error( ERR_DROP, "idServerGameSystemLocal::RestartGameProgs on game failed" );
        return;
    }
    
    game->Shutdown( true );
    
    InitGameModule( true );
}


/*
===============
idServerGameSystemLocal::InitGameProgs

Called on a normal map change, not on a map_restart
===============
*/
void idServerGameSystemLocal::InitGameProgs( void )
{
    sv.num_tagheaders = 0;
    sv.num_tags = 0;
    
    cvar_t* var = cvarSystem->Get( "bot_enable", "1", CVAR_LATCH );
    bot_enable = var ? var->integer : 0;
    
    // load the dll or bytecode
    gvm = Sys_LoadDll( "sgame" );
    if( !gvm )
    {
        Com_Error( ERR_FATAL, "idServerGameSystemLocal::InitGameProgs on game failed" );
    }
    
    // Get the entry point.
    gameDllEntry = ( idGame * ( QDECL* )( gameImports_t* ) )Sys_GetProcAddress( gvm, "dllEntry" );
    if( !gameDllEntry )
    {
        Com_Error( ERR_FATAL, "gameDllEntry on game failed.\n" );
    }
    
    svs.gameStarted = true;
    
    // Init the export table.
    InitExportTable();
    
    game = gameDllEntry( &exports );
    
    InitGameModule( false );
}

/*
====================
idServerGameSystemLocal::GameCommand

See if the current console command is claimed by the game
====================
*/
bool idServerGameSystemLocal::GameCommand( void )
{
    if( sv.state != SS_GAME )
    {
        return false;
    }
    
    return game->ConsoleCommand();
}

/*
====================
idServerGameSystemLocal::GameIsSinglePlayer
====================
*/
bool idServerGameSystemLocal::GameIsSinglePlayer( void )
{
    return ( bool )( com_gameInfo.spGameTypes & ( 1 << g_gameType->integer ) );
}

/*
====================
idServerGameSystemLocal::GameIsCoop

This is a modified SinglePlayer, no savegame capability for example
====================
*/
bool idServerGameSystemLocal::GameIsCoop( void )
{
    return ( bool )( com_gameInfo.coopGameTypes & ( 1 << g_gameType->integer ) );
}

/*
====================
idServerGameSystemLocal::GetTag
return false if unable to retrieve tag information for this client

Dushan - I have no idea if this ever worked in Wolfenstein: Enemy Territory
====================
*/
extern bool CL_GetTag( S32 clientNum, UTF8* tagname, orientation_t* _or );

bool idServerGameSystemLocal::GetTag( S32 clientNum, S32 tagFileNumber, UTF8* tagname, orientation_t* _or )
{
    S32 i;
    
    if( tagFileNumber > 0 && tagFileNumber <= sv.num_tagheaders )
    {
        for( i = sv.tagHeadersExt[tagFileNumber - 1].start; i < sv.tagHeadersExt[tagFileNumber - 1].start + sv.tagHeadersExt[tagFileNumber - 1].count; i++ )
        {
            if( !Q_stricmp( sv.tags[i].name, tagname ) )
            {
                VectorCopy( sv.tags[i].origin, _or->origin );
                VectorCopy( sv.tags[i].axis[0], _or->axis[0] );
                VectorCopy( sv.tags[i].axis[1], _or->axis[1] );
                VectorCopy( sv.tags[i].axis[2], _or->axis[2] );
                return true;
            }
        }
    }
    
    // Gordon: lets try and remove the inconsitancy between ded/non-ded servers...
    // Gordon: bleh, some code in clientthink_real really relies on this working on player models...
#ifndef DEDICATED				// TTimo: dedicated only binary defines DEDICATED
    if( com_dedicated->integer )
    {
        return false;
    }
    
    return CL_GetTag( clientNum, tagname, _or );
#else
    return false;
#endif
}
