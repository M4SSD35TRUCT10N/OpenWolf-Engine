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
// File name:   serverCcmds.cpp
// Version:     v1.01
// Created:
// Compilers:   Visual Studio 2017, gcc 7.3.0
// Description:
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#ifdef DEDICATED
#include <null/null_precompiled.h>
#else
#include <OWLib/precompiled.h>
#endif

idServerCcmdsSystemLocal serverCcmdsLocal;

/*
===============
idServerCcmdsSystemLocal::idServerCcmdsSystemLocal
===============
*/
idServerCcmdsSystemLocal::idServerCcmdsSystemLocal( void )
{
}

/*
===============
idServerCcmdsSystemLocal::~idServerCcmdsSystemLocal
===============
*/
idServerCcmdsSystemLocal::~idServerCcmdsSystemLocal( void )
{
}

/*
===============================================================================
OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/

/*
==================
idServerCcmdsSystemLocal::GetPlayerByName

Returns the player with name from Cmd_Argv(1)
==================
*/
client_t* idServerCcmdsSystemLocal::GetPlayerByName( void )
{
    S32 i;
    UTF8* s, cleanName[64];
    client_t* cl;
    
    // make sure server is running
    if( !com_sv_running->integer )
    {
        return NULL;
    }
    
    if( Cmd_Argc() < 2 )
    {
        Com_Printf( "idServerCcmdsSystemLocal::GetPlayerByName - No player specified.\n" );
        return NULL;
    }
    
    s = Cmd_Argv( 1 );
    
    // check for a name match
    for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
    {
        if( cl->state <= CS_ZOMBIE )
        {
            continue;
        }
        
        if( !Q_stricmp( cl->name, s ) )
        {
            return cl;
        }
        
        Q_strncpyz( cleanName, cl->name, sizeof( cleanName ) );
        Q_CleanStr( cleanName );
        
        if( !Q_stricmp( cleanName, s ) )
        {
            return cl;
        }
    }
    
    Com_Printf( "idServerCcmdsSystemLocal::GetPlayerByName - Player %s is not on the server\n", s );
    
    return NULL;
}

