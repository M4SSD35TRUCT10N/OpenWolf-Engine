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
// File name:   cmd.cpp
// Version:     v1.01
// Created:
// Compilers:   Visual Studio 2017, gcc 7.3.0
// Description: Quake script command processing module
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#ifdef DEDICATED
#include <null/null_precompiled.h>
#else
#include <OWLib/precompiled.h>
#endif

#define MAX_CMD_BUFFER  131072
#define MAX_CMD_LINE    1024

typedef struct
{
    U8*           data;
    S32             maxsize;
    S32             cursize;
} cmd_t;

S32             cmd_wait;
cmd_t           cmd_text;
U8            cmd_text_buf[MAX_CMD_BUFFER];

// Delay stuff

#define MAX_DELAYED_COMMANDS    64
#define CMD_DELAY_FRAME_FIRE 1
#define CMD_DELAY_UNUSED 0

typedef enum
{
    CMD_DELAY_MSEC,
    CMD_DELAY_FRAME
} cmdDelayType_t;

typedef struct
{
    UTF8    name[MAX_CMD_LINE];
    UTF8    text[MAX_CMD_LINE];
    S32     delay;
    cmdDelayType_t 	type;
} delayed_cmd_s;

delayed_cmd_s delayed_cmd[MAX_DELAYED_COMMANDS];

typedef struct cmd_function_s
{
    struct cmd_function_s* next;
    UTF8*           name;
    xcommand_t      function;
    completionFunc_t	complete;
} cmd_function_t;

typedef struct cmdContext_s
{
    S32		argc;
    UTF8*	argv[ MAX_STRING_TOKENS ];	// points into cmd.tokenized
    UTF8	tokenized[ BIG_INFO_STRING + MAX_STRING_TOKENS ];	// will have 0 bytes inserted
    UTF8	cmd[ BIG_INFO_STRING ]; // the original command we received (no token processing)
} cmdContext_t;

static cmdContext_t		cmd;
static cmdContext_t		savedCmd;

// static S32      cmd.argc;
// static UTF8    *cmd.argv[MAX_STRING_TOKENS];	// points into cmd.tokenized
// static UTF8     cmd.tokenized[BIG_INFO_STRING + MAX_STRING_TOKENS];	// will have 0 bytes inserted
// static UTF8     cmd.cmd[BIG_INFO_STRING];	// the original command we received (no token processing)

static cmd_function_t* cmd_functions;	// possible commands to execute



//=============================================================================

/*
============
Cmd_FindCommand
============
*/
cmd_function_t* Cmd_FindCommand( StringEntry cmd_name )
{
    cmd_function_t* cmd;
    for( cmd = cmd_functions; cmd; cmd = cmd->next )
        if( !Q_stricmp( cmd_name, cmd->name ) )
            return cmd;
    return NULL;
}

/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "cmd use rocket ; +attack ; wait ; -attack ; cmd use blaster"
============
*/
void Cmd_Wait_f( void )
{
    if( Cmd_Argc() == 2 )
    {
        cmd_wait = atoi( Cmd_Argv( 1 ) );
        if( cmd_wait < 0 )
        {
            cmd_wait = 1; // ignore the argument
        }
    }
    else
    {
        cmd_wait = 1;
    }
}


/*
=============================================================================

						COMMAND BUFFER

=============================================================================
*/

/*
============
Cbuf_Init
============
*/
void Cbuf_Init( void )
{
    cmd_text.data = cmd_text_buf;
    cmd_text.maxsize = MAX_CMD_BUFFER;
    cmd_text.cursize = 0;
}

/*
============
Cbuf_AddText

Adds command text at the end of the buffer, does NOT add a final \n
============
*/
void Cbuf_AddText( StringEntry text )
{
    S32             l;
    
    l = strlen( text );
    
    if( cmd_text.cursize + l >= cmd_text.maxsize )
    {
        Com_Printf( "Cbuf_AddText: overflow\n" );
        return;
    }
    ::memcpy( &cmd_text.data[cmd_text.cursize], text, l );
    cmd_text.cursize += l;
}


/*
============
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
============
*/
void Cbuf_InsertText( StringEntry text )
{
    S32             len;
    S32             i;
    
    len = strlen( text ) + 1;
    if( len + cmd_text.cursize > cmd_text.maxsize )
    {
        Com_Printf( "Cbuf_InsertText overflowed\n" );
        return;
    }
    
    // move the existing command text
    for( i = cmd_text.cursize - 1; i >= 0; i-- )
    {
        cmd_text.data[i + len] = cmd_text.data[i];
    }
    
    // copy the new text in
    ::memcpy( cmd_text.data, text, len - 1 );
    
    // add a \n
    cmd_text.data[len - 1] = '\n';
    
    cmd_text.cursize += len;
}


/*
============
Cbuf_ExecuteText
============
*/
void Cbuf_ExecuteText( S32 exec_when, StringEntry text )
{
    switch( exec_when )
    {
        case EXEC_NOW:
            if( text && strlen( text ) > 0 )
            {
                Com_DPrintf( S_COLOR_YELLOW "EXEC_NOW %s\n", text );
                Cmd_ExecuteString( text );
            }
            else
            {
                Cbuf_Execute();
                Com_DPrintf( S_COLOR_YELLOW "EXEC_NOW %s\n", cmd_text.data );
            }
            break;
        case EXEC_INSERT:
            Cbuf_InsertText( text );
            break;
        case EXEC_APPEND:
            Cbuf_AddText( text );
            break;
        default:
            Com_Error( ERR_FATAL, "Cbuf_ExecuteText: bad exec_when" );
    }
}

/*
============
Cbuf_Execute
============
*/
void Cbuf_Execute( void )
{
    S32             i;
    UTF8*           text;
    UTF8            line[MAX_CMD_LINE];
    S32             quotes;
    
    while( cmd_text.cursize )
    {
        if( cmd_wait )
        {
            // skip out while text still remains in buffer, leaving it
            // for next frame
            cmd_wait--;
            break;
        }
        
        // find a \n or ; line break
        text = ( UTF8* )cmd_text.data;
        
        quotes = 0;
        for( i = 0; i < cmd_text.cursize; i++ )
        {
            if( text[i] == '\\' && text[i + 1] == '"' )
            {
                i++;
                continue;
            }
            
            if( text[i] == '"' )
            {
                quotes++;
            }
            if( !( quotes & 1 ) && text[i] == ';' )
            {
                break;			// don't break if inside a quoted string
            }
            if( text[i] == '\n' || text[i] == '\r' )
            {
                break;
            }
        }
        
        if( i >= ( MAX_CMD_LINE - 1 ) )
        {
            i = MAX_CMD_LINE - 1;
        }
        
        memcpy( line, text, i );
        line[i] = 0;
        
// delete the text from the command buffer and move remaining commands down
// this is necessary because commands (exec) can insert data at the
// beginning of the text buffer

        if( i == cmd_text.cursize )
        {
            cmd_text.cursize = 0;
        }
        else
        {
            i++;
            cmd_text.cursize -= i;
            memmove( text, text + i, cmd_text.cursize );
        }
        
// execute the command line

        Cmd_ExecuteString( line );
    }
}

/*
==============================================================================

						COMMANDS DELAYING

==============================================================================
*/


/*
===============
Cdelay_Frame
===============
*/

