////////////////////////////////////////////////////////////////////////////////////////
// Copyright(C) 1999 - 2010 id Software LLC, a ZeniMax Media company.
// Copyright(C) 2005 Stuart Dalton (badcdev@gmail.com)
// Copyright(C) 2011 - 2018 Dusan Jocic <dusanjocic@msn.com>
//
// This file is part of OpenWolf.
//
// OpenWolf is free software; you can redistribute it
// and / or modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the License,
// or (at your option) any later version.
//
// OpenWolf is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenWolf; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110 - 1301  USA
//
// -------------------------------------------------------------------------------------
// File name:   s_openal.cpp
// Version:     v1.01
// Created:
// Compilers:   Visual Studio 2017, gcc 7.3.0
// Description:
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#include <OWLib/precompiled.h>

#ifdef _MSC_VER
// MSVC users must install the OpenAL SDK which doesn't use the AL/*.h scheme.
#include <al.h>
#include <alc.h>
#elif defined (MACOS_X)
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

// Console variables specific to OpenAL
cvar_t* s_alPrecache;
cvar_t* s_alGain;
cvar_t* s_alSources;
cvar_t* s_alDopplerFactor;
cvar_t* s_alDopplerSpeed;
cvar_t* s_alMinDistance;
cvar_t* s_alMaxDistance;
cvar_t* s_alRolloff;
cvar_t* s_alGraceDistance;
cvar_t* s_alDriver;
cvar_t* s_alDevice;
cvar_t* s_alAvailableDevices;

bool QAL_Init( StringEntry libname )
{
    return true;
}

/*
=================
S_AL_Format
=================
*/
static U32 S_AL_Format( int width, int channels )
{
    U32 format = AL_FORMAT_MONO16;
    
    // Work out format
    if( width == 1 )
    {
        if( channels == 1 )
            format = AL_FORMAT_MONO8;
        else if( channels == 2 )
            format = AL_FORMAT_STEREO8;
    }
    else if( width == 2 )
    {
        if( channels == 1 )
            format = AL_FORMAT_MONO16;
        else if( channels == 2 )
            format = AL_FORMAT_STEREO16;
    }
    
    return format;
}

/*
=================
S_AL_ErrorMsg
=================
*/
static StringEntry S_AL_ErrorMsg( S32 error )
{
    switch( error )
    {
        case AL_NO_ERROR:
            return "No error";
        case AL_INVALID_NAME:
            return "Invalid name";
        case AL_INVALID_ENUM:
            return "Invalid enumerator";
        case AL_INVALID_VALUE:
            return "Invalid value";
        case AL_INVALID_OPERATION:
            return "Invalid operation";
        case AL_OUT_OF_MEMORY:
            return "Out of memory";
        default:
            return "Unknown error";
    }
}

/*
=================
S_AL_ClearError
=================
*/
static void S_AL_ClearError( bool quiet )
{
    S32 error = alGetError();
    
    if( quiet )
        return;
        
    if( error != AL_NO_ERROR )
    {
        Com_Printf( S_COLOR_YELLOW "WARNING: unhandled AL error: %s\n", S_AL_ErrorMsg( error ) );
    }
}


//===========================================================================


typedef struct alSfx_s
{
    UTF8            filename[MAX_QPATH];
    U32             buffer;		// OpenAL buffer
    snd_info_t      info;		// information for this sound like rate, sample count..
    
    bool            isDefault;	// Couldn't be loaded - use default FX
    bool            inMemory;	// Sound is stored in memory
    bool            isLocked;	// Sound is locked (can not be unloaded)
    S32             lastUsedTime;	// Time last used
    
    S32             loopCnt;	// number of loops using this sfx
    S32             loopActiveCnt;	// number of playing loops using this sfx
    S32             masterLoopSrc;	// All other sources looping this buffer are synced to this master src
} alSfx_t;

static bool alBuffersInitialised = false;

// Sound effect storage, data structures
#define MAX_SFX 4096
static alSfx_t  knownSfx[MAX_SFX];
static S32      numSfx = 0;

static sfxHandle_t default_sfx;

/*
=================
S_AL_BufferFindFree

Find a free handle
=================
*/
static sfxHandle_t S_AL_BufferFindFree( void )
{
    S32 i;
    
    for( i = 0; i < MAX_SFX; i++ )
    {
        // Got one
        if( knownSfx[i].filename[0] == '\0' )
        {
            if( i >= numSfx )
                numSfx = i + 1;
            return i;
        }
    }
    
    // Shit...
    Com_Error( ERR_FATAL, "S_AL_BufferFindFree: No free sound handles" );
    return -1;
}

/*
=================
S_AL_BufferFind

Find a sound effect if loaded, set up a handle otherwise
=================
*/
static sfxHandle_t S_AL_BufferFind( StringEntry filename )
{
    // Look it up in the table
    sfxHandle_t     sfx = -1;
    S32             i;
    
    for( i = 0; i < numSfx; i++ )
    {
        if( !Q_stricmp( knownSfx[i].filename, filename ) )
        {
            sfx = i;
            break;
        }
    }
    
    // Not found in table?
    if( sfx == -1 )
    {
        alSfx_t* ptr;
        
        sfx = S_AL_BufferFindFree();
        
        // Clear and copy the filename over
        ptr = &knownSfx[sfx];
        ::memset( ptr, 0, sizeof( *ptr ) );
        ptr->masterLoopSrc = -1;
        strcpy( ptr->filename, filename );
    }
    
    // Return the handle
    return sfx;
}

/*
=================
S_AL_BufferUseDefault
=================
*/
static void S_AL_BufferUseDefault( sfxHandle_t sfx )
{
    if( sfx == default_sfx )
        Com_Error( ERR_FATAL, "Can't load default sound effect %s\n", knownSfx[sfx].filename );
        
    Com_Printf( S_COLOR_YELLOW "WARNING: Using default sound for %s\n", knownSfx[sfx].filename );
    knownSfx[sfx].isDefault = true;
    knownSfx[sfx].buffer = knownSfx[default_sfx].buffer;
}

/*
=================
S_AL_BufferUnload
=================
*/
static void S_AL_BufferUnload( sfxHandle_t sfx )
{
    S32 error;
    
    if( knownSfx[sfx].filename[0] == '\0' )
        return;
        
    if( !knownSfx[sfx].inMemory )
        return;
        
    // Delete it
    S_AL_ClearError( false );
    alDeleteBuffers( 1, &knownSfx[sfx].buffer );
    if( ( error = alGetError() ) != AL_NO_ERROR )
        Com_Printf( S_COLOR_RED "ERROR: Can't delete sound buffer for %s\n", knownSfx[sfx].filename );
        
    knownSfx[sfx].inMemory = false;
}

/*
=================
S_AL_BufferEvict
=================
*/
static bool  S_AL_BufferEvict( void )
{
    S32 i, oldestBuffer = -1;
    S32 oldestTime = Sys_Milliseconds();
    
    for( i = 0; i < numSfx; i++ )
    {
        if( !knownSfx[i].filename[0] )
            continue;
            
        if( !knownSfx[i].inMemory )
            continue;
            
        if( knownSfx[i].lastUsedTime < oldestTime )
        {
            oldestTime = knownSfx[i].lastUsedTime;
            oldestBuffer = i;
        }
    }
    
    if( oldestBuffer >= 0 )
    {
        S_AL_BufferUnload( oldestBuffer );
        return true;
    }
    else
        return false;
}