/*
==================
idServerCcmdsSystemLocal::Map_f

Restart the server on a different map
==================
*/
void idServerCcmdsSystemLocal::Map_f( void )
{
    S32 savegameTime = -1;
    UTF8* cmd, *map, smapname[MAX_QPATH], mapname[MAX_QPATH], expanded[MAX_QPATH], *cl_profileStr = cvarSystem->VariableString( "cl_profile" );
    bool killBots, cheat, buildScript;
    
    map = Cmd_Argv( 1 );
    if( !map )
    {
        return;
    }
    
    if( !com_gameInfo.spEnabled )
    {
        if( !Q_stricmp( Cmd_Argv( 0 ), "spdevmap" ) || !Q_stricmp( Cmd_Argv( 0 ), "spmap" ) )
        {
            Com_Printf( "Single Player is not enabled.\n" );
            return;
        }
    }
    
    buildScript = ( bool )cvarSystem->VariableIntegerValue( "com_buildScript" );
    
    if( serverGameSystem->GameIsSinglePlayer() )
    {
        if( !buildScript && sv_reloading->integer && sv_reloading->integer != RELOAD_NEXTMAP ) // game is in 'reload' mode, don't allow starting new maps yet.
        {
            return;
        }
        
        // Trap a savegame load
        if( strstr( map, ".sav" ) )
        {
            // open the savegame, read the mapname, and copy it to the map string
            UTF8 savemap[MAX_QPATH], savedir[MAX_QPATH];
            U8* buffer;
            S32 size, csize;
            
            if( com_gameInfo.usesProfiles && cl_profileStr[0] )
            {
                Com_sprintf( savedir, sizeof( savedir ), "profiles/%s/save/", cl_profileStr );
            }
            else
            {
                Q_strncpyz( savedir, "save/", sizeof( savedir ) );
            }
            
            if( !( strstr( map, savedir ) == map ) )
            {
                Com_sprintf( savemap, sizeof( savemap ), "%s%s", savedir, map );
            }
            else
            {
                strcpy( savemap, map );
            }
            
            size = fileSystem->ReadFile( savemap, NULL );
            if( size < 0 )
            {
                Com_Printf( "Can't find savegame %s\n", savemap );
                return;
            }
            
            //buffer = Hunk_AllocateTempMemory(size);
            fileSystem->ReadFile( savemap, ( void** )&buffer );
            
            if( Q_stricmp( savemap, va( "%scurrent.sav", savedir ) ) != 0 )
            {
                // copy it to the current savegame file
                fileSystem->WriteFile( va( "%scurrent.sav", savedir ), buffer, size );
                // make sure it is the correct size
                csize = fileSystem->ReadFile( va( "%scurrent.sav", savedir ), NULL );
                if( csize != size )
                {
                    Hunk_FreeTempMemory( buffer );
                    fileSystem->Delete( va( "%scurrent.sav", savedir ) );
// TTimo
#ifdef __linux__
                    Com_Error( ERR_DROP, "Unable to save game.\n\nPlease check that you have at least 5mb free of disk space in your home directory." );
#else
                    Com_Error( ERR_DROP, "Insufficient free disk space.\n\nPlease free at least 5mb of free space on game drive." );
#endif
                    return;
                }
            }
            
            // set the cvar, so the game knows it needs to load the savegame once the clients have connected
            cvarSystem->Set( "savegame_loading", "1" );
            
            // set the filename
            cvarSystem->Set( "savegame_filename", savemap );
            
            // the mapname is at the very start of the savegame file
            Q_strncpyz( savemap, ( UTF8* )( buffer + sizeof( S32 ) ), sizeof( savemap ) );	// skip the version
            Q_strncpyz( smapname, savemap, sizeof( smapname ) );
            map = smapname;
            
            savegameTime = *( S32* )( buffer + sizeof( S32 ) + MAX_QPATH );
            
            if( savegameTime >= 0 )
            {
                svs.time = savegameTime;
            }
            
            Hunk_FreeTempMemory( buffer );
        }
        else
        {
            cvarSystem->Set( "savegame_loading", "0" );	// make sure it's turned off
            
            // set the filename
            cvarSystem->Set( "savegame_filename", "" );
        }
    }
    else
    {
        cvarSystem->Set( "savegame_loading", "0" );	// make sure it's turned off
        
        // set the filename
        cvarSystem->Set( "savegame_filename", "" );
    }
    
    // make sure the level exists before trying to change, so that
    // a typo at the server console won't end the game
    Com_sprintf( expanded, sizeof( expanded ), "maps/%s.world", map );
    
    if( fileSystem->ReadFile( expanded, NULL ) == -1 )
    {
        Com_Printf( "Can't find map %s\n", expanded );
        return;
    }
    
    cvarSystem->Set( "gamestate", va( "%i", GS_INITIALIZE ) ); // NERVE - SMF - reset gamestate on map/devmap
    
    cvarSystem->Set( "g_currentRound", "0" ); // NERVE - SMF - reset the current round
    cvarSystem->Set( "g_nextTimeLimit", "0" ); // NERVE - SMF - reset the next time limit
    
    // START Mad Doctor I changes, 8/14/2002.  Need a way to force load a single player map as single player
    if( !Q_stricmp( Cmd_Argv( 0 ), "spdevmap" ) || !Q_stricmp( Cmd_Argv( 0 ), "spmap" ) )
    {
        // This is explicitly asking for a single player load of this map
        cvarSystem->Set( "g_gametype", va( "%i", com_gameInfo.defaultSPGameType ) );
        
        // force latched values to get set
        cvarSystem->Get( "g_gametype", va( "%i", com_gameInfo.defaultSPGameType ), CVAR_SERVERINFO | CVAR_USERINFO | CVAR_LATCH );
        
        // enable bot support for AI
        cvarSystem->Set( "bot_enable", "1" );
    }
    
    // Rafael gameskill
//  cvarSystem->Get ("g_gameskill", "3", CVAR_SERVERINFO | CVAR_LATCH);
    // done
    
    cmd = Cmd_Argv( 0 );
    
    if( !Q_stricmp( cmd, "devmap" ) )
    {
        cheat = true;
        killBots = true;
    }
    else if( !Q_stricmp( Cmd_Argv( 0 ), "spdevmap" ) )
    {
        cheat = true;
        killBots = true;
    }
    else
    {
        cheat = false;
        killBots = false;
    }
    
    // save the map name here cause on a map restart we reload the q3config.cfg
    // and thus nuke the arguments of the map command
    Q_strncpyz( mapname, map, sizeof( mapname ) );
    
    // start up the map
    serverInitSystem->SpawnServer( mapname, killBots );
    
    // set the cheat value
    // if the level was started with "map <levelname>", then
    // cheats will not be allowed.  If started with "devmap <levelname>"
    // then cheats will be allowed
    if( cheat )
    {
        cvarSystem->Set( "sv_cheats", "1" );
    }
    else
    {
        cvarSystem->Set( "sv_cheats", "1" );
    }
}