void Cdelay_Frame( void )
{
    S32 i;
    bool run_it;
    
    for( i = 0; ( i < MAX_DELAYED_COMMANDS ); i++ )
    {
        run_it = false;
        
        if( delayed_cmd[i].delay == CMD_DELAY_UNUSED )
            continue;
            
        //check if we should run the command (both type)
        if( delayed_cmd[i].type == CMD_DELAY_MSEC && delayed_cmd[i].delay < Sys_Milliseconds() )
        {
            run_it = true;
        }
        else if( delayed_cmd[i].type == CMD_DELAY_FRAME )
        {
            delayed_cmd[i].delay -= 1;
            if( delayed_cmd[i].delay == CMD_DELAY_FRAME_FIRE )
                run_it = true;
        }
        
        if( run_it )
        {
            delayed_cmd[i].delay = CMD_DELAY_UNUSED;
            Cbuf_ExecuteText( EXEC_NOW, delayed_cmd[i].text );
        }
    }
}





/*
==============================================================================

						SCRIPT COMMANDS

==============================================================================
*/


/*
===============
Cmd_Exec_f
===============
*/

/*
===============
Cmd_Exec_f
===============
*/
static void Cmd_ExecFile( UTF8* f )
{
    S32 i;
    
    COM_Compress( f );
    
    cvarSystem->Get( "arg_all", Cmd_ArgsFrom( 2 ), CVAR_TEMP | CVAR_ROM | CVAR_USER_CREATED );
    cvarSystem->Set( "arg_all", Cmd_ArgsFrom( 2 ) );
    cvarSystem->Get( "arg_count", va( "%i", Cmd_Argc() - 2 ), CVAR_TEMP | CVAR_ROM | CVAR_USER_CREATED );
    cvarSystem->Set( "arg_count", va( "%i", Cmd_Argc() - 2 ) );
    
    for( i = Cmd_Argc() - 2; i; i-- )
    {
        cvarSystem->Get( va( "arg_%i", i ), Cmd_Argv( i + 1 ), CVAR_TEMP | CVAR_ROM | CVAR_USER_CREATED );
        cvarSystem->Set( va( "arg_%i", i ), Cmd_Argv( i + 1 ) );
    }
    
    Cbuf_InsertText( f );
}

void Cmd_Exec_f( void )
{
    union
    {
        UTF8*	c;
        void*	v;
    } f;
    S32		len;
    UTF8	filename[MAX_QPATH];
    fileHandle_t h;
    bool success = false;
    
    if( Cmd_Argc() < 2 )
    {
        Com_Printf( "exec <filename> (args) : execute a script file\n" );
        return;
    }
    
    Com_Printf( "execing %s\n", Cmd_Argv( 1 ) );
    
    Q_strncpyz( filename, Cmd_Argv( 1 ), sizeof( filename ) );
    COM_DefaultExtension( filename, sizeof( filename ), ".cfg" );
    
    len = fileSystem->SV_FOpenFileRead( filename, &h );
    if( h )
    {
        success = true;
        f.v = Hunk_AllocateTempMemory( len + 1 );
        fileSystem->Read( f.v, len, h );
        f.c[len] = 0;
        fileSystem->FCloseFile( h );
        Cmd_ExecFile( f.c );
        Hunk_FreeTempMemory( f.v );
    }
    
    fileSystem->ReadFile( filename, &f.v );
    if( f.c )
    {
        success = true;
        Cmd_ExecFile( f.c );
        fileSystem->FreeFile( f.v );
    }
    
    if( !success )
        Com_Printf( "couldn't exec %s\n", Cmd_Argv( 1 ) );
}


/*
===============
Cmd_Vstr_f

Inserts the current value of a variable as command text
===============
*/
void Cmd_Vstr_f( void )
{
    UTF8*           v;
    
    if( Cmd_Argc() != 2 )
    {
        Com_Printf( "vstr <variablename> : execute a variable command\n" );
        return;
    }
    
    v = cvarSystem->VariableString( Cmd_Argv( 1 ) );
    Cbuf_InsertText( va( "%s\n", v ) );
}

/*
===============
Helper functions for Cmd_If_f & Cmd_ModCase_f
===============
*/
#ifdef DEDICATED
static const UTF8 modifierList[] = "not supported in the dedicated server";
#else
static const UTF8 modifierList[] = "shift, ctrl, alt, command/cmd, mode, super; ! negates; e.g. shift,!alt";

static const struct
{
    UTF8 name[8];
    U16 count;
    U16 bit;
    U32 index;
} modifierKeys[] =
{
    { "shift",   5,  1, K_SHIFT   },
    { "ctrl",    4,  2, K_CTRL    },
    { "alt",     3,  4, K_ALT     },
    { "command", 7,  8, K_COMMAND },
    { "cmd",     3,  8, K_COMMAND },
    { "mode",    4, 16, K_MODE    },
    { "super",   5, 32, K_SUPER   },
    { "",        0,  0, 0         }
};
// Following is no. of bits required for modifiers in the above list
#define NUM_RECOGNISED_MODIFIERS 6

typedef struct
{
    U16 down, up;
    S32 bits;
} modifierMask_t;

static modifierMask_t getModifierMask( StringEntry mods )
{
    S32 i;
    modifierMask_t mask;
    StringEntry ptr;
    static const modifierMask_t none;

    mask = none;

    --mods;
    while( *++mods == ' ' ) /* skip leading spaces */;
    ptr = mods;

    while( *ptr )
    {
        S32 invert = ( *ptr == '!' );
        if( invert )
            ++ptr;
        for( i = 0; modifierKeys[i].bit; ++i )
        {
            // is it this modifier?
            if( !Q_strnicmp( ptr, modifierKeys[i].name, modifierKeys[i].count )
                    && ( ptr[modifierKeys[i].count] == ' ' ||
                         ptr[modifierKeys[i].count] == ',' ||
                         ptr[modifierKeys[i].count] == 0 ) )
            {
                if( invert )
                    mask.up   |= modifierKeys[i].bit;
                else
                    mask.down |= modifierKeys[i].bit;
                if( ( mask.down & mask.up ) & modifierKeys[i].bit )
                {
                    Com_Printf( "can't have %s both pressed and not pressed\n", modifierKeys[i].name );
                    return none;
                }
                // right, parsed a word - skip it, maybe a comma, and any spaces
                ptr += modifierKeys[i].count - 1;
                while( *++ptr == ' ' ) /**/;
                if( *ptr == ',' )
                    while( *++ptr == ' ' ) /**/;
                // ready to parse the next one
                break;
            }
        }
        if( !modifierKeys[i].bit )
        {
            Com_Printf( "unknown modifier key name in \"%s\"\n", mods );
            return none;
        }
    }

    for( i = 0; i < NUM_RECOGNISED_MODIFIERS; ++i )
    {
        if( mask.up & ( 1 << i ) )
        {
            ++mask.bits;
        }
        if( mask.down & ( 1 << i ) )
        {
            ++mask.bits;
        }
    }

    return mask;
}

static S32 checkKeysDown( modifierMask_t mask )
{
    S32 i;
    for( i = 0; modifierKeys[i].bit; ++i )
    {
        if( ( mask.down & modifierKeys[i].bit ) && keys[modifierKeys[i].index].down == 0 )
        {
            return 0; // should be pressed, isn't pressed
        }
        if( ( mask.up   & modifierKeys[i].bit ) && keys[modifierKeys[i].index].down )
        {
            return 0; // should not be pressed, is pressed
        }
    }

    return 1; // all (not) pressed as requested
}