/*
=================
S_AL_BufferLoad
=================
*/
static void S_AL_BufferLoad( sfxHandle_t sfx )
{
    S32 error;
    U32 format;
    void* data;
    snd_info_t info;
    alSfx_t* curSfx = &knownSfx[sfx];
    
    // Nothing?
    if( curSfx->filename[0] == '\0' )
        return;
        
    // Player SFX
    if( curSfx->filename[0] == '*' )
        return;
        
    // Already done?
    if( ( curSfx->inMemory ) || ( curSfx->isDefault ) )
        return;
        
    // Try to load
    data = S_CodecLoad( curSfx->filename, &info );
    if( !data )
    {
        S_AL_BufferUseDefault( sfx );
        return;
    }
    
    format = S_AL_Format( info.width, info.channels );
    
    // Create a buffer
    S_AL_ClearError( false );
    alGenBuffers( 1, &curSfx->buffer );
    if( ( error = alGetError() ) != AL_NO_ERROR )
    {
        S_AL_BufferUseDefault( sfx );
        Z_Free( data );
        Com_Printf( S_COLOR_RED "ERROR: Can't create a sound buffer for %s - %s\n", curSfx->filename, S_AL_ErrorMsg( error ) );
        return;
    }
    
    // Fill the buffer
    if( info.size == 0 )
    {
        // We have no data to buffer, so buffer silence
        U8 dummyData[2] = { 0 };
        
        alBufferData( curSfx->buffer, AL_FORMAT_MONO16, ( void* )dummyData, 2, 22050 );
    }
    else
        alBufferData( curSfx->buffer, format, data, info.size, info.rate );
        
    error = alGetError();
    
    // If we ran out of memory, start evicting the least recently used sounds
    while( error == AL_OUT_OF_MEMORY )
    {
        if( !S_AL_BufferEvict() )
        {
            S_AL_BufferUseDefault( sfx );
            Z_Free( data );
            Com_Printf( S_COLOR_RED "ERROR: Out of memory loading %s\n", curSfx->filename );
            return;
        }
        
        // Try load it again
        alBufferData( curSfx->buffer, format, data, info.size, info.rate );
        error = alGetError();
    }
    
    // Some other error condition
    if( error != AL_NO_ERROR )
    {
        S_AL_BufferUseDefault( sfx );
        Z_Free( data );
        Com_Printf( S_COLOR_RED "ERROR: Can't fill sound buffer for %s - %s\n", curSfx->filename, S_AL_ErrorMsg( error ) );
        return;
    }
    
    curSfx->info = info;
    
    // Free the memory
    Z_Free( data );
    
    // Woo!
    curSfx->inMemory = true;
}

/*
=================
S_AL_BufferUse
=================
*/
static void S_AL_BufferUse( sfxHandle_t sfx )
{
    if( knownSfx[sfx].filename[0] == '\0' )
        return;
        
    if( ( !knownSfx[sfx].inMemory ) && ( !knownSfx[sfx].isDefault ) )
        S_AL_BufferLoad( sfx );
        
    knownSfx[sfx].lastUsedTime = Sys_Milliseconds();
}

/*
=================
S_AL_BufferInit
=================
*/
static bool  S_AL_BufferInit( void )
{
    if( alBuffersInitialised )
        return true;
        
    // Clear the hash table, and SFX table
    ::memset( knownSfx, 0, sizeof( knownSfx ) );
    numSfx = 0;
    
    // Load the default sound, and lock it
    default_sfx = S_AL_BufferFind( "sound/null.wav" );
    S_AL_BufferUse( default_sfx );
    knownSfx[default_sfx].isLocked = true;
    
    // All done
    alBuffersInitialised = true;
    return true;
}

/*
=================
S_AL_BufferShutdown
=================
*/
static void S_AL_BufferShutdown( void )
{
    S32 i;
    
    if( !alBuffersInitialised )
        return;
        
    // Unlock the default sound effect
    knownSfx[default_sfx].isLocked = false;
    
    // Free all used effects
    for( i = 0; i < numSfx; i++ )
        S_AL_BufferUnload( i );
        
    // Clear the tables
    ::memset( knownSfx, 0, sizeof( knownSfx ) );
    
    // All undone
    alBuffersInitialised = false;
}

/*
=================
S_AL_RegisterSound
=================
*/
static sfxHandle_t S_AL_RegisterSound( StringEntry sample )
{
    sfxHandle_t sfx = S_AL_BufferFind( sample );
    
    if( s_alPrecache->integer && ( !knownSfx[sfx].inMemory ) && ( !knownSfx[sfx].isDefault ) )
        S_AL_BufferLoad( sfx );
    knownSfx[sfx].lastUsedTime = Com_Milliseconds();
    
    return sfx;
}

/*
=================
S_AL_BufferGet

Return's an sfx's buffer
=================
*/
static ALuint S_AL_BufferGet( sfxHandle_t sfx )
{
    return knownSfx[sfx].buffer;
}


//===========================================================================


typedef struct src_s
{
    ALuint          alSource;	// OpenAL source object
    sfxHandle_t     sfx;		// Sound effect in use
    
    S32             lastUsedTime;	// Last time used
    alSrcPriority_t priority;	// Priority
    S32             entity;		// Owning entity (-1 if none)
    S32             channel;	// Associated channel (-1 if none)
    
    bool            isActive;	// Is this source currently in use?
    bool            isPlaying;	// Is this source currently playing, or stopped?
    bool            isLocked;	// This is locked (un-allocatable)
    bool            isLooping;	// Is this a looping effect (attached to an entity)
    bool            isTracking;	// Is this object tracking its owner
    
    F32             curGain;	// gain employed if source is within maxdistance.
    F32             scaleGain;	// Last gain value for this source. 0 if muted.
    
    F32             lastTimePos;	// On stopped loops, the last position in the buffer
    S32             lastSampleTime;	// Time when this was stopped
    vec3_t          loopSpeakerPos;	// Origin of the loop speaker
    
    bool            local;		// Is this local (relative to the cam)
} src_t;

#ifdef MACOS_X
#define MAX_SRC 64
#else
#define MAX_SRC 128
#endif
static src_t    srcList[MAX_SRC];
static S32      srcCount = 0;
static S32      srcActiveCnt = 0;
static bool     alSourcesInitialised = false;
static vec3_t   lastListenerOrigin = { 0.0f, 0.0f, 0.0f };

typedef struct sentity_s
{
    vec3_t          origin;
    
    bool            srcAllocated;	// If a src_t has been allocated to this entity
    S32             srcIndex;
    
    bool            loopAddedThisFrame;
    alSrcPriority_t loopPriority;
    sfxHandle_t     loopSfx;
    bool            startLoopingSound;
} sentity_t;

static sentity_t entityList[MAX_GENTITIES];

/*
=================
S_AL_SanitiseVector
=================
*/
#define S_AL_SanitiseVector(v) _S_AL_SanitiseVector(v,__LINE__)
static void _S_AL_SanitiseVector( vec3_t v, S32 line )
{
    if( Q_isnan( v[0] ) || Q_isnan( v[1] ) || Q_isnan( v[2] ) )
    {
        Com_DPrintf( S_COLOR_YELLOW "WARNING: vector with one or more NaN components "
                     "being passed to OpenAL at %s:%d -- zeroing\n", __FILE__, line );
        VectorClear( v );
    }
}

#define AL_THIRD_PERSON_THRESHOLD_SQ (48.0f*48.0f)

/*
=================
S_AL_Gain
Set gain to 0 if muted, otherwise set it to given value.
=================
*/

static void S_AL_Gain( U32 source, F32 gainval )
{
    if( s_muted->integer )
        alSourcef( source, AL_GAIN, 0.0f );
    else
        alSourcef( source, AL_GAIN, gainval );
}

/*
=================
S_AL_ScaleGain
Adapt the gain if necessary to get a quicker fadeout when the source is too far away.
=================
*/

static void S_AL_ScaleGain( src_t* chksrc, vec3_t origin )
{
    F32 distance;
    
    if( !chksrc->local )
        distance = Distance( origin, lastListenerOrigin );
        
    // If we exceed a certain distance, scale the gain linearly until the sound
    // vanishes into nothingness.
    if( !chksrc->local && ( distance -= s_alMaxDistance->value ) > 0 )
    {
        F32 scaleFactor;
        
        if( distance >= s_alGraceDistance->value )
            scaleFactor = 0.0f;
        else
            scaleFactor = 1.0f - distance / s_alGraceDistance->value;
            
        scaleFactor *= chksrc->curGain;
        
        if( chksrc->scaleGain != scaleFactor )
        {
            chksrc->scaleGain = scaleFactor;
            S_AL_Gain( chksrc->alSource, chksrc->scaleGain );
        }
    }
    else if( chksrc->scaleGain != chksrc->curGain )
    {
        chksrc->scaleGain = chksrc->curGain;
        S_AL_Gain( chksrc->alSource, chksrc->scaleGain );
    }
}

/*
=================
S_AL_HearingThroughEntity
=================
*/
static bool  S_AL_HearingThroughEntity( S32 entityNum )
{
    F32 distanceSq;
    
    if( clc.clientNum == entityNum )
    {
        // FIXME: <tim@ngus.net> 28/02/06 This is an outrageous hack to detect
        // whether or not the player is rendering in third person or not. We can't
        // ask the renderer because the renderer has no notion of entities and we
        // can't ask cgame since that would involve changing the API and hence mod
        // compatibility. I don't think there is any way around this, but I'll leave
        // the FIXME just in case anyone has a bright idea.
        distanceSq = DistanceSquared( entityList[entityNum].origin, lastListenerOrigin );
        
        if( distanceSq > AL_THIRD_PERSON_THRESHOLD_SQ )
            return false;		//we're the player, but third person
        else
            return true;		//we're the player
    }
    else
        return false;			//not the player
}

