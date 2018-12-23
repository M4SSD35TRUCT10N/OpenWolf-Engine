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
// File name:   sys_unix.cpp
// Version:     v1.01
// Created:
// Compilers:   Visual Studio 2017, gcc 7.3.0
// Description:
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#include <OWLib/precompiled.h>

#ifdef __LINUX__

#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <pwd.h>
#include <libgen.h>
#include <fcntl.h>
#include <fenv.h>

bool stdinIsATTY;

// Used to determine where to store user-specific files
static UTF8 homePath[ MAX_OSPATH ] = { 0 };

static UTF8 exit_cmdline[MAX_CMD] = "";

/*
==================
Sys_DefaultHomePath
==================
*/
UTF8* Sys_DefaultHomePath( UTF8* buffer, S32 size )
{
    UTF8* p;
    
    if( !*homePath )
    {
        if( ( p = getenv( "HOME" ) ) != NULL )
        {
            Q_strncpyz( buffer, p, size );
#ifdef MACOS_X
            Q_strcat( buffer, size, "/Library/Application Support/" PRODUCT_NAME );
#else
            Q_strcat( buffer, size, "/." PRODUCT_NAME );
#endif
        }
    }
    
    return buffer;
}

#ifndef MACOS_X
/*
================
Sys_TempPath
================
*/
StringEntry Sys_TempPath( void )
{
    StringEntry TMPDIR = getenv( "TMPDIR" );
    
    if( TMPDIR == NULL || TMPDIR[ 0 ] == '\0' )
        return "/tmp";
    else
        return TMPDIR;
}
#endif

/*
==================
chmod OR on a file
==================
*/
void Sys_Chmod( UTF8* file, S32 mode )
{
    struct stat s_buf;
    S32 perm;
    if( stat( file, &s_buf ) != 0 )
    {
        Com_Printf( "stat('%s')  failed: errno %d\n", file, errno );
        return;
    }
    perm = s_buf.st_mode | mode;
    if( chmod( file, perm ) != 0 )
    {
        Com_Printf( "chmod('%s', %d) failed: errno %d\n", file, perm, errno );
    }
    Com_DPrintf( "chmod +%d '%s'\n", mode, file );
}

/*
================
Sys_Milliseconds
================
*/
/* base time in seconds, that's our origin
   timeval:tv_sec is an S32:
   assuming this wraps every 0x7fffffff - ~68 years since the Epoch (1970) - we're safe till 2038
   using unsigned long data type to work right with Sys_XTimeToSysTime */

time_t initial_tv_sec = 0;

/* current time in ms, using sys_timeBase as origin
   NOTE: sys_timeBase*1000 + curtime -> ms since the Epoch
     0x7fffffff ms - ~24 days
   although timeval:tv_usec is an S32, I'm not sure wether it is actually used as an unsigned S32
     (which would affect the wrap period) */
S32 Sys_Milliseconds( void )
{
    struct timeval tp;
    
    gettimeofday( &tp, NULL );
    
    return ( tp.tv_sec - initial_tv_sec ) * 1000 + tp.tv_usec / 1000;
}

/*
==================
Sys_RandomBytes
==================
*/
bool Sys_RandomBytes( U8* string, S32 len )
{
    FILE* fp;
    
    fp = fopen( "/dev/urandom", "r" );
    if( !fp )
        return false;
        
    if( !fread( string, sizeof( U8 ), len, fp ) )
    {
        fclose( fp );
        return false;
    }
    
    fclose( fp );
    return true;
}

/*
==================
Sys_GetCurrentUser
==================
*/
UTF8* Sys_GetCurrentUser( void )
{
    struct passwd* p;
    
    if( ( p = getpwuid( getuid() ) ) == NULL )
    {
        return "player";
    }
    return p->pw_name;
}

/*
==================
Sys_GetClipboardData
==================
*/
#ifndef MACOS_X
UTF8* Sys_GetClipboardData( void )
{
    return NULL;
}
#endif

#define MEM_THRESHOLD 96*1024*1024

/*
==================
Sys_LowPhysicalMemory

TODO
==================
*/
bool Sys_LowPhysicalMemory( void )
{
    return false;
}

/*
==================
Sys_Basename
==================
*/
StringEntry Sys_Basename( UTF8* path )
{
    return basename( path );
}

/*
==================
Sys_Dirname
==================
*/
StringEntry Sys_Dirname( UTF8* path )
{
    return dirname( path );
}