/*
================
idServerCcmdsSystemLocal::CheckTransitionGameState

NERVE - SMF
================
*/
bool idServerCcmdsSystemLocal::CheckTransitionGameState( gamestate_t new_gs, gamestate_t old_gs )
{
    if( old_gs == new_gs && new_gs != GS_PLAYING )
    {
        return false;
    }
    
//  if ( old_gs == GS_WARMUP && new_gs != GS_WARMUP_COUNTDOWN )
//      return false;

//  if ( old_gs == GS_WARMUP_COUNTDOWN && new_gs != GS_PLAYING )
//      return false;

    if( old_gs == GS_WAITING_FOR_PLAYERS && new_gs != GS_WARMUP )
    {
        return false;
    }
    
    if( old_gs == GS_INTERMISSION && new_gs != GS_WARMUP )
    {
        return false;
    }
    
    if( old_gs == GS_RESET && ( new_gs != GS_WAITING_FOR_PLAYERS && new_gs != GS_WARMUP ) )
    {
        return false;
    }
    
    return true;
}

/*
================
idServerCcmdsSystemLocal::TransitionGameState

NERVE - SMF
================
*/
bool idServerCcmdsSystemLocal::TransitionGameState( gamestate_t new_gs, gamestate_t old_gs, S32 delay )
{
    if( !serverGameSystem->GameIsSinglePlayer() && !serverGameSystem->GameIsCoop() )
    {
        // we always do a warmup before starting match
        if( old_gs == GS_INTERMISSION && new_gs == GS_PLAYING )
        {
            new_gs = GS_WARMUP;
        }
    }
    
    // check if its a valid state transition
    if( !CheckTransitionGameState( new_gs, old_gs ) )
    {
        return false;
    }
    
    if( new_gs == GS_RESET )
    {
        new_gs = GS_WARMUP;
    }
    
    cvarSystem->Set( "gamestate", va( "%i", new_gs ) );
    
    return true;
}

void MSG_PrioritiseEntitystateFields( void );
void MSG_PrioritisePlayerStateFields( void );

/*
================
idServerCcmdsSystemLocal::FieldInfo_f
================
*/
void idServerCcmdsSystemLocal::FieldInfo_f( void )
{
    MSG_PrioritiseEntitystateFields();
    MSG_PrioritisePlayerStateFields();
}

