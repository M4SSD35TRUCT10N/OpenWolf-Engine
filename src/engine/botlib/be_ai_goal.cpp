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
// File name:   be_ai_goal.cpp
// Version:     v1.00
// Created:
// Compilers:   Visual Studio 2015
// Description: goal AI
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#include <OWLib/precompiled.h>

//#define DEBUG_AI_GOAL
#ifdef RANDOMIZE
#define UNDECIDEDFUZZY
#endif //RANDOMIZE
#define DROPPEDWEIGHT
//minimum avoid goal time
#define AVOID_MINIMUM_TIME		10
//default avoid goal time
#define AVOID_DEFAULT_TIME		30
//avoid dropped goal time
#define AVOID_DROPPED_TIME		10
//
#define TRAVELTIME_SCALE		0.01f
//item flags
#define IFL_NOTFREE				1		//not in free for all
#define IFL_NOTTEAM				2		//not in team play
#define IFL_NOTSINGLE			4		//not in single player
#define IFL_NOTBOT				8		//bot should never go for this
#define IFL_ROAM				16		//bot roam goal

//location in the map "target_location"
typedef struct maplocation_s
{
    vec3_t origin;
    S32 areanum;
    UTF8 name[MAX_EPAIRKEY];
    struct maplocation_s* next;
} maplocation_t;

//camp spots "info_camp"
typedef struct campspot_s
{
    vec3_t origin;
    S32 areanum;
    UTF8 name[MAX_EPAIRKEY];
    F32 range;
    F32 weight;
    F32 wait;
    F32 random;
    struct campspot_s* next;
} campspot_t;

//FIXME: these are game specific
typedef enum
{
    GT_FFA,				// free for all
    GT_TOURNAMENT,		// one on one tournament
    GT_SINGLE_PLAYER,	// single player tournament
    
    //-- team games go after this --
    
    GT_TEAM,			// team deathmatch
    GT_CTF,				// capture the flag
#ifdef MISSIONPACK
    GT_1FCTF,
    GT_OBELISK,
    GT_HARVESTER,
#endif
    GT_MAX_GAME_TYPE
} gametype_t;

typedef struct levelitem_s
{
    S32 number;							//number of the level item
    S32 iteminfo;						//index into the item info
    S32 flags;							//item flags
    F32 weight;						//fixed roam weight
    vec3_t origin;						//origin of the item
    S32 goalareanum;					//area the item is in
    vec3_t goalorigin;					//goal origin within the area
    S32 entitynum;						//entity number
    F32 timeout;						//item is removed after this time
    struct levelitem_s* prev, *next;
} levelitem_t;

typedef struct iteminfo_s
{
    UTF8 classname[32];					//classname of the item
    UTF8 name[MAX_STRINGFIELD];			//name of the item
    UTF8 model[MAX_STRINGFIELD];		//model of the item
    S32 modelindex;						//model index
    S32 type;							//item type
    S32 index;							//index in the inventory
    F32 respawntime;					//respawn time
    vec3_t mins;						//mins of the item
    vec3_t maxs;						//maxs of the item
    S32 number;							//number of the item info
} iteminfo_t;

#define ITEMINFO_OFS(x)	(S32)&(((iteminfo_t *)0)->x)

fielddef_t iteminfo_fields[] =
{
    {"name", ITEMINFO_OFS( name ), FT_STRING},
    {"model", ITEMINFO_OFS( model ), FT_STRING},
    {"modelindex", ITEMINFO_OFS( modelindex ), FT_INT},
    {"type", ITEMINFO_OFS( type ), FT_INT},
    {"index", ITEMINFO_OFS( index ), FT_INT},
    {"respawntime", ITEMINFO_OFS( respawntime ), FT_FLOAT},
    {"mins", ITEMINFO_OFS( mins ), FT_FLOAT | FT_ARRAY, 3},
    {"maxs", ITEMINFO_OFS( maxs ), FT_FLOAT | FT_ARRAY, 3},
    {0, 0, 0}
};

structdef_t iteminfo_struct =
{
    sizeof( iteminfo_t ), iteminfo_fields
};

typedef struct itemconfig_s
{
    S32 numiteminfo;
    iteminfo_t* iteminfo;
} itemconfig_t;

//goal state
typedef struct bot_goalstate_s
{
    struct weightconfig_s* itemweightconfig;	//weight config
    S32* itemweightindex;						//index from item to weight
    //
    S32 client;									//client using this goal state
    S32 lastreachabilityarea;					//last area with reachabilities the bot was in
    //
    bot_goal_t goalstack[MAX_GOALSTACK];		//goal stack
    S32 goalstacktop;							//the top of the goal stack
    //
    S32 avoidgoals[MAX_AVOIDGOALS];				//goals to avoid
    F32 avoidgoaltimes[MAX_AVOIDGOALS];		//times to avoid the goals
} bot_goalstate_t;

bot_goalstate_t* botgoalstates[MAX_CLIENTS + 1]; // bk001206 - FIXME: init?
//item configuration
itemconfig_t* itemconfig = NULL; // bk001206 - init
//level items
levelitem_t* levelitemheap = NULL; // bk001206 - init
levelitem_t* freelevelitems = NULL; // bk001206 - init
levelitem_t* levelitems = NULL; // bk001206 - init
S32 numlevelitems = 0;
//map locations
maplocation_t* maplocations = NULL; // bk001206 - init
//camp spots
campspot_t* campspots = NULL; // bk001206 - init
//the game type
S32 g_gametype = 0; // bk001206 - init
//additional dropped item weight
libvar_t* droppedweight = NULL; // bk001206 - init