/*
==================
Sys_Mkdir
==================
*/
bool Sys_Mkdir( StringEntry path )
{
    S32 result = mkdir( path, 0750 );
    
    if( result != 0 )
        return errno == EEXIST;
        
    return true;
}

/*
==================
Sys_Cwd
==================
*/
UTF8* Sys_Cwd( void )
{
#ifdef MACOS_X
    UTF8* apppath = Sys_DefaultAppPath();
    
    if( apppath[0] && apppath[0] != '.' )
    {
        return apppath;
    }
#endif
    static UTF8 cwd[MAX_OSPATH];
    
    UTF8* result = getcwd( cwd, sizeof( cwd ) - 1 );
    if( result != cwd )
        return NULL;
        
    cwd[MAX_OSPATH - 1] = 0;
    
    return cwd;
}

/*
==============================================================

DIRECTORY SCANNING

==============================================================
*/

#define MAX_FOUND_FILES 0x1000

/*
==================
Sys_ListFilteredFiles
==================
*/
void Sys_ListFilteredFiles( StringEntry basedir, UTF8* subdirs, UTF8* filter, UTF8** list, S32* numfiles )
{
    UTF8          search[MAX_OSPATH], newsubdirs[MAX_OSPATH];
    UTF8          filename[MAX_OSPATH];
    DIR*           fdir;
    struct dirent* d;
    struct stat   st;
    
    if( *numfiles >= MAX_FOUND_FILES - 1 )
    {
        return;
    }
    
    if( strlen( subdirs ) )
    {
        Com_sprintf( search, sizeof( search ), "%s/%s", basedir, subdirs );
    }
    else
    {
        Com_sprintf( search, sizeof( search ), "%s", basedir );
    }
    
    if( ( fdir = opendir( search ) ) == NULL )
    {
        return;
    }
    
    while( ( d = readdir( fdir ) ) != NULL )
    {
        Com_sprintf( filename, sizeof( filename ), "%s/%s", search, d->d_name );
        if( stat( filename, &st ) == -1 )
            continue;
            
        if( st.st_mode & S_IFDIR )
        {
            if( Q_stricmp( d->d_name, "." ) && Q_stricmp( d->d_name, ".." ) )
            {
                if( strlen( subdirs ) )
                {
                    Com_sprintf( newsubdirs, sizeof( newsubdirs ), "%s/%s", subdirs, d->d_name );
                }
                else
                {
                    Com_sprintf( newsubdirs, sizeof( newsubdirs ), "%s", d->d_name );
                }
                Sys_ListFilteredFiles( basedir, newsubdirs, filter, list, numfiles );
            }
        }
        if( *numfiles >= MAX_FOUND_FILES - 1 )
        {
            break;
        }
        Com_sprintf( filename, sizeof( filename ), "%s/%s", subdirs, d->d_name );
        if( !Com_FilterPath( filter, filename, false ) )
            continue;
        list[ *numfiles ] = CopyString( filename );
        ( *numfiles )++;
    }
    
    closedir( fdir );
}

/*
==================
Sys_ListFiles
==================
*/
UTF8** Sys_ListFiles( StringEntry directory, StringEntry extension, UTF8* filter, S32* numfiles, bool wantsubs )
{
    struct dirent* d;
    DIR*           fdir;
    bool      dironly = wantsubs;
    UTF8          search[MAX_OSPATH];
    S32           nfiles;
    UTF8**          listCopy;
    UTF8*          list[MAX_FOUND_FILES];
    S32           i;
    struct stat   st;
    
    S32           extLen;
    
    if( filter )
    {
    
        nfiles = 0;
        Sys_ListFilteredFiles( directory, "", filter, list, &nfiles );
        
        list[ nfiles ] = NULL;
        *numfiles = nfiles;
        
        if( !nfiles )
            return NULL;
            
        listCopy = Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
        for( i = 0 ; i < nfiles ; i++ )
        {
            listCopy[i] = list[i];
        }
        listCopy[i] = NULL;
        
        return listCopy;
    }
    
    if( !extension )
        extension = "";
        
    if( extension[0] == '/' && extension[1] == 0 )
    {
        extension = "";
        dironly = true;
    }
    
    extLen = strlen( extension );
    
    // search
    nfiles = 0;
    
    if( ( fdir = opendir( directory ) ) == NULL )
    {
        *numfiles = 0;
        return NULL;
    }
    
    while( ( d = readdir( fdir ) ) != NULL )
    {
        Com_sprintf( search, sizeof( search ), "%s/%s", directory, d->d_name );
        if( stat( search, &st ) == -1 )
            continue;
        if( ( dironly && !( st.st_mode & S_IFDIR ) ) ||
                ( !dironly && ( st.st_mode & S_IFDIR ) ) )
            continue;
            
        if( *extension )
        {
            if( strlen( d->d_name ) < strlen( extension ) ||
                    Q_stricmp(
                        d->d_name + strlen( d->d_name ) - strlen( extension ),
                        extension ) )
            {
                continue; // didn't match
            }
        }
        
        if( nfiles == MAX_FOUND_FILES - 1 )
            break;
        list[ nfiles ] = CopyString( d->d_name );
        nfiles++;
    }
    
    list[ nfiles ] = NULL;
    
    closedir( fdir );
    
    // return a copy of the list
    *numfiles = nfiles;
    
    if( !nfiles )
    {
        return NULL;
    }
    
    listCopy = Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
    for( i = 0 ; i < nfiles ; i++ )
    {
        listCopy[i] = list[i];
    }
    listCopy[i] = NULL;
    
    return listCopy;
}