/*
================
idServerCcmdsSystemLocal::MapRestart_f

Completely restarts a level, but doesn't send a new gamestate to the clients.
This allows fair starts with variable load times.
================
*/
void idServerCcmdsSystemLocal::MapRestart_f( void )
{
    S32 i, delay = 0;
    client_t* client;
    UTF8* denied;
    bool isBot;
    gamestate_t new_gs, old_gs; // NERVE - SMF
    
    // make sure we aren't restarting twice in the same frame
    if( com_frameTime == sv.serverId )
    {
        return;
    }
    
    // make sure server is running
    if( !com_sv_running->integer )
    {
        Com_Printf( "Server is not running.\n" );
        return;
    }
    
    if( Cmd_Argc() > 1 )
    {
        delay = atoi( Cmd_Argv( 1 ) );
    }
    
    if( delay )
    {
        sv.restartTime = svs.time + delay * 1000;
        serverInitSystem->SetConfigstring( CS_WARMUP, va( "%i", sv.restartTime ) );
        return;
    }
    
    // NERVE - SMF - read in gamestate or just default to GS_PLAYING
    old_gs = ( gamestate_t )atoi( cvarSystem->VariableString( "gamestate" ) );
    
    if( serverGameSystem->GameIsSinglePlayer() || serverGameSystem->GameIsCoop() )
    {
        new_gs = GS_PLAYING;
    }
    else
    {
        if( Cmd_Argc() > 2 )
        {
            new_gs = ( gamestate_t )atoi( Cmd_Argv( 2 ) );
        }
        else
        {
            new_gs = GS_PLAYING;
        }
    }
    
    if( !TransitionGameState( new_gs, old_gs, delay ) )
    {
        return;
    }
    
    // check for changes in variables that can't just be restarted
    // check for maxclients change
    if( sv_maxclients->modified )
    {
        UTF8 mapname[MAX_QPATH];
        
        Com_Printf( "sv_maxclients variable change -- restarting.\n" );
        // restart the map the slow way
        Q_strncpyz( mapname, cvarSystem->VariableString( "mapname" ), sizeof( mapname ) );
        
        serverInitSystem->SpawnServer( mapname, false );
        return;
    }
    
    // Check for loading a saved game
    if( cvarSystem->VariableIntegerValue( "savegame_loading" ) )
    {
        // open the current savegame, and find out what the time is, everything else we can ignore
        UTF8 savemap[MAX_QPATH], *cl_profileStr = cvarSystem->VariableString( "cl_profile" );
        U8* buffer;
        S32 size, savegameTime;
        
        if( com_gameInfo.usesProfiles )
        {
            Com_sprintf( savemap, sizeof( savemap ), "profiles/%s/save/current.sav", cl_profileStr );
        }
        else
        {
            Q_strncpyz( savemap, "save/current.sav", sizeof( savemap ) );
        }
        
        size = fileSystem->ReadFile( savemap, NULL );
        if( size < 0 )
        {
            Com_Printf( "Can't find savegame %s\n", savemap );
            return;
        }
        
        //buffer = Hunk_AllocateTempMemory(size);
        fileSystem->ReadFile( savemap, ( void** )&buffer );
        
        // the mapname is at the very start of the savegame file
        savegameTime = *( S32* )( buffer + sizeof( S32 ) + MAX_QPATH );
        
        if( savegameTime >= 0 )
        {
            svs.time = savegameTime;
        }
        
        Hunk_FreeTempMemory( buffer );
    }
    // done.
    
    // toggle the server bit so clients can detect that a
    // map_restart has happened
    svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;
    
    // generate a new serverid
    // TTimo - don't update restartedserverId there, otherwise we won't deal correctly with multiple map_restart
    sv.serverId = com_frameTime;
    cvarSystem->Set( "sv_serverid", va( "%i", sv.serverId ) );
    
    // reset all the vm data in place without changing memory allocation
    // note that we do NOT set sv.state = SS_LOADING, so configstrings that
    // had been changed from their default values will generate broadcast updates
    sv.state = SS_LOADING;
    sv.restarting = true;
    
    cvarSystem->Set( "sv_serverRestarting", "1" );
    
    serverGameSystem->RestartGameProgs();
    
    // run a few frames to allow everything to settle
    for( i = 0; i < GAME_INIT_FRAMES; i++ )
    {
        game->RunFrame( svs.time );
        svs.time += FRAMETIME;
    }
    
    // create a baseline for more efficient communications
    // Gordon: meh, this wont work here as the client doesn't know it has happened
    // CreateBaseline ();
    
    sv.state = SS_GAME;
    sv.restarting = false;
    
    // connect and begin all the clients
    for( i = 0; i < sv_maxclients->integer; i++ )
    {
        client = &svs.clients[i];
        
        // send the new gamestate to all connected clients
        if( client->state < CS_CONNECTED )
        {
            continue;
        }
        
        if( client->netchan.remoteAddress.type == NA_BOT )
        {
            if( serverGameSystem->GameIsSinglePlayer() || serverGameSystem->GameIsCoop() )
            {
                continue;		// dont carry across bots in single player
            }
            isBot = true;
        }
        else
        {
            isBot = false;
        }
        
        // add the map_restart command
        serverMainSystem->AddServerCommand( client, "map_restart\n" );
        
        // connect the client again, without the firstTime flag
        denied = static_cast< UTF8* >( game->ClientConnect( i, false, isBot ) );
        if( denied )
        {
            // this generally shouldn't happen, because the client
            // was connected before the level change
            serverClientSystem->DropClient( client, denied );
            
            if( ( !serverGameSystem->GameIsSinglePlayer() ) || ( !isBot ) )
            {
                Com_Printf( "idServerBotSystemLocal::MapRestart_f(%d): dropped client %i - denied!\n", delay, i );	// bk010125
            }
            
            continue;
        }
        
        client->state = CS_ACTIVE;
        
        serverClientSystem->ClientEnterWorld( client, &client->lastUsercmd );
    }
    
    // run another frame to allow things to look at all the players
    game->RunFrame( svs.time );
    //SV_BotFrame( svs.time );
    svs.time += FRAMETIME;
    
    cvarSystem->Set( "sv_serverRestarting", "0" );
}