//========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//========================================================================
bot_goalstate_t* BotGoalStateFromHandle( S32 handle )
{
    if( handle <= 0 || handle > MAX_CLIENTS )
    {
        botimport.Print( PRT_FATAL, "goal state handle %d out of range\n", handle );
        return NULL;
    } //end if
    if( !botgoalstates[handle] )
    {
        botimport.Print( PRT_FATAL, "invalid goal state %d\n", handle );
        return NULL;
    } //end if
    return botgoalstates[handle];
} //end of the function BotGoalStateFromHandle
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotInterbreedGoalFuzzyLogic( S32 parent1, S32 parent2, S32 child )
{
    bot_goalstate_t* p1, *p2, *c;
    
    p1 = BotGoalStateFromHandle( parent1 );
    p2 = BotGoalStateFromHandle( parent2 );
    c = BotGoalStateFromHandle( child );
    
    InterbreedWeightConfigs( p1->itemweightconfig, p2->itemweightconfig,
                             c->itemweightconfig );
} //end of the function BotInterbreedingGoalFuzzyLogic
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotSaveGoalFuzzyLogic( S32 goalstate, UTF8* filename )
{
    bot_goalstate_t* gs;
    
    gs = BotGoalStateFromHandle( goalstate );
    
    //WriteWeightConfig(filename, gs->itemweightconfig);
} //end of the function BotSaveGoalFuzzyLogic
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotMutateGoalFuzzyLogic( S32 goalstate, F32 range )
{
    bot_goalstate_t* gs;
    
    gs = BotGoalStateFromHandle( goalstate );
    
    EvolveWeightConfig( gs->itemweightconfig );
} //end of the function BotMutateGoalFuzzyLogic
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
itemconfig_t* LoadItemConfig( UTF8* filename )
{
    S32 max_iteminfo;
    token_t token;
    UTF8 path[MAX_PATH];
    source_t* source;
    itemconfig_t* ic;
    iteminfo_t* ii;
    
    max_iteminfo = ( S32 ) LibVarValue( "max_iteminfo", "256" );
    if( max_iteminfo < 0 )
    {
        botimport.Print( PRT_ERROR, "max_iteminfo = %d\n", max_iteminfo );
        max_iteminfo = 256;
        LibVarSet( "max_iteminfo", "256" );
    }
    
    strncpy( path, filename, MAX_PATH );
    PC_SetBaseFolder( BOTFILESBASEFOLDER );
    source = LoadSourceFile( path );
    if( !source )
    {
        botimport.Print( PRT_ERROR, "counldn't load %s\n", path );
        return NULL;
    } //end if
    //initialize item config
    ic = ( itemconfig_t* ) GetClearedHunkMemory( sizeof( itemconfig_t ) +
            max_iteminfo * sizeof( iteminfo_t ) );
    ic->iteminfo = ( iteminfo_t* )( ( UTF8* ) ic + sizeof( itemconfig_t ) );
    ic->numiteminfo = 0;
    //parse the item config file
    while( PC_ReadToken( source, &token ) )
    {
        if( !strcmp( token.string, "iteminfo" ) )
        {
            if( ic->numiteminfo >= max_iteminfo )
            {
                SourceError( source, "more than %d item info defined\n", max_iteminfo );
                FreeMemory( ic );
                FreeSource( source );
                return NULL;
            } //end if
            ii = &ic->iteminfo[ic->numiteminfo];
            ::memset( ii, 0, sizeof( iteminfo_t ) );
            if( !PC_ExpectTokenType( source, TT_STRING, 0, &token ) )
            {
                FreeMemory( ic );
                FreeMemory( source );
                return NULL;
            } //end if
            StripDoubleQuotes( token.string );
            strncpy( ii->classname, token.string, sizeof( ii->classname ) - 1 );
            if( !ReadStructure( source, &iteminfo_struct, ( UTF8* ) ii ) )
            {
                FreeMemory( ic );
                FreeSource( source );
                return NULL;
            } //end if
            ii->number = ic->numiteminfo;
            ic->numiteminfo++;
        } //end if
        else
        {
            SourceError( source, "unknown definition %s\n", token.string );
            FreeMemory( ic );
            FreeSource( source );
            return NULL;
        } //end else
    } //end while
    FreeSource( source );
    //
    if( !ic->numiteminfo ) botimport.Print( PRT_WARNING, "no item info loaded\n" );
    botimport.Print( PRT_MESSAGE, "loaded %s\n", path );
    return ic;
} //end of the function LoadItemConfig
//===========================================================================
// index to find the weight function of an iteminfo
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
S32* ItemWeightIndex( weightconfig_t* iwc, itemconfig_t* ic )
{
    S32* index, i;
    
    //initialize item weight index
    index = ( S32* ) GetClearedMemory( sizeof( S32 ) * ic->numiteminfo );
    
    for( i = 0; i < ic->numiteminfo; i++ )
    {
        index[i] = FindFuzzyWeight( iwc, ic->iteminfo[i].classname );
        if( index[i] < 0 )
        {
            Log_Write( "item info %d \"%s\" has no fuzzy weight\r\n", i, ic->iteminfo[i].classname );
        } //end if
    } //end for
    return index;
} //end of the function ItemWeightIndex
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void InitLevelItemHeap( void )
{
    S32 i, max_levelitems;
    
    if( levelitemheap ) FreeMemory( levelitemheap );
    
    max_levelitems = ( S32 ) LibVarValue( "max_levelitems", "256" );
    levelitemheap = ( levelitem_t* ) GetClearedMemory( max_levelitems * sizeof( levelitem_t ) );
    
    for( i = 0; i < max_levelitems - 1; i++ )
    {
        levelitemheap[i].next = &levelitemheap[i + 1];
    } //end for
    levelitemheap[max_levelitems - 1].next = NULL;
    //
    freelevelitems = levelitemheap;
} //end of the function InitLevelItemHeap
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
levelitem_t* AllocLevelItem( void )
{
    levelitem_t* li;
    
    li = freelevelitems;
    if( !li )
    {
        botimport.Print( PRT_FATAL, "out of level items\n" );
        return NULL;
    } //end if
    //
    freelevelitems = freelevelitems->next;
    ::memset( li, 0, sizeof( levelitem_t ) );
    return li;
} //end of the function AllocLevelItem
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void FreeLevelItem( levelitem_t* li )
{
    li->next = freelevelitems;
    freelevelitems = li;
} //end of the function FreeLevelItem
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void AddLevelItemToList( levelitem_t* li )
{
    if( levelitems ) levelitems->prev = li;
    li->prev = NULL;
    li->next = levelitems;
    levelitems = li;
} //end of the function AddLevelItemToList
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void RemoveLevelItemFromList( levelitem_t* li )
{
    if( li->prev ) li->prev->next = li->next;
    else levelitems = li->next;
    if( li->next ) li->next->prev = li->prev;
} //end of the function RemoveLevelItemFromList
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void BotFreeInfoEntities( void )
{
    maplocation_t* ml, *nextml;
    campspot_t* cs, *nextcs;
    
    for( ml = maplocations; ml; ml = nextml )
    {
        nextml = ml->next;
        FreeMemory( ml );
    } //end for
    maplocations = NULL;
    for( cs = campspots; cs; cs = nextcs )
    {
        nextcs = cs->next;
        FreeMemory( cs );
    } //end for
    campspots = NULL;
} //end of the function BotFreeInfoEntities
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void BotInitInfoEntities( void )
{
    UTF8 classname[MAX_EPAIRKEY];
    maplocation_t* ml;
    campspot_t* cs;
    S32 ent, numlocations, numcampspots;
    
    BotFreeInfoEntities();
    //
    numlocations = 0;
    numcampspots = 0;
    for( ent = AAS_NextBSPEntity( 0 ); ent; ent = AAS_NextBSPEntity( ent ) )
    {
        if( !AAS_ValueForBSPEpairKey( ent, "classname", classname, MAX_EPAIRKEY ) ) continue;
        
        //map locations
        if( !strcmp( classname, "target_location" ) )
        {
            ml = ( maplocation_t* ) GetClearedMemory( sizeof( maplocation_t ) );
            AAS_VectorForBSPEpairKey( ent, "origin", ml->origin );
            AAS_ValueForBSPEpairKey( ent, "message", ml->name, sizeof( ml->name ) );
            ml->areanum = AAS_PointAreaNum( ml->origin );
            ml->next = maplocations;
            maplocations = ml;
            numlocations++;
        } //end if
        //camp spots
        else if( !strcmp( classname, "info_camp" ) )
        {
            cs = ( campspot_t* ) GetClearedMemory( sizeof( campspot_t ) );
            AAS_VectorForBSPEpairKey( ent, "origin", cs->origin );
            //cs->origin[2] += 16;
            AAS_ValueForBSPEpairKey( ent, "message", cs->name, sizeof( cs->name ) );
            AAS_FloatForBSPEpairKey( ent, "range", &cs->range );
            AAS_FloatForBSPEpairKey( ent, "weight", &cs->weight );
            AAS_FloatForBSPEpairKey( ent, "wait", &cs->wait );
            AAS_FloatForBSPEpairKey( ent, "random", &cs->random );
            cs->areanum = AAS_PointAreaNum( cs->origin );
            if( !cs->areanum )
            {
                botimport.Print( PRT_MESSAGE, "camp spot at %1.1f %1.1f %1.1f in solid\n", cs->origin[0], cs->origin[1], cs->origin[2] );
                FreeMemory( cs );
                continue;
            } //end if
            cs->next = campspots;
            campspots = cs;
            //AAS_DrawPermanentCross(cs->origin, 4, LINECOLOR_YELLOW);
            numcampspots++;
        } //end else if
    } //end for
    if( bot_developer )
    {
        botimport.Print( PRT_MESSAGE, "%d map locations\n", numlocations );
        botimport.Print( PRT_MESSAGE, "%d camp spots\n", numcampspots );
    } //end if
} //end of the function BotInitInfoEntities
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void BotInitLevelItems( void )
{
    S32 i, spawnflags, value;
    UTF8 classname[MAX_EPAIRKEY];
    vec3_t origin, end;
    S32 ent, goalareanum;
    itemconfig_t* ic;
    levelitem_t* li;
    bsp_trace_t trace;
    
    //initialize the map locations and camp spots
    BotInitInfoEntities();
    
    //initialize the level item heap
    InitLevelItemHeap();
    levelitems = NULL;
    numlevelitems = 0;
    //
    ic = itemconfig;
    if( !ic ) return;
    
    //if there's no AAS file loaded
    if( !AAS_Loaded() ) return;
    
    //update the modelindexes of the item info
    for( i = 0; i < ic->numiteminfo; i++ )
    {
        //ic->iteminfo[i].modelindex = AAS_IndexFromModel(ic->iteminfo[i].model);
        if( !ic->iteminfo[i].modelindex )
        {
            Log_Write( "item %s has modelindex 0", ic->iteminfo[i].classname );
        } //end if
    } //end for
    
    for( ent = AAS_NextBSPEntity( 0 ); ent; ent = AAS_NextBSPEntity( ent ) )
    {
        if( !AAS_ValueForBSPEpairKey( ent, "classname", classname, MAX_EPAIRKEY ) ) continue;
        //
        spawnflags = 0;
        AAS_IntForBSPEpairKey( ent, "spawnflags", &spawnflags );
        //
        for( i = 0; i < ic->numiteminfo; i++ )
        {
            if( !strcmp( classname, ic->iteminfo[i].classname ) ) break;
        } //end for
        if( i >= ic->numiteminfo )
        {
            Log_Write( "entity %s unknown item\r\n", classname );
            continue;
        } //end if
        //get the origin of the item
        if( !AAS_VectorForBSPEpairKey( ent, "origin", origin ) )
        {
            botimport.Print( PRT_ERROR, "item %s without origin\n", classname );
            continue;
        } //end else
        //
        goalareanum = 0;
        //if it is a floating item
        if( spawnflags & 1 )
        {
            //if the item is not floating in water
            if( !( AAS_PointContents( origin ) & CONTENTS_WATER ) )
            {
                VectorCopy( origin, end );
                end[2] -= 32;
                trace = AAS_Trace( origin, ic->iteminfo[i].mins, ic->iteminfo[i].maxs, end, -1, CONTENTS_SOLID | CONTENTS_PLAYERCLIP );
                //if the item not near the ground
                if( trace.fraction >= 1 )
                {
                    //if the item is not reachable from a jumppad
                    goalareanum = AAS_BestReachableFromJumpPadArea( origin, ic->iteminfo[i].mins, ic->iteminfo[i].maxs );
                    Log_Write( "item %s reachable from jumppad area %d\r\n", ic->iteminfo[i].classname, goalareanum );
                    //botimport.Print(PRT_MESSAGE, "item %s reachable from jumppad area %d\r\n", ic->iteminfo[i].classname, goalareanum);
                    if( !goalareanum ) continue;
                } //end if
            } //end if
        } //end if
        
        li = AllocLevelItem();
        if( !li ) return;
        //
        li->number = ++numlevelitems;
        li->timeout = 0;
        li->entitynum = 0;
        //
        li->flags = 0;
        AAS_IntForBSPEpairKey( ent, "notfree", &value );
        if( value ) li->flags |= IFL_NOTFREE;
        AAS_IntForBSPEpairKey( ent, "notteam", &value );
        if( value ) li->flags |= IFL_NOTTEAM;
        AAS_IntForBSPEpairKey( ent, "notsingle", &value );
        if( value ) li->flags |= IFL_NOTSINGLE;
        AAS_IntForBSPEpairKey( ent, "notbot", &value );
        if( value ) li->flags |= IFL_NOTBOT;
        if( !strcmp( classname, "item_botroam" ) )
        {
            li->flags |= IFL_ROAM;
            AAS_FloatForBSPEpairKey( ent, "weight", &li->weight );
        } //end if
        //if not a stationary item
        if( !( spawnflags & 1 ) )
        {
            if( !AAS_DropToFloor( origin, ic->iteminfo[i].mins, ic->iteminfo[i].maxs ) )
            {
                botimport.Print( PRT_MESSAGE, "%s in solid at (%1.1f %1.1f %1.1f)\n",
                                 classname, origin[0], origin[1], origin[2] );
            } //end if
        } //end if
        //item info of the level item
        li->iteminfo = i;
        //origin of the item
        VectorCopy( origin, li->origin );
        //
        if( goalareanum )
        {
            li->goalareanum = goalareanum;
            VectorCopy( origin, li->goalorigin );
        } //end if
        else
        {
            //get the item goal area and goal origin
            li->goalareanum = AAS_BestReachableArea( origin,
                              ic->iteminfo[i].mins, ic->iteminfo[i].maxs,
                              li->goalorigin );
            if( !li->goalareanum )
            {
                botimport.Print( PRT_MESSAGE, "%s not reachable for bots at (%1.1f %1.1f %1.1f)\n",
                                 classname, origin[0], origin[1], origin[2] );
            } //end if
        } //end else
        //
        AddLevelItemToList( li );
    } //end for
    botimport.Print( PRT_MESSAGE, "found %d level items\n", numlevelitems );
} //end of the function BotInitLevelItems
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotGoalName( S32 number, UTF8* name, S32 size )
{
    levelitem_t* li;
    
    if( !itemconfig ) return;
    //
    for( li = levelitems; li; li = li->next )
    {
        if( li->number == number )
        {
            strncpy( name, itemconfig->iteminfo[li->iteminfo].name, size - 1 );
            name[size - 1] = '\0';
            return;
        } //end for
    } //end for
    strcpy( name, "" );
    return;
} //end of the function BotGoalName
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotResetAvoidGoals( S32 goalstate )
{
    bot_goalstate_t* gs;
    
    gs = BotGoalStateFromHandle( goalstate );
    if( !gs ) return;
    ::memset( gs->avoidgoals, 0, MAX_AVOIDGOALS * sizeof( S32 ) );
    ::memset( gs->avoidgoaltimes, 0, MAX_AVOIDGOALS * sizeof( F32 ) );
} //end of the function BotResetAvoidGoals
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotDumpAvoidGoals( S32 goalstate )
{
    S32 i;
    bot_goalstate_t* gs;
    UTF8 name[32];
    
    gs = BotGoalStateFromHandle( goalstate );
    if( !gs ) return;
    for( i = 0; i < MAX_AVOIDGOALS; i++ )
    {
        if( gs->avoidgoaltimes[i] >= AAS_Time() )
        {
            BotGoalName( gs->avoidgoals[i], name, 32 );
            Log_Write( "avoid goal %s, number %d for %f seconds", name,
                       gs->avoidgoals[i], gs->avoidgoaltimes[i] - AAS_Time() );
        } //end if
    } //end for
} //end of the function BotDumpAvoidGoals
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotAddToAvoidGoals( bot_goalstate_t* gs, S32 number, F32 avoidtime )
{
    S32 i;
    
    for( i = 0; i < MAX_AVOIDGOALS; i++ )
    {
        //if the avoid goal is already stored
        if( gs->avoidgoals[i] == number )
        {
            gs->avoidgoals[i] = number;
            gs->avoidgoaltimes[i] = AAS_Time() + avoidtime;
            return;
        } //end if
    } //end for
    
    for( i = 0; i < MAX_AVOIDGOALS; i++ )
    {
        //if this avoid goal has expired
        if( gs->avoidgoaltimes[i] < AAS_Time() )
        {
            gs->avoidgoals[i] = number;
            gs->avoidgoaltimes[i] = AAS_Time() + avoidtime;
            return;
        } //end if
    } //end for
} //end of the function BotAddToAvoidGoals
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void BotRemoveFromAvoidGoals( S32 goalstate, S32 number )
{
    S32 i;
    bot_goalstate_t* gs;
    
    gs = BotGoalStateFromHandle( goalstate );
    if( !gs ) return;
    //don't use the goals the bot wants to avoid
    for( i = 0; i < MAX_AVOIDGOALS; i++ )
    {
        if( gs->avoidgoals[i] == number && gs->avoidgoaltimes[i] >= AAS_Time() )
        {
            gs->avoidgoaltimes[i] = 0;
            return;
        } //end if
    } //end for
} //end of the function BotRemoveFromAvoidGoals
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
F32 BotAvoidGoalTime( S32 goalstate, S32 number )
{
    S32 i;
    bot_goalstate_t* gs;
    
    gs = BotGoalStateFromHandle( goalstate );
    if( !gs ) return 0;
    //don't use the goals the bot wants to avoid
    for( i = 0; i < MAX_AVOIDGOALS; i++ )
    {
        if( gs->avoidgoals[i] == number && gs->avoidgoaltimes[i] >= AAS_Time() )
        {
            return gs->avoidgoaltimes[i] - AAS_Time();
        } //end if
    } //end for
    return 0;
} //end of the function BotAvoidGoalTime
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void BotSetAvoidGoalTime( S32 goalstate, S32 number, F32 avoidtime )
{
    bot_goalstate_t* gs;
    levelitem_t* li;
    
    gs = BotGoalStateFromHandle( goalstate );
    if( !gs )
        return;
    if( avoidtime < 0 )
    {
        if( !itemconfig )
            return;
        //
        for( li = levelitems; li; li = li->next )
        {
            if( li->number == number )
            {
                avoidtime = itemconfig->iteminfo[li->iteminfo].respawntime;
                if( !avoidtime )
                    avoidtime = AVOID_DEFAULT_TIME;
                if( avoidtime < AVOID_MINIMUM_TIME )
                    avoidtime = AVOID_MINIMUM_TIME;
                BotAddToAvoidGoals( gs, number, avoidtime );
                return;
            } //end for
        } //end for
        return;
    } //end if
    else
    {
        BotAddToAvoidGoals( gs, number, avoidtime );
    } //end else
} //end of the function BotSetAvoidGoalTime
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
S32 BotGetLevelItemGoal( S32 index, UTF8* name, bot_goal_t* goal )
{
    levelitem_t* li;
    
    if( !itemconfig ) return -1;
    li = levelitems;
    if( index >= 0 )
    {
        for( ; li; li = li->next )
        {
            if( li->number == index )
            {
                li = li->next;
                break;
            } //end if
        } //end for
    } //end for
    for( ; li; li = li->next )
    {
        //
        if( g_gametype == GT_SINGLE_PLAYER )
        {
            if( li->flags & IFL_NOTSINGLE ) continue;
        }
        else if( g_gametype >= GT_TEAM )
        {
            if( li->flags & IFL_NOTTEAM ) continue;
        }
        else
        {
            if( li->flags & IFL_NOTFREE ) continue;
        }
        if( li->flags & IFL_NOTBOT ) continue;
        //
        if( !Q_stricmp( name, itemconfig->iteminfo[li->iteminfo].name ) )
        {
            goal->areanum = li->goalareanum;
            VectorCopy( li->goalorigin, goal->origin );
            goal->entitynum = li->entitynum;
            VectorCopy( itemconfig->iteminfo[li->iteminfo].mins, goal->mins );
            VectorCopy( itemconfig->iteminfo[li->iteminfo].maxs, goal->maxs );
            goal->number = li->number;
            goal->flags = GFL_ITEM;
            if( li->timeout ) goal->flags |= GFL_DROPPED;
            //botimport.Print(PRT_MESSAGE, "found li %s\n", itemconfig->iteminfo[li->iteminfo].name);
            return li->number;
        } //end if
    } //end for
    return -1;
} //end of the function BotGetLevelItemGoal
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
S32 BotGetMapLocationGoal( UTF8* name, bot_goal_t* goal )
{
    maplocation_t* ml;
    vec3_t mins = { -8, -8, -8}, maxs = {8, 8, 8};
    
    for( ml = maplocations; ml; ml = ml->next )
    {
        if( !Q_stricmp( ml->name, name ) )
        {
            goal->areanum = ml->areanum;
            VectorCopy( ml->origin, goal->origin );
            goal->entitynum = 0;
            VectorCopy( mins, goal->mins );
            VectorCopy( maxs, goal->maxs );
            return true;
        } //end if
    } //end for
    return false;
} //end of the function BotGetMapLocationGoal
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
S32 BotGetNextCampSpotGoal( S32 num, bot_goal_t* goal )
{
    S32 i;
    campspot_t* cs;
    vec3_t mins = { -8, -8, -8}, maxs = {8, 8, 8};
    
    if( num < 0 ) num = 0;
    i = num;
    for( cs = campspots; cs; cs = cs->next )
    {
        if( --i < 0 )
        {
            goal->areanum = cs->areanum;
            VectorCopy( cs->origin, goal->origin );
            goal->entitynum = 0;
            VectorCopy( mins, goal->mins );
            VectorCopy( maxs, goal->maxs );
            return num + 1;
        } //end if
    } //end for
    return 0;
} //end of the function BotGetNextCampSpotGoal
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void BotFindEntityForLevelItem( levelitem_t* li )
{
    S32 ent, modelindex;
    itemconfig_t* ic;
    aas_entityinfo_t entinfo;
    vec3_t dir;
    
    ic = itemconfig;
    if( !itemconfig ) return;
    for( ent = AAS_NextEntity( 0 ); ent; ent = AAS_NextEntity( ent ) )
    {
        //get the model index of the entity
        modelindex = AAS_EntityModelindex( ent );
        //
        if( !modelindex ) continue;
        //get info about the entity
        AAS_EntityInfo( ent, &entinfo );
        //if the entity is still moving
        if( entinfo.origin[0] != entinfo.lastvisorigin[0] ||
                entinfo.origin[1] != entinfo.lastvisorigin[1] ||
                entinfo.origin[2] != entinfo.lastvisorigin[2] ) continue;
        //
        if( ic->iteminfo[li->iteminfo].modelindex == modelindex )
        {
            //check if the entity is very close
            VectorSubtract( li->origin, entinfo.origin, dir );
            if( VectorLength( dir ) < 30 )
            {
                //found an entity for this level item
                li->entitynum = ent;
            } //end if
        } //end if
    } //end for
} //end of the function BotFindEntityForLevelItem
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================