/*
===============
Cmd_ModCase_f

Takes a sequence of modifier/command pairs
Executes the command for the first matching modifier set

===============
*/
void Cmd_ModCase_f( void )
{
    S32   argc = Cmd_Argc();
    S32   index = 0;
    S32   max = 0;
    S32   count = ( argc - 1 ) / 2; // round down :-)
    UTF8* v;

    S32 mods[1 << NUM_RECOGNISED_MODIFIERS];
    // want 'modifierMask_t mods[argc / 2 - 1];' (variable array, C99)
    // but MSVC apparently doesn't like that

    if( argc < 3 )
    {
        Com_Printf( "modcase <modifiers> <command> [<modifiers> <command>] ... [<command>]\n" );
        return;
    }

    while( index < count )
    {
        modifierMask_t mask = getModifierMask( Cmd_Argv( 2 * index + 1 ) );
        if( mask.bits == 0 )
            return; // parse failure (reported) - abort
        mods[index] = checkKeysDown( mask ) ? mask.bits : 0;
        if( max < mods[index] )
            max = mods[index];
        ++index;
    }

    // If we have a tail command, use it as default
    v = ( argc & 1 ) ? NULL : Cmd_Argv( argc - 1 );

    // Search for a suitable command to execute.
    // Search is done as if the commands are sorted by modifier count
    // (descending) then parameter index no. (ascending).
    for( ; max > 0; --max )
    {
        S32 i;
        for( i = 0; i < index; ++i )
        {
            if( mods[i] == max )
            {
                v = Cmd_Argv( 2 * i + 2 );
                goto found;
            }
        }
    }

found:
    if( v )
    {
        if( *v == '/' || *v == '\\' )
            Cbuf_InsertText( va( "%s\n", v + 1 ) );
        else
            Cbuf_InsertText( va( "vstr %s\n", v ) );
    }
}
#endif // !DEDICATED

/*
===============
Cmd_If_f

Compares two values, if true executes the third argument, if false executes the forth
===============
*/
void Cmd_If_f( void )
{
    UTF8*	v = NULL;
    S32 	v1;
    S32 	v2;
    UTF8*	vt;
    UTF8*	vf = NULL;
    UTF8*  op;
#ifndef DEDICATED
    modifierMask_t mask;
#endif
    S32   argc;
    
    switch( argc = Cmd_Argc() )
    {
        case 4:
            vf = Cmd_Argv( 3 );
        case 3:
            vt = Cmd_Argv( 2 );
#ifdef DEDICATED
            Com_Printf( "if <modifiers>... is not supported on the server -- assuming true.\n" );
            v = vt;
#else
            v = Cmd_Argv( 1 );
            mask = getModifierMask( v );
            if( mask.bits == 0 )
            {
                return;
            }
            v = checkKeysDown( mask ) ? vt : vf;
#endif
            break;
            
        case 6:
            vf = Cmd_Argv( 5 );
        case 5:
            vt = Cmd_Argv( 4 );
            v1 = atoi( Cmd_Argv( 1 ) );
            op = Cmd_Argv( 2 );
            v2 = atoi( Cmd_Argv( 3 ) );
            if( ( !strcmp( op, "=" ) && v1 == v2 ) ||
                    ( !strcmp( op, "!=" ) && v1 != v2 ) ||
                    ( !strcmp( op, "<" ) && v1 <  v2 ) ||
                    ( !strcmp( op, "<=" ) && v1 <= v2 ) ||
                    ( !strcmp( op, ">" ) && v1 >  v2 ) ||
                    ( !strcmp( op, ">=" ) && v1 >= v2 ) )
            {
                v = vt;
            }
            else if( ( !strcmp( op, "=" ) && v1 != v2 ) ||
                     ( !strcmp( op, "!=" ) && v1 == v2 ) ||
                     ( !strcmp( op, "<" ) && v1 >= v2 ) ||
                     ( !strcmp( op, "<=" ) && v1 >  v2 ) ||
                     ( !strcmp( op, ">" ) && v1 <= v2 ) ||
                     ( !strcmp( op, ">=" ) && v1 <  v2 ) )
            {
                v = vf;
            }
            else
            {
                Com_Printf( "invalid operator in if command. valid operators are = != < > >= <=\n" );
                return;
            }
            break;
            
        default:
            Com_Printf( "if <value1> <operator> <value2> <cmdthen> (<cmdelse>) : compares the first two values and executes <cmdthen> if true, <cmdelse> if false\n"
                        "if <modifiers> <cmdthen> (<cmdelse>) : check if modifiers are (not) pressed\n"
                        "-- modifiers are %s\n"
                        "-- commands are cvar names unless prefixed with / or \\\n",
                        modifierList );
            return;
    }
    if( v )
    {
        if( *v == '/' || *v == '\\' )
            Cbuf_InsertText( va( "%s\n", v + 1 ) );
        else
            Cbuf_InsertText( va( "vstr %s\n", v ) );
    }
}

/*
===============
Cmd_Math_f

Compares two cvars, if true vstr the third, if false vstr the forth
===============
*/
void Cmd_Math_f( void )
{
    UTF8*  v;
    UTF8*  v1;
    UTF8*  v2;
    UTF8*  op;
    if( Cmd_Argc() == 3 )
    {
        v = Cmd_Argv( 1 );
        op = Cmd_Argv( 2 );
        if( !strcmp( op, "++" ) )
        {
            cvarSystem->SetValueSafe( v, ( cvarSystem->VariableValue( v ) + 1 ) );
        }
        else if( !strcmp( op, "--" ) )
        {
            cvarSystem->SetValueSafe( v, ( cvarSystem->VariableValue( v ) - 1 ) );
        }
        else
        {
            Com_Printf( "math <variableToSet> = <variable1> <operator> <variable2>\nmath <variableToSet> <operator> <variable1>\nmath <variableToSet> ++\nmath <variableToSet> --\nvalid operators are + - * / \n" );
            return;
        }
    }
    else if( Cmd_Argc() == 4 )
    {
        v = Cmd_Argv( 1 );
        op = Cmd_Argv( 2 );
        v1 = Cmd_Argv( 3 );
        if( !strcmp( op, "+" ) )
        {
            cvarSystem->SetValueSafe( v, ( cvarSystem->VariableValue( v ) + cvarSystem->VariableValue( v1 ) ) );
        }
        else if( !strcmp( op, "-" ) )
        {
            cvarSystem->SetValueSafe( v, ( cvarSystem->VariableValue( v ) - cvarSystem->VariableValue( v1 ) ) );
        }
        else if( !strcmp( op, "*" ) )
        {
            cvarSystem->SetValueSafe( v, ( cvarSystem->VariableValue( v ) * cvarSystem->VariableValue( v1 ) ) );
        }
        else if( !strcmp( op, "/" ) )
        {
            if( !( cvarSystem->VariableValue( v1 ) == 0 ) )
            {
                cvarSystem->SetValueSafe( v, ( cvarSystem->VariableValue( v ) / cvarSystem->VariableValue( v1 ) ) );
            }
        }
        else
        {
            Com_Printf( "math <variableToSet> = <variable1> <operator> <variable2>\nmath <variableToSet> <operator> <variable1>\nmath <variableToSet> ++\nmath <variableToSet> --\nvalid operators are + - * / \n" );
            return;
        }
    }
    else if( Cmd_Argc() == 6 )
    {
        v = Cmd_Argv( 1 );
        v1 = Cmd_Argv( 3 );
        op = Cmd_Argv( 4 );
        v2 = Cmd_Argv( 5 );
        if( !strcmp( op, "+" ) )
        {
            cvarSystem->SetValueSafe( v, ( cvarSystem->VariableValue( v1 ) + cvarSystem->VariableValue( v2 ) ) );
        }
        else if( !strcmp( op, "-" ) )
        {
            cvarSystem->SetValueSafe( v, ( cvarSystem->VariableValue( v1 ) - cvarSystem->VariableValue( v2 ) ) );
        }
        else if( !strcmp( op, "*" ) )
        {
            cvarSystem->SetValueSafe( v, ( cvarSystem->VariableValue( v1 ) * cvarSystem->VariableValue( v2 ) ) );
        }
        else if( !strcmp( op, "/" ) )
        {
            if( !( cvarSystem->VariableValue( v2 ) == 0 ) )
            {
                cvarSystem->SetValueSafe( v, ( cvarSystem->VariableValue( v1 ) / cvarSystem->VariableValue( v2 ) ) );
            }
        }
        else
        {
            Com_Printf( "math <variableToSet> = <variable1> <operator> <variable2>\nmath <variableToSet> <operator> <variable1>\nmath <variableToSet> ++\nmath <variableToSet> --\nvalid operators are + - * / \n" );
            return;
        }
    }
    else
    {
        Com_Printf( "math <variableToSet> = <variable1> <operator> <variable2>\nmath <variableToSet> <operator> <variable1>\nmath <variableToSet> ++\nmath <variableToSet> --\nvalid operators are + - * / \n" );
        return;
    }
}