/*
=================
S_AL_SrcInit
=================
*/
static bool S_AL_SrcInit( void )
{
    S32 i, limit, error;
    
    // Clear the sources data structure
    ::memset( srcList, 0, sizeof( srcList ) );
    srcCount = 0;
    srcActiveCnt = 0;
    
    // Cap s_alSources to MAX_SRC
    limit = s_alSources->integer;
    if( limit > MAX_SRC )
        limit = MAX_SRC;
    else if( limit < 16 )
        limit = 16;
        
    S_AL_ClearError( false );
    // Allocate as many sources as possible
    for( i = 0; i < limit; i++ )
    {
        alGenSources( 1, &srcList[i].alSource );
        if( ( error = alGetError() ) != AL_NO_ERROR )
            break;
        srcCount++;
    }
    
    // All done. Print this for informational purposes
    Com_Printf( "Allocated %d sources.\n", srcCount );
    alSourcesInitialised = true;
    return true;
}

/*
=================
S_AL_SrcShutdown
=================
*/
static void S_AL_SrcShutdown( void )
{
    S32 i;
    src_t* curSource;
    
    if( !alSourcesInitialised )
        return;
        
    // Destroy all the sources
    for( i = 0; i < srcCount; i++ )
    {
        curSource = &srcList[i];
        
        if( curSource->isLocked )
            Com_DPrintf( S_COLOR_YELLOW "WARNING: Source %d is locked\n", i );
            
        if( curSource->entity > 0 )
            entityList[curSource->entity].srcAllocated = false;
            
        alSourceStop( srcList[i].alSource );
        alDeleteSources( 1, &srcList[i].alSource );
    }
    
    ::memset( srcList, 0, sizeof( srcList ) );
    
    alSourcesInitialised = false;
}

/*
=================
S_AL_SrcSetup
=================
*/
static void S_AL_SrcSetup( srcHandle_t src, sfxHandle_t sfx, alSrcPriority_t priority, S32 entity, S32 channel, bool local )
{
    U32 buffer;
    src_t* curSource;
    
    // Mark the SFX as used, and grab the raw AL buffer
    S_AL_BufferUse( sfx );
    buffer = S_AL_BufferGet( sfx );
    
    // Set up src struct
    curSource = &srcList[src];
    
    curSource->lastUsedTime = Sys_Milliseconds();
    curSource->sfx = sfx;
    curSource->priority = priority;
    curSource->entity = entity;
    curSource->channel = channel;
    curSource->isPlaying = false;
    curSource->isLocked = false;
    curSource->isLooping = false;
    curSource->isTracking = false;
    curSource->curGain = s_alGain->value * s_volume->value;
    curSource->scaleGain = curSource->curGain;
    curSource->local = local;
    
    // Set up OpenAL source
    alSourcei( curSource->alSource, AL_BUFFER, buffer );
    alSourcef( curSource->alSource, AL_PITCH, 1.0f );
    S_AL_Gain( curSource->alSource, curSource->curGain );
    alSourcefv( curSource->alSource, AL_POSITION, vec3_origin );
    alSourcefv( curSource->alSource, AL_VELOCITY, vec3_origin );
    alSourcei( curSource->alSource, AL_LOOPING, AL_FALSE );
    alSourcef( curSource->alSource, AL_REFERENCE_DISTANCE, s_alMinDistance->value );
    
    if( local )
    {
        alSourcei( curSource->alSource, AL_SOURCE_RELATIVE, AL_TRUE );
        alSourcef( curSource->alSource, AL_ROLLOFF_FACTOR, 0.0f );
    }
    else
    {
        alSourcei( curSource->alSource, AL_SOURCE_RELATIVE, AL_FALSE );
        alSourcef( curSource->alSource, AL_ROLLOFF_FACTOR, s_alRolloff->value );
    }
}

/*
=================
S_AL_NewLoopMaster
Remove given source as loop master if it is the master and hand off master status to another source in this case.
=================
*/

static void S_AL_SaveLoopPos( src_t* dest, U32 alSource )
{
    S32 error;
    
    S_AL_ClearError( false );
    
    alGetSourcef( alSource, AL_SEC_OFFSET, &dest->lastTimePos );
    if( ( error = alGetError() ) != AL_NO_ERROR )
    {
        // Old OpenAL implementations don't support AL_SEC_OFFSET
        if( error != AL_INVALID_ENUM )
        {
            Com_Printf( S_COLOR_YELLOW "WARNING: Could not get time offset for alSource %d: %s\n", alSource, S_AL_ErrorMsg( error ) );
        }
        
        dest->lastTimePos = -1;
    }
    else
        dest->lastSampleTime = Sys_Milliseconds();
}

/*
=================
S_AL_NewLoopMaster
Remove given source as loop master if it is the master and hand off master status to another source in this case.
=================
*/

static void S_AL_NewLoopMaster( src_t* rmSource, bool iskilled )
{
    S32 index;
    src_t* curSource = NULL;
    alSfx_t* curSfx;
    
    curSfx = &knownSfx[rmSource->sfx];
    
    if( rmSource->isPlaying )
        curSfx->loopActiveCnt--;
    if( iskilled )
        curSfx->loopCnt--;
        
    if( curSfx->loopCnt )
    {
        if( rmSource->priority == SRCPRI_ENTITY )
        {
            if( !iskilled && rmSource->isPlaying )
            {
                // only sync ambient loops...
                // It makes more sense to have sounds for weapons/projectiles unsynced
                S_AL_SaveLoopPos( rmSource, rmSource->alSource );
            }
        }
        else if( rmSource == &srcList[curSfx->masterLoopSrc] )
        {
            S32 firstInactive = -1;
            
            // Only if rmSource was the master and if there are still playing loops for
            // this sound will we need to find a new master.
            
            if( iskilled || curSfx->loopActiveCnt )
            {
                for( index = 0; index < srcCount; index++ )
                {
                    curSource = &srcList[index];
                    
                    if( curSource->sfx == rmSource->sfx && curSource != rmSource &&
                            curSource->isActive && curSource->isLooping && curSource->priority == SRCPRI_AMBIENT )
                    {
                        if( curSource->isPlaying )
                        {
                            curSfx->masterLoopSrc = index;
                            break;
                        }
                        else if( firstInactive < 0 )
                            firstInactive = index;
                    }
                }
            }
            
            if( !curSfx->loopActiveCnt )
            {
                if( firstInactive < 0 )
                {
                    if( iskilled )
                    {
                        curSfx->masterLoopSrc = -1;
                        return;
                    }
                    else
                        curSource = rmSource;
                }
                else
                    curSource = &srcList[firstInactive];
                    
                if( rmSource->isPlaying )
                {
                    // this was the last not stopped source, save last sample position + time
                    S_AL_SaveLoopPos( curSource, rmSource->alSource );
                }
                else
                {
                    // second case: all loops using this sound have stopped due to listener being of of range,
                    // and now the inactive master gets deleted. Just move over the soundpos settings to the
                    // new master.
                    curSource->lastTimePos = rmSource->lastTimePos;
                    curSource->lastSampleTime = rmSource->lastSampleTime;
                }
            }
        }
    }
    else
        curSfx->masterLoopSrc = -1;
}

/*
=================
S_AL_SrcKill
=================
*/
static void S_AL_SrcKill( srcHandle_t src )
{
    src_t* curSource = &srcList[src];
    
    // I'm not touching it. Unlock it first.
    if( curSource->isLocked )
        return;
        
    // Remove the entity association and loop master status
    if( curSource->isLooping )
    {
        curSource->isLooping = false;
        
        if( curSource->entity != -1 )
        {
            sentity_t* curEnt = &entityList[curSource->entity];
            
            curEnt->srcAllocated = false;
            curEnt->srcIndex = -1;
            curEnt->loopAddedThisFrame = false;
            curEnt->startLoopingSound = false;
        }
        
        S_AL_NewLoopMaster( curSource, true );
    }
    
    // Stop it if it's playing
    if( curSource->isPlaying )
    {
        alSourceStop( curSource->alSource );
        curSource->isPlaying = false;
    }
    
    // Remove the buffer
    alSourcei( curSource->alSource, AL_BUFFER, 0 );
    
    curSource->sfx = 0;
    curSource->lastUsedTime = 0;
    curSource->priority = ( alSrcPriority_t )0;
    curSource->entity = -1;
    curSource->channel = -1;
    
    if( curSource->isActive )
    {
        curSource->isActive = false;
        srcActiveCnt--;
    }
    curSource->isLocked = false;
    curSource->isTracking = false;
    curSource->local = false;
}