/*
=================
idServerCcmdsSystemLocal::LoadGame_f
=================
*/
void idServerCcmdsSystemLocal::LoadGame_f( void )
{
    S32 size;
    UTF8 filename[MAX_QPATH], mapname[MAX_QPATH], savedir[MAX_QPATH], *cl_profileStr = cvarSystem->VariableString( "cl_profile" );
    U8* buffer;
    
    // dont allow command if another loadgame is pending
    if( cvarSystem->VariableIntegerValue( "savegame_loading" ) )
    {
        return;
    }
    if( sv_reloading->integer )
    {
        // (SA) disabling
        // if(sv_reloading->integer && sv_reloading->integer != RELOAD_FAILED )    // game is in 'reload' mode, don't allow starting new maps yet.
        return;
    }
    
    Q_strncpyz( filename, Cmd_Argv( 1 ), sizeof( filename ) );
    if( !filename[0] )
    {
        Com_Printf( "You must specify a savegame to load\n" );
        return;
    }
    
    if( com_gameInfo.usesProfiles && cl_profileStr[0] )
    {
        Com_sprintf( savedir, sizeof( savedir ), "profiles/%s/save/", cl_profileStr );
    }
    else
    {
        Q_strncpyz( savedir, "save/", sizeof( savedir ) );
    }
    
    //if ( Q_strncmp( filename, "save/", 5 ) && Q_strncmp( filename, "save\\", 5 ) )
    //{
    //	Q_strncpyz( filename, va("save/%s", filename), sizeof( filename ) );
    //}
    
    // go through a va to avoid vsnprintf call with same source and target
    Q_strncpyz( filename, va( "%s%s", savedir, filename ), sizeof( filename ) );
    
    // enforce .sav extension
    if( !strstr( filename, "." ) || Q_strncmp( strstr( filename, "." ) + 1, "sav", 3 ) )
    {
        Q_strcat( filename, sizeof( filename ), ".sav" );
    }
    
    // use '/' instead of '\\' for directories
    while( strstr( filename, "\\" ) )
    {
        *( UTF8* )strstr( filename, "\\" ) = '/';
    }
    
    size = fileSystem->ReadFile( filename, NULL );
    if( size < 0 )
    {
        Com_Printf( "Can't find savegame %s\n", filename );
        return;
    }
    
    //buffer = Hunk_AllocateTempMemory(size);
    fileSystem->ReadFile( filename, ( void** )&buffer );
    
    // read the mapname, if it is the same as the current map, then do a fast load
    Q_strncpyz( mapname, ( StringEntry )( buffer + sizeof( S32 ) ), sizeof( mapname ) );
    
    if( com_sv_running->integer && ( com_frameTime != sv.serverId ) )
    {
        // check mapname
        if( !Q_stricmp( mapname, sv_mapname->string ) ) // same
        {
            if( Q_stricmp( filename, va( "%scurrent.sav", savedir ) ) != 0 )
            {
                // copy it to the current savegame file
                fileSystem->WriteFile( va( "%scurrent.sav", savedir ), buffer, size );
            }
            
            Hunk_FreeTempMemory( buffer );
            
            cvarSystem->Set( "savegame_loading", "2" );	// 2 means it's a restart, so stop rendering until we are loaded
            
            // set the filename
            cvarSystem->Set( "savegame_filename", filename );
            
            // quick-restart the server
            MapRestart_f();	// savegame will be loaded after restart
            
            return;
        }
    }
    
    Hunk_FreeTempMemory( buffer );
    
    // otherwise, do a slow load
    if( cvarSystem->VariableIntegerValue( "sv_cheats" ) )
    {
        Cbuf_ExecuteText( EXEC_APPEND, va( "spdevmap %s", filename ) );
    }
    else     // no cheats
    {
        Cbuf_ExecuteText( EXEC_APPEND, va( "spmap %s", filename ) );
    }
}