/*
===============
Cmd_Strcmp_f

Compares two strings, if true executes the third argument, if false executes the forth
===============
*/
void Cmd_Strcmp_f( void )
{
    UTF8*	v;
    UTF8*	 v1;
    UTF8*	 v2;
    UTF8*	vt;
    UTF8*	vf;
    UTF8*  op;
    
    if( ( Cmd_Argc() == 6 ) || ( Cmd_Argc() == 5 ) )
    {
        v1 = Cmd_Argv( 1 );
        op = Cmd_Argv( 2 );
        v2 = Cmd_Argv( 3 );
        vt = Cmd_Argv( 4 );
        if( ( !strcmp( op, "=" ) && !strcmp( v1, v2 ) ) ||
                ( !strcmp( op, "!=" ) && strcmp( v1, v2 ) ) )
        {
            v = vt;
        }
        else if( ( !strcmp( op, "=" ) && strcmp( v1, v2 ) ) ||
                 ( !strcmp( op, "!=" ) && !strcmp( v1, v2 ) ) )
        {
            if( Cmd_Argc() == 6 )
            {
                vf = Cmd_Argv( 5 );
                v = vf;
            }
            else
            {
                return;
            }
        }
        else
        {
            Com_Printf( "invalid operator in strcmp command. valid operators are = != \n" );
            return;
        }
    }
    else
    {
        Com_Printf( "strcmp <string1> <operator> <string2> <cmdthen> (<cmdelse>) : compares the first two strings and executes <cmdthen> if true, <cmdelse> if false\n" );
        return;
    }
    Cbuf_InsertText( va( "%s\n", v ) );
}

/*
===============
Cmd_Concat_f

concatenates cvars together
===============
*/
void Cmd_Concat_f( void )
{
    UTF8*  v;
    UTF8*  v1;
    UTF8*  v2;
    UTF8  vc[MAX_CVAR_VALUE_STRING];
    if( Cmd_Argc() != 4 )
    {
        Com_Printf( "concat <variableToSet> <variable1> <variable2> : concatenates variable1 and variable2 and sets the result to variableToSet\n" );
        return;
    }
    
    v  = Cmd_Argv( 1 );
    v1 = cvarSystem->VariableString( Cmd_Argv( 2 ) );
    v2 = cvarSystem->VariableString( Cmd_Argv( 3 ) );
    Com_sprintf( vc, sizeof( vc ), "%s%s", v1, v2 );
    cvarSystem->Set( Cmd_Argv( 1 ), vc );
}

/*
===============
Cmd_Calc_f

Does math and displays the value into the chat/console, this is used for basic math functions
===============
*/
void Cmd_Calc_f( void )
{
    UTF8*	arg1;
    UTF8*	arg2;
    UTF8*	func;
    
    if( Cmd_Argc() < 3 )
    {
        Com_Printf( "calc <number> <function> <number>, accepted functions: +, -, /, */x\n" );
        return;
    }
    
    arg1 = Cmd_Argv( 1 );
    func = Cmd_Argv( 2 );
    arg2 = Cmd_Argv( 3 );
    
    // Add
    if( !strcmp( func, "+" ) )
    {
        Com_Printf( "%s %s %s = %f\n", arg1, func, arg2, ( atof( arg1 ) + atof( arg2 ) ) );
        return;
    }
    
    // Subtract
    else if( !strcmp( func, "-" ) )
    {
        Com_Printf( "%s %s %s = %f\n", arg1, func, arg2, ( atof( arg1 ) - atof( arg2 ) ) );
        return;
    }
    
    // Divide
    else if( !strcmp( func, "/" ) )
    {
        if( atof( arg2 ) == 0.f )
        {
            Com_Printf( "Cannot divide by zero!\n" );
            return;
        }
        Com_Printf( "%s %s %s = %f\n", arg1, func, arg2, ( atof( arg1 ) / atof( arg2 ) ) );
        return;
    }
    
    // Multiply
    else if( !strcmp( func, "*" ) || !strcmp( func, "x" ) )
    {
        Com_Printf( "%s %s %s = %f\n", arg1, func, arg2, ( atof( arg1 ) * atof( arg2 ) ) );
        return;
    }
    
    // Invalid function, help the poor guy out
    Com_Printf( "calc <number> <function> <number>, accepted functions: +, -, /, */x\n" );
}



/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
void Cmd_Echo_f( void )
{
    Com_Printf( "%s\n", Cmd_Args( ) );
}


/*
===============
Cmd_Undelay_f

Removes a pending delay with a given name
===============
*/
void Cmd_Undelay_f( void )
{
    S32 i;
    UTF8* find, *limit;
    
    // Check if the call is valid
    if( Cmd_Argc() < 1 )
    {
        Com_Printf( "undelay <name> (command)\nremoves all commands with <name> in them.\nif (command) is specified, the removal will be limited only to delays whose commands contain (command).\n" );
        return;
    }
    
    find = Cmd_Argv( 1 );
    limit = Cmd_Argv( 2 );
    
    for( i = 0; ( i < MAX_DELAYED_COMMANDS ); i++ )
    {
        if( delayed_cmd[i].delay != CMD_DELAY_UNUSED && strstr( delayed_cmd[i].name, find ) && strstr( delayed_cmd[i].text, limit ) ) // the limit test will always pass if limit is a null string
        {
            delayed_cmd[i].delay = CMD_DELAY_UNUSED;
        }
    }
}


/*
===============
Cmd_UndelayAll_f

Removes all pending delays
===============
*/
void Cmd_UndelayAll_f( void )
{
    S32 i;
    
    for( i = 0; ( i < MAX_DELAYED_COMMANDS ); i++ )
    {
        delayed_cmd[i].delay = CMD_DELAY_UNUSED;
    }
}