/*
=================
S_AL_SrcAlloc
=================
*/
static srcHandle_t S_AL_SrcAlloc( alSrcPriority_t priority, S32 entnum, S32 channel )
{
    S32 i;
    S32 empty = -1;
    S32 weakest = -1;
    S32 weakest_time = Sys_Milliseconds();
    S32 weakest_pri = 999;
    F32 weakest_gain = 1000.0;
    bool weakest_isplaying = true;
    S32 weakest_numloops = 0;
    src_t* curSource;
    
    for( i = 0; i < srcCount; i++ )
    {
        curSource = &srcList[i];
        
        // If it's locked, we aren't even going to look at it
        if( curSource->isLocked )
            continue;
            
        // Is it empty or not?
        if( !curSource->isActive )
        {
            empty = i;
            break;
        }
        
        if( curSource->isPlaying )
        {
            if( weakest_isplaying && curSource->priority < priority &&
                    ( curSource->priority < weakest_pri ||
                      ( !curSource->isLooping && ( curSource->scaleGain < weakest_gain || curSource->lastUsedTime < weakest_time ) ) ) )
            {
                // If it has lower priority, is fainter or older, flag it as weak
                // the last two values are only compared if it's not a looping sound, because we want to prevent two
                // loops (loops are added EVERY frame) fighting for a slot
                weakest_pri = curSource->priority;
                weakest_time = curSource->lastUsedTime;
                weakest_gain = curSource->scaleGain;
                weakest = i;
            }
        }
        else
        {
            weakest_isplaying = false;
            
            if( weakest < 0 ||
                    knownSfx[curSource->sfx].loopCnt > weakest_numloops ||
                    curSource->priority < weakest_pri || curSource->lastUsedTime < weakest_time )
            {
                // Sources currently not playing of course have lowest priority
                // also try to always keep at least one loop master for every loop sound
                weakest_pri = curSource->priority;
                weakest_time = curSource->lastUsedTime;
                weakest_numloops = knownSfx[curSource->sfx].loopCnt;
                weakest = i;
            }
        }
        
        // The channel system is not actually adhered to by baseq3, and not
        // implemented in snd_dma.c, so while the following is strictly correct, it
        // causes incorrect behaviour versus defacto baseq3
#if 0
        // Is it an exact match, and not on channel 0?
        if( ( curSource->entity == entnum ) && ( curSource->channel == channel ) && ( channel != 0 ) )
        {
            S_AL_SrcKill( i );
            return i;
        }
#endif
    }
    
    if( empty == -1 )
        empty = weakest;
        
    if( empty >= 0 )
    {
        S_AL_SrcKill( empty );
        srcList[empty].isActive = true;
        srcActiveCnt++;
    }
    
    return empty;
}

/*
=================
S_AL_SrcLock

Locked sources will not be automatically reallocated or managed
=================
*/
static void S_AL_SrcLock( srcHandle_t src )
{
    srcList[src].isLocked = true;
}

/*
=================
S_AL_SrcUnlock

Once unlocked, the source may be reallocated again
=================
*/
static void S_AL_SrcUnlock( srcHandle_t src )
{
    srcList[src].isLocked = false;
}

/*
=================
S_AL_UpdateEntityPosition
=================
*/
static void S_AL_UpdateEntityPosition( S32 entityNum, const vec3_t origin )
{
    vec3_t sanOrigin;
    
    VectorCopy( origin, sanOrigin );
    S_AL_SanitiseVector( sanOrigin );
    if( entityNum < 0 || entityNum > MAX_GENTITIES )
        Com_Error( ERR_DROP, "S_AL_UpdateEntityPosition: bad entitynum %i", entityNum );
    VectorCopy( sanOrigin, entityList[entityNum].origin );
}

/*
=================
S_AL_CheckInput
Check whether input values from mods are out of range.
Necessary for i.g. Western Quake3 mod which is buggy.
=================
*/
static bool S_AL_CheckInput( S32 entityNum, sfxHandle_t sfx )
{
    if( entityNum < 0 || entityNum > MAX_GENTITIES )
        Com_Error( ERR_DROP, "ERROR: S_AL_CheckInput: bad entitynum %i", entityNum );
        
    if( sfx < 0 || sfx >= numSfx )
    {
        Com_Printf( S_COLOR_RED "ERROR: S_AL_CheckInput: handle %i out of range\n", sfx );
        return true;
    }
    
    return false;
}

/*
=================
S_AL_StartLocalSound

Play a local (non-spatialized) sound effect
=================
*/
static void S_AL_StartLocalSound( sfxHandle_t sfx, S32 channel )
{
    srcHandle_t src;
    
    if( S_AL_CheckInput( 0, sfx ) )
        return;
        
    // Try to grab a source
    src = S_AL_SrcAlloc( SRCPRI_LOCAL, -1, channel );
    
    if( src == -1 )
        return;
        
    // Set up the effect
    S_AL_SrcSetup( src, sfx, SRCPRI_LOCAL, -1, channel, true );
    
    // Start it playing
    srcList[src].isPlaying = true;
    alSourcePlay( srcList[src].alSource );
}

/*
=================
S_AL_StartSound

Play a one-shot sound effect
=================
*/
static void S_AL_StartSound( vec3_t origin, S32 entnum, S32 entchannel, sfxHandle_t sfx )
{
    vec3_t sorigin;
    srcHandle_t src;
    src_t* curSource;
    
    if( origin )
    {
        if( S_AL_CheckInput( 0, sfx ) )
            return;
            
        VectorCopy( origin, sorigin );
    }
    else
    {
        if( S_AL_CheckInput( entnum, sfx ) )
            return;
            
        if( S_AL_HearingThroughEntity( entnum ) )
        {
            S_AL_StartLocalSound( sfx, entchannel );
            return;
        }
        
        VectorCopy( entityList[entnum].origin, sorigin );
    }
    
    S_AL_SanitiseVector( sorigin );
    
    if( ( srcActiveCnt > 5 * srcCount / 3 ) &&
            ( DistanceSquared( sorigin, lastListenerOrigin ) >=
              ( s_alMaxDistance->value + s_alGraceDistance->value ) * ( s_alMaxDistance->value + s_alGraceDistance->value ) ) )
    {
        // We're getting tight on sources and source is not within hearing distance so don't add it
        return;
    }
    
    // Try to grab a source
    src = S_AL_SrcAlloc( SRCPRI_ONESHOT, entnum, entchannel );
    if( src == -1 )
        return;
        
    S_AL_SrcSetup( src, sfx, SRCPRI_ONESHOT, entnum, entchannel, false );
    
    curSource = &srcList[src];
    
    if( !origin )
        curSource->isTracking = true;
        
    alSourcefv( curSource->alSource, AL_POSITION, sorigin );
    S_AL_ScaleGain( curSource, sorigin );
    
    // Start it playing
    curSource->isPlaying = true;
    alSourcePlay( curSource->alSource );
}

/*
=================
S_AL_ClearLoopingSounds
=================
*/
static void S_AL_ClearLoopingSounds( bool killall )
{
    S32 i;
    
    for( i = 0; i < srcCount; i++ )
    {
        if( ( srcList[i].isLooping ) && ( srcList[i].entity != -1 ) )
            entityList[srcList[i].entity].loopAddedThisFrame = false;
    }
}