/*
==================
Sys_FreeFileList
==================
*/
void Sys_FreeFileList( UTF8** list )
{
    S32 i;
    
    if( !list )
    {
        return;
    }
    
    for( i = 0 ; list[i] ; i++ )
    {
        Z_Free( list[i] );
    }
    
    Z_Free( list );
}

/*
==================
Sys_Sleep

Block execution for msec or until input is recieved.
==================
*/
void Sys_Sleep( S32 msec )
{
    if( msec == 0 )
        return;
        
    if( stdinIsATTY )
    {
        fd_set fdset;
        
        FD_ZERO( &fdset );
        FD_SET( STDIN_FILENO, &fdset );
        if( msec < 0 )
        {
            select( STDIN_FILENO + 1, &fdset, NULL, NULL, NULL );
        }
        else
        {
            struct timeval timeout;
            
            timeout.tv_sec = msec / 1000;
            timeout.tv_usec = ( msec % 1000 ) * 1000;
            select( STDIN_FILENO + 1, &fdset, NULL, NULL, &timeout );
        }
    }
    else
    {
        // With nothing to select() on, we can't wait indefinitely
        if( msec < 0 )
            msec = 10;
            
        usleep( msec * 1000 );
    }
}

/*
==============
Sys_ErrorDialog

Display an error message
==============
*/
void Sys_ErrorDialog( StringEntry error )
{
    UTF8 buffer[ 1024 ];
    U32 size;
    S32 f = -1;
    StringEntry homepath = cvarSystem->VariableString( "fs_homepath" );
    StringEntry gamedir = cvarSystem->VariableString( "fs_gamedir" );
    StringEntry fileName = "crashlog.txt";
    UTF8* ospath = fileSystem->BuildOSPath( homepath, gamedir, fileName );
    
    Sys_Print( va( "%s\n", error ) );
    
#ifndef DEDICATED
    Sys_Dialog( DT_ERROR, va( "%s. See \"%s\" for details.", error, ospath ), "Error" );
#endif
    
    // Make sure the write path for the crashlog exists...
    if( fileSystem->CreatePath( ospath ) )
    {
        Com_Printf( "ERROR: couldn't create path '%s' for crash log.\n", ospath );
        return;
    }
    
    // We might be crashing because we maxed out the Quake MAX_FILE_HANDLES,
    // which will come through here, so we don't want to recurse forever by
    // calling fileSystem->FOpenFileWrite()...use the Unix system APIs instead.
    f = open( ospath, O_CREAT | O_TRUNC | O_WRONLY, 0640 );
    if( f == -1 )
    {
        Com_Printf( "ERROR: couldn't open %s\n", fileName );
        return;
    }
    
    // We're crashing, so we don't care much if write() or close() fails.
    while( ( size = CON_LogRead( buffer, sizeof( buffer ) ) ) > 0 )
    {
        if( write( f, buffer, size ) != size )
        {
            Com_Printf( "ERROR: couldn't fully write to %s\n", fileName );
            break;
        }
    }
    
    close( f );
}