/*
===============
Cmd_Delay_f

Delays a comand
===============
*/
void Cmd_Delay_f( void )
{
    S32 i, delay, type, lastchar;
    UTF8* raw_delay, *name, *cmd;
    bool availiable_cmd = false;
    
    // Check if the call is valid
    if( Cmd_Argc() < 2 )
    {
        Com_Printf( "delay (name) <delay in milliseconds> <command>\ndelay <delay in frames>f <command>\nexecutes <command> after the delay\n" );
        return;
    }
    
    raw_delay = Cmd_Argv( 1 );
    if( !isdigit( raw_delay[0] ) )
    {
        name = raw_delay;
        raw_delay = Cmd_Argv( 2 );
        cmd = Cmd_ArgsFrom( 3 );
    }
    else
    {
        name = "";
        cmd = Cmd_ArgsFrom( 2 );
    }
    delay = atoi( raw_delay );
    
    if( delay < 1 )
    {
        Com_Printf( "delay: the delay must be a positive integer" );
        return;
    }
    
    //search for an unused slot
    for( i = 0; ( i < MAX_DELAYED_COMMANDS ); i++ )
    {
        if( delayed_cmd[i].delay == CMD_DELAY_UNUSED )
        {
            availiable_cmd = true;
            break;
        }
    }
    
    if( !availiable_cmd )
    {
        Com_Printf( "WARNING: Maximum amount of delayed commands reached." );
        return;
    }
    
    lastchar = strlen( raw_delay ) - 1;
    
    if( raw_delay[ lastchar ] == 'f' )
    {
        delay += CMD_DELAY_FRAME_FIRE;
        type = CMD_DELAY_FRAME;
    }
    else
    {
        type = CMD_DELAY_MSEC;
        delay += Sys_Milliseconds();
    }
    
    delayed_cmd[i].delay = delay;
    delayed_cmd[i].type = ( cmdDelayType_t )type;
    Q_strncpyz( delayed_cmd[i].text, cmd, MAX_CMD_LINE );
    Q_strncpyz( delayed_cmd[i].name, name, MAX_CMD_LINE );
}

/*
===============
Cmd_Random_f

Give a random integer
===============
*/
void Cmd_Random_f( void )
{
    S32 	v1;
    S32 	v2;
    
    if( Cmd_Argc() == 4 )
    {
        v1 = atoi( Cmd_Argv( 2 ) );
        v2 = atoi( Cmd_Argv( 3 ) );
        cvarSystem->SetValueLatched( Cmd_Argv( 1 ), ( S32 )( rand() / ( F32 )RAND_MAX * ( MAX( v1, v2 ) - MIN( v1, v2 ) ) + MIN( v1, v2 ) ) );
    }
    else
    {
        Com_Printf( "random <variableToSet> <value1> <value2>\n" );
    }
}

/*
=============================================================================

					ALIASES

=============================================================================
*/

typedef struct cmd_alias_s
{
    struct cmd_alias_s*	next;
    UTF8*				name;
    UTF8*				exec;
} cmd_alias_t;

static cmd_alias_t*	cmd_aliases = NULL;

/*
============
Cmd_RunAlias_f
============
*/
void Cmd_RunAlias_f( void )
{
    cmd_alias_t*	alias;
    UTF8*		 name = Cmd_Argv( 0 );
    UTF8*		 args = Cmd_ArgsFrom( 1 );
    
    // Find existing alias
    for( alias = cmd_aliases; alias; alias = alias->next )
    {
        if( !Q_stricmp( name, alias->name ) )
            break;
    }
    
    if( !alias )
        Com_Error( ERR_FATAL, "Alias: Alias %s doesn't exist", name );
        
    Cbuf_InsertText( va( "%s %s", alias->exec, args ) );
}

/*
============
Cmd_WriteAliases
============
*/
void Cmd_WriteAliases( fileHandle_t f )
{
    UTF8 buffer[1024] = "clearaliases\n";
    cmd_alias_t* alias = cmd_aliases;
    fileSystem->Write( buffer, strlen( buffer ), f );
    while( alias )
    {
        Com_sprintf( buffer, sizeof( buffer ), "alias %s \"%s\"\n", alias->name, Cmd_EscapeString( alias->exec ) );
        fileSystem->Write( buffer, strlen( buffer ), f );
        alias = alias->next;
    }
}

/*
============
Cmd_AliasList_f
============
*/
void Cmd_AliasList_f( void )
{
    cmd_alias_t*	alias;
    S32			i;
    UTF8*		match;
    
    if( Cmd_Argc() > 1 )
        match = Cmd_Argv( 1 );
    else
        match = NULL;
        
    i = 0;
    for( alias = cmd_aliases; alias; alias = alias->next )
    {
        if( match && !Com_Filter( match, alias->name, false ) )
            continue;
        Com_Printf( "%s ==> %s\n", alias->name, alias->exec );
        i++;
    }
    Com_Printf( "%i aliases\n", i );
}

/*
============
Cmd_ClearAliases_f
============
*/
void Cmd_ClearAliases_f( void )
{
    cmd_alias_t* alias = cmd_aliases;
    cmd_alias_t* next;
    while( alias )
    {
        next = alias->next;
        Cmd_RemoveCommand( alias->name );
        Z_Free( alias->name );
        Z_Free( alias->exec );
        Z_Free( alias );
        alias = next;
    }
    cmd_aliases = NULL;
    
    // update autogen.cfg
    cvar_modifiedFlags |= CVAR_ARCHIVE;
}

/*
============
Cmd_UnAlias_f
============
*/
void Cmd_UnAlias_f( void )
{
    cmd_alias_t* alias, **back;
    StringEntry	name;
    
    // Get args
    if( Cmd_Argc() < 2 )
    {
        Com_Printf( "unalias <name> : delete an alias\n" );
        return;
    }
    name = Cmd_Argv( 1 );
    
    back = &cmd_aliases;
    while( 1 )
    {
        alias = *back;
        if( !alias )
            return;
        if( !Q_stricmp( name, alias->name ) )
        {
            *back = alias->next;
            Z_Free( alias->name );
            Z_Free( alias->exec );
            Z_Free( alias );
            Cmd_RemoveCommand( name );
            
            // update autogen.cfg
            cvar_modifiedFlags |= CVAR_ARCHIVE;
            return;
        }
        back = &alias->next;
    }
}