/*
==================
idServerCcmdsSystemLocal::TempBanNetAddress
==================
*/
void idServerCcmdsSystemLocal::TempBanNetAddress( netadr_t address, S32 length )
{
    S32 i, oldesttime = 0, oldest = -1;
    
    for( i = 0; i < MAX_TEMPBAN_ADDRESSES; i++ )
    {
        if( !svs.tempBanAddresses[i].endtime || svs.tempBanAddresses[i].endtime < svs.time )
        {
            // found a free slot
            svs.tempBanAddresses[i].adr = address;
            svs.tempBanAddresses[i].endtime = svs.time + ( length * 1000 );
            return;
        }
        else
        {
            if( oldest == -1 || oldesttime > svs.tempBanAddresses[i].endtime )
            {
                oldesttime = svs.tempBanAddresses[i].endtime;
                oldest = i;
            }
        }
    }
    
    svs.tempBanAddresses[oldest].adr = address;
    svs.tempBanAddresses[oldest].endtime = svs.time + length;
}

/*
==================
idServerCcmdsSystemLocal::TempBanIsBanned
==================
*/
bool idServerCcmdsSystemLocal::TempBanIsBanned( netadr_t address )
{
    S32 i;
    
    for( i = 0; i < MAX_TEMPBAN_ADDRESSES; i++ )
    {
        if( svs.tempBanAddresses[i].endtime && svs.tempBanAddresses[i].endtime > svs.time )
        {
            if( NET_CompareAdr( address, svs.tempBanAddresses[i].adr ) )
            {
                return true;
            }
        }
    }
    
    return false;
}