#ifndef MACOS_X
/*
==============
Sys_ZenityCommand
==============
*/
static S32 Sys_ZenityCommand( dialogType_t type, StringEntry message, StringEntry title )
{
    StringEntry options = "";
    UTF8       command[ 1024 ];
    
    switch( type )
    {
        default:
        case DT_INFO:
            options = "--info";
            break;
        case DT_WARNING:
            options = "--warning";
            break;
        case DT_ERROR:
            options = "--error";
            break;
        case DT_YES_NO:
            options = "--question --ok-label=\"Yes\" --cancel-label=\"No\"";
            break;
        case DT_OK_CANCEL:
            options = "--question --ok-label=\"OK\" --cancel-label=\"Cancel\"";
            break;
    }
    
    Com_sprintf( command, sizeof( command ), "zenity %s --text=\"%s\" --title=\"%s\"",
                 options, message, title );
                 
    return system( command );
}

/*
==============
Sys_KdialogCommand
==============
*/
static S32 Sys_KdialogCommand( dialogType_t type, StringEntry message, StringEntry title )
{
    StringEntry options = "";
    UTF8       command[ 1024 ];
    
    switch( type )
    {
        default:
        case DT_INFO:
            options = "--msgbox";
            break;
        case DT_WARNING:
            options = "--sorry";
            break;
        case DT_ERROR:
            options = "--error";
            break;
        case DT_YES_NO:
            options = "--warningyesno";
            break;
        case DT_OK_CANCEL:
            options = "--warningcontinuecancel";
            break;
    }
    
    Com_sprintf( command, sizeof( command ), "kdialog %s \"%s\" --title \"%s\"",
                 options, message, title );
                 
    return system( command );
}

/*
==============
Sys_XmessageCommand
==============
*/
static S32 Sys_XmessageCommand( dialogType_t type, StringEntry message, StringEntry title )
{
    StringEntry options = "";
    UTF8       command[ 1024 ];
    
    switch( type )
    {
        default:
            options = "-buttons OK";
            break;
        case DT_YES_NO:
            options = "-buttons Yes:0,No:1";
            break;
        case DT_OK_CANCEL:
            options = "-buttons OK:0,Cancel:1";
            break;
    }
    
    Com_sprintf( command, sizeof( command ), "xmessage -center %s \"%s\"",
                 options, message );
                 
    return system( command );
}

/*
==============
Sys_Dialog

Display a *nix dialog box
==============
*/
dialogResult_t Sys_Dialog( dialogType_t type, StringEntry message, StringEntry title )
{
    typedef enum
    {
        NONE = 0,
        ZENITY,
        KDIALOG,
        XMESSAGE,
        NUM_DIALOG_PROGRAMS
    } dialogCommandType_t;
    typedef S32( *dialogCommandBuilder_t )( dialogType_t, StringEntry, StringEntry );
    
    StringEntry              session = getenv( "DESKTOP_SESSION" );
    bool                tried[ NUM_DIALOG_PROGRAMS ] = { false };
    dialogCommandBuilder_t  commands[ NUM_DIALOG_PROGRAMS ] = { NULL };
    dialogCommandType_t     preferredCommandType = NONE;
    
    commands[ ZENITY ] = &Sys_ZenityCommand;
    commands[ KDIALOG ] = &Sys_KdialogCommand;
    commands[ XMESSAGE ] = &Sys_XmessageCommand;
    
    // This may not be the best way
    if( !Q_stricmp( session, "gnome" ) )
        preferredCommandType = ZENITY;
    else if( !Q_stricmp( session, "kde" ) )
        preferredCommandType = KDIALOG;
        
    while( 1 )
    {
        S32 i;
        S32 exitCode;
        
        for( i = NONE + 1; i < NUM_DIALOG_PROGRAMS; i++ )
        {
            if( preferredCommandType != NONE && preferredCommandType != i )
                continue;
                
            if( !tried[ i ] )
            {
                exitCode = commands[ i ]( type, message, title );
                
                if( exitCode >= 0 )
                {
                    switch( type )
                    {
                        case DT_YES_NO:
                            return exitCode ? DR_NO : DR_YES;
                        case DT_OK_CANCEL:
                            return exitCode ? DR_CANCEL : DR_OK;
                        default:
                            return DR_OK;
                    }
                }
                
                tried[ i ] = true;
                
                // The preference failed, so start again in order
                if( preferredCommandType != NONE )
                {
                    preferredCommandType = NONE;
                    break;
                }
            }
        }
        
        for( i = NONE + 1; i < NUM_DIALOG_PROGRAMS; i++ )
        {
            if( !tried[ i ] )
                continue;
        }
        
        break;
    }
    
    Com_DPrintf( S_COLOR_YELLOW "WARNING: failed to show a dialog\n" );
    return DR_OK;
}
#endif