//NOTE: enum entityType_t in bg_public.h
#define ET_ITEM			2

void BotUpdateEntityItems( void )
{
    S32 ent, i, modelindex;
    vec3_t dir;
    levelitem_t* li, *nextli;
    aas_entityinfo_t entinfo;
    itemconfig_t* ic;
    
    //timeout current entity items if necessary
    for( li = levelitems; li; li = nextli )
    {
        nextli = li->next;
        //if it is a item that will time out
        if( li->timeout )
        {
            //timeout the item
            if( li->timeout < AAS_Time() )
            {
                RemoveLevelItemFromList( li );
                FreeLevelItem( li );
            } //end if
        } //end if
    } //end for
    //find new entity items
    ic = itemconfig;
    if( !itemconfig ) return;
    //
    for( ent = AAS_NextEntity( 0 ); ent; ent = AAS_NextEntity( ent ) )
    {
        if( AAS_EntityType( ent ) != ET_ITEM ) continue;
        //get the model index of the entity
        modelindex = AAS_EntityModelindex( ent );
        //
        if( !modelindex ) continue;
        //get info about the entity
        AAS_EntityInfo( ent, &entinfo );
        //FIXME: don't do this
        //skip all floating items for now
        //if (entinfo.groundent != ENTITYNUM_WORLD) continue;
        //if the entity is still moving
        if( entinfo.origin[0] != entinfo.lastvisorigin[0] ||
                entinfo.origin[1] != entinfo.lastvisorigin[1] ||
                entinfo.origin[2] != entinfo.lastvisorigin[2] ) continue;
        //check if the entity is already stored as a level item
        for( li = levelitems; li; li = li->next )
        {
            //if the level item is linked to an entity
            if( li->entitynum && li->entitynum == ent )
            {
                //the entity is re-used if the models are different
                if( ic->iteminfo[li->iteminfo].modelindex != modelindex )
                {
                    //remove this level item
                    RemoveLevelItemFromList( li );
                    FreeLevelItem( li );
                    li = NULL;
                    break;
                } //end if
                else
                {
                    if( entinfo.origin[0] != li->origin[0] ||
                            entinfo.origin[1] != li->origin[1] ||
                            entinfo.origin[2] != li->origin[2] )
                    {
                        VectorCopy( entinfo.origin, li->origin );
                        //also update the goal area number
                        li->goalareanum = AAS_BestReachableArea( li->origin,
                                          ic->iteminfo[li->iteminfo].mins, ic->iteminfo[li->iteminfo].maxs,
                                          li->goalorigin );
                    } //end if
                    break;
                } //end else
            } //end if
        } //end for
        if( li ) continue;
        //try to link the entity to a level item
        for( li = levelitems; li; li = li->next )
        {
            //if this level item is already linked
            if( li->entitynum ) continue;
            //
            if( g_gametype == GT_SINGLE_PLAYER )
            {
                if( li->flags & IFL_NOTSINGLE ) continue;
            }
            else if( g_gametype >= GT_TEAM )
            {
                if( li->flags & IFL_NOTTEAM ) continue;
            }
            else
            {
                if( li->flags & IFL_NOTFREE ) continue;
            }
            //if the model of the level item and the entity are the same
            if( ic->iteminfo[li->iteminfo].modelindex == modelindex )
            {
                //check if the entity is very close
                VectorSubtract( li->origin, entinfo.origin, dir );
                if( VectorLength( dir ) < 30 )
                {
                    //found an entity for this level item
                    li->entitynum = ent;
                    //if the origin is different
                    if( entinfo.origin[0] != li->origin[0] ||
                            entinfo.origin[1] != li->origin[1] ||
                            entinfo.origin[2] != li->origin[2] )
                    {
                        //update the level item origin
                        VectorCopy( entinfo.origin, li->origin );
                        //also update the goal area number
                        li->goalareanum = AAS_BestReachableArea( li->origin,
                                          ic->iteminfo[li->iteminfo].mins, ic->iteminfo[li->iteminfo].maxs,
                                          li->goalorigin );
                    } //end if
#ifdef DEBUG
                    Log_Write( "linked item %s to an entity", ic->iteminfo[li->iteminfo].classname );
#endif //DEBUG
                    break;
                } //end if
            } //end else
        } //end for
        if( li ) continue;
        //check if the model is from a known item
        for( i = 0; i < ic->numiteminfo; i++ )
        {
            if( ic->iteminfo[i].modelindex == modelindex )
            {
                break;
            } //end if
        } //end for
        //if the model is not from a known item
        if( i >= ic->numiteminfo ) continue;
        //allocate a new level item
        li = AllocLevelItem();
        //
        if( !li ) continue;
        //entity number of the level item
        li->entitynum = ent;
        //number for the level item
        li->number = numlevelitems + ent;
        //set the item info index for the level item
        li->iteminfo = i;
        //origin of the item
        VectorCopy( entinfo.origin, li->origin );
        //get the item goal area and goal origin
        li->goalareanum = AAS_BestReachableArea( li->origin,
                          ic->iteminfo[i].mins, ic->iteminfo[i].maxs,
                          li->goalorigin );
        //never go for items dropped into jumppads
        if( AAS_AreaJumpPad( li->goalareanum ) )
        {
            FreeLevelItem( li );
            continue;
        } //end if
        //time this item out after 30 seconds
        //dropped items disappear after 30 seconds
        li->timeout = AAS_Time() + 30;
        //add the level item to the list
        AddLevelItemToList( li );
        //botimport.Print(PRT_MESSAGE, "found new level item %s\n", ic->iteminfo[i].classname);
    } //end for
    /*
    for (li = levelitems; li; li = li->next)
    {
    	if (!li->entitynum)
    	{
    		BotFindEntityForLevelItem(li);
    	} //end if
    } //end for*/
} //end of the function BotUpdateEntityItems
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotDumpGoalStack( S32 goalstate )
{
    S32 i;
    bot_goalstate_t* gs;
    UTF8 name[32];
    
    gs = BotGoalStateFromHandle( goalstate );
    if( !gs ) return;
    for( i = 1; i <= gs->goalstacktop; i++ )
    {
        BotGoalName( gs->goalstack[i].number, name, 32 );
        Log_Write( "%d: %s", i, name );
    } //end for
} //end of the function BotDumpGoalStack
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotPushGoal( S32 goalstate, bot_goal_t* goal )
{
    bot_goalstate_t* gs;
    
    gs = BotGoalStateFromHandle( goalstate );
    if( !gs ) return;
    if( gs->goalstacktop >= MAX_GOALSTACK - 1 )
    {
        botimport.Print( PRT_ERROR, "goal heap overflow\n" );
        BotDumpGoalStack( goalstate );
        return;
    } //end if
    gs->goalstacktop++;
    ::memcpy( &gs->goalstack[gs->goalstacktop], goal, sizeof( bot_goal_t ) );
} //end of the function BotPushGoal
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotPopGoal( S32 goalstate )
{
    bot_goalstate_t* gs;
    
    gs = BotGoalStateFromHandle( goalstate );
    if( !gs ) return;
    if( gs->goalstacktop > 0 ) gs->goalstacktop--;
} //end of the function BotPopGoal
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotEmptyGoalStack( S32 goalstate )
{
    bot_goalstate_t* gs;
    
    gs = BotGoalStateFromHandle( goalstate );
    if( !gs ) return;
    gs->goalstacktop = 0;
} //end of the function BotEmptyGoalStack
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
S32 BotGetTopGoal( S32 goalstate, bot_goal_t* goal )
{
    bot_goalstate_t* gs;
    
    gs = BotGoalStateFromHandle( goalstate );
    if( !gs ) return false;
    if( !gs->goalstacktop ) return false;
    ::memcpy( goal, &gs->goalstack[gs->goalstacktop], sizeof( bot_goal_t ) );
    return true;
} //end of the function BotGetTopGoal
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
S32 BotGetSecondGoal( S32 goalstate, bot_goal_t* goal )
{
    bot_goalstate_t* gs;
    
    gs = BotGoalStateFromHandle( goalstate );
    if( !gs ) return false;
    if( gs->goalstacktop <= 1 ) return false;
    ::memcpy( goal, &gs->goalstack[gs->goalstacktop - 1], sizeof( bot_goal_t ) );
    return true;
} //end of the function BotGetSecondGoal
//===========================================================================
// pops a new long term goal on the goal stack in the goalstate
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
S32 BotChooseLTGItem( S32 goalstate, vec3_t origin, S32* inventory, S32 travelflags )
{
    S32 areanum, t, weightnum;
    F32 weight, bestweight, avoidtime;
    iteminfo_t* iteminfo;
    itemconfig_t* ic;
    levelitem_t* li, *bestitem;
    bot_goal_t goal;
    bot_goalstate_t* gs;
    
    gs = BotGoalStateFromHandle( goalstate );
    if( !gs )
        return false;
    if( !gs->itemweightconfig )
        return false;
    //get the area the bot is in
    areanum = BotReachabilityArea( origin, gs->client );
    //if the bot is in solid or if the area the bot is in has no reachability links
    if( !areanum || !AAS_AreaReachability( areanum ) )
    {
        //use the last valid area the bot was in
        areanum = gs->lastreachabilityarea;
    } //end if
    //remember the last area with reachabilities the bot was in
    gs->lastreachabilityarea = areanum;
    //if still in solid
    if( !areanum )
        return false;
    //the item configuration
    ic = itemconfig;
    if( !itemconfig )
        return false;
    //best weight and item so far
    bestweight = 0;
    bestitem = NULL;
    ::memset( &goal, 0, sizeof( bot_goal_t ) );
    //go through the items in the level
    for( li = levelitems; li; li = li->next )
    {
        if( g_gametype == GT_SINGLE_PLAYER )
        {
            if( li->flags & IFL_NOTSINGLE )
                continue;
        }
        else if( g_gametype >= GT_TEAM )
        {
            if( li->flags & IFL_NOTTEAM )
                continue;
        }
        else
        {
            if( li->flags & IFL_NOTFREE )
                continue;
        }
        if( li->flags & IFL_NOTBOT )
            continue;
        //if the item is not in a possible goal area
        if( !li->goalareanum )
            continue;
        //FIXME: is this a good thing? added this for items that never spawned into the game (f.i. CTF flags in obelisk)
        if( !li->entitynum && !( li->flags & IFL_ROAM ) )
            continue;
        //get the fuzzy weight function for this item
        iteminfo = &ic->iteminfo[li->iteminfo];
        weightnum = gs->itemweightindex[iteminfo->number];
        if( weightnum < 0 )
            continue;
            
#ifdef UNDECIDEDFUZZY
        weight = FuzzyWeightUndecided( inventory, gs->itemweightconfig, weightnum );
#else
        weight = FuzzyWeight( inventory, gs->itemweightconfig, weightnum );
#endif //UNDECIDEDFUZZY
#ifdef DROPPEDWEIGHT
        //HACK: to make dropped items more attractive
        if( li->timeout )
            weight += droppedweight->value;
#endif //DROPPEDWEIGHT
        //use weight scale for item_botroam
        if( li->flags & IFL_ROAM ) weight *= li->weight;
        //
        if( weight > 0 )
        {
            //get the travel time towards the goal area
            t = AAS_AreaTravelTimeToGoalArea( areanum, origin, li->goalareanum, travelflags );
            //if the goal is reachable
            if( t > 0 )
            {
                //if this item won't respawn before we get there
                avoidtime = BotAvoidGoalTime( goalstate, li->number );
                if( avoidtime - t * 0.009f > 0 )
                    continue;
                //
                weight /= ( F32 ) t * TRAVELTIME_SCALE;
                //
                if( weight > bestweight )
                {
                    bestweight = weight;
                    bestitem = li;
                } //end if
            } //end if
        } //end if
    } //end for
    //if no goal item found
    if( !bestitem )
    {
        /*
        //if not in lava or slime
        if (!AAS_AreaLava(areanum) && !AAS_AreaSlime(areanum))
        {
        	if (AAS_RandomGoalArea(areanum, travelflags, &goal.areanum, goal.origin))
        	{
        		VectorSet(goal.mins, -15, -15, -15);
        		VectorSet(goal.maxs, 15, 15, 15);
        		goal.entitynum = 0;
        		goal.number = 0;
        		goal.flags = GFL_ROAM;
        		goal.iteminfo = 0;
        		//push the goal on the stack
        		BotPushGoal(goalstate, &goal);
        		//
        #ifdef DEBUG
        		botimport.Print(PRT_MESSAGE, "chosen roam goal area %d\n", goal.areanum);
        #endif //DEBUG
        		return true;
        	} //end if
        } //end if
        */
        return false;
    } //end if
    //create a bot goal for this item
    iteminfo = &ic->iteminfo[bestitem->iteminfo];
    VectorCopy( bestitem->goalorigin, goal.origin );
    VectorCopy( iteminfo->mins, goal.mins );
    VectorCopy( iteminfo->maxs, goal.maxs );
    goal.areanum = bestitem->goalareanum;
    goal.entitynum = bestitem->entitynum;
    goal.number = bestitem->number;
    goal.flags = GFL_ITEM;
    if( bestitem->timeout )
        goal.flags |= GFL_DROPPED;
    if( bestitem->flags & IFL_ROAM )
        goal.flags |= GFL_ROAM;
    goal.iteminfo = bestitem->iteminfo;
    //if it's a dropped item
    if( bestitem->timeout )
    {
        avoidtime = AVOID_DROPPED_TIME;
    } //end if
    else
    {
        avoidtime = iteminfo->respawntime;
        if( !avoidtime )
            avoidtime = AVOID_DEFAULT_TIME;
        if( avoidtime < AVOID_MINIMUM_TIME )
            avoidtime = AVOID_MINIMUM_TIME;
    } //end else
    //add the chosen goal to the goals to avoid for a while
    BotAddToAvoidGoals( gs, bestitem->number, avoidtime );
    //push the goal on the stack
    BotPushGoal( goalstate, &goal );
    //
    return true;
} //end of the function BotChooseLTGItem
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
S32 BotChooseNBGItem( S32 goalstate, vec3_t origin, S32* inventory, S32 travelflags, bot_goal_t* ltg, F32 maxtime )
{
    S32 areanum, t, weightnum, ltg_time;
    F32 weight, bestweight, avoidtime;
    iteminfo_t* iteminfo;
    itemconfig_t* ic;
    levelitem_t* li, *bestitem;
    bot_goal_t goal;
    bot_goalstate_t* gs;
    
    gs = BotGoalStateFromHandle( goalstate );
    if( !gs )
        return false;
    if( !gs->itemweightconfig )
        return false;
    //get the area the bot is in
    areanum = BotReachabilityArea( origin, gs->client );
    //if the bot is in solid or if the area the bot is in has no reachability links
    if( !areanum || !AAS_AreaReachability( areanum ) )
    {
        //use the last valid area the bot was in
        areanum = gs->lastreachabilityarea;
    } //end if
    //remember the last area with reachabilities the bot was in
    gs->lastreachabilityarea = areanum;
    //if still in solid
    if( !areanum )
        return false;
    //
    if( ltg ) ltg_time = AAS_AreaTravelTimeToGoalArea( areanum, origin, ltg->areanum, travelflags );
    else ltg_time = 99999;
    //the item configuration
    ic = itemconfig;
    if( !itemconfig )
        return false;
    //best weight and item so far
    bestweight = 0;
    bestitem = NULL;
    ::memset( &goal, 0, sizeof( bot_goal_t ) );
    //go through the items in the level
    for( li = levelitems; li; li = li->next )
    {
        if( g_gametype == GT_SINGLE_PLAYER )
        {
            if( li->flags & IFL_NOTSINGLE )
                continue;
        }
        else if( g_gametype >= GT_TEAM )
        {
            if( li->flags & IFL_NOTTEAM )
                continue;
        }
        else
        {
            if( li->flags & IFL_NOTFREE )
                continue;
        }
        if( li->flags & IFL_NOTBOT )
            continue;
        //if the item is in a possible goal area
        if( !li->goalareanum )
            continue;
        //FIXME: is this a good thing? added this for items that never spawned into the game (f.i. CTF flags in obelisk)
        if( !li->entitynum && !( li->flags & IFL_ROAM ) )
            continue;
        //get the fuzzy weight function for this item
        iteminfo = &ic->iteminfo[li->iteminfo];
        weightnum = gs->itemweightindex[iteminfo->number];
        if( weightnum < 0 )
            continue;
        //
#ifdef UNDECIDEDFUZZY
        weight = FuzzyWeightUndecided( inventory, gs->itemweightconfig, weightnum );
#else
        weight = FuzzyWeight( inventory, gs->itemweightconfig, weightnum );
#endif //UNDECIDEDFUZZY
#ifdef DROPPEDWEIGHT
        //HACK: to make dropped items more attractive
        if( li->timeout )
            weight += droppedweight->value;
#endif //DROPPEDWEIGHT
        //use weight scale for item_botroam
        if( li->flags & IFL_ROAM ) weight *= li->weight;
        //
        if( weight > 0 )
        {
            //get the travel time towards the goal area
            t = AAS_AreaTravelTimeToGoalArea( areanum, origin, li->goalareanum, travelflags );
            //if the goal is reachable
            if( t > 0 && t < maxtime )
            {
                //if this item won't respawn before we get there
                avoidtime = BotAvoidGoalTime( goalstate, li->number );
                if( avoidtime - t * 0.009 > 0 )
                    continue;
                //
                weight /= ( F32 ) t * TRAVELTIME_SCALE;
                //
                if( weight > bestweight )
                {
                    t = 0;
                    if( ltg && !li->timeout )
                    {
                        //get the travel time from the goal to the long term goal
                        t = AAS_AreaTravelTimeToGoalArea( li->goalareanum, li->goalorigin, ltg->areanum, travelflags );
                    } //end if
                    //if the travel back is possible and doesn't take too long
                    if( t <= ltg_time )
                    {
                        bestweight = weight;
                        bestitem = li;
                    } //end if
                } //end if
            } //end if
        } //end if
    } //end for
    //if no goal item found
    if( !bestitem )
        return false;
    //create a bot goal for this item
    iteminfo = &ic->iteminfo[bestitem->iteminfo];
    VectorCopy( bestitem->goalorigin, goal.origin );
    VectorCopy( iteminfo->mins, goal.mins );
    VectorCopy( iteminfo->maxs, goal.maxs );
    goal.areanum = bestitem->goalareanum;
    goal.entitynum = bestitem->entitynum;
    goal.number = bestitem->number;
    goal.flags = GFL_ITEM;
    if( bestitem->timeout )
        goal.flags |= GFL_DROPPED;
    if( bestitem->flags & IFL_ROAM )
        goal.flags |= GFL_ROAM;
    goal.iteminfo = bestitem->iteminfo;
    //if it's a dropped item
    if( bestitem->timeout )
    {
        avoidtime = AVOID_DROPPED_TIME;
    } //end if
    else
    {
        avoidtime = iteminfo->respawntime;
        if( !avoidtime )
            avoidtime = AVOID_DEFAULT_TIME;
        if( avoidtime < AVOID_MINIMUM_TIME )
            avoidtime = AVOID_MINIMUM_TIME;
    } //end else
    //add the chosen goal to the goals to avoid for a while
    BotAddToAvoidGoals( gs, bestitem->number, avoidtime );
    //push the goal on the stack
    BotPushGoal( goalstate, &goal );
    //
    return true;
} //end of the function BotChooseNBGItem
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
S32 BotTouchingGoal( vec3_t origin, bot_goal_t* goal )
{
    S32 i;
    vec3_t boxmins, boxmaxs;
    vec3_t absmins, absmaxs;
    vec3_t safety_maxs = {0, 0, 0}; //{4, 4, 10};
    vec3_t safety_mins = {0, 0, 0}; //{-4, -4, 0};
    
    AAS_PresenceTypeBoundingBox( PRESENCE_NORMAL, boxmins, boxmaxs );
    VectorSubtract( goal->mins, boxmaxs, absmins );
    VectorSubtract( goal->maxs, boxmins, absmaxs );
    VectorAdd( absmins, goal->origin, absmins );
    VectorAdd( absmaxs, goal->origin, absmaxs );
    //make the box a little smaller for safety
    VectorSubtract( absmaxs, safety_maxs, absmaxs );
    VectorSubtract( absmins, safety_mins, absmins );
    
    for( i = 0; i < 3; i++ )
    {
        if( origin[i] < absmins[i] || origin[i] > absmaxs[i] ) return false;
    } //end for
    return true;
} //end of the function BotTouchingGoal
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
S32 BotItemGoalInVisButNotVisible( S32 viewer, vec3_t eye, vec3_t viewangles, bot_goal_t* goal )
{
    aas_entityinfo_t entinfo;
    bsp_trace_t trace;
    vec3_t middle;
    
    if( !( goal->flags & GFL_ITEM ) ) return false;
    //
    VectorAdd( goal->mins, goal->mins, middle );
    VectorScale( middle, 0.5f, middle );
    VectorAdd( goal->origin, middle, middle );
    //
    trace = AAS_Trace( eye, NULL, NULL, middle, viewer, CONTENTS_SOLID );
    //if the goal middle point is visible
    if( trace.fraction >= 1 )
    {
        //the goal entity number doesn't have to be valid
        //just assume it's valid
        if( goal->entitynum <= 0 )
            return false;
        //
        //if the entity data isn't valid
        AAS_EntityInfo( goal->entitynum, &entinfo );
        //NOTE: for some wacko reason entities are sometimes
        // not updated
        //if (!entinfo.valid) return true;
        if( entinfo.ltime < AAS_Time() - 0.5 )
            return true;
    } //end if
    return false;
} //end of the function BotItemGoalInVisButNotVisible
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotResetGoalState( S32 goalstate )
{
    bot_goalstate_t* gs;
    
    gs = BotGoalStateFromHandle( goalstate );
    if( !gs ) return;
    ::memset( gs->goalstack, 0, MAX_GOALSTACK * sizeof( bot_goal_t ) );
    gs->goalstacktop = 0;
    BotResetAvoidGoals( goalstate );
} //end of the function BotResetGoalState
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
S32 BotLoadItemWeights( S32 goalstate, UTF8* filename )
{
    bot_goalstate_t* gs;
    
    gs = BotGoalStateFromHandle( goalstate );
    if( !gs ) return BLERR_CANNOTLOADITEMWEIGHTS;
    //load the weight configuration
    gs->itemweightconfig = ReadWeightConfig( filename );
    if( !gs->itemweightconfig )
    {
        botimport.Print( PRT_FATAL, "couldn't load weights\n" );
        return BLERR_CANNOTLOADITEMWEIGHTS;
    } //end if
    //if there's no item configuration
    if( !itemconfig ) return BLERR_CANNOTLOADITEMWEIGHTS;
    //create the item weight index
    gs->itemweightindex = ItemWeightIndex( gs->itemweightconfig, itemconfig );
    //everything went ok
    return BLERR_NOERROR;
} //end of the function BotLoadItemWeights
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotFreeItemWeights( S32 goalstate )
{
    bot_goalstate_t* gs;
    
    gs = BotGoalStateFromHandle( goalstate );
    if( !gs ) return;
    if( gs->itemweightconfig ) FreeWeightConfig( gs->itemweightconfig );
    if( gs->itemweightindex ) FreeMemory( gs->itemweightindex );
} //end of the function BotFreeItemWeights
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
S32 BotAllocGoalState( S32 client )
{
    S32 i;
    
    for( i = 1; i <= MAX_CLIENTS; i++ )
    {
        if( !botgoalstates[i] )
        {
            botgoalstates[i] = ( bot_goalstate_t* )GetClearedMemory( sizeof( bot_goalstate_t ) );
            botgoalstates[i]->client = client;
            return i;
        } //end if
    } //end for
    return 0;
} //end of the function BotAllocGoalState
//========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//========================================================================
void BotFreeGoalState( S32 handle )
{
    if( handle <= 0 || handle > MAX_CLIENTS )
    {
        botimport.Print( PRT_FATAL, "goal state handle %d out of range\n", handle );
        return;
    } //end if
    if( !botgoalstates[handle] )
    {
        botimport.Print( PRT_FATAL, "invalid goal state handle %d\n", handle );
        return;
    } //end if
    BotFreeItemWeights( handle );
    FreeMemory( botgoalstates[handle] );
    botgoalstates[handle] = NULL;
} //end of the function BotFreeGoalState
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
S32 BotSetupGoalAI( void )
{
    UTF8* filename;
    
    //check if teamplay is on
    g_gametype = ( S32 )LibVarValue( "g_gametype", "0" );
    //item configuration file
    filename = LibVarString( "itemconfig", "items.c" );
    //load the item configuration
    itemconfig = LoadItemConfig( filename );
    if( !itemconfig )
    {
        botimport.Print( PRT_FATAL, "couldn't load item config\n" );
        return BLERR_CANNOTLOADITEMCONFIG;
    } //end if
    //
    droppedweight = LibVar( "droppedweight", "1000" );
    //everything went ok
    return BLERR_NOERROR;
} //end of the function BotSetupGoalAI
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void BotShutdownGoalAI( void )
{
    S32 i;
    
    if( itemconfig ) FreeMemory( itemconfig );
    itemconfig = NULL;
    if( levelitemheap ) FreeMemory( levelitemheap );
    levelitemheap = NULL;
    freelevelitems = NULL;
    levelitems = NULL;
    numlevelitems = 0;
    
    BotFreeInfoEntities();
    
    for( i = 1; i <= MAX_CLIENTS; i++ )
    {
        if( botgoalstates[i] )
        {
            BotFreeGoalState( i );
        } //end if
    } //end for
} //end of the function BotShutdownGoalAI