/*
============
Cmd_Alias_f
============
*/
void Cmd_Alias_f( void )
{
    cmd_alias_t*	alias;
    StringEntry	name;
    
    // Get args
    if( Cmd_Argc() < 2 )
    {
        Com_Printf( "alias <name> : show an alias\n" );
        Com_Printf( "alias <name> <exec> : create an alias\n" );
        return;
    }
    name = Cmd_Argv( 1 );
    
    // Find existing alias
    for( alias = cmd_aliases; alias; alias = alias->next )
    {
        if( !Q_stricmp( name, alias->name ) )
            break;
    }
    
    // Modify/create an alias
    if( Cmd_Argc() > 2 )
    {
        cmd_function_t*	cmd;
        
        // Crude protection from infinite loops
        if( !Q_stricmp( Cmd_Argv( 2 ), name ) )
        {
            Com_Printf( "Can't make an alias to itself\n" );
            return;
        }
        
        // Don't allow overriding builtin commands
        cmd = Cmd_FindCommand( name );
        if( cmd && cmd->function != Cmd_RunAlias_f )
        {
            Com_Printf( "Can't override a builtin function with an alias\n" );
            return;
        }
        
        // Create/update an alias
        if( !alias )
        {
            alias = ( cmd_alias_t* )S_Malloc( sizeof( cmd_alias_t ) );
            alias->name = CopyString( name );
            alias->exec = CopyString( Cmd_ArgsFrom( 2 ) );
            alias->next = cmd_aliases;
            cmd_aliases = alias;
            Cmd_AddCommand( name, Cmd_RunAlias_f );
        }
        else
        {
            // Reallocate the exec string
            Z_Free( alias->exec );
            alias->exec = CopyString( Cmd_ArgsFrom( 2 ) );
            Cmd_AddCommand( name, Cmd_RunAlias_f );
        }
    }
    
    // Show the alias
    if( !alias )
        Com_Printf( "Alias %s does not exist\n", name );
    else if( Cmd_Argc() == 2 )
        Com_Printf( "%s ==> %s\n", alias->name, alias->exec );
        
    // update autogen.cfg
    cvar_modifiedFlags |= CVAR_ARCHIVE;
}

/*
============
Cmd_AliasCompletion
============
*/
void	Cmd_AliasCompletion( void( *callback )( StringEntry s ) )
{
    cmd_alias_t*	alias;
    
    for( alias = cmd_aliases ; alias ; alias = alias->next )
    {
        callback( alias->name );
    }
}

/*
============
Cmd_DelayCompletion
============
*/
void	Cmd_DelayCompletion( void( *callback )( StringEntry s ) )
{
    S32 i;
    
    for( i = 0; i < MAX_DELAYED_COMMANDS; i++ )
    {
        if( delayed_cmd[i].delay != CMD_DELAY_UNUSED )
            callback( delayed_cmd[i].name );
    }
}

/*
=============================================================================

					COMMAND EXECUTION

=============================================================================
*/


/*
============
Cmd_SaveCmdContext
============
*/
void Cmd_SaveCmdContext( void )
{
    ::memcpy( &savedCmd, &cmd, sizeof( cmdContext_t ) );
}

/*
============
Cmd_RestoreCmdContext
============
*/
void Cmd_RestoreCmdContext( void )
{
    ::memcpy( &cmd, &savedCmd, sizeof( cmdContext_t ) );
}

/*
============
Cmd_Argc
============
*/
S32 Cmd_Argc( void )
{
    return cmd.argc;
}

/*
============
Cmd_Argv
============
*/
UTF8* Cmd_Argv( S32 arg )
{
    if( ( U32 )arg >= cmd.argc )
    {
        return "";
    }
    return cmd.argv[arg];
}

/*
============
Cmd_ArgvBuffer

The interpreted versions use this because
they can't have pointers returned to them
============
*/
void Cmd_ArgvBuffer( S32 arg, UTF8* buffer, S32 bufferLength )
{
    Q_strncpyz( buffer, Cmd_Argv( arg ), bufferLength );
}


/*
============
Cmd_Args

Returns a single string containing argv(1) to argv(argc()-1)
============
*/
UTF8*           Cmd_Args( void )
{
    static UTF8     cmd_args[MAX_STRING_CHARS];
    S32             i;
    
    cmd_args[0] = 0;
    for( i = 1; i < cmd.argc; i++ )
    {
        strcat( cmd_args, cmd.argv[i] );
        if( i != cmd.argc - 1 )
        {
            strcat( cmd_args, " " );
        }
    }
    
    return cmd_args;
}

/*
============
Cmd_Args

Returns a single string containing argv(arg) to argv(argc()-1)
============
*/
UTF8*           Cmd_ArgsFrom( S32 arg )
{
    static UTF8     cmd_args[BIG_INFO_STRING];
    S32             i;
    
    cmd_args[0] = 0;
    if( arg < 0 )
    {
        arg = 0;
    }
    for( i = arg; i < cmd.argc; i++ )
    {
        strcat( cmd_args, cmd.argv[i] );
        if( i != cmd.argc - 1 )
        {
            strcat( cmd_args, " " );
        }
    }
    
    return cmd_args;
}

/*
============
Cmd_ArgsBuffer

The interpreted versions use this because
they can't have pointers returned to them
============
*/
void Cmd_ArgsBuffer( UTF8* buffer, S32 bufferLength )
{
    Q_strncpyz( buffer, Cmd_Args(), bufferLength );
}

/*
============
Cmd_LiteralArgsBuffer

The interpreted versions use this because
they can't have pointers returned to them
============
*/
void	Cmd_LiteralArgsBuffer( UTF8* buffer, S32 bufferLength )
{
    Q_strncpyz( buffer, cmd.cmd, bufferLength );
}


/*
============
Cmd_Cmd

Retrieve the unmodified command string
For rcon use when you want to transmit without altering quoting
ATVI Wolfenstein Misc #284
============
*/
UTF8*           Cmd_Cmd()
{
    return cmd.cmd;
}

/*
============
Cmd_Cmd_FromNth

Retrieve the unmodified command string
For rcon use when you want to transmit without altering quoting
ATVI Wolfenstein Misc #284
============
*/
UTF8* Cmd_Cmd_FromNth( S32 count )
{
    UTF8* ret = cmd.cmd - 1;
    S32 i = 0, q = 0;
    
    while( count && *++ret )
    {
        if( !q && *ret == ' ' )
            i = 1; // space found outside quotation marks
        if( i && *ret != ' ' )
        {
            i = 0; // non-space found after space outside quotation marks
            --count; // one word fewer to scan
        }
        if( *ret == '"' )
            q = !q; // found a quotation mark
        else if( *ret == '\\' && ret[1] == '"' )
            ++ret;
    }
    
    return ret;
}