/*
==================
Sys_DoStartProcess
actually forks and starts a process

UGLY HACK:
  Sys_StartProcess works with a command line only
  if this command line is actually a binary with command line parameters,
  the only way to do this on unix is to use a system() call
  but system doesn't replace the current process, which leads to a situation like:
  wolf.x86--spawned_process.x86
  in the case of auto-update for instance, this will cause write access denied on wolf.x86:
  wolf-beta/2002-March/000079.html
  we hack the implementation here, if there are no space in the command line, assume it's a straight process and use execl
  otherwise, use system ..
  The clean solution would be Sys_StartProcess and Sys_StartProcess_Args..
==================
*/
void Sys_DoStartProcess( UTF8* cmdline )
{
    switch( fork() )
    {
        case - 1:
            // main thread
            break;
        case 0:
            if( strchr( cmdline, ' ' ) )
            {
                system( cmdline );
            }
            else
            {
                execl( cmdline, cmdline, NULL );
                printf( "execl failed: %s\n", strerror( errno ) );
            }
            _exit( 0 );
            break;
    }
}

/*
==================
Sys_StartProcess
if !doexit, start the process asap
otherwise, push it for execution at exit
(i.e. let complete shutdown of the game and freeing of resources happen)
NOTE: might even want to add a small delay?
==================
*/
void Sys_StartProcess( UTF8* cmdline, bool doexit )
{

    if( doexit )
    {
        Com_DPrintf( "Sys_StartProcess %s (delaying to final exit)\n", cmdline );
        Q_strncpyz( exit_cmdline, cmdline, MAX_CMD );
        Cbuf_ExecuteText( EXEC_APPEND, "quit\n" );
        return;
    }
    
    Com_DPrintf( "Sys_StartProcess %s\n", cmdline );
    Sys_DoStartProcess( cmdline );
}

/*
=================
Sys_OpenURL
=================
*/
void Sys_OpenURL( StringEntry url, bool doexit )
{
    UTF8* basepath, *homepath, *pwdpath;
    UTF8 fname[20];
    UTF8 fn[MAX_OSPATH];
    UTF8 cmdline[MAX_CMD];
    
    static bool doexit_spamguard = false;
    
    if( doexit_spamguard )
    {
        Com_DPrintf( "Sys_OpenURL: already in a doexit sequence, ignoring %s\n", url );
        return;
    }
    
    Com_Printf( "Open URL: %s\n", url );
    // opening an URL on *nix can mean a lot of things ..
    // just spawn a script instead of deciding for the user :-)
    
    // do the setup before we fork
    // search for an openurl.sh script
    // search procedure taken from Sys_LoadDll
    Q_strncpyz( fname, "openurl.sh", 20 );
    
    pwdpath = Sys_Cwd();
    Com_sprintf( fn, MAX_OSPATH, "%s/%s", pwdpath, fname );
    if( access( fn, X_OK ) == -1 )
    {
        Com_DPrintf( "%s not found\n", fn );
        // try in home path
        homepath = cvarSystem->VariableString( "fs_homepath" );
        Com_sprintf( fn, MAX_OSPATH, "%s/%s", homepath, fname );
        if( access( fn, X_OK ) == -1 )
        {
            Com_DPrintf( "%s not found\n", fn );
            // basepath, last resort
            basepath = cvarSystem->VariableString( "fs_basepath" );
            Com_sprintf( fn, MAX_OSPATH, "%s/%s", basepath, fname );
            if( access( fn, X_OK ) == -1 )
            {
                Com_DPrintf( "%s not found\n", fn );
                Com_Printf( "Can't find script '%s' to open requested URL (use +set developer 1 for more verbosity)\n", fname );
                // we won't quit
                return;
            }
        }
    }
    
    // show_bug.cgi?id=612
    if( doexit )
    {
        doexit_spamguard = true;
    }
    
    Com_DPrintf( "URL script: %s\n", fn );
    
    // build the command line
    Com_sprintf( cmdline, MAX_CMD, "%s '%s' &", fn, url );
    
    Sys_StartProcess( cmdline, doexit );
    
}