/*
=================
S_AL_SrcLoop
=================
*/
static void S_AL_SrcLoop( alSrcPriority_t priority, sfxHandle_t sfx, const vec3_t origin, const vec3_t velocity, S32 entityNum )
{
    S32 src;
    sentity_t* sent = &entityList[entityNum];
    src_t* curSource;
    vec3_t sorigin, svelocity;
    
    if( S_AL_CheckInput( entityNum, sfx ) )
        return;
        
    // Do we need to allocate a new source for this entity
    if( !sent->srcAllocated )
    {
        // Try to get a channel
        src = S_AL_SrcAlloc( priority, entityNum, -1 );
        if( src == -1 )
        {
            Com_DPrintf( S_COLOR_YELLOW "WARNING: Failed to allocate source " "for loop sfx %d on entity %d\n", sfx, entityNum );
            return;
        }
        
        curSource = &srcList[src];
        
        sent->startLoopingSound = true;
        
        curSource->lastTimePos = -1.0;
        curSource->lastSampleTime = Sys_Milliseconds();
    }
    else
    {
        src = sent->srcIndex;
        curSource = &srcList[src];
    }
    
    sent->srcAllocated = true;
    sent->srcIndex = src;
    
    sent->loopPriority = priority;
    sent->loopSfx = sfx;
    
    // If this is not set then the looping sound is stopped.
    sent->loopAddedThisFrame = true;
    
    // UGH
    // These lines should be called via S_AL_SrcSetup, but we
    // can't call that yet as it buffers sfxes that may change
    // with subsequent calls to S_AL_SrcLoop
    curSource->entity = entityNum;
    curSource->isLooping = true;
    
    if( S_AL_HearingThroughEntity( entityNum ) )
    {
        curSource->local = true;
        
        VectorClear( sorigin );
        
        alSourcefv( curSource->alSource, AL_POSITION, sorigin );
        alSourcefv( curSource->alSource, AL_VELOCITY, sorigin );
    }
    else
    {
        curSource->local = false;
        
        if( origin )
            VectorCopy( origin, sorigin );
        else
            VectorCopy( sent->origin, sorigin );
            
        S_AL_SanitiseVector( sorigin );
        
        VectorCopy( sorigin, curSource->loopSpeakerPos );
        
        if( velocity )
        {
            VectorCopy( velocity, svelocity );
            S_AL_SanitiseVector( svelocity );
        }
        else
            VectorClear( svelocity );
            
        alSourcefv( curSource->alSource, AL_POSITION, ( ALfloat* ) sorigin );
        alSourcefv( curSource->alSource, AL_VELOCITY, ( ALfloat* ) velocity );
    }
}

/*
=================
S_AL_AddLoopingSound
=================
*/
static void S_AL_AddLoopingSound( S32 entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx )
{
    S_AL_SrcLoop( SRCPRI_ENTITY, sfx, origin, velocity, entityNum );
}

/*
=================
S_AL_AddRealLoopingSound
=================
*/
static void S_AL_AddRealLoopingSound( S32 entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx )
{
    S_AL_SrcLoop( SRCPRI_AMBIENT, sfx, origin, velocity, entityNum );
}

/*
=================
S_AL_StopLoopingSound
=================
*/
static void S_AL_StopLoopingSound( S32 entityNum )
{
    if( entityList[entityNum].srcAllocated )
        S_AL_SrcKill( entityList[entityNum].srcIndex );
}

/*
=================
S_AL_SrcUpdate

Update state (move things around, manage sources, and so on)
=================
*/
static void S_AL_SrcUpdate( void )
{
    S32 i, entityNum, state;
    src_t* curSource;
    
    for( i = 0; i < srcCount; i++ )
    {
        entityNum = srcList[i].entity;
        curSource = &srcList[i];
        
        if( curSource->isLocked )
            continue;
            
        if( !curSource->isActive )
            continue;
            
        // Update source parameters
        if( ( s_alGain->modified ) || ( s_volume->modified ) )
            curSource->curGain = s_alGain->value * s_volume->value;
        if( ( s_alRolloff->modified ) && ( !curSource->local ) )
            alSourcef( curSource->alSource, AL_ROLLOFF_FACTOR, s_alRolloff->value );
        if( s_alMinDistance->modified )
            alSourcef( curSource->alSource, AL_REFERENCE_DISTANCE, s_alMinDistance->value );
            
        if( curSource->isLooping )
        {
            sentity_t* sent = &entityList[entityNum];
            
            // If a looping effect hasn't been touched this frame, pause or kill it
            if( sent->loopAddedThisFrame )
            {
                alSfx_t* curSfx;
                
                // The sound has changed without an intervening removal
                if( curSource->isActive && !sent->startLoopingSound && curSource->sfx != sent->loopSfx )
                {
                    S_AL_NewLoopMaster( curSource, true );
                    
                    curSource->isPlaying = false;
                    alSourceStop( curSource->alSource );
                    alSourcei( curSource->alSource, AL_BUFFER, 0 );
                    sent->startLoopingSound = true;
                }
                
                // The sound hasn't been started yet
                if( sent->startLoopingSound )
                {
                    S_AL_SrcSetup( i, sent->loopSfx, sent->loopPriority, entityNum, -1, curSource->local );
                    curSource->isLooping = true;
                    
                    knownSfx[curSource->sfx].loopCnt++;
                    sent->startLoopingSound = false;
                }
                
                curSfx = &knownSfx[curSource->sfx];
                
                S_AL_ScaleGain( curSource, curSource->loopSpeakerPos );
                if( !curSource->scaleGain )
                {
                    if( curSource->isPlaying )
                    {
                        // Sound is mute, stop playback until we are in range again
                        S_AL_NewLoopMaster( curSource, false );
                        alSourceStop( curSource->alSource );
                        curSource->isPlaying = false;
                    }
                    else if( !curSfx->loopActiveCnt && curSfx->masterLoopSrc < 0 )
                        curSfx->masterLoopSrc = i;
                        
                    continue;
                }
                
                if( !curSource->isPlaying )
                {
                    if( curSource->priority == SRCPRI_AMBIENT )
                    {
                        // If there are other ambient looping sources with the same sound,
                        // make sure the sound of these sources are in sync.
                        
                        if( curSfx->loopActiveCnt )
                        {
                            int             offset, error;
                            
                            // we already have a master loop playing, get buffer position.
                            S_AL_ClearError( false );
                            alGetSourcei( srcList[curSfx->masterLoopSrc].alSource, AL_SAMPLE_OFFSET, &offset );
                            if( ( error = alGetError() ) != AL_NO_ERROR )
                            {
                                if( error != AL_INVALID_ENUM )
                                {
                                    Com_Printf( S_COLOR_YELLOW "WARNING: Cannot get sample offset from source %d: "
                                                "%s\n", i, S_AL_ErrorMsg( error ) );
                                }
                            }
                            else
                                alSourcei( curSource->alSource, AL_SAMPLE_OFFSET, offset );
                        }
                        else if( curSfx->loopCnt && curSfx->masterLoopSrc >= 0 )
                        {
                            F32 secofs;
                            
                            src_t* master = &srcList[curSfx->masterLoopSrc];
                            
                            // This loop sound used to be played, but all sources are stopped. Use last sample position/time
                            // to calculate offset so the player thinks the sources continued playing while they were inaudible.
                            
                            if( master->lastTimePos >= 0 )
                            {
                                secofs = master->lastTimePos + ( Sys_Milliseconds() - master->lastSampleTime ) / 1000.0f;
                                secofs = fmodf( secofs, ( float )curSfx->info.samples / curSfx->info.rate );
                                
                                alSourcef( curSource->alSource, AL_SEC_OFFSET, secofs );
                            }
                            
                            // I be the master now
                            curSfx->masterLoopSrc = i;
                        }
                        else
                            curSfx->masterLoopSrc = i;
                    }
                    else if( curSource->lastTimePos >= 0 )
                    {
                        float           secofs;
                        
                        // For unsynced loops (SRCPRI_ENTITY) just carry on playing as if the sound was never stopped
                        
                        secofs = curSource->lastTimePos + ( Sys_Milliseconds() - curSource->lastSampleTime ) / 1000.0f;
                        secofs = fmodf( secofs, ( float )curSfx->info.samples / curSfx->info.rate );
                        alSourcef( curSource->alSource, AL_SEC_OFFSET, secofs );
                    }
                    
                    curSfx->loopActiveCnt++;
                    
                    alSourcei( curSource->alSource, AL_LOOPING, AL_TRUE );
                    curSource->isPlaying = true;
                    alSourcePlay( curSource->alSource );
                }
                
                // Update locality
                if( curSource->local )
                {
                    alSourcei( curSource->alSource, AL_SOURCE_RELATIVE, AL_TRUE );
                    alSourcef( curSource->alSource, AL_ROLLOFF_FACTOR, 0.0f );
                }
                else
                {
                    alSourcei( curSource->alSource, AL_SOURCE_RELATIVE, AL_FALSE );
                    alSourcef( curSource->alSource, AL_ROLLOFF_FACTOR, s_alRolloff->value );
                }
                
            }
            else if( curSource->priority == SRCPRI_AMBIENT )
            {
                if( curSource->isPlaying )
                {
                    S_AL_NewLoopMaster( curSource, false );
                    alSourceStop( curSource->alSource );
                    curSource->isPlaying = false;
                }
            }
            else
                S_AL_SrcKill( i );
                
            continue;
        }
        
        // Check if it's done, and flag it
        alGetSourcei( curSource->alSource, AL_SOURCE_STATE, &state );
        if( state == AL_STOPPED )
        {
            curSource->isPlaying = false;
            S_AL_SrcKill( i );
            continue;
        }
        
        // Query relativity of source, don't move if it's true
        alGetSourcei( curSource->alSource, AL_SOURCE_RELATIVE, &state );
        
        // See if it needs to be moved
        if( curSource->isTracking && !state )
        {
            alSourcefv( curSource->alSource, AL_POSITION, entityList[entityNum].origin );
            S_AL_ScaleGain( curSource, entityList[entityNum].origin );
        }
    }
}