/*
================
idServerCcmdsSystemLocal::Status_f
================
*/
void idServerCcmdsSystemLocal::Status_f( void )
{
    S32 i, j, l, ping;
    client_t* cl;
    playerState_t* ps;
    StringEntry s;
    U8 cpu, avg; //Dushan
    
    // make sure server is running
    if( !com_sv_running->integer )
    {
        Com_Printf( "Server is not running.\n" );
        return;
    }
    
    // Dushan
    cpu = ( svs.stats.latched_active + svs.stats.latched_idle );
    if( cpu )
    {
        cpu = 100 * svs.stats.latched_active / cpu;
    }
    avg = 1000 * svs.stats.latched_active / STATFRAMES;
    
    Com_Printf( "cpu utilization  : %3i%%\n", ( S32 )cpu );
    Com_Printf( "avg response time: %i ms\n", ( S32 )avg );
    
    Com_Printf( "map: %s\n", sv_mapname->string );
    
    Com_Printf( "num score ping name            lastmsg address               qport rate\n" );
    Com_Printf( "--- ----- ---- --------------- ------- --------------------- ----- -----\n" );
    
    for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
    {
        if( !cl->state )
        {
            continue;
        }
        Com_Printf( "%3i ", i );
        
        ps = serverGameSystem->GameClientNum( i );
        
        Com_Printf( "%5i ", ps->persistant[PERS_SCORE] );
        
        if( cl->state == CS_CONNECTED )
        {
            Com_Printf( "CNCT " );
        }
        else if( cl->state == CS_ZOMBIE )
        {
            Com_Printf( "ZMBI " );
        }
        else
        {
            ping = cl->ping < 9999 ? cl->ping : 9999;
            Com_Printf( "%4i ", ping );
        }
        
        Com_Printf( "%s", cl->name );
        l = 16 - strlen( cl->name );
        for( j = 0; j < l; j++ )
        {
            Com_Printf( " " );
        }
        
        Com_Printf( "%7i ", svs.time - cl->lastPacketTime );
        
        s = NET_AdrToString( cl->netchan.remoteAddress );
        Com_Printf( "%s", s );
        l = 22 - strlen( s );
        for( j = 0; j < l; j++ )
        {
            Com_Printf( " " );
        }
        
        Com_Printf( "%5i", cl->netchan.qport );
        
        Com_Printf( " %5i", cl->rate );
        
        Com_Printf( "\n" );
    }
    Com_Printf( "\n" );
}

/*
==================
SV_ConSay_f
==================
*/
void idServerCcmdsSystemLocal::ConSay_f( void )
{
    UTF8* p;
    UTF8 text[1024];
    
    // make sure server is running
    if( !com_sv_running->integer )
    {
        Com_Printf( "Server is not running.\n" );
        return;
    }
    
    if( Cmd_Argc() < 2 )
    {
        return;
    }
    
    strcpy( text, "console: " );
    p = Cmd_Args();
    
    if( *p == '"' )
    {
        p++;
        p[strlen( p ) - 1] = 0;
    }
    
    strcat( text, p );
    
    serverMainSystem->SendServerCommand( NULL, "chat \"%s\"", text );
}


/*
==================
idServerCcmdsSystemLocal::Heartbeat_f

Also called by idServerClientSystemLocal::DropClient, idServerClientSystemLocal::DirectConnect, and idServerInitSystemLocal::SpawnServer
==================
*/
void idServerCcmdsSystemLocal::Heartbeat_f( void )
{
    svs.nextHeartbeatTime = -9999999;
}

/*
===========
idServerCcmdsSystemLocal::Serverinfo_f

Examine the serverinfo string
===========
*/
void idServerCcmdsSystemLocal::Serverinfo_f( void )
{
    Com_Printf( "Server info settings:\n" );
    
    Info_Print( cvarSystem->InfoString( CVAR_SERVERINFO | CVAR_SERVERINFO_NOUPDATE ) );
}

/*
===========
SV_Systeminfo_f

Examine or change the serverinfo string
===========
*/
void idServerCcmdsSystemLocal::Systeminfo_f( void )
{
    // make sure server is running
    if( !com_sv_running->integer )
    {
        Com_Printf( "Server is not running.\n" );
        return;
    }
    
    Com_Printf( "System info settings:\n" );
    
    Info_Print( cvarSystem->InfoString( CVAR_SERVERINFO | CVAR_SERVERINFO_NOUPDATE ) );
}