/*
============
Cmd_EscapeString

Escape all \$ in a string into \$$
============
*/
UTF8* Cmd_EscapeString( StringEntry in )
{
    static UTF8 buffer[MAX_STRING_CHARS];
    UTF8* out = buffer;
    while( *in )
    {
        if( out + 3 - buffer >= sizeof( buffer ) )
        {
            break;
        }
        if( in[0] == '\\' && in[1] == '$' )
        {
            out[0] = '\\';
            out[1] = '$';
            out[2] = '$';
            in += 2;
            out += 3;
        }
        else
        {
            *out++ = *in++;
        }
    }
    *out = '\0';
    return buffer;
}
/*
============
Cmd_TokenizeString

Parses the given string into command line tokens.
The text is copied to a seperate buffer and 0 characters
are inserted in the apropriate place, The argv array
will point into this temporary buffer.
============
*/
// NOTE TTimo define that to track tokenization issues
//#define TKN_DBG
static void Cmd_TokenizeString2( StringEntry text_in, bool ignoreQuotes, bool parseCvar )
{
    UTF8*	text;
    UTF8*	textOut;
    StringEntry cvarName;
    UTF8 buffer[ BIG_INFO_STRING ];
    
#ifdef TKN_DBG
    // FIXME TTimo blunt hook to try to find the tokenization of userinfo
    Com_DPrintf( "Cmd_TokenizeString: %s\n", text_in );
#endif
    
    // clear previous args
    cmd.argc = 0;
    cmd.cmd[ 0 ] = '\0';
    
    if( !text_in )
    {
        return;
    }
    
    // parse for cvar substitution
    if( parseCvar )
    {
        Q_strncpyz( buffer, text_in, sizeof( buffer ) );
        text = buffer;
        textOut = cmd.cmd;
        while( *text )
        {
            if( text[0] != '\\' || text[1] != '$' )
            {
                if( textOut == sizeof( cmd.cmd ) + cmd.cmd - 1 )
                    break;
                *textOut++ = *text++;
                continue;
            }
            text += 2;
            cvarName = text;
            while( *text && *text != '\\' )
                text++;
            if( *text == '\\' )
            {
                *text = 0;
                if( cvarSystem->Flags( cvarName ) != CVAR_NONEXISTENT )
                {
                    UTF8 cvarValue[ MAX_CVAR_VALUE_STRING ];
                    UTF8* badchar;
                    cvarSystem->VariableStringBuffer( cvarName, cvarValue, sizeof( cvarValue ) );
                    do
                    {
                        badchar = strchr( cvarValue, ';' );
                        if( badchar )
                            *badchar = '.';
                        else
                        {
                            badchar = strchr( cvarValue, '\n' );
                            if( badchar )
                                *badchar = '.';
                        }
                    }
                    while( badchar );
                    Q_strncpyz( textOut, cvarValue, sizeof( cmd.cmd ) - ( textOut - cmd.cmd ) );
                    while( *textOut )
                        textOut++;
                    if( textOut == sizeof( cmd.cmd ) + cmd.cmd - 1 )
                        break;
                }
                else
                {
                    cvarName -= 2;
                    while( *cvarName && textOut < sizeof( cmd.cmd ) + cmd.cmd - 1 )
                        *textOut++ = *cvarName++;
                    if( textOut == sizeof( cmd.cmd ) + cmd.cmd - 1 )
                        break;
                    *textOut++ = '\\';
                }
                text++;
            }
            else
            {
                cvarName -= 2;
                while( *cvarName && textOut < sizeof( cmd.cmd ) + cmd.cmd - 1 )
                    *textOut++ = *cvarName++;
                if( textOut == sizeof( cmd.cmd ) + cmd.cmd - 1 )
                    break;
            }
        }
        *textOut = 0;
        
        // "\$$" --> "\$"
        text = textOut = cmd.cmd;
        while( text[0] )
        {
            if( text[0] == '\\'  && text[1]  && text[1] == '$' && text[2] && text[2] == '$' )
            {
                textOut[0] = '\\';
                textOut[1] = '$';
                textOut += 2;
                text += 3;
            }
            else
                *textOut++ = *text++;
        }
        *textOut = '\0';
    }
    else
        Q_strncpyz( cmd.cmd, text_in, sizeof( cmd.cmd ) );
        
    text = cmd.cmd;
    textOut = cmd.tokenized;
    
    while( 1 )
    {
        if( cmd.argc == MAX_STRING_TOKENS )
        {
            return;			// this is usually something malicious
        }
        
        while( 1 )
        {
            // skip whitespace
            while( *text > '\0' && *text <= ' ' )
            {
                text++;
            }
            if( !*text )
            {
                return;			// all tokens parsed
            }
            
            // skip // comments
            if( text[0] == '/' && text[1] == '/' )
            {
                return;			// all tokens parsed
            }
            
            // skip /* */ comments
            if( text[0] == '/' && text[1] == '*' )
            {
                while( *text && ( text[0] != '*' || text[1] != '/' ) )
                {
                    text++;
                }
                if( !*text )
                {
                    return;		// all tokens parsed
                }
                text += 2;
            }
            else
            {
                break;			// we are ready to parse a token
            }
        }
        
        // handle quote escaping
        if( !ignoreQuotes && text[0] == '\\' && text[1] == '"' )
        {
            *textOut++ = '"';
            text += 2;
            continue;
        }
        
        // handle quoted strings
        if( !ignoreQuotes && *text == '"' )
        {
            cmd.argv[cmd.argc] = textOut;
            cmd.argc++;
            text++;
            while( *text && *text != '"' )
            {
                if( text[0] == '\\' && text[1] == '"' )
                {
                    *textOut++ = '"';
                    text += 2;
                    continue;
                }
                *textOut++ = *text++;
            }
            *textOut++ = 0;
            if( !*text )
            {
                return;		// all tokens parsed
            }
            text++;
            continue;
        }
        
        // regular token
        cmd.argv[cmd.argc] = textOut;
        cmd.argc++;
        
        // skip until whitespace, quote, or command
        while( *text > ' ' || *text < '\0' )
        {
            if( !ignoreQuotes && text[0] == '\\' && text[1] == '"' )
            {
                *textOut++ = '"';
                text += 2;
                continue;
            }
            
            if( !ignoreQuotes && text[0] == '"' )
            {
                break;
            }
            
            if( text[0] == '/' && text[1] == '/' )
            {
                break;
            }
            
            // skip /* */ comments
            if( text[0] == '/' && text[1] == '*' )
            {
                break;
            }
            
            *textOut++ = *text++;
        }
        
        *textOut++ = 0;
        
        if( !*text )
        {
            return;		// all tokens parsed
        }
    }
    
}

/*
============
Cmd_TokenizeString
============
*/
void Cmd_TokenizeString( StringEntry text_in )
{
    Cmd_TokenizeString2( text_in, false, false );
}

/*
============
Cmd_TokenizeStringIgnoreQuotes
============
*/
void Cmd_TokenizeStringIgnoreQuotes( StringEntry text_in )
{
    Cmd_TokenizeString2( text_in, true, false );
}

/*
============
Cmd_TokenizeStringParseCvar
============
*/
void Cmd_TokenizeStringParseCvar( StringEntry text_in )
{
    Cmd_TokenizeString2( text_in, false, true );
}


/*
============
Cmd_AddCommand
============
*/
void Cmd_AddCommand( StringEntry cmd_name, xcommand_t function )
{
    cmd_function_t* cmd;
    
    // fail if the command already exists
    for( cmd = cmd_functions; cmd; cmd = cmd->next )
    {
        if( !strcmp( cmd_name, cmd->name ) )
        {
            // allow completion-only commands to be silently doubled
            if( function != NULL )
            {
                Com_Printf( "Cmd_AddCommand: %s already defined\n", cmd_name );
            }
            return;
        }
    }
    
    // use a small malloc to avoid zone fragmentation
    cmd = ( cmd_function_t* )S_Malloc( sizeof( cmd_function_t ) );
    cmd->name = CopyString( cmd_name );
    cmd->function = function;
    cmd->next = cmd_functions;
    cmd->complete = NULL;
    cmd_functions = cmd;
}

/*
============
Cmd_SetCommandCompletionFunc
============
*/
void Cmd_SetCommandCompletionFunc( StringEntry command, completionFunc_t complete )
{
    cmd_function_t*	cmd;
    
    for( cmd = cmd_functions; cmd; cmd = cmd->next )
    {
        if( !Q_stricmp( command, cmd->name ) )
        {
            cmd->complete = complete;
        }
    }
}


/*
============
Cmd_RemoveCommand
============
*/
void Cmd_RemoveCommand( StringEntry cmd_name )
{
    cmd_function_t** back = &cmd_functions;
    
    while( 1 )
    {
        cmd_function_t* cmd = *back;
        if( !cmd )
        {
            // command wasn't active
            return;
        }
        if( !strcmp( cmd_name, cmd->name ) )
        {
            *back = cmd->next;
            Z_Free( cmd->name );
            Z_Free( cmd );
            return;
        }
        back = &cmd->next;
    }
}


