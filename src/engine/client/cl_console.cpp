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
// File name:   cl_console.cpp
// Version:     v1.00
// Created:
// Compilers:   Visual Studio 2015
// Description:
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#include <OWLib/precompiled.h>

S32 g_console_field_width = 78;

#define CONSOLE_COLOR '7'
#define DEFAULT_CONSOLE_WIDTH   78
#define NUM_CON_TIMES 4
#define CON_TEXTSIZE 65536

typedef struct
{
    bool        initialized;
    
    S16         text[CON_TEXTSIZE];
    S32         current;	// line where next message will be printed
    S32         x;			// offset in current line for next print
    S32         display;	// bottom of console displays this line
    
    S32         linewidth;	// characters across screen
    S32         totallines;	// total lines in console scrollback
    
    F32         xadjust;	// for wide aspect screens
    
    F32         displayFrac;	// aproaches finalFrac at scr_conspeed
    F32         finalFrac;	// 0.0 to 1.0 lines of console to display
    F32         desiredFrac;	// ydnar: for variable console heights
    
    S32         vislines;	// in scanlines
    
    S32         times[NUM_CON_TIMES];	// cls.realtime time the line was generated
    // for transparent notify lines
    vec4_t      color;
    
    S32          acLength;	// Arnout: autocomplete buffer length
} console_t;

console_t       con;

cvar_t*         con_debug;
cvar_t*         con_conspeed;
cvar_t*         con_notifytime;
cvar_t*         con_autoclear;

// Color and alpha for console
cvar_t*		scr_conUseShader;

cvar_t*		scr_conColorAlpha;
cvar_t*		scr_conColorRed;
cvar_t*		scr_conColorBlue;
cvar_t*		scr_conColorGreen;

// Color and alpha for bar under console
cvar_t*		scr_conBarHeight;

cvar_t*		scr_conBarColorAlpha;
cvar_t*		scr_conBarColorRed;
cvar_t*		scr_conBarColorBlue;
cvar_t*		scr_conBarColorGreen;

cvar_t*		scr_conUseOld;
cvar_t*		scr_conBarSize;
cvar_t*		scr_conHeight;

// DHM - Nerve :: Must hold CTRL + SHIFT + ~ to get console
cvar_t*         con_restricted;

vec4_t          console_highlightcolor = { 0.5, 0.5, 0.2, 0.45 };


/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f( void )
{
    con.acLength = 0;
    
    if( con_restricted->integer && ( !keys[K_CTRL].down || !keys[K_SHIFT].down ) )
    {
        return;
    }
    
    // ydnar: persistent console input is more useful
    // Arnout: added cvar
    if( con_autoclear->integer )
    {
        Field_Clear( &g_consoleField );
    }
    
    g_consoleField.widthInChars = g_console_field_width;
    
    Con_ClearNotify();
    
    // ydnar: multiple console size support
    if( cls.keyCatchers & KEYCATCH_CONSOLE )
    {
        cls.keyCatchers &= ~KEYCATCH_CONSOLE;
        con.desiredFrac = 0.0;
    }
    else
    {
        cls.keyCatchers |= KEYCATCH_CONSOLE;
        
        // S16 console
        if( keys[K_CTRL].down )
        {
            con.desiredFrac = ( 5.0 * SMALLCHAR_HEIGHT ) / cls.glconfig.vidHeight;
        }
        // full console
        else if( keys[K_ALT].down )
        {
            con.desiredFrac = 1.0;
        }
        // normal half-screen console
        else
        {
            con.desiredFrac = 0.5;
        }
    }
}