/*
=================
S_AL_SrcShutup
=================
*/
static void S_AL_SrcShutup( void )
{
    S32 i;
    
    for( i = 0; i < srcCount; i++ )
        S_AL_SrcKill( i );
}

/*
=================
S_AL_SrcGet
=================
*/
static ALuint S_AL_SrcGet( srcHandle_t src )
{
    return srcList[src].alSource;
}


//===========================================================================

static srcHandle_t streamSourceHandles[MAX_RAW_STREAMS];
static bool  streamPlaying[MAX_RAW_STREAMS];
static ALuint   streamSources[MAX_RAW_STREAMS];

/*
=================
S_AL_AllocateStreamChannel
=================
*/
static void S_AL_AllocateStreamChannel( S32 stream )
{
    if( ( stream < 0 ) || ( stream >= MAX_RAW_STREAMS ) )
        return;
        
    // Allocate a streamSource at high priority
    streamSourceHandles[stream] = S_AL_SrcAlloc( SRCPRI_STREAM, -2, 0 );
    if( streamSourceHandles[stream] == -1 )
        return;
        
    // Lock the streamSource so nobody else can use it, and get the raw streamSource
    S_AL_SrcLock( streamSourceHandles[stream] );
    streamSources[stream] = S_AL_SrcGet( streamSourceHandles[stream] );
    
    // make sure that after unmuting the S_AL_Gain in S_Update() does not turn
    // volume up prematurely for this source
    srcList[streamSourceHandles[stream]].scaleGain = 0.0f;
    
    // Set some streamSource parameters
    alSourcei( streamSources[stream], AL_BUFFER, 0 );
    alSourcei( streamSources[stream], AL_LOOPING, AL_FALSE );
    alSource3f( streamSources[stream], AL_POSITION, 0.0, 0.0, 0.0 );
    alSource3f( streamSources[stream], AL_VELOCITY, 0.0, 0.0, 0.0 );
    alSource3f( streamSources[stream], AL_DIRECTION, 0.0, 0.0, 0.0 );
    alSourcef( streamSources[stream], AL_ROLLOFF_FACTOR, 0.0 );
    alSourcei( streamSources[stream], AL_SOURCE_RELATIVE, AL_TRUE );
}

/*
=================
S_AL_FreeStreamChannel
=================
*/
static void S_AL_FreeStreamChannel( S32 stream )
{
    if( ( stream < 0 ) || ( stream >= MAX_RAW_STREAMS ) )
        return;
        
    // Release the output streamSource
    S_AL_SrcUnlock( streamSourceHandles[stream] );
    streamSources[stream] = 0;
    streamSourceHandles[stream] = -1;
}

/*
=================
S_AL_RawSamples
=================
*/
static void S_AL_RawSamples( S32 stream, S32 samples, S32 rate, S32 width, S32 channels, const U8* data, F32 volume )
{
    U32 buffer, format;
    
    if( ( stream < 0 ) || ( stream >= MAX_RAW_STREAMS ) )
        return;
        
    format = S_AL_Format( width, channels );
    
    // Create the streamSource if necessary
    if( streamSourceHandles[stream] == -1 )
    {
        S_AL_AllocateStreamChannel( stream );
        
        // Failed?
        if( streamSourceHandles[stream] == -1 )
        {
            Com_Printf( S_COLOR_RED "ERROR: Can't allocate streaming streamSource\n" );
            return;
        }
    }
    
    // Create a buffer, and stuff the data into it
    alGenBuffers( 1, &buffer );
    alBufferData( buffer, format, ( ALvoid* ) data, ( samples * width * channels ), rate );
    
    // Shove the data onto the streamSource
    alSourceQueueBuffers( streamSources[stream], 1, &buffer );
    
    // Volume
    S_AL_Gain( streamSources[stream], volume * s_volume->value * s_alGain->value );
}

/*
=================
S_AL_StreamUpdate
=================
*/
static void S_AL_StreamUpdate( S32 stream )
{
    S32 numBuffers, state;
    
    if( ( stream < 0 ) || ( stream >= MAX_RAW_STREAMS ) )
        return;
        
    if( streamSourceHandles[stream] == -1 )
        return;
        
    // Un-queue any buffers, and delete them
    alGetSourcei( streamSources[stream], AL_BUFFERS_PROCESSED, &numBuffers );
    while( numBuffers-- )
    {
        ALuint          buffer;
        
        alSourceUnqueueBuffers( streamSources[stream], 1, &buffer );
        alDeleteBuffers( 1, &buffer );
    }
    
    // Start the streamSource playing if necessary
    alGetSourcei( streamSources[stream], AL_BUFFERS_QUEUED, &numBuffers );
    
    alGetSourcei( streamSources[stream], AL_SOURCE_STATE, &state );
    if( state == AL_STOPPED )
    {
        streamPlaying[stream] = false;
        
        // If there are no buffers queued up, release the streamSource
        if( !numBuffers )
            S_AL_FreeStreamChannel( stream );
    }
    
    if( !streamPlaying[stream] && numBuffers )
    {
        alSourcePlay( streamSources[stream] );
        streamPlaying[stream] = true;
    }
}

/*
=================
S_AL_StreamDie
=================
*/
static void S_AL_StreamDie( S32 stream )
{
    S32 numBuffers;
    
    if( ( stream < 0 ) || ( stream >= MAX_RAW_STREAMS ) )
        return;
        
    if( streamSourceHandles[stream] == -1 )
        return;
        
    streamPlaying[stream] = false;
    alSourceStop( streamSources[stream] );
    
    // Un-queue any buffers, and delete them
    alGetSourcei( streamSources[stream], AL_BUFFERS_PROCESSED, &numBuffers );
    while( numBuffers-- )
    {
        ALuint          buffer;
        
        alSourceUnqueueBuffers( streamSources[stream], 1, &buffer );
        alDeleteBuffers( 1, &buffer );
    }
    
    S_AL_FreeStreamChannel( stream );
}


//===========================================================================


#define NUM_MUSIC_BUFFERS	4
#define	MUSIC_BUFFER_SIZE 4096

static bool musicPlaying = false;
static srcHandle_t musicSourceHandle = -1;
static U32 musicSource;
static U32 musicBuffers[NUM_MUSIC_BUFFERS];

static snd_stream_t* mus_stream;
static snd_stream_t* intro_stream;
static UTF8 s_backgroundLoop[MAX_QPATH];

static U8 decode_buffer[MUSIC_BUFFER_SIZE];

/*
=================
S_AL_MusicSourceGet
=================
*/
static void S_AL_MusicSourceGet( void )
{
    // Allocate a musicSource at high priority
    musicSourceHandle = S_AL_SrcAlloc( SRCPRI_STREAM, -2, 0 );
    if( musicSourceHandle == -1 )
        return;
        
    // Lock the musicSource so nobody else can use it, and get the raw musicSource
    S_AL_SrcLock( musicSourceHandle );
    musicSource = S_AL_SrcGet( musicSourceHandle );
    
    // make sure that after unmuting the S_AL_Gain in S_Update() does not turn
    // volume up prematurely for this source
    srcList[musicSourceHandle].scaleGain = 0.0f;
    
    // Set some musicSource parameters
    alSource3f( musicSource, AL_POSITION, 0.0, 0.0, 0.0 );
    alSource3f( musicSource, AL_VELOCITY, 0.0, 0.0, 0.0 );
    alSource3f( musicSource, AL_DIRECTION, 0.0, 0.0, 0.0 );
    alSourcef( musicSource, AL_ROLLOFF_FACTOR, 0.0 );
    alSourcei( musicSource, AL_SOURCE_RELATIVE, AL_TRUE );
}

/*
=================
S_AL_MusicSourceFree
=================
*/
static void S_AL_MusicSourceFree( void )
{
    // Release the output musicSource
    S_AL_SrcUnlock( musicSourceHandle );
    musicSource = 0;
    musicSourceHandle = -1;
}