// Dushan
bool Sys_OpenUrl( StringEntry url )
{
    UTF8* browser = getenv( "BROWSER" );
    UTF8* kde_session = getenv( "KDE_FULL_SESSION" );
    UTF8* gnome_session = getenv( "GNOME_DESKTOP_SESSION_ID" );
    //Try to use xdg-open, if not, try default, then kde, gnome
    if( browser )
    {
        Sys_Fork( browser, url );
        return true;
    }
    else if( kde_session && Q_stricmp( "true", kde_session ) == 0 )
    {
        Sys_Fork( "konqueror", url );
        return true;
    }
    else if( gnome_session )
    {
        Sys_Fork( "gnome-open", url );
        return true;
    }
    else
    {
        Sys_Fork( "/usr/bin/firefox", url );
    }
    // open url somehow
    return true;
}

bool Sys_Fork( StringEntry path, StringEntry cmdLine )
{
    S32 pid;
    
    pid = fork();
    if( pid == 0 )
    {
        struct stat filestat;
        
        //Try to set the executable bit
        if( stat( path, &filestat ) == 0 )
        {
            chmod( path, filestat.st_mode | S_IXUSR );
        }
        execlp( path, path, cmdLine, NULL );
        printf( "Exec Failed for: %s\n", path );
        _exit( 255 );
    }
    else if( pid == -1 )
    {
        return false;
    }
    return true;
}

/*
==============
Sys_GLimpSafeInit

Unix specific "safe" GL implementation initialisation
==============
*/
void Sys_GLimpSafeInit( void )
{
    // NOP
}

/*
==============
Sys_GLimpInit

Unix specific GL implementation initialisation
==============
*/
void Sys_GLimpInit( void )
{
    // NOP
}

void Sys_SetFloatEnv( void )
{
    // rounding towards 0
    fesetround( FE_TOWARDZERO );
}

/*
==============
Sys_PlatformInit

Unix specific initialisation
==============
*/
void Sys_PlatformInit( void )
{
    StringEntry term = getenv( "TERM" );
    
    signal( SIGHUP, Sys_SigHandler );
    signal( SIGQUIT, Sys_SigHandler );
    signal( SIGTRAP, Sys_SigHandler );
    signal( SIGIOT, Sys_SigHandler );
    signal( SIGBUS, Sys_SigHandler );
    
    stdinIsATTY = isatty( STDIN_FILENO ) &&
                  !( term && ( !strcmp( term, "raw" ) || !strcmp( term, "dumb" ) ) );
}

/*
==============
Sys_SetEnv

set/unset environment variables (empty value removes it)
==============
*/

void Sys_SetEnv( StringEntry name, StringEntry value )
{
    if( value && *value )
        setenv( name, value, 1 );
    else
        unsetenv( name );
}

/*
==============
Sys_PID
==============
*/
S32 Sys_PID( void )
{
    return getpid( );
}

/*
==============
Sys_PIDIsRunning
==============
*/
bool Sys_PIDIsRunning( S32 pid )
{
    return kill( pid, 0 ) == 0;
}

/*
==============
Sys_IsNumLockDown
==============
*/
bool Sys_IsNumLockDown( void )
{
    // Dushan : FIX ME for Linux
    return false;
}

#ifdef MACOS_X
/*
=================
Sys_StripAppBundle

Discovers if passed dir is suffixed with the directory structure of a Mac OS X
.app bundle. If it is, the .app directory structure is stripped off the end and
the result is returned. If not, dir is returned untouched.
=================
*/
UTF8* Sys_StripAppBundle( UTF8* dir )
{
    static UTF8 cwd[MAX_OSPATH];
    
    Q_strncpyz( cwd, dir, sizeof( cwd ) );
    if( strcmp( Sys_Basename( cwd ), "MacOS" ) )
        return dir;
    Q_strncpyz( cwd, Sys_Dirname( cwd ), sizeof( cwd ) );
    if( strcmp( Sys_Basename( cwd ), "Contents" ) )
        return dir;
    Q_strncpyz( cwd, Sys_Dirname( cwd ), sizeof( cwd ) );
    if( !strstr( Sys_Basename( cwd ), ".app" ) )
        return dir;
    Q_strncpyz( cwd, Sys_Dirname( cwd ), sizeof( cwd ) );
    return cwd;
}
#endif // MACOS_X

/*
================
Sys_SteamInit
================
*/
void Sys_SteamInit( void )
{
}

/*
================
Sys_SteamShutdown
================
*/
void Sys_SteamShutdown( void )
{
}

#endif