/*
============
Cmd_CommandCompletion
============
*/
void Cmd_CommandCompletion( void ( *callback )( StringEntry s ) )
{
    cmd_function_t* cmd;
    
    for( cmd = cmd_functions; cmd; cmd = cmd->next )
    {
        callback( cmd->name );
    }
}

/*
============
Cmd_CompleteArgument
============
*/
void Cmd_CompleteArgument( StringEntry command, UTF8* args, S32 argNum )
{
    cmd_function_t*	cmd;
    
    for( cmd = cmd_functions; cmd; cmd = cmd->next )
    {
        if( !Q_stricmp( command, cmd->name ) && cmd->complete )
        {
            cmd->complete( args, argNum );
        }
    }
}


/*
============
Cmd_ExecuteString

A complete command line has been parsed, so try to execute it
============
*/
void Cmd_ExecuteString( StringEntry text )
{
    // execute the command line
    Cmd_TokenizeStringParseCvar( text );
    if( !Cmd_Argc() )
    {
        return;					// no tokens
    }
    
    // check registered command functions
    cmd_function_t* cmdFunc, **prev;
    for( prev = &cmd_functions; *prev; prev = &cmdFunc->next )
    {
        cmdFunc = *prev;
        if( !Q_stricmp( cmd.argv[0], cmdFunc->name ) )
        {
            // rearrange the links so that the command will be
            // near the head of the list next time it is used
            *prev = cmdFunc->next;
            cmdFunc->next = cmd_functions;
            cmd_functions = cmdFunc;
            
            // perform the action
            if( !cmdFunc->function )
            {
                // let the cgame or game handle it
                break;
            }
            else
            {
                cmdFunc->function();
            }
            return;
        }
    }
    
    // check cvars
    if( cvarSystem->Command() )
    {
        return;
    }
    
    // check client game commands
    if( com_cl_running && com_cl_running->integer && CL_GameCommand() )
    {
        return;
    }
    
    // check server game commands
    if( com_sv_running && com_sv_running->integer && serverGameSystem->GameCommand() )
    {
        return;
    }
    
    // check ui commands
    if( com_cl_running && com_cl_running->integer && UI_GameCommand() )
    {
        return;
    }
    
    // send it as a server command if we are connected
    // this will usually result in a chat message
    CL_ForwardCommandToServer( text );
}

/*
============
Cmd_List_f
============
*/
void Cmd_List_f( void )
{
    cmd_function_t* cmd;
    S32             i;
    UTF8*           match;
    
    if( Cmd_Argc() > 1 )
    {
        match = Cmd_Argv( 1 );
    }
    else
    {
        match = NULL;
    }
    
    i = 0;
    for( cmd = cmd_functions; cmd; cmd = cmd->next )
    {
        if( match && !Com_Filter( match, cmd->name, false ) )
        {
            continue;
        }
        
        Com_Printf( "%s\n", cmd->name );
        i++;
    }
    Com_Printf( "%i commands\n", i );
}

/*
==================
Cmd_CompleteCfgName
==================
*/
void Cmd_CompleteCfgName( UTF8* args, S32 argNum )
{
    if( argNum == 2 )
    {
        Field_CompleteFilename( "", "cfg", false );
    }
}

/*
==================
Cmd_CompleteAliasName
==================
*/
void Cmd_CompleteAliasName( UTF8* args, S32 argNum )
{
    if( argNum == 2 )
    {
        Field_CompleteAlias( );
    }
}

/*
==================
Cmd_CompleteConcat
==================
*/
void Cmd_CompleteConcat( UTF8* args, S32 argNum )
{
    // Skip
    UTF8* p = Com_SkipTokens( args, argNum - 1, " " );
    
    if( p > args )
        Field_CompleteCommand( p, false, true );
}

/*
==================
Cmd_CompleteIf
==================
*/
void Cmd_CompleteIf( UTF8* args, S32 argNum )
{
    if( argNum == 5 || argNum == 6 )
    {
        // Skip
        UTF8* p = Com_SkipTokens( args, argNum - 1, " " );
        
        if( p > args )
            Field_CompleteCommand( p, false, true );
    }
}

/*
==================
Cmd_CompleteDelay
==================
*/
void Cmd_CompleteDelay( UTF8* args, S32 argNum )
{
    if( argNum == 3 || argNum == 4 )
    {
        // Skip "delay "
        UTF8* p = Com_SkipTokens( args, 1, " " );
        
        if( p > args )
            Field_CompleteCommand( p, true, true );
    }
}

/*
==================
Cmd_CompleteUnDelay
==================
*/
void Cmd_CompleteUnDelay( UTF8* args, S32 argNum )
{
    if( argNum == 2 )
    {
        Field_CompleteDelay( );
    }
}


/*
============
Cmd_Init
============
*/
void Cmd_Init( void )
{
    Cmd_AddCommand( "cmdlist", Cmd_List_f );
    Cmd_AddCommand( "exec", Cmd_Exec_f );
    Cmd_SetCommandCompletionFunc( "exec", Cmd_CompleteCfgName );
    Cmd_AddCommand( "vstr", Cmd_Vstr_f );
    Cmd_SetCommandCompletionFunc( "vstr", &idCVarSystemLocal::CompleteCvarName );
    Cmd_AddCommand( "echo", Cmd_Echo_f );
    Cmd_AddCommand( "wait", Cmd_Wait_f );
#ifndef DEDICATED
    Cmd_AddCommand( "modcase", Cmd_ModCase_f );
#endif
    Cmd_AddCommand( "if", Cmd_If_f );
    Cmd_SetCommandCompletionFunc( "if", Cmd_CompleteIf );
    Cmd_AddCommand( "calc", Cmd_Calc_f );
    Cmd_AddCommand( "math", Cmd_Math_f );
    Cmd_SetCommandCompletionFunc( "math", &idCVarSystemLocal::CompleteCvarName );
    Cmd_AddCommand( "concat", Cmd_Concat_f );
    Cmd_SetCommandCompletionFunc( "concat", Cmd_CompleteConcat );
    Cmd_AddCommand( "strcmp", Cmd_Strcmp_f );
    Cmd_SetCommandCompletionFunc( "strcmp", Cmd_CompleteIf );
    Cmd_AddCommand( "alias", Cmd_Alias_f );
    Cmd_SetCommandCompletionFunc( "alias", Cmd_CompleteAliasName );
    Cmd_AddCommand( "unalias", Cmd_UnAlias_f );
    Cmd_SetCommandCompletionFunc( "unalias", Cmd_CompleteAliasName );
    Cmd_AddCommand( "aliaslist", Cmd_AliasList_f );
    Cmd_AddCommand( "clearaliases", Cmd_ClearAliases_f );
    Cmd_AddCommand( "delay", Cmd_Delay_f );
    Cmd_SetCommandCompletionFunc( "delay", Cmd_CompleteDelay );
    Cmd_AddCommand( "undelay", Cmd_Undelay_f );
    Cmd_SetCommandCompletionFunc( "undelay", Cmd_CompleteUnDelay );
    Cmd_AddCommand( "undelayAll", Cmd_UndelayAll_f );
    Cmd_AddCommand( "random", Cmd_Random_f );
}