/*
=================
S_AL_CloseMusicFiles
=================
*/
static void S_AL_CloseMusicFiles( void )
{
    if( intro_stream )
    {
        S_CodecCloseStream( intro_stream );
        intro_stream = NULL;
    }
    
    if( mus_stream )
    {
        S_CodecCloseStream( mus_stream );
        mus_stream = NULL;
    }
}

/*
=================
S_AL_StopBackgroundTrack
=================
*/
static void S_AL_StopBackgroundTrack( void )
{
    if( !musicPlaying )
        return;
        
    // Stop playing
    alSourceStop( musicSource );
    
    // De-queue the musicBuffers
    alSourceUnqueueBuffers( musicSource, NUM_MUSIC_BUFFERS, musicBuffers );
    
    // Destroy the musicBuffers
    alDeleteBuffers( NUM_MUSIC_BUFFERS, musicBuffers );
    
    // Free the musicSource
    S_AL_MusicSourceFree();
    
    // Unload the stream
    S_AL_CloseMusicFiles();
    
    musicPlaying = false;
}

/*
=================
S_AL_MusicProcess
=================
*/
static void S_AL_MusicProcess( ALuint b )
{
    S32 error, l;
    U32 format;
    snd_stream_t*   curstream;
    
    S_AL_ClearError( false );
    
    if( intro_stream )
        curstream = intro_stream;
    else
        curstream = mus_stream;
        
    if( !curstream )
        return;
        
    l = S_CodecReadStream( curstream, MUSIC_BUFFER_SIZE, decode_buffer );
    
    // Run out data to read, start at the beginning again
    if( l == 0 )
    {
        S_CodecCloseStream( curstream );
        
        // the intro stream just finished playing so we don't need to reopen
        // the music stream.
        if( intro_stream )
            intro_stream = NULL;
        else
            mus_stream = S_CodecOpenStream( s_backgroundLoop );
            
        curstream = mus_stream;
        
        if( !curstream )
        {
            S_AL_StopBackgroundTrack();
            return;
        }
        
        l = S_CodecReadStream( curstream, MUSIC_BUFFER_SIZE, decode_buffer );
    }
    
    format = S_AL_Format( curstream->info.width, curstream->info.channels );
    
    if( l == 0 )
    {
        // We have no data to buffer, so buffer silence
        U8 dummyData[2] = { 0 };
        
        alBufferData( b, AL_FORMAT_MONO16, ( void* )dummyData, 2, 22050 );
    }
    else
        alBufferData( b, format, decode_buffer, l, curstream->info.rate );
        
    if( ( error = alGetError() ) != AL_NO_ERROR )
    {
        S_AL_StopBackgroundTrack();
        Com_Printf( S_COLOR_RED "ERROR: while buffering data for music stream - %s\n", S_AL_ErrorMsg( error ) );
        return;
    }
}

/*
=================
S_AL_StartBackgroundTrack
=================
*/
static void S_AL_StartBackgroundTrack( StringEntry intro, StringEntry loop )
{
    S32 i;
    bool issame;
    
    // Stop any existing music that might be playing
    S_AL_StopBackgroundTrack();
    
    if( ( !intro || !*intro ) && ( !loop || !*loop ) )
        return;
        
    // Allocate a musicSource
    S_AL_MusicSourceGet();
    if( musicSourceHandle == -1 )
        return;
        
    if( !loop || !*loop )
    {
        loop = intro;
        issame = true;
    }
    else if( intro && *intro && !strcmp( intro, loop ) )
        issame = true;
    else
        issame = false;
        
    // Copy the loop over
    strncpy( s_backgroundLoop, loop, sizeof( s_backgroundLoop ) );
    
    if( !issame )
    {
        // Open the intro and don't mind whether it succeeds.
        // The important part is the loop.
        intro_stream = S_CodecOpenStream( intro );
    }
    else
        intro_stream = NULL;
        
    mus_stream = S_CodecOpenStream( s_backgroundLoop );
    if( !mus_stream )
    {
        S_AL_CloseMusicFiles();
        S_AL_MusicSourceFree();
        return;
    }
    
    // Generate the musicBuffers
    alGenBuffers( NUM_MUSIC_BUFFERS, musicBuffers );
    
    // Queue the musicBuffers up
    for( i = 0; i < NUM_MUSIC_BUFFERS; i++ )
    {
        S_AL_MusicProcess( musicBuffers[i] );
    }
    
    alSourceQueueBuffers( musicSource, NUM_MUSIC_BUFFERS, musicBuffers );
    
    // Set the initial gain property
    S_AL_Gain( musicSource, s_alGain->value * s_musicVolume->value );
    
    // Start playing
    alSourcePlay( musicSource );
    
    musicPlaying = true;
}

/*
=================
S_AL_MusicUpdate
=================
*/
static void S_AL_MusicUpdate( void )
{
    S32 numBuffers, state;
    
    if( !musicPlaying )
        return;
        
    alGetSourcei( musicSource, AL_BUFFERS_PROCESSED, &numBuffers );
    while( numBuffers-- )
    {
        ALuint          b;
        
        alSourceUnqueueBuffers( musicSource, 1, &b );
        S_AL_MusicProcess( b );
        alSourceQueueBuffers( musicSource, 1, &b );
    }
    
    // Hitches can cause OpenAL to be starved of buffers when streaming.
    // If this happens, it will stop playback. This restarts the source if
    // it is no longer playing, and if there are buffers available
    alGetSourcei( musicSource, AL_SOURCE_STATE, &state );
    alGetSourcei( musicSource, AL_BUFFERS_QUEUED, &numBuffers );
    if( state == AL_STOPPED && numBuffers )
    {
        Com_DPrintf( S_COLOR_YELLOW "Restarted OpenAL music\n" );
        alSourcePlay( musicSource );
    }
    
    // Set the gain property
    S_AL_Gain( musicSource, s_alGain->value * s_musicVolume->value );
}


//===========================================================================


// Local state variables
static ALCdevice* alDevice;
static ALCcontext* alContext;

#ifdef _WIN32
#define ALDRIVER_DEFAULT "OpenAL32.dll"
#elif defined(MACOS_X)
#define ALDRIVER_DEFAULT "/System/Library/Frameworks/OpenAL.framework/OpenAL"
#else
#define ALDRIVER_DEFAULT "libopenal.so.1"
#endif

/*
=================
S_AL_StopAllSounds
=================
*/
static void S_AL_StopAllSounds( void )
{
    S32 i;
    
    S_AL_SrcShutup();
    S_AL_StopBackgroundTrack();
    for( i = 0; i < MAX_RAW_STREAMS; i++ )
        S_AL_StreamDie( i );
}

/*
=================
S_AL_Respatialize
=================
*/
static void S_AL_Respatialize( S32 entityNum, const vec3_t origin, vec3_t axis[3], S32 inwater )
{
    F32 velocity[3] = { 0.0f, 0.0f, 0.0f };
    F32 orientation[6];
    vec3_t sorigin;
    
    VectorCopy( origin, sorigin );
    S_AL_SanitiseVector( sorigin );
    
    S_AL_SanitiseVector( axis[0] );
    S_AL_SanitiseVector( axis[1] );
    S_AL_SanitiseVector( axis[2] );
    
    orientation[0] = axis[0][0];
    orientation[1] = axis[0][1];
    orientation[2] = axis[0][2];
    orientation[3] = axis[2][0];
    orientation[4] = axis[2][1];
    orientation[5] = axis[2][2];
    
    VectorCopy( sorigin, lastListenerOrigin );
    
    // Set OpenAL listener paramaters
    alListenerfv( AL_POSITION, ( ALfloat* ) sorigin );
    alListenerfv( AL_VELOCITY, velocity );
    alListenerfv( AL_ORIENTATION, orientation );
}

/*
=================
S_AL_Update
=================
*/
static void S_AL_Update( void )
{
    S32 i;
    
    if( s_muted->modified )
    {
        // muted state changed. Let S_AL_Gain turn up all sources again.
        for( i = 0; i < srcCount; i++ )
        {
            if( srcList[i].isActive )
                S_AL_Gain( srcList[i].alSource, srcList[i].scaleGain );
        }
        
        s_muted->modified = false;
    }
    
    // Update SFX channels
    S_AL_SrcUpdate();
    
    // Update streams
    for( i = 0; i < MAX_RAW_STREAMS; i++ )
        S_AL_StreamUpdate( i );
    S_AL_MusicUpdate();
    
    // Doppler
    if( s_doppler->modified )
    {
        s_alDopplerFactor->modified = true;
        s_doppler->modified = false;
    }
    
    // Doppler parameters
    if( s_alDopplerFactor->modified )
    {
        if( s_doppler->integer )
            alDopplerFactor( s_alDopplerFactor->value );
        else
            alDopplerFactor( 0.0f );
        s_alDopplerFactor->modified = false;
    }
    if( s_alDopplerSpeed->modified )
    {
        alDopplerVelocity( s_alDopplerSpeed->value );
        s_alDopplerSpeed->modified = false;
    }
    
    // Clear the modified flags on the other cvars
    s_alGain->modified = false;
    s_volume->modified = false;
    s_musicVolume->modified = false;
    s_alMinDistance->modified = false;
    s_alRolloff->modified = false;
}