/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f( void )
{
    S32 i;
    
    chat_team = false;
    Field_Clear( &chatField );
    chatField.widthInChars = 30;
    
    for( i = 1; i < Cmd_Argc(); i++ )
    {
        Com_sprintf( chatField.buffer, MAX_SAY_TEXT, "%s%s ", chatField.buffer, Cmd_Argv( i ) );
    }
    chatField.cursor += strlen( chatField.buffer );
    
    cls.keyCatchers ^= KEYCATCH_MESSAGE;
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f( void )
{
    S32 i;
    
    chat_team = true;
    Field_Clear( &chatField );
    chatField.widthInChars = 25;
    
    for( i = 1; i < Cmd_Argc(); i++ )
    {
        Com_sprintf( chatField.buffer, MAX_SAY_TEXT, "%s%s ", chatField.buffer, Cmd_Argv( i ) );
    }
    chatField.cursor += strlen( chatField.buffer );
    
    cls.keyCatchers ^= KEYCATCH_MESSAGE;
}

/*
================
Con_MessageMode3_f
================
*/
void Con_MessageMode3_f( void )
{
    S32 i;
    
    chat_team = false;
    chat_buddy = true;
    Field_Clear( &chatField );
    chatField.widthInChars = 26;
    
    for( i = 1; i < Cmd_Argc(); i++ )
    {
        Com_sprintf( chatField.buffer, MAX_SAY_TEXT, "%s%s ", chatField.buffer, Cmd_Argv( i ) );
    }
    chatField.cursor += strlen( chatField.buffer );
    cls.keyCatchers ^= KEYCATCH_MESSAGE;
}

void Con_OpenConsole_f( void )
{
    if( !( cls.keyCatchers & KEYCATCH_CONSOLE ) )
    {
        Con_ToggleConsole_f();
    }
}

StringEntry Con_GetText( S32 console )
{
    if( console >= 0 && con.text )
    {
        return ( UTF8* )con.text;
    }
    else
    {
        return NULL;
    }
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f( void )
{
    S32             i;
    
    for( i = 0; i < CON_TEXTSIZE; i++ )
    {
        con.text[i] = ( ColorIndex( CONSOLE_COLOR ) << 8 ) | ' ';
    }
    
    Con_Bottom();				// go to end
}

/*
================
Con_Dump_f

Save the console contents out to a file
================
*/
void Con_Dump_f( void )
{
    S32             l, x, i;
    S16*          line;
    fileHandle_t    f;
    UTF8            buffer[1024];
    
    if( Cmd_Argc() != 2 )
    {
        Com_Printf( "usage: condump <filename>\n" );
        return;
    }
    
    f = fileSystem->FOpenFileWrite( Cmd_Argv( 1 ) );
    if( !f )
    {
        Com_Printf( "ERROR: couldn't open.\n" );
        return;
    }
    
    Com_Printf( "Dumped console text to %s.\n", Cmd_Argv( 1 ) );
    
    // skip empty lines
    for( l = con.current - con.totallines + 1; l <= con.current; l++ )
    {
        line = con.text + ( l % con.totallines ) * con.linewidth;
        for( x = 0; x < con.linewidth; x++ )
            if( ( line[x] & 0xff ) != ' ' )
            {
                break;
            }
        if( x != con.linewidth )
        {
            break;
        }
    }
    
    // write the remaining lines
    buffer[con.linewidth] = '\0';
    for( ; l <= con.current; l++ )
    {
        line = con.text + ( l % con.totallines ) * con.linewidth;
        for( i = 0; i < con.linewidth; i++ )
            buffer[i] = line[i] & 0xff;
        for( x = con.linewidth - 1; x >= 0; x-- )
        {
            if( buffer[x] == ' ' )
            {
                buffer[x] = 0;
            }
            else
            {
                break;
            }
        }
        strcat( buffer, "\n" );
        fileSystem->Write( buffer, strlen( buffer ), f );
    }
    
    fileSystem->FCloseFile( f );
}

/*
================
Con_Search_f

Scroll up to the first console line containing a string
================
*/
void Con_Search_f( void )
{
    S32		l, i, x;
    S16*	line;
    UTF8	buffer[MAXPRINTMSG];
    S32		direction;
    S32		c = Cmd_Argc();
    
    if( c < 2 )
    {
        Com_Printf( "usage: %s <string1> <string2> <...>\n", Cmd_Argv( 0 ) );
        return;
    }
    
    if( !Q_stricmp( Cmd_Argv( 0 ), "searchDown" ) )
    {
        direction = 1;
    }
    else
    {
        direction = -1;
    }
    
    // check the lines
    buffer[con.linewidth] = 0;
    for( l = con.display - 1 + direction; l <= con.current && con.current - l < con.totallines; l += direction )
    {
        line = con.text + ( l % con.totallines ) * con.linewidth;
        for( i = 0; i < con.linewidth; i++ )
            buffer[i] = line[i] & 0xff;
        for( x = con.linewidth - 1 ; x >= 0 ; x-- )
        {
            if( buffer[x] == ' ' )
                buffer[x] = 0;
            else
                break;
        }
        // Don't search commands
        for( i = 1; i < c; i++ )
        {
            if( Q_stristr( buffer, Cmd_Argv( i ) ) )
            {
                con.display = l + 1;
                if( con.display > con.current )
                    con.display = con.current;
                return;
            }
        }
    }
}


/*
================
Con_Grep_f

Find all console lines containing a string
================
*/
void Con_Grep_f( void )
{
    S32		l, x, i;
    S16*	line;
    UTF8	buffer[1024];
    UTF8	buffer2[1024];
    UTF8	printbuf[CON_TEXTSIZE];
    UTF8*	search;
    UTF8	lastcolor;
    
    if( Cmd_Argc() != 2 )
    {
        Com_Printf( "usage: grep <string>\n" );
        return;
    }
    
    // skip empty lines
    for( l = con.current - con.totallines + 1 ; l <= con.current ; l++ )
    {
        line = con.text + ( l % con.totallines ) * con.linewidth;
        for( x = 0 ; x < con.linewidth ; x++ )
            if( ( line[x] & 0xff ) != ' ' )
                break;
        if( x != con.linewidth )
            break;
    }
    
    // check the remaining lines
    buffer[con.linewidth] = 0;
    search = Cmd_Argv( 1 );
    printbuf[0] = '\0';
    lastcolor = 7;
    for( ; l <= con.current ; l++ )
    {
        line = con.text + ( l % con.totallines ) * con.linewidth;
        for( i = 0, x = 0; i < con.linewidth; i++ )
        {
            if( line[i] >> 8 != lastcolor )
            {
                lastcolor = line[i] >> 8;
                buffer[x++] = Q_COLOR_ESCAPE;
                buffer[x++] = lastcolor + '0';
            }
            buffer[x++] = line[i] & 0xff;
        }
        for( x = con.linewidth - 1 ; x >= 0 ; x-- )
        {
            if( buffer[x] == ' ' )
                buffer[x] = 0;
            else
                break;
        }
        // Don't search commands
        strcpy( buffer2, buffer );
        Q_CleanStr( buffer2 );
        if( Q_stristr( buffer2, search ) )
        {
            strcat( printbuf, buffer );
            strcat( printbuf, "\n" );
        }
    }
    if( printbuf[0] )
        Com_Printf( "%s", printbuf );
}



/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify( void )
{
    S32             i;
    
    for( i = 0; i < NUM_CON_TIMES; i++ )
    {
        con.times[i] = 0;
    }
}



/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize( void )
{
    S32 i, j, width, oldwidth, oldtotallines, numlines, numchars;
    S16	tbuf[CON_TEXTSIZE];
    
    if( cls.glconfig.vidWidth )
    {
        if( scr_conUseOld->integer )
        {
            width = cls.glconfig.vidWidth / SCR_ConsoleFontCharWidth( 'W' );
        }
        else
        {
            width = ( cls.glconfig.vidWidth - 30 ) / SCR_ConsoleFontCharWidth( 'W' );
        }
        g_consoleField.widthInChars = width - Q_PrintStrlen( cl_consolePrompt->string ) - 1;
    }
    else
    {
        width = 0;
    }
    
    if( width == con.linewidth )
        return;
        
    if( con.linewidth < 1 )			// video hasn't been initialized yet
    {
        width = DEFAULT_CONSOLE_WIDTH;
        con.linewidth = width;
        con.totallines = CON_TEXTSIZE / con.linewidth;
        for( i = 0; i < CON_TEXTSIZE; i++ )
        
            con.text[i] = ( ColorIndex( COLOR_WHITE ) << 8 ) | ' ';
    }
    else
    {
        oldwidth = con.linewidth;
        con.linewidth = width;
        oldtotallines = con.totallines;
        con.totallines = CON_TEXTSIZE / con.linewidth;
        numlines = oldtotallines;
        
        if( con.totallines < numlines )
            numlines = con.totallines;
            
        numchars = oldwidth;
        
        if( con.linewidth < numchars )
            numchars = con.linewidth;
            
        ::memcpy( tbuf, con.text, CON_TEXTSIZE * sizeof( S16 ) );
        for( i = 0; i < CON_TEXTSIZE; i++ )
        {
            con.text[i] = ( ColorIndex( COLOR_WHITE ) << 8 ) | ' ';
        }
        
        for( i = 0 ; i < numlines ; i++ )
        {
            for( j = 0 ; j < numchars ; j++ )
            {
                con.text[( con.totallines - 1 - i ) * con.linewidth + j] = tbuf[( ( con.current - i + oldtotallines ) % oldtotallines ) * oldwidth + j];
            }
        }
    }
    
    con.current = con.totallines - 1;
    con.display = con.current;
}


/*
================
Con_Init
================
*/
void Con_Init( void )
{
    Com_Printf( "\n----- Console Initialization -------\n" );
    
    con_notifytime = cvarSystem->Get( "con_notifytime", "7", 0 );	// JPW NERVE increased per id req for obits
    con_conspeed = cvarSystem->Get( "scr_conspeed", "3", 0 );
    con_debug = cvarSystem->Get( "con_debug", "0", CVAR_ARCHIVE );	//----(SA)    added
    con_autoclear = cvarSystem->Get( "con_autoclear", "1", CVAR_ARCHIVE );
    con_restricted = cvarSystem->Get( "con_restricted", "0", CVAR_INIT );	// DHM - Nerve
    
    // Defines cvar for color and alpha for console/bar under console
    scr_conUseShader = cvarSystem->Get( "scr_conUseShader", "0", CVAR_ARCHIVE );
    
    scr_conColorAlpha = cvarSystem->Get( "scr_conColorAlpha", "0.5", CVAR_ARCHIVE );
    scr_conColorRed = cvarSystem->Get( "scr_conColorRed", "0", CVAR_ARCHIVE );
    scr_conColorBlue = cvarSystem->Get( "scr_conColorBlue", "0.3", CVAR_ARCHIVE );
    scr_conColorGreen = cvarSystem->Get( "scr_conColorGreen", "0.23", CVAR_ARCHIVE );
    
    scr_conUseOld = cvarSystem->Get( "scr_conUseOld", "0", CVAR_ARCHIVE );
    
    scr_conBarHeight = cvarSystem->Get( "scr_conBarHeight", "2", CVAR_ARCHIVE );
    
    scr_conBarColorAlpha = cvarSystem->Get( "scr_conBarColorAlpha", "0.3", CVAR_ARCHIVE );
    scr_conBarColorRed = cvarSystem->Get( "scr_conBarColorRed", "1", CVAR_ARCHIVE );
    scr_conBarColorBlue = cvarSystem->Get( "scr_conBarColorBlue", "1", CVAR_ARCHIVE );
    scr_conBarColorGreen = cvarSystem->Get( "scr_conBarColorGreen", "1", CVAR_ARCHIVE );
    
    scr_conHeight = cvarSystem->Get( "scr_conHeight", "50", CVAR_ARCHIVE );
    
    scr_conBarSize = cvarSystem->Get( "scr_conBarSize", "2", CVAR_ARCHIVE );
    
    // Done defining cvars for console colors
    
    Field_Clear( &g_consoleField );
    g_consoleField.widthInChars = g_console_field_width;
    
    Cmd_AddCommand( "toggleConsole", Con_ToggleConsole_f );
    Cmd_AddCommand( "clear", Con_Clear_f );
    Cmd_AddCommand( "condump", Con_Dump_f );
    Cmd_AddCommand( "search", Con_Search_f );
    Cmd_AddCommand( "searchDown", Con_Search_f );
    Cmd_AddCommand( "grep", Con_Grep_f );
    
    // ydnar: these are deprecated in favor of cgame/ui based version
    Cmd_AddCommand( "clMessageMode", Con_MessageMode_f );
    Cmd_AddCommand( "clMessageMode2", Con_MessageMode2_f );
    Cmd_AddCommand( "clMessageMode3", Con_MessageMode3_f );
    
    Com_Printf( "Console initialized.\n" );
}


/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed( bool skipnotify )
{
    S32             i;
    
    // mark time for transparent overlay
    if( con.current >= 0 )
    {
        if( skipnotify )
        {
            con.times[con.current % NUM_CON_TIMES] = 0;
        }
        else
        {
            con.times[con.current % NUM_CON_TIMES] = cls.realtime;
        }
    }
    
    con.x = 0;
    if( con.display == con.current )
    {
        con.display++;
    }
    con.current++;
    for( i = 0; i < con.linewidth; i++ )
        con.text[( con.current % con.totallines ) * con.linewidth + i] = ( ColorIndex( CONSOLE_COLOR ) << 8 ) | ' ';
}

/*
================
CL_ConsolePrint

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
#if defined( _WIN32 ) && defined( NDEBUG )
#pragma optimize( "g", off )	// SMF - msvc totally screws this function up with optimize on
#endif

void CL_ConsolePrint( UTF8* txt )
{
    S32             y;
    S32             c, l;
    S32             color;
    bool        skipnotify = false;	// NERVE - SMF
    S32             prev;		// NERVE - SMF
    
    // NERVE - SMF - work around for text that shows up in console but not in notify
    if( !Q_strncmp( txt, "[skipnotify]", 12 ) )
    {
        skipnotify = true;
        txt += 12;
    }
    
    // for some demos we don't want to ever show anything on the console
    if( cl_noprint && cl_noprint->integer )
    {
        return;
    }
    
    if( !con.initialized )
    {
        con.color[0] = con.color[1] = con.color[2] = con.color[3] = 1.0f;
        con.linewidth = -1;
        Con_CheckResize();
        con.initialized = true;
    }
    
    if( !skipnotify && !( cls.keyCatchers & KEYCATCH_CONSOLE ) && strncmp( txt, "EXCL: ", 6 ) )
    {
        // feed the text to cgame
        Cmd_SaveCmdContext( );
        Cmd_TokenizeString( txt );
        CL_GameConsoleText( );
        Cmd_RestoreCmdContext( );
    }
    
    color = ColorIndex( CONSOLE_COLOR );
    
    while( ( c = *txt ) != 0 )
    {
        if( Q_IsColorString( txt ) )
        {
            if( *( txt + 1 ) == COLOR_NULL )
            {
                color = ColorIndex( CONSOLE_COLOR );
            }
            else
            {
                color = ColorIndex( *( txt + 1 ) );
            }
            txt += 2;
            continue;
        }
        
        // count word length
        for( l = 0; l < con.linewidth; l++ )
        {
            if( txt[l] <= ' ' && txt[l] >= 0 )
            {
                break;
            }
            
        }
        
        // word wrap
        if( l != con.linewidth && ( con.x + l >= con.linewidth ) )
        {
            Con_Linefeed( skipnotify );
            
        }
        
        txt++;
        
        switch( c )
        {
            case '\n':
                Con_Linefeed( skipnotify );
                break;
            case '\r':
                con.x = 0;
                break;
            default:			// display character and advance
                y = con.current % con.totallines;
                // rain - sign extension caused the character to carry over
                // into the color info for high ascii chars; casting c to unsigned
                con.text[y * con.linewidth + con.x] = ( color << 8 ) | ( U8 )c;
                con.x++;
                if( con.x >= con.linewidth )
                {
                
                    Con_Linefeed( skipnotify );
                    con.x = 0;
                }
                break;
        }
    }
    
    // mark time for transparent overlay
    if( con.current >= 0 )
    {
        // NERVE - SMF
        if( skipnotify )
        {
            prev = con.current % NUM_CON_TIMES - 1;
            if( prev < 0 )
            {
                prev = NUM_CON_TIMES - 1;
            }
            con.times[prev] = 0;
        }
        else
        {
            // -NERVE - SMF
            con.times[con.current % NUM_CON_TIMES] = cls.realtime;
        }
    }
}

#if defined( _WIN32 ) && defined( NDEBUG )
#pragma optimize( "g", on )		// SMF - re-enabled optimization
#endif

/*
==============================================================================

DRAWING

==============================================================================
*/


/*
================
Con_DrawInput

Draw the editline after a ] prompt
================
*/
void Con_DrawInput( void )
{
    S32		y;
    UTF8	prompt[ MAX_STRING_CHARS ];
    vec4_t	color;
    qtime_t realtime;
    
    if( cls.state != CA_DISCONNECTED && !( cls.keyCatchers & KEYCATCH_CONSOLE ) )
    {
        return;
    }
    
    Com_RealTime( &realtime );
    
    y = con.vislines - ( SCR_ConsoleFontCharHeight() * 2 ) + 2 ;
    
    Com_sprintf( prompt,  sizeof( prompt ), "^0[^3%02d%c%02d^0]^7 %s", realtime.tm_hour, ( realtime.tm_sec & 1 ) ? ':' : ' ', realtime.tm_min, cl_consolePrompt->string );
    
    color[0] = 1.0f;
    color[1] = 1.0f;
    color[2] = 1.0f;
    color[3] = ( scr_conUseOld->integer ? 1.0f : con.displayFrac * 2.0f );
    
    SCR_DrawSmallStringExt( con.xadjust + cl_conXOffset->integer, y + 10, prompt, color, false, false );
    
    Q_CleanStr( prompt );
    Field_Draw( &g_consoleField, con.xadjust + cl_conXOffset->integer + SCR_ConsoleFontStringWidth( prompt, strlen( prompt ) ), y + 10, true, true, color[3] );
}


/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify( void )
{
    S32             x, v;
    S16*            text;
    S32             i;
    S32             time;
    S32             skip;
    S32             currentColor;
    
    currentColor = 7;
    renderSystem->SetColor( g_color_table[currentColor] );
    
    v = 0;
    for( i = con.current - NUM_CON_TIMES + 1; i <= con.current; i++ )
    {
        if( i < 0 )
        {
            continue;
        }
        time = con.times[i % NUM_CON_TIMES];
        if( time == 0 )
        {
            continue;
        }
        time = cls.realtime - time;
        if( time > con_notifytime->value * 1000 )
        {
            continue;
        }
        text = con.text + ( i % con.totallines ) * con.linewidth;
        
        if( cl.snap.ps.pm_type != PM_INTERMISSION && cls.keyCatchers & ( KEYCATCH_UI | KEYCATCH_CGAME ) )
        {
            continue;
        }
        
        for( x = 0; x < con.linewidth; x++ )
        {
            if( ( text[x] & 0xff ) == ' ' )
            {
                continue;
            }
            if( ( ( text[x] >> 8 ) & COLOR_BITS ) != currentColor )
            {
                currentColor = ( text[x] >> 8 ) & COLOR_BITS;
                renderSystem->SetColor( g_color_table[currentColor] );
            }
            SCR_DrawSmallChar( cl_conXOffset->integer + con.xadjust + ( x + 1 ) * SMALLCHAR_WIDTH, v, text[x] & 0xff );
        }
        
        v += SMALLCHAR_HEIGHT;
    }
    
    renderSystem->SetColor( NULL );
    
    if( cls.keyCatchers & ( KEYCATCH_UI | KEYCATCH_CGAME ) )
    {
        return;
    }
    
    // draw the chat line
    if( cls.keyCatchers & KEYCATCH_MESSAGE )
    {
        if( chat_team )
        {
            UTF8            buf[128];
            
            CL_TranslateString( "say_team:", buf );
            SCR_DrawBigString( 8, v, buf, 1.0f, false );
            skip = strlen( buf ) + 2;
        }
        else if( chat_buddy )
        {
            UTF8            buf[128];
            
            CL_TranslateString( "say_fireteam:", buf );
            SCR_DrawBigString( 8, v, buf, 1.0f, false );
            skip = strlen( buf ) + 2;
        }
        else if( chat_irc )
        {
            UTF8            buf[128];
            
            CL_TranslateString( "say_irc:", buf );
            SCR_DrawBigString( 8, v, buf, 1.0f, false );
            skip = strlen( buf ) + 2;
        }
        else
        {
            UTF8            buf[128];
            
            CL_TranslateString( "say:", buf );
            SCR_DrawBigString( 8, v, buf, 1.0f, false );
            skip = strlen( buf ) + 1;
        }
        
        Field_BigDraw( &chatField, skip * BIGCHAR_WIDTH, 232, true, true );
        
        v += BIGCHAR_HEIGHT;
    }
    
}

/*
================
Con_DrawSolidConsole

Draws the console with the solid background
================
*/
void Con_DrawSolidConsole( F32 frac )
{
    S32				i, x, y;
    S32				rows;
    S16*			text;
    S32				row;
    S32				lines;
    S32				currentColor;
    vec4_t			color;
    F32           totalwidth;
    F32           currentWidthLocation = 0;
    
    if( scr_conUseOld->integer )
    {
        lines = cls.glconfig.vidHeight * frac;
        if( lines <= 0 )
            return;
            
        if( lines > cls.glconfig.vidHeight )
            lines = cls.glconfig.vidHeight;
    }
    else
    {
        lines = cls.glconfig.vidHeight * frac;
    }
    
    // on wide screens, we will center the text
    if( !scr_conUseOld->integer )
    {
        con.xadjust = 15;
    }
    else
    {
        // Dushan - Old console need 0
        con.xadjust = 0;
    }
    SCR_AdjustFrom640( &con.xadjust, NULL, NULL, NULL );
    
    // draw the background
    if( scr_conUseOld->integer )
    {
        y = frac * SCREEN_HEIGHT;
        if( y < 1 )
        {
            y = 0;
        }
        else
        {
            if( scr_conUseShader->integer )
            {
                SCR_DrawPic( 0, 0, SCREEN_WIDTH, y, cls.consoleShader );
            }
            else
            {
                // This will be overwrote, so ill just abuse it here, no need to define another array
                color[0] = scr_conColorRed->value;
                color[1] = scr_conColorGreen->value;
                color[2] = scr_conColorBlue->value;
                color[3] = scr_conColorAlpha->value;
                
                SCR_FillRect( 0, 0, SCREEN_WIDTH, y, color );
            }
        }
        
        color[0] = scr_conBarColorRed->value;
        color[1] = scr_conBarColorGreen->value;
        color[2] = scr_conBarColorBlue->value;
        color[3] = scr_conBarColorAlpha->value;
        
        SCR_FillRect( 0, y, SCREEN_WIDTH, scr_conBarSize->value, color );
    }
    else
    {
        color[0] = scr_conColorRed->value;
        color[1] = scr_conColorGreen->value;
        color[2] = scr_conColorBlue->value;
        color[3] = frac * 2 * scr_conColorAlpha->value;
        SCR_FillRect( 10, 10, 620, 460 * scr_conHeight->integer * 0.01, color );
        
        color[0] = scr_conBarColorRed->value;
        color[1] = scr_conBarColorGreen->value;
        color[2] = scr_conBarColorBlue->value;
        color[3] = frac * 2 * scr_conBarColorAlpha->value;
        SCR_FillRect( 10, 10, 620, 1, color );	//top
        SCR_FillRect( 10, 460 * scr_conHeight->integer * 0.01 + 10, 621, 1, color );	//bottom
        SCR_FillRect( 10, 10, 1, 460 * scr_conHeight->integer * 0.01, color );	//left
        SCR_FillRect( 630, 10, 1, 460 * scr_conHeight->integer * 0.01, color );	//right
    }
    
    
    // draw the version number
    color[0] = 1.0f;
    color[1] = 1.0f;
    color[2] = 1.0f;
    color[3] = ( scr_conUseOld->integer ? 1.0f : frac * 2.0f );
    renderSystem->SetColor( color );
    
    i = strlen( Q3_VERSION );
    totalwidth = SCR_ConsoleFontStringWidth( Q3_VERSION, i ) + cl_conXOffset->integer;
    if( !scr_conUseOld->integer )
    {
        totalwidth += 30;
    }
    for( x = 0 ; x < i ; x++ )
    {
        SCR_DrawConsoleFontChar( cls.glconfig.vidWidth - totalwidth + currentWidthLocation, lines - SCR_ConsoleFontCharHeight() * 2, Q3_VERSION[x] );
        currentWidthLocation += SCR_ConsoleFontCharWidth( Q3_VERSION[x] );
    }
    
    // engine string
    i = strlen( Q3_ENGINE );
    totalwidth = SCR_ConsoleFontStringWidth( Q3_ENGINE, i ) + cl_conXOffset->integer;
    if( !scr_conUseOld->integer )
    {
        totalwidth += 30;
    }
    
    currentWidthLocation = 0;
    for( x = 0 ; x < i ; x++ )
    {
        SCR_DrawConsoleFontChar( cls.glconfig.vidWidth - totalwidth + currentWidthLocation, lines - SCR_ConsoleFontCharHeight(), Q3_ENGINE[x] );
        currentWidthLocation += SCR_ConsoleFontCharWidth( Q3_ENGINE[x] );
    }
    
    // draw the text
    con.vislines = lines;
    rows = ( lines ) / SCR_ConsoleFontCharHeight() - 3;		// rows of text to draw
    if( scr_conUseOld->integer )
        rows++;
        
    y = lines - ( SCR_ConsoleFontCharHeight() * 3 ) + 10;
    
    // draw from the bottom up
    if( con.display != con.current )
    {
        // draw arrows to show the buffer is backscrolled
        color[0] = 1.0f;
        color[1] = 0.0f;
        color[2] = 0.0f;
        color[3] = ( scr_conUseOld->integer ? 1.0f : frac * 2.0f );
        renderSystem->SetColor( color );
        for( x = 0 ; x < con.linewidth - ( scr_conUseOld->integer ? 0 : 4 ); x += 4 )
            SCR_DrawConsoleFontChar( con.xadjust + ( x + 1 ) * SCR_ConsoleFontCharWidth( '^' ), y, '^' );
        y -= SCR_ConsoleFontCharHeight();
        rows--;
    }
    
    row = con.display;
    
    if( con.x == 0 )
    {
        row--;
    }
    
    currentColor = 7;
    color[0] = g_color_table[currentColor][0];
    color[1] = g_color_table[currentColor][1];
    color[2] = g_color_table[currentColor][2];
    color[3] = ( scr_conUseOld->integer ? 1.0f : frac * 2.0f );
    renderSystem->SetColor( color );
    
    for( i = 0 ; i < rows ; i++, y -= SCR_ConsoleFontCharHeight(), row-- )
    {
        F32 currentWidthLocation = cl_conXOffset->integer;
        
        if( row < 0 )
            break;
        if( con.current - row >= con.totallines )
        {
            // past scrollback wrap point
            continue;
        }
        
        text = con.text + ( row % con.totallines ) * con.linewidth;
        
        for( x = 0 ; x < con.linewidth ; x++ )
        {
            if( ( ( text[x] >> 8 ) & 31 ) != currentColor )
            {
                currentColor = ( text[x] >> 8 ) & 31;
                color[0] = g_color_table[currentColor][0];
                color[1] = g_color_table[currentColor][1];
                color[2] = g_color_table[currentColor][2];
                color[3] = ( scr_conUseOld->integer ? 1.0f : frac * 2.0f );
                renderSystem->SetColor( color );
            }
            
            SCR_DrawConsoleFontChar( con.xadjust + currentWidthLocation, y, text[x] & 0xff );
            currentWidthLocation += SCR_ConsoleFontCharWidth( text[x] & 0xff );
        }
    }
    
    // draw the input prompt, user text, and cursor if desired
    Con_DrawInput();
    
    renderSystem->SetColor( NULL );
}
extern cvar_t*  con_drawnotify;

/*
==================
Con_DrawConsole
==================
*/
void Con_DrawConsole( void )
{
    // check for console width changes from a vid mode change
    Con_CheckResize();
    
    // if disconnected, render console full screen
    if( cls.state == CA_DISCONNECTED )
    {
        if( !( cls.keyCatchers & ( KEYCATCH_UI | KEYCATCH_CGAME ) ) )
        {
            Con_DrawSolidConsole( 1.0 );
            return;
        }
    }
    
    if( con.displayFrac )
    {
        Con_DrawSolidConsole( con.displayFrac );
    }
    else
    {
        // draw notify lines
        if( cls.state == CA_ACTIVE && con_drawnotify->integer )
        {
            Con_DrawNotify();
        }
    }
}

//================================================================

/*
==================
Con_RunConsole

Scroll it up or down
==================
*/
void Con_RunConsole( void )
{
    // decide on the destination height of the console
    if( cls.keyCatchers & KEYCATCH_CONSOLE )
        if( scr_conUseOld->integer )
        {
            con.finalFrac = MAX( 0.10, 0.01 * scr_conHeight->integer ); // configured console percentage
        }
        else
        {
            con.finalFrac = 0.5;
        }
    else
        con.finalFrac = 0;				// none visible
        
    // scroll towards the destination height
    if( con.finalFrac < con.displayFrac )
    {
        con.displayFrac -= con_conspeed->value * cls.realFrametime * 0.001;
        if( con.finalFrac > con.displayFrac )
            con.displayFrac = con.finalFrac;
            
    }
    else if( con.finalFrac > con.displayFrac )
    {
        con.displayFrac += con_conspeed->value * cls.realFrametime * 0.001;
        if( con.finalFrac < con.displayFrac )
            con.displayFrac = con.finalFrac;
    }
    
}


void Con_PageUp( void )
{
    con.display -= 2;
    if( con.current - con.display >= con.totallines )
    {
        con.display = con.current - con.totallines + 1;
    }
}

void Con_PageDown( void )
{
    con.display += 2;
    if( con.display > con.current )
    {
        con.display = con.current;
    }
}

void Con_Top( void )
{
    con.display = con.totallines;
    if( con.current - con.display >= con.totallines )
    {
        con.display = con.current - con.totallines + 1;
    }
}

void Con_Bottom( void )
{
    con.display = con.current;
}


void Con_Close( void )
{
    if( !com_cl_running->integer )
    {
        return;
    }
    Field_Clear( &g_consoleField );
    Con_ClearNotify();
    cls.keyCatchers &= ~KEYCATCH_CONSOLE;
    con.finalFrac = 0;			// none visible
    con.displayFrac = 0;
}