/*
===========
idServerCcmdsSystemLocal::DumpUser_f

Examine all a users info strings FIXME: move to game
===========
*/
void idServerCcmdsSystemLocal::DumpUser_f( void )
{
    client_t* cl;
    
    // make sure server is running
    if( !com_sv_running->integer )
    {
        Com_Printf( "Server is not running.\n" );
        return;
    }
    
    if( Cmd_Argc() != 2 )
    {
        Com_Printf( "Usage: info <userid>\n" );
        return;
    }
    
    cl = GetPlayerByName();
    if( !cl )
    {
        return;
    }
    
    Com_Printf( "userinfo\n" );
    Com_Printf( "--------\n" );
    Info_Print( cl->userinfo );
}


/*
=================
idServerCcmdsSystemLocal::KillServer
=================
*/
void idServerCcmdsSystemLocal::KillServer_f( void )
{
    serverInitSystem->Shutdown( "killserver" );
}

/*
=================
idServerCcmdsSystemLocal::GameCompleteStatus_f

NERVE - SMF
=================
*/
void idServerCcmdsSystemLocal::GameCompleteStatus_f( void )
{
    serverMainSystem->MasterGameCompleteStatus();
}

//===========================================================

/*
==================
idServerCcmdsSystemLocal::CompleteMapName
==================
*/
void idServerCcmdsSystemLocal::CompleteMapName( UTF8* args, S32 argNum )
{
    if( argNum == 2 )
    {
        Field_CompleteFilename( "maps", "world", true );
    }
}

/*
==================
idServerCcmdsSystemLocal::AddOperatorCommands
==================
*/
void idServerCcmdsSystemLocal::AddOperatorCommands( void )
{
    static bool initialized;
    
    if( initialized )
    {
        return;
    }
    
    initialized = true;
    
    Cmd_AddCommand( "heartbeat", &idServerCcmdsSystemLocal::Heartbeat_f );
    Cmd_AddCommand( "status", &idServerCcmdsSystemLocal::Status_f );
    Cmd_AddCommand( "serverinfo", &idServerCcmdsSystemLocal::Serverinfo_f );
    Cmd_AddCommand( "systeminfo", &idServerCcmdsSystemLocal::Systeminfo_f );
    Cmd_AddCommand( "dumpuser", &idServerCcmdsSystemLocal::DumpUser_f );
    Cmd_AddCommand( "map_restart", &idServerCcmdsSystemLocal::MapRestart_f );
    Cmd_AddCommand( "fieldinfo", &idServerCcmdsSystemLocal::FieldInfo_f );
    Cmd_AddCommand( "sectorlist", &idServerWorldSystemLocal::SectorList_f );
    Cmd_AddCommand( "map", &idServerCcmdsSystemLocal::Map_f );
    Cmd_SetCommandCompletionFunc( "map", &idServerCcmdsSystemLocal::CompleteMapName );
    Cmd_AddCommand( "gameCompleteStatus", &idServerCcmdsSystemLocal::GameCompleteStatus_f ); // NERVE - SMF
#ifndef PRE_RELEASE_DEMO_NODEVMAP
    Cmd_AddCommand( "devmap", &idServerCcmdsSystemLocal::Map_f );
    Cmd_SetCommandCompletionFunc( "devmap", &idServerCcmdsSystemLocal::CompleteMapName );
    Cmd_AddCommand( "spmap", &idServerCcmdsSystemLocal::Map_f );
    Cmd_SetCommandCompletionFunc( "devmap", &idServerCcmdsSystemLocal::CompleteMapName );
    Cmd_AddCommand( "spdevmap", &idServerCcmdsSystemLocal::Map_f );
    Cmd_SetCommandCompletionFunc( "devmap", &idServerCcmdsSystemLocal::CompleteMapName );
#endif
    Cmd_AddCommand( "loadgame", &idServerCcmdsSystemLocal::LoadGame_f );
    Cmd_AddCommand( "killserver", &idServerCcmdsSystemLocal::KillServer_f );
    
    if( com_dedicated->integer )
    {
        Cmd_AddCommand( "say", &idServerCcmdsSystemLocal::ConSay_f );
    }
}