/*
=================
S_AL_DisableSounds
=================
*/
static void S_AL_DisableSounds( void )
{
    S_AL_StopAllSounds();
}

/*
=================
S_AL_BeginRegistration
=================
*/
static void S_AL_BeginRegistration( void )
{
    if( !numSfx )
        S_AL_BufferInit();
}

/*
=================
S_AL_ClearSoundBuffer
=================
*/
static void S_AL_ClearSoundBuffer( void )
{
    S_AL_SrcShutdown();
    S_AL_SrcInit();
}

/*
=================
S_AL_SoundList
=================
*/
static void S_AL_SoundList( void )
{
}

/*
=================
S_AL_SoundInfo
=================
*/
static void S_AL_SoundInfo( void )
{
    Com_Printf( "OpenAL info:\n" );
    Com_Printf( "  Vendor:     %s\n", alGetString( AL_VENDOR ) );
    Com_Printf( "  Version:    %s\n", alGetString( AL_VERSION ) );
    Com_Printf( "  Renderer:   %s\n", alGetString( AL_RENDERER ) );
    Com_Printf( "  AL Extensions: %s\n", alGetString( AL_EXTENSIONS ) );
    Com_Printf( "  ALC Extensions: %s\n", alcGetString( alDevice, ALC_EXTENSIONS ) );
    if( alcIsExtensionPresent( NULL, "ALC_ENUMERATION_EXT" ) )
    {
        Com_Printf( "  Device:     %s\n", alcGetString( alDevice, ALC_DEVICE_SPECIFIER ) );
        Com_Printf( "Available Devices:\n%s", s_alAvailableDevices->string );
    }
}

/*
=================
S_AL_Shutdown
=================
*/
static void S_AL_Shutdown( void )
{
    // Shut down everything
    S32 i;
    
    for( i = 0; i < MAX_RAW_STREAMS; i++ )
        S_AL_StreamDie( i );
    S_AL_StopBackgroundTrack();
    S_AL_SrcShutdown();
    S_AL_BufferShutdown();
    
    alcDestroyContext( alContext );
    alcCloseDevice( alDevice );
    
    for( i = 0; i < MAX_RAW_STREAMS; i++ )
    {
        streamSourceHandles[i] = -1;
        streamPlaying[i] = false;
        streamSources[i] = 0;
    }
}

/*
=================
S_AL_Init
=================
*/
bool S_AL_Init( soundInterface_t* si )
{
    StringEntry device = NULL;
    S32 i;
    
    if( !si )
    {
        return false;
    }
    
    for( i = 0; i < MAX_RAW_STREAMS; i++ )
    {
        streamSourceHandles[i] = -1;
        streamPlaying[i] = false;
        streamSources[i] = 0;
    }
    
    // New console variables
    s_alPrecache = cvarSystem->Get( "s_alPrecache", "1", CVAR_ARCHIVE );
    s_alGain = cvarSystem->Get( "s_alGain", "1.0", CVAR_ARCHIVE );
    s_alSources = cvarSystem->Get( "s_alSources", "96", CVAR_ARCHIVE );
    s_alDopplerFactor = cvarSystem->Get( "s_alDopplerFactor", "1.0", CVAR_ARCHIVE );
    s_alDopplerSpeed = cvarSystem->Get( "s_alDopplerSpeed", "2200", CVAR_ARCHIVE );
    s_alMinDistance = cvarSystem->Get( "s_alMinDistance", "120", CVAR_CHEAT );
    s_alMaxDistance = cvarSystem->Get( "s_alMaxDistance", "1024", CVAR_CHEAT );
    s_alRolloff = cvarSystem->Get( "s_alRolloff", "2", CVAR_CHEAT );
    s_alGraceDistance = cvarSystem->Get( "s_alGraceDistance", "512", CVAR_CHEAT );
    
    s_alDriver = cvarSystem->Get( "s_alDriver", ALDRIVER_DEFAULT, CVAR_ARCHIVE | CVAR_LATCH );
    
    s_alDevice = cvarSystem->Get( "s_alDevice", "", CVAR_ARCHIVE | CVAR_LATCH );
    
    // Load QAL
    if( !QAL_Init( s_alDriver->string ) )
    {
        Com_Printf( "Failed to load library: \"%s\".\n", s_alDriver->string );
        return false;
    }
    
    device = s_alDevice->string;
    if( device && !*device )
        device = NULL;
        
    // Device enumeration support (extension is implemented reasonably only on Windows right now).
    if( alcIsExtensionPresent( NULL, "ALC_ENUMERATION_EXT" ) )
    {
        UTF8            devicenames[1024] = "";
        StringEntry     devicelist;
        StringEntry     defaultdevice;
        S32             curlen;
        
        // get all available devices + the default device name.
        devicelist = alcGetString( NULL, ALC_DEVICE_SPECIFIER );
        defaultdevice = alcGetString( NULL, ALC_DEFAULT_DEVICE_SPECIFIER );
        
#ifdef _WIN32
        // check whether the default device is generic hardware. If it is, change to
        // Generic Software as that one works more reliably with various sound systems.
        // If it's not, use OpenAL's default selection as we don't want to ignore
        // native hardware acceleration.
        if( !device && !strcmp( defaultdevice, "Generic Hardware" ) )
            device = "Generic Software";
#endif
            
        // dump a list of available devices to a cvar for the user to see.
        while( ( curlen = strlen( devicelist ) ) )
        {
            Q_strcat( devicenames, sizeof( devicenames ), devicelist );
            Q_strcat( devicenames, sizeof( devicenames ), "\n" );
            
            devicelist += curlen + 1;
        }
        
        s_alAvailableDevices = cvarSystem->Get( "s_alAvailableDevices", devicenames, CVAR_ROM | CVAR_NORESTART );
    }
    
    alDevice = alcOpenDevice( device );
    if( !alDevice && device )
    {
        Com_Printf( "Failed to open OpenAL device '%s', trying default.\n", device );
        alDevice = alcOpenDevice( NULL );
    }
    
    if( !alDevice )
    {
        Com_Printf( "Failed to open OpenAL device.\n" );
        return false;
    }
    
    // Create OpenAL context
    alContext = alcCreateContext( alDevice, NULL );
    if( !alContext )
    {
        alcCloseDevice( alDevice );
        Com_Printf( "Failed to create OpenAL context.\n" );
        return false;
    }
    alcMakeContextCurrent( alContext );
    
    // Initialize sources, buffers, music
    S_AL_BufferInit();
    S_AL_SrcInit();
    
    // Set up OpenAL parameters (doppler, etc)
    alDistanceModel( AL_INVERSE_DISTANCE_CLAMPED );
    alDopplerFactor( s_alDopplerFactor->value );
    alDopplerVelocity( s_alDopplerSpeed->value );
    
    si->Shutdown = S_AL_Shutdown;
    si->StartSound = S_AL_StartSound;
    si->StartLocalSound = S_AL_StartLocalSound;
    si->StartBackgroundTrack = S_AL_StartBackgroundTrack;
    si->StopBackgroundTrack = S_AL_StopBackgroundTrack;
    si->RawSamples = S_AL_RawSamples;
    si->StopAllSounds = S_AL_StopAllSounds;
    si->ClearLoopingSounds = S_AL_ClearLoopingSounds;
    si->AddLoopingSound = S_AL_AddLoopingSound;
    si->AddRealLoopingSound = S_AL_AddRealLoopingSound;
    si->StopLoopingSound = S_AL_StopLoopingSound;
    si->Respatialize = S_AL_Respatialize;
    si->UpdateEntityPosition = S_AL_UpdateEntityPosition;
    si->Update = S_AL_Update;
    si->DisableSounds = S_AL_DisableSounds;
    si->BeginRegistration = S_AL_BeginRegistration;
    si->RegisterSound = S_AL_RegisterSound;
    si->ClearSoundBuffer = S_AL_ClearSoundBuffer;
    si->SoundInfo = S_AL_SoundInfo;
    si->SoundList = S_AL_SoundList;
    
    return true;
}
