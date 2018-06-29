////////////////////////////////////////////////////////////////////////////////////////
// Copyright(C) 1999-2005 id Software, Inc.
// Copyright(C) 2000-2009 Darklegion Development
// Copyright(C) 2011-2018 Dusan Jocic <dusanjocic@msn.com>
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
// File name:   parse.cpp
// Version:     v1.00
// Created:
// Compilers:   Visual Studio 2015
// Description:
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#include <OWLib/precompiled.h>

//script flags
#define SCFL_NOERRORS             0x0001
#define SCFL_NOWARNINGS           0x0002
#define SCFL_NOSTRINGWHITESPACES  0x0004
#define SCFL_NOSTRINGESCAPECHARS  0x0008
#define SCFL_PRIMITIVE            0x0010
#define SCFL_NOBINARYNUMBERS      0x0020
#define SCFL_NONUMBERVALUES       0x0040

//token types
#define TT_STRING           1     // string
#define TT_LITERAL          2     // literal
#define TT_NUMBER           3     // number
#define TT_NAME             4     // name
#define TT_PUNCTUATION      5     // punctuation

//string sub type
//---------------
//    the length of the string
//literal sub type
//----------------
//    the ASCII code of the literal
//number sub type
//---------------
#define TT_DECIMAL          0x0008  // decimal number
#define TT_HEX              0x0100  // hexadecimal number
#define TT_OCTAL            0x0200  // octal number
#define TT_BINARY           0x0400  // binary number
#define TT_FLOAT            0x0800  // floating point number
#define TT_INTEGER          0x1000  // integer number
#define TT_LONG             0x2000  // long number
#define TT_UNSIGNED         0x4000  // unsigned number
//punctuation sub type
//--------------------
#define P_RSHIFT_ASSIGN       1
#define P_LSHIFT_ASSIGN       2
#define P_PARMS               3
#define P_PRECOMPMERGE        4

#define P_LOGIC_AND           5
#define P_LOGIC_OR            6
#define P_LOGIC_GEQ           7
#define P_LOGIC_LEQ           8
#define P_LOGIC_EQ            9
#define P_LOGIC_UNEQ          10

#define P_MUL_ASSIGN          11
#define P_DIV_ASSIGN          12
#define P_MOD_ASSIGN          13
#define P_ADD_ASSIGN          14
#define P_SUB_ASSIGN          15
#define P_INC                 16
#define P_DEC                 17

#define P_BIN_AND_ASSIGN      18
#define P_BIN_OR_ASSIGN       19
#define P_BIN_XOR_ASSIGN      20
#define P_RSHIFT              21
#define P_LSHIFT              22

#define P_POINTERREF          23
#define P_CPP1                24
#define P_CPP2                25
#define P_MUL                 26
#define P_DIV                 27
#define P_MOD                 28
#define P_ADD                 29
#define P_SUB                 30
#define P_ASSIGN              31

#define P_BIN_AND             32
#define P_BIN_OR              33
#define P_BIN_XOR             34
#define P_BIN_NOT             35

#define P_LOGIC_NOT           36
#define P_LOGIC_GREATER       37
#define P_LOGIC_LESS          38

#define P_REF                 39
#define P_COMMA               40
#define P_SEMICOLON           41
#define P_COLON               42
#define P_QUESTIONMARK        43

#define P_PARENTHESESOPEN     44
#define P_PARENTHESESCLOSE    45
#define P_BRACEOPEN           46
#define P_BRACECLOSE          47
#define P_SQBRACKETOPEN       48
#define P_SQBRACKETCLOSE      49
#define P_BACKSLASH           50

#define P_PRECOMP             51
#define P_DOLLAR              52

//name sub type
//-------------
//    the length of the name

//punctuation
typedef struct Punctuation_s
{
    UTF8* p;                    //punctuation character(s)
    S32 n;                      //punctuation indication
    struct Punctuation_s* next; //next punctuation
} Punctuation_t;

//token
typedef struct Token_s
{
    UTF8 string[MAX_TOKEN_CHARS]; //available token
    S32 type;                     //last read token type
    S32 subtype;                  //last read token sub type
    U64 intvalue;   //integer value
    F64 floatvalue;            //floating point value
    UTF8* whitespace_p;           //start of white space before token
    UTF8* endwhitespace_p;        //start of white space before token
    S32 line;                     //line the token was on
    S32 linescrossed;             //lines crossed in white space
    struct Token_s* next;         //next token in chain
} Token_t;

//script file
typedef struct Script_s
{
    UTF8 filename[1024];            //file name of the script
    UTF8* buffer;                   //buffer containing the script
    UTF8* script_p;                 //current pointer in the script
    UTF8* end_p;                    //pointer to the end of the script
    UTF8* lastscript_p;             //script pointer before reading token
    UTF8* whitespace_p;             //begin of the white space
    UTF8* endwhitespace_p;
    S32 length;                     //length of the script in bytes
    S32 line;                       //current line in script
    S32 lastline;                   //line before reading token
    S32 tokenavailable;             //set by UnreadLastToken
    S32 flags;                      //several script flags
    Punctuation_t* punctuations;    //the punctuations used in the script
    Punctuation_t** punctuationtable;
    Token_t token;                  //available token
    struct Script_s* next;          //next script in a chain
} Script_t;


#define DEFINE_FIXED      0x0001

#define BUILTIN_LINE      1
#define BUILTIN_FILE      2
#define BUILTIN_DATE      3
#define BUILTIN_TIME      4
#define BUILTIN_STDC      5

#define INDENT_IF         0x0001
#define INDENT_ELSE       0x0002
#define INDENT_ELIF       0x0004
#define INDENT_IFDEF      0x0008
#define INDENT_IFNDEF     0x0010

//macro definitions
typedef struct Define_s
{
    UTF8* name;                 //define name
    S32 flags;                  //define flags
    S32 builtin;                // > 0 if builtin define
    S32 numparms;               //number of define parameters
    Token_t* parms;             //define parameters
    Token_t* tokens;            //macro tokens (possibly containing parm tokens)
    struct Define_s* next;      //next defined macro in a list
    struct Define_s* hashnext;  //next define in the hash chain
} Define_t;

//indents
//used for conditional compilation directives:
//#if, #else, #elif, #ifdef, #ifndef
typedef struct Indent_s
{
    S32 type;               //indent type
    S32 skip;               //true if skipping current indent
    Script_t* script;       //script the indent was in
    struct Indent_s* next;  //next indent on the indent stack
} Indent_t;

//source file
typedef struct Source_s
{
    UTF8 filename[MAX_QPATH];     //file name of the script
    UTF8 includepath[MAX_QPATH];  //path to include files
    Punctuation_t* punctuations;  //punctuations to use
    Script_t* scriptstack;        //stack with scripts of the source
    Token_t* tokens;              //tokens to read first
    Define_t* defines;            //list with macro definitions
    Define_t** definehash;        //hash chain with defines
    Indent_t* indentstack;        //stack with indents
    S32 skip;                     // > 0 if skipping conditional code
    Token_t token;                //last read token
} Source_t;

#define MAX_DEFINEPARMS     128

//directive name with parse function
typedef struct directive_s
{
    UTF8* name;
    S32( *func )( Source_t* source );
} directive_t;

#define DEFINEHASHSIZE    1024

static S32 Parse_ReadToken( Source_t* source, Token_t* token );
static bool Parse_AddDefineToSourceFromString( Source_t* source,
        UTF8* string );

S32 numtokens;

//list with global defines added to every source loaded
Define_t* globaldefines;

//longer punctuations first
Punctuation_t Default_Punctuations[] =
{
    //binary operators
    {">>=", P_RSHIFT_ASSIGN, NULL},
    {"<<=", P_LSHIFT_ASSIGN, NULL},
    //
    {"...", P_PARMS, NULL},
    //define merge operator
    {"##", P_PRECOMPMERGE, NULL},
    //logic operators
    {"&&", P_LOGIC_AND, NULL},
    {"||", P_LOGIC_OR, NULL},
    {">=", P_LOGIC_GEQ, NULL},
    {"<=", P_LOGIC_LEQ, NULL},
    {"==", P_LOGIC_EQ, NULL},
    {"!=", P_LOGIC_UNEQ, NULL},
    //arithmatic operators
    {"*=", P_MUL_ASSIGN, NULL},
    {"/=", P_DIV_ASSIGN, NULL},
    {"%=", P_MOD_ASSIGN, NULL},
    {"+=", P_ADD_ASSIGN, NULL},
    {"-=", P_SUB_ASSIGN, NULL},
    {"++", P_INC, NULL},
    {"--", P_DEC, NULL},
    //binary operators
    {"&=", P_BIN_AND_ASSIGN, NULL},
    {"|=", P_BIN_OR_ASSIGN, NULL},
    {"^=", P_BIN_XOR_ASSIGN, NULL},
    {">>", P_RSHIFT, NULL},
    {"<<", P_LSHIFT, NULL},
    //reference operators
    {"->", P_POINTERREF, NULL},
    //C++
    {"::", P_CPP1, NULL},
    {".*", P_CPP2, NULL},
    //arithmatic operators
    {"*", P_MUL, NULL},
    {"/", P_DIV, NULL},
    {"%", P_MOD, NULL},
    {"+", P_ADD, NULL},
    {"-", P_SUB, NULL},
    {"=", P_ASSIGN, NULL},
    //binary operators
    {"&", P_BIN_AND, NULL},
    {"|", P_BIN_OR, NULL},
    {"^", P_BIN_XOR, NULL},
    {"~", P_BIN_NOT, NULL},
    //logic operators
    {"!", P_LOGIC_NOT, NULL},
    {">", P_LOGIC_GREATER, NULL},
    {"<", P_LOGIC_LESS, NULL},
    //reference operator
    {".", P_REF, NULL},
    //seperators
    {",", P_COMMA, NULL},
    {";", P_SEMICOLON, NULL},
    //label indication
    {":", P_COLON, NULL},
    //if statement
    {"?", P_QUESTIONMARK, NULL},
    //embracements
    {"(", P_PARENTHESESOPEN, NULL},
    {")", P_PARENTHESESCLOSE, NULL},
    {"{", P_BRACEOPEN, NULL},
    {"}", P_BRACECLOSE, NULL},
    {"[", P_SQBRACKETOPEN, NULL},
    {"]", P_SQBRACKETCLOSE, NULL},
    //
    {"\\", P_BACKSLASH, NULL},
    //precompiler operator
    {"#", P_PRECOMP, NULL},
    {"$", P_DOLLAR, NULL},
    {NULL, 0}
};

/*
===============
Parse_CreatePunctuationTable
===============
*/
static void Parse_CreatePunctuationTable( Script_t* script, Punctuation_t* punctuations )
{
    S32 i;
    Punctuation_t* p, *lastp, *newp;
    
    //get memory for the table
    if( !script->punctuationtable ) script->punctuationtable = ( Punctuation_t** )
                Z_Malloc( 256 * sizeof( Punctuation_t* ) );
    ::memset( script->punctuationtable, 0, 256 * sizeof( Punctuation_t* ) );
    //add the punctuations in the list to the punctuation table
    for( i = 0; punctuations[i].p; i++ )
    {
        newp = &punctuations[i];
        lastp = NULL;
        //sort the punctuations in this table entry on length (longer punctuations first)
        for( p = script->punctuationtable[( U32 ) newp->p[0]]; p; p = p->next )
        {
            if( strlen( p->p ) < strlen( newp->p ) )
            {
                newp->next = p;
                if( lastp ) lastp->next = newp;
                else script->punctuationtable[( U32 ) newp->p[0]] = newp;
                break;
            }
            lastp = p;
        }
        if( !p )
        {
            newp->next = NULL;
            if( lastp ) lastp->next = newp;
            else script->punctuationtable[( U32 ) newp->p[0]] = newp;
        }
    }
}

/*
===============
Parse_ScriptError
===============
*/
static __attribute__( ( format( printf, 2, 3 ) ) ) void Parse_ScriptError( Script_t* script, UTF8* str, ... )
{
    UTF8 text[1024];
    va_list ap;
    
    if( script->flags & SCFL_NOERRORS ) return;
    
    va_start( ap, str );
    vsprintf( text, str, ap );
    va_end( ap );
    Com_Printf( "file %s, line %d: %s\n", script->filename, script->line, text );
}

/*
===============
Parse_ScriptWarning
===============
*/
static __attribute__( ( format( printf, 2, 3 ) ) ) void Parse_ScriptWarning( Script_t* script, UTF8* str, ... )
{
    UTF8 text[1024];
    va_list ap;
    
    if( script->flags & SCFL_NOWARNINGS ) return;
    
    va_start( ap, str );
    vsprintf( text, str, ap );
    va_end( ap );
    Com_Printf( "file %s, line %d: %s\n", script->filename, script->line, text );
}

/*
===============
Parse_SetScriptPunctuations
===============
*/
static void Parse_SetScriptPunctuations( Script_t* script, Punctuation_t* p )
{
    if( p ) Parse_CreatePunctuationTable( script, p );
    else  Parse_CreatePunctuationTable( script, Default_Punctuations );
    if( p ) script->punctuations = p;
    else script->punctuations = Default_Punctuations;
}

/*
===============
Parse_ReadWhiteSpace
===============
*/
static S32 Parse_ReadWhiteSpace( Script_t* script )
{
    while( 1 )
    {
        //skip white space
        while( *script->script_p <= ' ' )
        {
            if( !*script->script_p ) return 0;
            if( *script->script_p == '\n' ) script->line++;
            script->script_p++;
        }
        //skip comments
        if( *script->script_p == '/' )
        {
            //comments //
            if( *( script->script_p + 1 ) == '/' )
            {
                script->script_p++;
                do
                {
                    script->script_p++;
                    if( !*script->script_p ) return 0;
                }
                while( *script->script_p != '\n' );
                script->line++;
                script->script_p++;
                if( !*script->script_p ) return 0;
                continue;
            }
            //comments /* */
            else if( *( script->script_p + 1 ) == '*' )
            {
                script->script_p++;
                do
                {
                    script->script_p++;
                    if( !*script->script_p ) return 0;
                    if( *script->script_p == '\n' ) script->line++;
                }
                while( !( *script->script_p == '*' && *( script->script_p + 1 ) == '/' ) );
                script->script_p++;
                if( !*script->script_p ) return 0;
                script->script_p++;
                if( !*script->script_p ) return 0;
                continue;
            }
        }
        break;
    }
    return 1;
}

/*
===============
Parse_ReadEscapeCharacter
===============
*/
static S32 Parse_ReadEscapeCharacter( Script_t* script, UTF8* ch )
{
    S32 c, val, i;
    
    //step over the leading '\\'
    script->script_p++;
    //determine the escape character
    switch( *script->script_p )
    {
        case '\\':
            c = '\\';
            break;
        case 'n':
            c = '\n';
            break;
        case 'r':
            c = '\r';
            break;
        case 't':
            c = '\t';
            break;
        case 'v':
            c = '\v';
            break;
        case 'b':
            c = '\b';
            break;
        case 'f':
            c = '\f';
            break;
        case 'a':
            c = '\a';
            break;
        case '\'':
            c = '\'';
            break;
        case '\"':
            c = '\"';
            break;
        case '\?':
            c = '\?';
            break;
        case 'x':
        {
            script->script_p++;
            for( i = 0, val = 0; ; i++, script->script_p++ )
            {
                c = *script->script_p;
                if( c >= '0' && c <= '9' ) c = c - '0';
                else if( c >= 'A' && c <= 'Z' ) c = c - 'A' + 10;
                else if( c >= 'a' && c <= 'z' ) c = c - 'a' + 10;
                else break;
                val = ( val << 4 ) + c;
            }
            script->script_p--;
            if( val > 0xFF )
            {
                Parse_ScriptWarning( script, "too large value in escape character" );
                val = 0xFF;
            }
            c = val;
            break;
        }
        default: //NOTE: decimal ASCII code, NOT octal
        {
            if( *script->script_p < '0' || *script->script_p > '9' ) Parse_ScriptError( script, "unknown escape UTF8" );
            for( i = 0, val = 0; ; i++, script->script_p++ )
            {
                c = *script->script_p;
                if( c >= '0' && c <= '9' ) c = c - '0';
                else break;
                val = val * 10 + c;
            }
            script->script_p--;
            if( val > 0xFF )
            {
                Parse_ScriptWarning( script, "too large value in escape character" );
                val = 0xFF;
            }
            c = val;
            break;
        }
    }
    //step over the escape character or the last digit of the number
    script->script_p++;
    //store the escape character
    *ch = c;
    //succesfully read escape character
    return 1;
}

/*
===============
Parse_ReadString

Reads C-like string. Escape characters are interpretted.
Quotes are included with the string.
Reads two strings with a white space between them as one string.
===============
*/
static S32 Parse_ReadString( Script_t* script, Token_t* token, S32 quote )
{
    S32 len, tmpline;
    UTF8* tmpscript_p;
    
    if( quote == '\"' ) token->type = TT_STRING;
    else token->type = TT_LITERAL;
    
    len = 0;
    //leading quote
    token->string[len++] = *script->script_p++;
    //
    while( 1 )
    {
        //minus 2 because trailing double quote and zero have to be appended
        if( len >= MAX_TOKEN_CHARS - 2 )
        {
            Parse_ScriptError( script, "string longer than MAX_TOKEN_CHARS = %d", MAX_TOKEN_CHARS );
            return 0;
        }
        //if there is an escape character and
        //if escape characters inside a string are allowed
        if( *script->script_p == '\\' && !( script->flags & SCFL_NOSTRINGESCAPECHARS ) )
        {
            if( !Parse_ReadEscapeCharacter( script, &token->string[len] ) )
            {
                token->string[len] = 0;
                return 0;
            }
            len++;
        }
        //if a trailing quote
        else if( *script->script_p == quote )
        {
            //step over the double quote
            script->script_p++;
            //if white spaces in a string are not allowed
            if( script->flags & SCFL_NOSTRINGWHITESPACES ) break;
            //
            tmpscript_p = script->script_p;
            tmpline = script->line;
            //read unusefull stuff between possible two following strings
            if( !Parse_ReadWhiteSpace( script ) )
            {
                script->script_p = tmpscript_p;
                script->line = tmpline;
                break;
            }
            //if there's no leading double qoute
            if( *script->script_p != quote )
            {
                script->script_p = tmpscript_p;
                script->line = tmpline;
                break;
            }
            //step over the new leading double quote
            script->script_p++;
        }
        else
        {
            if( *script->script_p == '\0' )
            {
                token->string[len] = 0;
                Parse_ScriptError( script, "missing trailing quote" );
                return 0;
            }
            if( *script->script_p == '\n' )
            {
                token->string[len] = 0;
                Parse_ScriptError( script, "newline inside string %s", token->string );
                return 0;
            }
            token->string[len++] = *script->script_p++;
        }
    }
    //trailing quote
    token->string[len++] = quote;
    //end string with a zero
    token->string[len] = '\0';
    //the sub type is the length of the string
    token->subtype = len;
    return 1;
}

/*
===============
Parse_ReadName
===============
*/
static S32 Parse_ReadName( Script_t* script, Token_t* token )
{
    S32 len = 0;
    UTF8 c;
    
    token->type = TT_NAME;
    do
    {
        token->string[len++] = *script->script_p++;
        if( len >= MAX_TOKEN_CHARS )
        {
            Parse_ScriptError( script, "name longer than MAX_TOKEN_CHARS = %d", MAX_TOKEN_CHARS );
            return 0;
        }
        c = *script->script_p;
    }
    while( ( c >= 'a' && c <= 'z' ) ||
            ( c >= 'A' && c <= 'Z' ) ||
            ( c >= '0' && c <= '9' ) ||
            c == '_' );
    token->string[len] = '\0';
    //the sub type is the length of the name
    token->subtype = len;
    return 1;
}

/*
===============
Parse_NumberValue
===============
*/
static void Parse_NumberValue( UTF8* string, S32 subtype, U64* intvalue, F64* floatvalue )
{
    U64 dotfound = 0;
    
    *intvalue = 0;
    *floatvalue = 0;
    //floating point number
    if( subtype & TT_FLOAT )
    {
        while( *string )
        {
            if( *string == '.' )
            {
                if( dotfound ) return;
                dotfound = 10;
                string++;
            }
            if( dotfound )
            {
                *floatvalue = *floatvalue + ( F64 )( *string - '0' ) /
                              ( F64 ) dotfound;
                dotfound *= 10;
            }
            else
            {
                *floatvalue = *floatvalue * 10.0 + ( F64 )( *string - '0' );
            }
            string++;
        }
        *intvalue = ( U64 ) * floatvalue;
    }
    else if( subtype & TT_DECIMAL )
    {
        while( *string ) *intvalue = *intvalue * 10 + ( *string++ - '0' );
        *floatvalue = *intvalue;
    }
    else if( subtype & TT_HEX )
    {
        //step over the leading 0x or 0X
        string += 2;
        while( *string )
        {
            *intvalue <<= 4;
            if( *string >= 'a' && *string <= 'f' ) *intvalue += *string - 'a' + 10;
            else if( *string >= 'A' && *string <= 'F' ) *intvalue += *string - 'A' + 10;
            else *intvalue += *string - '0';
            string++;
        }
        *floatvalue = *intvalue;
    }
    else if( subtype & TT_OCTAL )
    {
        //step over the first zero
        string += 1;
        while( *string ) *intvalue = ( *intvalue << 3 ) + ( *string++ - '0' );
        *floatvalue = *intvalue;
    }
    else if( subtype & TT_BINARY )
    {
        //step over the leading 0b or 0B
        string += 2;
        while( *string ) *intvalue = ( *intvalue << 1 ) + ( *string++ - '0' );
        *floatvalue = *intvalue;
    }
}

/*
===============
Parse_ReadNumber
===============
*/
static S32 Parse_ReadNumber( Script_t* script, Token_t* token )
{
    S32 len = 0, i;
    S32 octal, dot;
    UTF8 c;
//  U64 intvalue = 0;
//  F64 floatvalue = 0;

    token->type = TT_NUMBER;
    //check for a hexadecimal number
    if( *script->script_p == '0' &&
            ( *( script->script_p + 1 ) == 'x' ||
              *( script->script_p + 1 ) == 'X' ) )
    {
        token->string[len++] = *script->script_p++;
        token->string[len++] = *script->script_p++;
        c = *script->script_p;
        //hexadecimal
        while( ( c >= '0' && c <= '9' ) ||
                ( c >= 'a' && c <= 'f' ) ||
                ( c >= 'A' && c <= 'A' ) )
        {
            token->string[len++] = *script->script_p++;
            if( len >= MAX_TOKEN_CHARS )
            {
                Parse_ScriptError( script, "hexadecimal number longer than MAX_TOKEN_CHARS = %d", MAX_TOKEN_CHARS );
                return 0;
            }
            c = *script->script_p;
        }
        token->subtype |= TT_HEX;
    }
#ifdef BINARYNUMBERS
    //check for a binary number
    else if( *script->script_p == '0' &&
             ( *( script->script_p + 1 ) == 'b' ||
               *( script->script_p + 1 ) == 'B' ) )
    {
        token->string[len++] = *script->script_p++;
        token->string[len++] = *script->script_p++;
        c = *script->script_p;
        //binary
        while( c == '0' || c == '1' )
        {
            token->string[len++] = *script->script_p++;
            if( len >= MAX_TOKEN_CHARS )
            {
                Parse_ScriptError( script, "binary number longer than MAX_TOKEN_CHARS = %d", MAX_TOKEN_CHARS );
                return 0;
            }
            c = *script->script_p;
        }
        token->subtype |= TT_BINARY;
    }
#endif //BINARYNUMBERS
    else //decimal or octal integer or floating point number
    {
        octal = false;
        dot = false;
        if( *script->script_p == '0' ) octal = true;
        while( 1 )
        {
            c = *script->script_p;
            if( c == '.' ) dot = true;
            else if( c == '8' || c == '9' ) octal = false;
            else if( c < '0' || c > '9' ) break;
            token->string[len++] = *script->script_p++;
            if( len >= MAX_TOKEN_CHARS - 1 )
            {
                Parse_ScriptError( script, "number longer than MAX_TOKEN_CHARS = %d", MAX_TOKEN_CHARS );
                return 0;
            }
        }
        if( octal ) token->subtype |= TT_OCTAL;
        else token->subtype |= TT_DECIMAL;
        if( dot ) token->subtype |= TT_FLOAT;
    }
    for( i = 0; i < 2; i++ )
    {
        c = *script->script_p;
        //check for a LONG number
        if( ( c == 'l' || c == 'L' )
                && !( token->subtype & TT_LONG ) )
        {
            script->script_p++;
            token->subtype |= TT_LONG;
        }
        //check for an UNSIGNED number
        else if( ( c == 'u' || c == 'U' )
                 && !( token->subtype & ( TT_UNSIGNED | TT_FLOAT ) ) )
        {
            script->script_p++;
            token->subtype |= TT_UNSIGNED;
        }
    }
    token->string[len] = '\0';
    Parse_NumberValue( token->string, token->subtype, &token->intvalue, &token->floatvalue );
    if( !( token->subtype & TT_FLOAT ) ) token->subtype |= TT_INTEGER;
    return 1;
}

/*
===============
Parse_ReadPunctuation
===============
*/
static S32 Parse_ReadPunctuation( Script_t* script, Token_t* token )
{
    S32 len;
    UTF8* p;
    Punctuation_t* punc;
    
    for( punc = script->punctuationtable[( U32 ) * script->script_p]; punc; punc = punc->next )
    {
        p = punc->p;
        len = strlen( p );
        //if the script contains at least as much characters as the punctuation
        if( script->script_p + len <= script->end_p )
        {
            //if the script contains the punctuation
            if( !strncmp( script->script_p, p, len ) )
            {
                strncpy( token->string, p, MAX_TOKEN_CHARS );
                script->script_p += len;
                token->type = TT_PUNCTUATION;
                //sub type is the number of the punctuation
                token->subtype = punc->n;
                return 1;
            }
        }
    }
    return 0;
}

/*
===============
Parse_ReadPrimitive
===============
*/
static S32 Parse_ReadPrimitive( Script_t* script, Token_t* token )
{
    S32 len;
    
    len = 0;
    while( *script->script_p > ' ' && *script->script_p != ';' )
    {
        if( len >= MAX_TOKEN_CHARS )
        {
            Parse_ScriptError( script, "primitive token longer than MAX_TOKEN_CHARS = %d", MAX_TOKEN_CHARS );
            return 0;
        }
        token->string[len++] = *script->script_p++;
    }
    if( len >= MAX_TOKEN_CHARS )
    {
        // The last len++ made len==MAX_TOKEN_CHARS, which will overflow.
        // Bring it back down and ensure we null terminate.
        len = MAX_TOKEN_CHARS - 1;
    }
    token->string[len] = 0;
    //copy the token into the script structure
    ::memcpy( &script->token, token, sizeof( Token_t ) );
    //primitive reading successfull
    return 1;
}

/*
===============
Parse_ReadScriptToken
===============
*/
static S32 Parse_ReadScriptToken( Script_t* script, Token_t* token )
{
    //if there is a token available (from UnreadToken)
    if( script->tokenavailable )
    {
        script->tokenavailable = 0;
        ::memcpy( token, &script->token, sizeof( Token_t ) );
        return 1;
    }
    //save script pointer
    script->lastscript_p = script->script_p;
    //save line counter
    script->lastline = script->line;
    //clear the token stuff
    ::memset( token, 0, sizeof( Token_t ) );
    //start of the white space
    script->whitespace_p = script->script_p;
    token->whitespace_p = script->script_p;
    //read unusefull stuff
    if( !Parse_ReadWhiteSpace( script ) ) return 0;
    
    script->endwhitespace_p = script->script_p;
    token->endwhitespace_p = script->script_p;
    //line the token is on
    token->line = script->line;
    //number of lines crossed before token
    token->linescrossed = script->line - script->lastline;
    //if there is a leading double quote
    if( *script->script_p == '\"' )
    {
        if( !Parse_ReadString( script, token, '\"' ) ) return 0;
    }
    //if an literal
    else if( *script->script_p == '\'' )
    {
        //if (!Parse_ReadLiteral(script, token)) return 0;
        if( !Parse_ReadString( script, token, '\'' ) ) return 0;
    }
    //if there is a number
    else if( ( *script->script_p >= '0' && *script->script_p <= '9' ) ||
             ( *script->script_p == '.' &&
               ( *( script->script_p + 1 ) >= '0' && *( script->script_p + 1 ) <= '9' ) ) )
    {
        if( !Parse_ReadNumber( script, token ) ) return 0;
    }
    //if this is a primitive script
    else if( script->flags & SCFL_PRIMITIVE )
    {
        return Parse_ReadPrimitive( script, token );
    }
    //if there is a name
    else if( ( *script->script_p >= 'a' && *script->script_p <= 'z' ) ||
             ( *script->script_p >= 'A' && *script->script_p <= 'Z' ) ||
             *script->script_p == '_' )
    {
        if( !Parse_ReadName( script, token ) ) return 0;
    }
    //check for punctuations
    else if( !Parse_ReadPunctuation( script, token ) )
    {
        Parse_ScriptError( script, "can't read token" );
        return 0;
    }
    //copy the token into the script structure
    ::memcpy( &script->token, token, sizeof( Token_t ) );
    //succesfully read a token
    return 1;
}

/*
===============
Parse_StripDoubleQuotes
===============
*/
static void Parse_StripDoubleQuotes( UTF8* string )
{
    if( *string == '\"' )
    {
        memmove( string, string + 1, strlen( string ) + 1 );
    }
    if( string[strlen( string ) - 1] == '\"' )
    {
        string[strlen( string ) - 1] = '\0';
    }
}

/*
===============
Parse_EndOfScript
===============
*/
static S32 Parse_EndOfScript( Script_t* script )
{
    return script->script_p >= script->end_p;
}

/*
===============
Parse_LoadScriptFile
===============
*/
static Script_t* Parse_LoadScriptFile( StringEntry filename )
{
    fileHandle_t fp;
    S32 length;
    void* buffer;
    Script_t* script;
    
    length = FS_FOpenFileRead( filename, &fp, false );
    if( !fp ) return NULL;
    
    buffer = Z_Malloc( sizeof( Script_t ) + length + 1 );
    ::memset( buffer, 0, sizeof( Script_t ) + length + 1 );
    
    script = ( Script_t* ) buffer;
    ::memset( script, 0, sizeof( Script_t ) );
    strcpy( script->filename, filename );
    script->buffer = ( UTF8* ) buffer + sizeof( Script_t );
    script->buffer[length] = 0;
    script->length = length;
    //pointer in script buffer
    script->script_p = script->buffer;
    //pointer in script buffer before reading token
    script->lastscript_p = script->buffer;
    //pointer to end of script buffer
    script->end_p = &script->buffer[length];
    //set if there's a token available in script->token
    script->tokenavailable = 0;
    //
    script->line = 1;
    script->lastline = 1;
    //
    Parse_SetScriptPunctuations( script, NULL );
    //
    FS_Read( script->buffer, length, fp );
    FS_FCloseFile( fp );
    //
    
    return script;
}

/*
===============
Parse_LoadScriptMemory
===============
*/
static Script_t* Parse_LoadScriptMemory( UTF8* ptr, S32 length, UTF8* name )
{
    void* buffer;
    Script_t* script;
    
    buffer = Z_Malloc( sizeof( Script_t ) + length + 1 );
    ::memset( buffer, 0, sizeof( Script_t ) + length + 1 );
    
    script = ( Script_t* ) buffer;
    ::memset( script, 0, sizeof( Script_t ) );
    strcpy( script->filename, name );
    script->buffer = ( UTF8* ) buffer + sizeof( Script_t );
    script->buffer[length] = 0;
    script->length = length;
    //pointer in script buffer
    script->script_p = script->buffer;
    //pointer in script buffer before reading token
    script->lastscript_p = script->buffer;
    //pointer to end of script buffer
    script->end_p = &script->buffer[length];
    //set if there's a token available in script->token
    script->tokenavailable = 0;
    //
    script->line = 1;
    script->lastline = 1;
    //
    Parse_SetScriptPunctuations( script, NULL );
    //
    ::memcpy( script->buffer, ptr, length );
    //
    return script;
}

/*
===============
Parse_FreeScript
===============
*/
static void Parse_FreeScript( Script_t* script )
{
    if( script->punctuationtable ) Z_Free( script->punctuationtable );
    Z_Free( script );
}

/*
===============
Parse_SourceError
===============
*/
static __attribute__( ( format( printf, 2, 3 ) ) ) void Parse_SourceError( Source_t* source, UTF8* str, ... )
{
    UTF8 text[1024];
    va_list ap;
    
    va_start( ap, str );
    vsprintf( text, str, ap );
    va_end( ap );
    Com_Printf( "file %s, line %d: %s\n", source->scriptstack->filename, source->scriptstack->line, text );
}

/*
===============
Parse_SourceWarning
===============
*/
static __attribute__( ( format( printf, 2, 3 ) ) ) void Parse_SourceWarning( Source_t* source, UTF8* str, ... )
{
    UTF8 text[1024];
    va_list ap;
    
    va_start( ap, str );
    vsprintf( text, str, ap );
    va_end( ap );
    Com_Printf( "file %s, line %d: %s\n", source->scriptstack->filename, source->scriptstack->line, text );
}

/*
===============
Parse_PushIndent
===============
*/
static void Parse_PushIndent( Source_t* source, S32 type, S32 skip )
{
    Indent_t* indent;
    
    indent = ( Indent_t* ) Z_Malloc( sizeof( Indent_t ) );
    indent->type = type;
    indent->script = source->scriptstack;
    indent->skip = ( skip != 0 );
    source->skip += indent->skip;
    indent->next = source->indentstack;
    source->indentstack = indent;
}

/*
===============
Parse_PopIndent
===============
*/
static void Parse_PopIndent( Source_t* source, S32* type, S32* skip )
{
    Indent_t* indent;
    
    *type = 0;
    *skip = 0;
    
    indent = source->indentstack;
    if( !indent ) return;
    
    //must be an indent from the current script
    if( source->indentstack->script != source->scriptstack ) return;
    
    *type = indent->type;
    *skip = indent->skip;
    source->indentstack = source->indentstack->next;
    source->skip -= indent->skip;
    Z_Free( indent );
}

/*
===============
Parse_PushScript
===============
*/
static void Parse_PushScript( Source_t* source, Script_t* script )
{
    Script_t* s;
    
    for( s = source->scriptstack; s; s = s->next )
    {
        if( !Q_stricmp( s->filename, script->filename ) )
        {
            Parse_SourceError( source, "%s recursively included", script->filename );
            return;
        }
    }
    //push the script on the script stack
    script->next = source->scriptstack;
    source->scriptstack = script;
}

/*
===============
Parse_CopyToken
===============
*/
static Token_t* Parse_CopyToken( Token_t* token )
{
    Token_t* t;
    
//  t = (Token_t *) malloc(sizeof(Token_t));
    t = ( Token_t* ) Z_Malloc( sizeof( Token_t ) );
//  t = freetokens;
    if( !t )
    {
        Com_Error( ERR_FATAL, "out of token space\n" );
        return NULL;
    }
//  freetokens = freetokens->next;
    ::memcpy( t, token, sizeof( Token_t ) );
    t->next = NULL;
    numtokens++;
    return t;
}

/*
===============
Parse_FreeToken
===============
*/
static void Parse_FreeToken( Token_t* token )
{
    //free(token);
    Z_Free( token );
//  token->next = freetokens;
//  freetokens = token;
    numtokens--;
}

/*
===============
Parse_ReadSourceToken
===============
*/
static S32 Parse_ReadSourceToken( Source_t* source, Token_t* token )
{
    Token_t* t;
    Script_t* script;
    S32 type, skip, lines;
    
    lines = 0;
    //if there's no token already available
    while( !source->tokens )
    {
        //if there's a token to read from the script
        if( Parse_ReadScriptToken( source->scriptstack, token ) )
        {
            token->linescrossed += lines;
            return true;
        }
        
        // if lines were crossed before the end of the script, count them
        lines += source->scriptstack->line - source->scriptstack->lastline;
        
        //if at the end of the script
        if( Parse_EndOfScript( source->scriptstack ) )
        {
            //remove all indents of the script
            while( source->indentstack &&
                    source->indentstack->script == source->scriptstack )
            {
                Parse_SourceWarning( source, "missing #endif" );
                Parse_PopIndent( source, &type, &skip );
            }
        }
        //if this was the initial script
        if( !source->scriptstack->next ) return false;
        //remove the script and return to the last one
        script = source->scriptstack;
        source->scriptstack = source->scriptstack->next;
        Parse_FreeScript( script );
    }
    //copy the already available token
    ::memcpy( token, source->tokens, sizeof( Token_t ) );
    //free the read token
    t = source->tokens;
    source->tokens = source->tokens->next;
    Parse_FreeToken( t );
    return true;
}

/*
===============
Parse_UnreadSourceToken
===============
*/
static S32 Parse_UnreadSourceToken( Source_t* source, Token_t* token )
{
    Token_t* t;
    
    t = Parse_CopyToken( token );
    t->next = source->tokens;
    source->tokens = t;
    return true;
}

/*
===============
Parse_ReadDefineParms
===============
*/
static S32 Parse_ReadDefineParms( Source_t* source, Define_t* define, Token_t** parms, S32 maxparms )
{
    Token_t token, *t, *last;
    S32 i, done, lastcomma, numparms, indent;
    
    if( !Parse_ReadSourceToken( source, &token ) )
    {
        Parse_SourceError( source, "define %s missing parms", define->name );
        return false;
    }
    //
    if( define->numparms > maxparms )
    {
        Parse_SourceError( source, "define with more than %d parameters", maxparms );
        return false;
    }
    //
    for( i = 0; i < define->numparms; i++ ) parms[i] = NULL;
    //if no leading "("
    if( strcmp( token.string, "(" ) )
    {
        Parse_UnreadSourceToken( source, &token );
        Parse_SourceError( source, "define %s missing parms", define->name );
        return false;
    }
    //read the define parameters
    for( done = 0, numparms = 0, indent = 0; !done; )
    {
        if( numparms >= maxparms )
        {
            Parse_SourceError( source, "define %s with too many parms", define->name );
            return false;
        }
        if( numparms >= define->numparms )
        {
            Parse_SourceWarning( source, "define %s has too many parms", define->name );
            return false;
        }
        parms[numparms] = NULL;
        lastcomma = 1;
        last = NULL;
        while( !done )
        {
            //
            if( !Parse_ReadSourceToken( source, &token ) )
            {
                Parse_SourceError( source, "define %s incomplete", define->name );
                return false;
            }
            //
            if( !strcmp( token.string, "," ) )
            {
                if( indent <= 0 )
                {
                    if( lastcomma ) Parse_SourceWarning( source, "too many comma's" );
                    lastcomma = 1;
                    break;
                }
            }
            lastcomma = 0;
            //
            if( !strcmp( token.string, "(" ) )
            {
                indent++;
                continue;
            }
            else if( !strcmp( token.string, ")" ) )
            {
                if( --indent <= 0 )
                {
                    if( !parms[define->numparms - 1] )
                    {
                        Parse_SourceWarning( source, "too few define parms" );
                    }
                    done = 1;
                    break;
                }
            }
            //
            if( numparms < define->numparms )
            {
                //
                t = Parse_CopyToken( &token );
                t->next = NULL;
                if( last ) last->next = t;
                else parms[numparms] = t;
                last = t;
            }
        }
        numparms++;
    }
    return true;
}

/*
===============
Parse_StringizeTokens
===============
*/
static S32 Parse_StringizeTokens( Token_t* tokens, Token_t* token )
{
    Token_t* t;
    
    token->type = TT_STRING;
    token->whitespace_p = NULL;
    token->endwhitespace_p = NULL;
    token->string[0] = '\0';
    strcat( token->string, "\"" );
    for( t = tokens; t; t = t->next )
    {
        strncat( token->string, t->string, MAX_TOKEN_CHARS - strlen( token->string ) );
    }
    strncat( token->string, "\"", MAX_TOKEN_CHARS - strlen( token->string ) );
    return true;
}

/*
===============
Parse_MergeTokens
===============
*/
static S32 Parse_MergeTokens( Token_t* t1, Token_t* t2 )
{
    //merging of a name with a name or number
    if( t1->type == TT_NAME && ( t2->type == TT_NAME || t2->type == TT_NUMBER ) )
    {
        strcat( t1->string, t2->string );
        return true;
    }
    //merging of two strings
    if( t1->type == TT_STRING && t2->type == TT_STRING )
    {
        //remove trailing double quote
        t1->string[strlen( t1->string ) - 1] = '\0';
        //concat without leading double quote
        strcat( t1->string, &t2->string[1] );
        return true;
    }
    //FIXME: merging of two number of the same sub type
    return false;
}

/*
===============
Parse_NameHash
===============
*/
//UTF8 primes[16] = {1, 3, 5, 7, 11, 13, 17, 19, 23, 27, 29, 31, 37, 41, 43, 47};
static S32 Parse_NameHash( UTF8* name )
{
    register S32 hash, i;
    
    hash = 0;
    for( i = 0; name[i] != '\0'; i++ )
    {
        hash += name[i] * ( 119 + i );
        //hash += (name[i] << 7) + i;
        //hash += (name[i] << (i&15));
    }
    hash = ( hash ^ ( hash >> 10 ) ^ ( hash >> 20 ) ) & ( DEFINEHASHSIZE - 1 );
    return hash;
}

/*
===============
Parse_AddDefineToHash
===============
*/
static void Parse_AddDefineToHash( Define_t* define, Define_t** definehash )
{
    S32 hash;
    
    hash = Parse_NameHash( define->name );
    define->hashnext = definehash[hash];
    definehash[hash] = define;
}

/*
===============
Parse_FindHashedDefine
===============
*/
static Define_t* Parse_FindHashedDefine( Define_t** definehash, UTF8* name )
{
    Define_t* d;
    S32 hash;
    
    hash = Parse_NameHash( name );
    for( d = definehash[hash]; d; d = d->hashnext )
    {
        if( !strcmp( d->name, name ) ) return d;
    }
    return NULL;
}

/*
===============
Parse_FindDefineParm
===============
*/
static S32 Parse_FindDefineParm( Define_t* define, UTF8* name )
{
    Token_t* p;
    S32 i;
    
    i = 0;
    for( p = define->parms; p; p = p->next )
    {
        if( !strcmp( p->string, name ) ) return i;
        i++;
    }
    return -1;
}

/*
===============
Parse_FreeDefine
===============
*/
static void Parse_FreeDefine( Define_t* define )
{
    Token_t* t, *next;
    
    //free the define parameters
    for( t = define->parms; t; t = next )
    {
        next = t->next;
        Parse_FreeToken( t );
    }
    //free the define tokens
    for( t = define->tokens; t; t = next )
    {
        next = t->next;
        Parse_FreeToken( t );
    }
    //free the define
    Z_Free( define );
}

/*
===============
Parse_ExpandBuiltinDefine
===============
*/
static S32 Parse_ExpandBuiltinDefine( Source_t* source, Token_t* deftoken, Define_t* define,
                                      Token_t** firsttoken, Token_t** lasttoken )
{
    Token_t* token;
    time_t t;
    
    UTF8* curtime;
    
    token = Parse_CopyToken( deftoken );
    switch( define->builtin )
    {
        case BUILTIN_LINE:
        {
            sprintf( token->string, "%d", deftoken->line );
            token->intvalue = deftoken->line;
            token->floatvalue = deftoken->line;
            token->type = TT_NUMBER;
            token->subtype = TT_DECIMAL | TT_INTEGER;
            *firsttoken = token;
            *lasttoken = token;
            break;
        }
        case BUILTIN_FILE:
        {
            strcpy( token->string, source->scriptstack->filename );
            token->type = TT_NAME;
            token->subtype = strlen( token->string );
            *firsttoken = token;
            *lasttoken = token;
            break;
        }
        case BUILTIN_DATE:
        {
            t = time( NULL );
            curtime = ctime( &t );
            strcpy( token->string, "\"" );
            strncat( token->string, curtime + 4, 7 );
            strncat( token->string + 7, curtime + 20, 4 );
            strcat( token->string, "\"" );
            free( curtime );
            token->type = TT_NAME;
            token->subtype = strlen( token->string );
            *firsttoken = token;
            *lasttoken = token;
            break;
        }
        case BUILTIN_TIME:
        {
            t = time( NULL );
            curtime = ctime( &t );
            strcpy( token->string, "\"" );
            strncat( token->string, curtime + 11, 8 );
            strcat( token->string, "\"" );
            free( curtime );
            token->type = TT_NAME;
            token->subtype = strlen( token->string );
            *firsttoken = token;
            *lasttoken = token;
            break;
        }
        case BUILTIN_STDC:
        default:
        {
            *firsttoken = NULL;
            *lasttoken = NULL;
            break;
        }
    }
    return true;
}

/*
===============
Parse_ExpandDefine
===============
*/
static S32 Parse_ExpandDefine( Source_t* source, Token_t* deftoken, Define_t* define,
                               Token_t** firsttoken, Token_t** lasttoken )
{
    Token_t* parms[MAX_DEFINEPARMS], *dt, *pt, *t;
    Token_t* t1, *t2, *first, *last, *nextpt, token;
    S32 parmnum, i;
    
    //if it is a builtin define
    if( define->builtin )
    {
        return Parse_ExpandBuiltinDefine( source, deftoken, define, firsttoken, lasttoken );
    }
    //if the define has parameters
    if( define->numparms )
    {
        if( !Parse_ReadDefineParms( source, define, parms, MAX_DEFINEPARMS ) ) return false;
    }
    //empty list at first
    first = NULL;
    last = NULL;
    //create a list with tokens of the expanded define
    for( dt = define->tokens; dt; dt = dt->next )
    {
        parmnum = -1;
        //if the token is a name, it could be a define parameter
        if( dt->type == TT_NAME )
        {
            parmnum = Parse_FindDefineParm( define, dt->string );
        }
        //if it is a define parameter
        if( parmnum >= 0 )
        {
            for( pt = parms[parmnum]; pt; pt = pt->next )
            {
                t = Parse_CopyToken( pt );
                //add the token to the list
                t->next = NULL;
                if( last ) last->next = t;
                else first = t;
                last = t;
            }
        }
        else
        {
            //if stringizing operator
            if( dt->string[0] == '#' && dt->string[1] == '\0' )
            {
                //the stringizing operator must be followed by a define parameter
                if( dt->next ) parmnum = Parse_FindDefineParm( define, dt->next->string );
                else parmnum = -1;
                //
                if( parmnum >= 0 )
                {
                    //step over the stringizing operator
                    dt = dt->next;
                    //stringize the define parameter tokens
                    if( !Parse_StringizeTokens( parms[parmnum], &token ) )
                    {
                        Parse_SourceError( source, "can't stringize tokens" );
                        return false;
                    }
                    t = Parse_CopyToken( &token );
                }
                else
                {
                    Parse_SourceWarning( source, "stringizing operator without define parameter" );
                    continue;
                }
            }
            else
            {
                t = Parse_CopyToken( dt );
            }
            //add the token to the list
            t->next = NULL;
            if( last ) last->next = t;
            else first = t;
            last = t;
        }
    }
    //check for the merging operator
    for( t = first; t; )
    {
        if( t->next )
        {
            //if the merging operator
            if( t->next->string[0] == '#' && t->next->string[1] == '#' )
            {
                t1 = t;
                t2 = t->next->next;
                if( t2 )
                {
                    if( !Parse_MergeTokens( t1, t2 ) )
                    {
                        Parse_SourceError( source, "can't merge %s with %s", t1->string, t2->string );
                        return false;
                    }
                    Parse_FreeToken( t1->next );
                    t1->next = t2->next;
                    if( t2 == last ) last = t1;
                    Parse_FreeToken( t2 );
                    continue;
                }
            }
        }
        t = t->next;
    }
    //store the first and last token of the list
    *firsttoken = first;
    *lasttoken = last;
    //free all the parameter tokens
    for( i = 0; i < define->numparms; i++ )
    {
        for( pt = parms[i]; pt; pt = nextpt )
        {
            nextpt = pt->next;
            Parse_FreeToken( pt );
        }
    }
    //
    return true;
}

/*
===============
Parse_ExpandDefineIntoSource
===============
*/
static S32 Parse_ExpandDefineIntoSource( Source_t* source, Token_t* deftoken, Define_t* define )
{
    Token_t* firsttoken, *lasttoken;
    
    if( !Parse_ExpandDefine( source, deftoken, define, &firsttoken, &lasttoken ) ) return false;
    
    if( firsttoken && lasttoken )
    {
        lasttoken->next = source->tokens;
        source->tokens = firsttoken;
        return true;
    }
    return false;
}

/*
===============
Parse_ConvertPath
===============
*/
static void Parse_ConvertPath( UTF8* path )
{
    UTF8* ptr;
    
    //remove double path seperators
    for( ptr = path; *ptr; )
    {
        if( ( *ptr == '\\' || *ptr == '/' ) &&
                ( *( ptr + 1 ) == '\\' || *( ptr + 1 ) == '/' ) )
        {
            memmove( ptr, ptr + 1, strlen( ptr ) );
        }
        else
        {
            ptr++;
        }
    }
    //set OS dependent path seperators
    for( ptr = path; *ptr; )
    {
        if( *ptr == '/' || *ptr == '\\' ) *ptr = PATH_SEP;
        ptr++;
    }
}

/*
===============
Parse_ReadLine

reads a token from the current line, continues reading on the next
line only if a backslash '\' is encountered.
===============
*/
static S32 Parse_ReadLine( Source_t* source, Token_t* token )
{
    S32 crossline;
    
    crossline = 0;
    do
    {
        if( !Parse_ReadSourceToken( source, token ) ) return false;
        
        if( token->linescrossed > crossline )
        {
            Parse_UnreadSourceToken( source, token );
            return false;
        }
        crossline = 1;
    }
    while( !strcmp( token->string, "\\" ) );
    return true;
}

/*
===============
Parse_OperatorPriority
===============
*/
typedef struct operator_s
{
    S32 _operator;
    S32 priority;
    S32 parentheses;
    struct operator_s* prev, *next;
} operator_t;

typedef struct value_s
{
    S64 intvalue;
    F64 floatvalue;
    S32 parentheses;
    struct value_s* prev, *next;
} value_t;

static S32 Parse_OperatorPriority( S32 op )
{
    switch( op )
    {
        case P_MUL:
            return 15;
        case P_DIV:
            return 15;
        case P_MOD:
            return 15;
        case P_ADD:
            return 14;
        case P_SUB:
            return 14;
            
        case P_LOGIC_AND:
            return 7;
        case P_LOGIC_OR:
            return 6;
        case P_LOGIC_GEQ:
            return 12;
        case P_LOGIC_LEQ:
            return 12;
        case P_LOGIC_EQ:
            return 11;
        case P_LOGIC_UNEQ:
            return 11;
            
        case P_LOGIC_NOT:
            return 16;
        case P_LOGIC_GREATER:
            return 12;
        case P_LOGIC_LESS:
            return 12;
            
        case P_RSHIFT:
            return 13;
        case P_LSHIFT:
            return 13;
            
        case P_BIN_AND:
            return 10;
        case P_BIN_OR:
            return 8;
        case P_BIN_XOR:
            return 9;
        case P_BIN_NOT:
            return 16;
            
        case P_COLON:
            return 5;
        case P_QUESTIONMARK:
            return 5;
    }
    return false;
}

#define MAX_VALUES    64
#define MAX_OPERATORS 64
#define AllocValue(val)                 \
  if (numvalues >= MAX_VALUES) {            \
    Parse_SourceError(source, "out of value space\n");    \
    error = 1;                    \
    break;                      \
  }                         \
  else                        \
    val = &value_heap[numvalues++];
#define FreeValue(val)
//
#define AllocOperator(op)               \
  if (numoperators >= MAX_OPERATORS) {        \
    Parse_SourceError(source, "out of operator space\n"); \
    error = 1;                    \
    break;                      \
  }                         \
  else                        \
    op = &operator_heap[numoperators++];
#define FreeOperator(op)

/*
===============
Parse_EvaluateTokens
===============
*/
static S32 Parse_EvaluateTokens( Source_t* source, Token_t* tokens, S64* intvalue, F64* floatvalue, S32 integer )
{
    operator_t* o, *firstoperator, *lastoperator;
    value_t* v, *firstvalue, *lastvalue, *v1, *v2;
    Token_t* t;
    S32 brace = 0;
    S32 parentheses = 0;
    S32 error = 0;
    S32 lastwasvalue = 0;
    S32 negativevalue = 0;
    S32 questmarkintvalue = 0;
    F64 questmarkfloatvalue = 0;
    S32 gotquestmarkvalue = false;
    S32 lastoperatortype = 0;
    //
    operator_t operator_heap[MAX_OPERATORS];
    S32 numoperators = 0;
    value_t value_heap[MAX_VALUES];
    S32 numvalues = 0;
    
    firstoperator = lastoperator = NULL;
    firstvalue = lastvalue = NULL;
    if( intvalue ) *intvalue = 0;
    if( floatvalue ) *floatvalue = 0;
    for( t = tokens; t; t = t->next )
    {
        switch( t->type )
        {
            case TT_NAME:
            {
                if( lastwasvalue || negativevalue )
                {
                    Parse_SourceError( source, "syntax error in #if/#elif" );
                    error = 1;
                    break;
                }
                if( strcmp( t->string, "defined" ) )
                {
                    Parse_SourceError( source, "undefined name %s in #if/#elif", t->string );
                    error = 1;
                    break;
                }
                t = t->next;
                if( !strcmp( t->string, "(" ) )
                {
                    brace = true;
                    t = t->next;
                }
                if( !t || t->type != TT_NAME )
                {
                    Parse_SourceError( source, "defined without name in #if/#elif" );
                    error = 1;
                    break;
                }
                //v = (value_t *) Z_Malloc(sizeof(value_t));
                AllocValue( v );
                if( Parse_FindHashedDefine( source->definehash, t->string ) )
                {
                    v->intvalue = 1;
                    v->floatvalue = 1;
                }
                else
                {
                    v->intvalue = 0;
                    v->floatvalue = 0;
                }
                v->parentheses = parentheses;
                v->next = NULL;
                v->prev = lastvalue;
                if( lastvalue ) lastvalue->next = v;
                else firstvalue = v;
                lastvalue = v;
                if( brace )
                {
                    t = t->next;
                    if( !t || strcmp( t->string, ")" ) )
                    {
                        Parse_SourceError( source, "defined without ) in #if/#elif" );
                        error = 1;
                        break;
                    }
                }
                brace = false;
                // defined() creates a value
                lastwasvalue = 1;
                break;
            }
            case TT_NUMBER:
            {
                if( lastwasvalue )
                {
                    Parse_SourceError( source, "syntax error in #if/#elif" );
                    error = 1;
                    break;
                }
                //v = (value_t *) Z_Malloc(sizeof(value_t));
                AllocValue( v );
                if( negativevalue )
                {
                    v->intvalue = - ( S32 ) t->intvalue;
                    v->floatvalue = - t->floatvalue;
                }
                else
                {
                    v->intvalue = t->intvalue;
                    v->floatvalue = t->floatvalue;
                }
                v->parentheses = parentheses;
                v->next = NULL;
                v->prev = lastvalue;
                if( lastvalue ) lastvalue->next = v;
                else firstvalue = v;
                lastvalue = v;
                //last token was a value
                lastwasvalue = 1;
                //
                negativevalue = 0;
                break;
            }
            case TT_PUNCTUATION:
            {
                if( negativevalue )
                {
                    Parse_SourceError( source, "misplaced minus sign in #if/#elif" );
                    error = 1;
                    break;
                }
                if( t->subtype == P_PARENTHESESOPEN )
                {
                    parentheses++;
                    break;
                }
                else if( t->subtype == P_PARENTHESESCLOSE )
                {
                    parentheses--;
                    if( parentheses < 0 )
                    {
                        Parse_SourceError( source, "too many ) in #if/#elsif" );
                        error = 1;
                    }
                    break;
                }
                //check for invalid operators on floating point values
                if( !integer )
                {
                    if( t->subtype == P_BIN_NOT || t->subtype == P_MOD ||
                            t->subtype == P_RSHIFT || t->subtype == P_LSHIFT ||
                            t->subtype == P_BIN_AND || t->subtype == P_BIN_OR ||
                            t->subtype == P_BIN_XOR )
                    {
                        Parse_SourceError( source, "illigal operator %s on floating point operands\n", t->string );
                        error = 1;
                        break;
                    }
                }
                switch( t->subtype )
                {
                    case P_LOGIC_NOT:
                    case P_BIN_NOT:
                    {
                        if( lastwasvalue )
                        {
                            Parse_SourceError( source, "! or ~ after value in #if/#elif" );
                            error = 1;
                            break;
                        }
                        break;
                    }
                    case P_INC:
                    case P_DEC:
                    {
                        Parse_SourceError( source, "++ or -- used in #if/#elif" );
                        break;
                    }
                    case P_SUB:
                    {
                        if( !lastwasvalue )
                        {
                            negativevalue = 1;
                            break;
                        }
                    }
                    
                    case P_MUL:
                    case P_DIV:
                    case P_MOD:
                    case P_ADD:
                    
                    case P_LOGIC_AND:
                    case P_LOGIC_OR:
                    case P_LOGIC_GEQ:
                    case P_LOGIC_LEQ:
                    case P_LOGIC_EQ:
                    case P_LOGIC_UNEQ:
                    
                    case P_LOGIC_GREATER:
                    case P_LOGIC_LESS:
                    
                    case P_RSHIFT:
                    case P_LSHIFT:
                    
                    case P_BIN_AND:
                    case P_BIN_OR:
                    case P_BIN_XOR:
                    
                    case P_COLON:
                    case P_QUESTIONMARK:
                    {
                        if( !lastwasvalue )
                        {
                            Parse_SourceError( source, "operator %s after operator in #if/#elif", t->string );
                            error = 1;
                            break;
                        }
                        break;
                    }
                    default:
                    {
                        Parse_SourceError( source, "invalid operator %s in #if/#elif", t->string );
                        error = 1;
                        break;
                    }
                }
                if( !error && !negativevalue )
                {
                    //o = (operator_t *) Z_Malloc(sizeof(operator_t));
                    AllocOperator( o );
                    o->_operator = t->subtype;
                    o->priority = Parse_OperatorPriority( t->subtype );
                    o->parentheses = parentheses;
                    o->next = NULL;
                    o->prev = lastoperator;
                    if( lastoperator ) lastoperator->next = o;
                    else firstoperator = o;
                    lastoperator = o;
                    lastwasvalue = 0;
                }
                break;
            }
            default:
            {
                Parse_SourceError( source, "unknown %s in #if/#elif", t->string );
                error = 1;
                break;
            }
        }
        if( error ) break;
    }
    if( !error )
    {
        if( !lastwasvalue )
        {
            Parse_SourceError( source, "trailing operator in #if/#elif" );
            error = 1;
        }
        else if( parentheses )
        {
            Parse_SourceError( source, "too many ( in #if/#elif" );
            error = 1;
        }
    }
    //
    gotquestmarkvalue = false;
    questmarkintvalue = 0;
    questmarkfloatvalue = 0;
    //while there are operators
    while( !error && firstoperator )
    {
        v = firstvalue;
        for( o = firstoperator; o->next; o = o->next )
        {
            //if the current operator is nested deeper in parentheses
            //than the next operator
            if( o->parentheses > o->next->parentheses ) break;
            //if the current and next operator are nested equally deep in parentheses
            if( o->parentheses == o->next->parentheses )
            {
                //if the priority of the current operator is equal or higher
                //than the priority of the next operator
                if( o->priority >= o->next->priority ) break;
            }
            //if the arity of the operator isn't equal to 1
            if( o->_operator != P_LOGIC_NOT
                    && o->_operator != P_BIN_NOT ) v = v->next;
            //if there's no value or no next value
            if( !v )
            {
                Parse_SourceError( source, "mising values in #if/#elif" );
                error = 1;
                break;
            }
        }
        if( error ) break;
        v1 = v;
        v2 = v->next;
        switch( o->_operator )
        {
            case P_LOGIC_NOT:
                v1->intvalue = !v1->intvalue;
                v1->floatvalue = !v1->floatvalue;
                break;
            case P_BIN_NOT:
                v1->intvalue = ~v1->intvalue;
                break;
            case P_MUL:
                v1->intvalue *= v2->intvalue;
                v1->floatvalue *= v2->floatvalue;
                break;
            case P_DIV:
                if( !v2->intvalue || !v2->floatvalue )
                {
                    Parse_SourceError( source, "divide by zero in #if/#elif\n" );
                    error = 1;
                    break;
                }
                v1->intvalue /= v2->intvalue;
                v1->floatvalue /= v2->floatvalue;
                break;
            case P_MOD:
                if( !v2->intvalue )
                {
                    Parse_SourceError( source, "divide by zero in #if/#elif\n" );
                    error = 1;
                    break;
                }
                v1->intvalue %= v2->intvalue;
                break;
            case P_ADD:
                v1->intvalue += v2->intvalue;
                v1->floatvalue += v2->floatvalue;
                break;
            case P_SUB:
                v1->intvalue -= v2->intvalue;
                v1->floatvalue -= v2->floatvalue;
                break;
            case P_LOGIC_AND:
                v1->intvalue = v1->intvalue && v2->intvalue;
                v1->floatvalue = v1->floatvalue && v2->floatvalue;
                break;
            case P_LOGIC_OR:
                v1->intvalue = v1->intvalue || v2->intvalue;
                v1->floatvalue = v1->floatvalue || v2->floatvalue;
                break;
            case P_LOGIC_GEQ:
                v1->intvalue = v1->intvalue >= v2->intvalue;
                v1->floatvalue = v1->floatvalue >= v2->floatvalue;
                break;
            case P_LOGIC_LEQ:
                v1->intvalue = v1->intvalue <= v2->intvalue;
                v1->floatvalue = v1->floatvalue <= v2->floatvalue;
                break;
            case P_LOGIC_EQ:
                v1->intvalue = v1->intvalue == v2->intvalue;
                v1->floatvalue = v1->floatvalue == v2->floatvalue;
                break;
            case P_LOGIC_UNEQ:
                v1->intvalue = v1->intvalue != v2->intvalue;
                v1->floatvalue = v1->floatvalue != v2->floatvalue;
                break;
            case P_LOGIC_GREATER:
                v1->intvalue = v1->intvalue > v2->intvalue;
                v1->floatvalue = v1->floatvalue > v2->floatvalue;
                break;
            case P_LOGIC_LESS:
                v1->intvalue = v1->intvalue < v2->intvalue;
                v1->floatvalue = v1->floatvalue < v2->floatvalue;
                break;
            case P_RSHIFT:
                v1->intvalue >>= v2->intvalue;
                break;
            case P_LSHIFT:
                v1->intvalue <<= v2->intvalue;
                break;
            case P_BIN_AND:
                v1->intvalue &= v2->intvalue;
                break;
            case P_BIN_OR:
                v1->intvalue |= v2->intvalue;
                break;
            case P_BIN_XOR:
                v1->intvalue ^= v2->intvalue;
                break;
            case P_COLON:
            {
                if( !gotquestmarkvalue )
                {
                    Parse_SourceError( source, ": without ? in #if/#elif" );
                    error = 1;
                    break;
                }
                if( integer )
                {
                    if( !questmarkintvalue ) v1->intvalue = v2->intvalue;
                }
                else
                {
                    if( !questmarkfloatvalue ) v1->floatvalue = v2->floatvalue;
                }
                gotquestmarkvalue = false;
                break;
            }
            case P_QUESTIONMARK:
            {
                if( gotquestmarkvalue )
                {
                    Parse_SourceError( source, "? after ? in #if/#elif" );
                    error = 1;
                    break;
                }
                questmarkintvalue = v1->intvalue;
                questmarkfloatvalue = v1->floatvalue;
                gotquestmarkvalue = true;
                break;
            }
        }
        if( error ) break;
        lastoperatortype = o->_operator;
        //if not an operator with arity 1
        if( o->_operator != P_LOGIC_NOT
                && o->_operator != P_BIN_NOT )
        {
            //remove the second value if not question mark operator
            if( o->_operator != P_QUESTIONMARK ) v = v->next;
            //
            if( v->prev ) v->prev->next = v->next;
            else firstvalue = v->next;
            if( v->next ) v->next->prev = v->prev;
            else lastvalue = v->prev;
            //Z_Free(v);
            FreeValue( v );
        }
        //remove the operator
        if( o->prev ) o->prev->next = o->next;
        else firstoperator = o->next;
        if( o->next ) o->next->prev = o->prev;
        else lastoperator = o->prev;
        //Z_Free(o);
        FreeOperator( o );
    }
    if( firstvalue )
    {
        if( intvalue ) *intvalue = firstvalue->intvalue;
        if( floatvalue ) *floatvalue = firstvalue->floatvalue;
    }
    for( o = firstoperator; o; o = lastoperator )
    {
        lastoperator = o->next;
        //Z_Free(o);
        FreeOperator( o );
    }
    for( v = firstvalue; v; v = lastvalue )
    {
        lastvalue = v->next;
        //Z_Free(v);
        FreeValue( v );
    }
    if( !error ) return true;
    if( intvalue ) *intvalue = 0;
    if( floatvalue ) *floatvalue = 0;
    return false;
}

/*
===============
Parse_Evaluate
===============
*/
static S32 Parse_Evaluate( Source_t* source, S64* intvalue, F64* floatvalue, S32 integer )
{
    Token_t token, *firsttoken, *lasttoken;
    Token_t* t, *nexttoken;
    Define_t* define;
    S32 defined = false;
    
    if( intvalue ) *intvalue = 0;
    if( floatvalue ) *floatvalue = 0;
    //
    if( !Parse_ReadLine( source, &token ) )
    {
        Parse_SourceError( source, "no value after #if/#elif" );
        return false;
    }
    firsttoken = NULL;
    lasttoken = NULL;
    do
    {
        //if the token is a name
        if( token.type == TT_NAME )
        {
            if( defined )
            {
                defined = false;
                t = Parse_CopyToken( &token );
                t->next = NULL;
                if( lasttoken ) lasttoken->next = t;
                else firsttoken = t;
                lasttoken = t;
            }
            else if( !strcmp( token.string, "defined" ) )
            {
                defined = true;
                t = Parse_CopyToken( &token );
                t->next = NULL;
                if( lasttoken ) lasttoken->next = t;
                else firsttoken = t;
                lasttoken = t;
            }
            else
            {
                //then it must be a define
                define = Parse_FindHashedDefine( source->definehash, token.string );
                if( !define )
                {
                    Parse_SourceError( source, "can't evaluate %s, not defined", token.string );
                    return false;
                }
                if( !Parse_ExpandDefineIntoSource( source, &token, define ) ) return false;
            }
        }
        //if the token is a number or a punctuation
        else if( token.type == TT_NUMBER || token.type == TT_PUNCTUATION )
        {
            t = Parse_CopyToken( &token );
            t->next = NULL;
            if( lasttoken ) lasttoken->next = t;
            else firsttoken = t;
            lasttoken = t;
        }
        else //can't evaluate the token
        {
            Parse_SourceError( source, "can't evaluate %s", token.string );
            return false;
        }
    }
    while( Parse_ReadLine( source, &token ) );
    //
    if( !Parse_EvaluateTokens( source, firsttoken, intvalue, floatvalue, integer ) ) return false;
    //
    for( t = firsttoken; t; t = nexttoken )
    {
        nexttoken = t->next;
        Parse_FreeToken( t );
    }
    //
    return true;
}

/*
===============
Parse_DollarEvaluate
===============
*/
static S32 Parse_DollarEvaluate( Source_t* source, S64* intvalue, F64* floatvalue, S32 integer )
{
    S32 indent, defined = false;
    Token_t token, *firsttoken, *lasttoken;
    Token_t* t, *nexttoken;
    Define_t* define;
    
    if( intvalue ) *intvalue = 0;
    if( floatvalue ) *floatvalue = 0;
    //
    if( !Parse_ReadSourceToken( source, &token ) )
    {
        Parse_SourceError( source, "no leading ( after $evalint/$evalfloat" );
        return false;
    }
    if( !Parse_ReadSourceToken( source, &token ) )
    {
        Parse_SourceError( source, "nothing to evaluate" );
        return false;
    }
    indent = 1;
    firsttoken = NULL;
    lasttoken = NULL;
    do
    {
        //if the token is a name
        if( token.type == TT_NAME )
        {
            if( defined )
            {
                defined = false;
                t = Parse_CopyToken( &token );
                t->next = NULL;
                if( lasttoken ) lasttoken->next = t;
                else firsttoken = t;
                lasttoken = t;
            }
            else if( !strcmp( token.string, "defined" ) )
            {
                defined = true;
                t = Parse_CopyToken( &token );
                t->next = NULL;
                if( lasttoken ) lasttoken->next = t;
                else firsttoken = t;
                lasttoken = t;
            }
            else
            {
                //then it must be a define
                define = Parse_FindHashedDefine( source->definehash, token.string );
                if( !define )
                {
                    Parse_SourceError( source, "can't evaluate %s, not defined", token.string );
                    return false;
                }
                if( !Parse_ExpandDefineIntoSource( source, &token, define ) ) return false;
            }
        }
        //if the token is a number or a punctuation
        else if( token.type == TT_NUMBER || token.type == TT_PUNCTUATION )
        {
            if( *token.string == '(' ) indent++;
            else if( *token.string == ')' ) indent--;
            if( indent <= 0 ) break;
            t = Parse_CopyToken( &token );
            t->next = NULL;
            if( lasttoken ) lasttoken->next = t;
            else firsttoken = t;
            lasttoken = t;
        }
        else //can't evaluate the token
        {
            Parse_SourceError( source, "can't evaluate %s", token.string );
            return false;
        }
    }
    while( Parse_ReadSourceToken( source, &token ) );
    //
    if( !Parse_EvaluateTokens( source, firsttoken, intvalue, floatvalue, integer ) ) return false;
    //
    for( t = firsttoken; t; t = nexttoken )
    {
        nexttoken = t->next;
        Parse_FreeToken( t );
    }
    //
    return true;
}

/*
===============
Parse_Directive_include
===============
*/
static S32 Parse_Directive_include( Source_t* source )
{
    Script_t* script;
    Token_t token;
    UTF8 path[MAX_QPATH];
    
    if( source->skip > 0 ) return true;
    //
    if( !Parse_ReadSourceToken( source, &token ) )
    {
        Parse_SourceError( source, "#include without file name" );
        return false;
    }
    if( token.linescrossed > 0 )
    {
        Parse_SourceError( source, "#include without file name" );
        return false;
    }
    if( token.type == TT_STRING )
    {
        Parse_StripDoubleQuotes( token.string );
        Parse_ConvertPath( token.string );
        script = Parse_LoadScriptFile( token.string );
        if( !script )
        {
            strcpy( path, source->includepath );
            strcat( path, token.string );
            script = Parse_LoadScriptFile( path );
        }
    }
    else if( token.type == TT_PUNCTUATION && *token.string == '<' )
    {
        strcpy( path, source->includepath );
        while( Parse_ReadSourceToken( source, &token ) )
        {
            if( token.linescrossed > 0 )
            {
                Parse_UnreadSourceToken( source, &token );
                break;
            }
            if( token.type == TT_PUNCTUATION && *token.string == '>' ) break;
            strncat( path, token.string, MAX_QPATH - 1 );
        }
        if( *token.string != '>' )
        {
            Parse_SourceWarning( source, "#include missing trailing >" );
        }
        if( !strlen( path ) )
        {
            Parse_SourceError( source, "#include without file name between < >" );
            return false;
        }
        Parse_ConvertPath( path );
        script = Parse_LoadScriptFile( path );
    }
    else
    {
        Parse_SourceError( source, "#include without file name" );
        return false;
    }
    if( !script )
    {
        Parse_SourceError( source, "file %s not found", path );
        return false;
    }
    Parse_PushScript( source, script );
    return true;
}

/*
===============
Parse_WhiteSpaceBeforeToken
===============
*/
static S32 Parse_WhiteSpaceBeforeToken( Token_t* token )
{
    return token->endwhitespace_p - token->whitespace_p > 0;
}

/*
===============
Parse_ClearTokenWhiteSpace
===============
*/
static void Parse_ClearTokenWhiteSpace( Token_t* token )
{
    token->whitespace_p = NULL;
    token->endwhitespace_p = NULL;
    token->linescrossed = 0;
}

/*
===============
Parse_Directive_undef
===============
*/
static S32 Parse_Directive_undef( Source_t* source )
{
    Token_t token;
    Define_t* define, *lastdefine;
    S32 hash;
    
    if( source->skip > 0 ) return true;
    //
    if( !Parse_ReadLine( source, &token ) )
    {
        Parse_SourceError( source, "undef without name" );
        return false;
    }
    if( token.type != TT_NAME )
    {
        Parse_UnreadSourceToken( source, &token );
        Parse_SourceError( source, "expected name, found %s", token.string );
        return false;
    }
    
    hash = Parse_NameHash( token.string );
    for( lastdefine = NULL, define = source->definehash[hash]; define; define = define->hashnext )
    {
        if( !strcmp( define->name, token.string ) )
        {
            if( define->flags & DEFINE_FIXED )
            {
                Parse_SourceWarning( source, "can't undef %s", token.string );
            }
            else
            {
                if( lastdefine ) lastdefine->hashnext = define->hashnext;
                else source->definehash[hash] = define->hashnext;
                Parse_FreeDefine( define );
            }
            break;
        }
        lastdefine = define;
    }
    return true;
}

/*
===============
Parse_Directive_elif
===============
*/
static S32 Parse_Directive_elif( Source_t* source )
{
    S64 value;
    S32 type, skip;
    
    Parse_PopIndent( source, &type, &skip );
    if( !type || type == INDENT_ELSE )
    {
        Parse_SourceError( source, "misplaced #elif" );
        return false;
    }
    if( !Parse_Evaluate( source, &value, NULL, true ) ) return false;
    skip = ( value == 0 );
    Parse_PushIndent( source, INDENT_ELIF, skip );
    return true;
}

/*
===============
Parse_Directive_if
===============
*/
static S32 Parse_Directive_if( Source_t* source )
{
    S64 value;
    S32 skip;
    
    if( !Parse_Evaluate( source, &value, NULL, true ) ) return false;
    skip = ( value == 0 );
    Parse_PushIndent( source, INDENT_IF, skip );
    return true;
}

/*
===============
Parse_Directive_line
===============
*/
static S32 Parse_Directive_line( Source_t* source )
{
    Parse_SourceError( source, "#line directive not supported" );
    return false;
}

/*
===============
Parse_Directive_error
===============
*/
static S32 Parse_Directive_error( Source_t* source )
{
    Token_t token;
    
    strcpy( token.string, "" );
    Parse_ReadSourceToken( source, &token );
    Parse_SourceError( source, "#error directive: %s", token.string );
    return false;
}

/*
===============
Parse_Directive_pragma
===============
*/
static S32 Parse_Directive_pragma( Source_t* source )
{
    Token_t token;
    
    Parse_SourceWarning( source, "#pragma directive not supported" );
    while( Parse_ReadLine( source, &token ) ) ;
    return true;
}

/*
===============
Parse_UnreadSignToken
===============
*/
static void Parse_UnreadSignToken( Source_t* source )
{
    Token_t token;
    
    token.line = source->scriptstack->line;
    token.whitespace_p = source->scriptstack->script_p;
    token.endwhitespace_p = source->scriptstack->script_p;
    token.linescrossed = 0;
    strcpy( token.string, "-" );
    token.type = TT_PUNCTUATION;
    token.subtype = P_SUB;
    Parse_UnreadSourceToken( source, &token );
}

/*
===============
Parse_Directive_eval
===============
*/
static S32 Parse_Directive_eval( Source_t* source )
{
    S64 value;
    Token_t token;
    
    if( !Parse_Evaluate( source, &value, NULL, true ) ) return false;
    //
    token.line = source->scriptstack->line;
    token.whitespace_p = source->scriptstack->script_p;
    token.endwhitespace_p = source->scriptstack->script_p;
    token.linescrossed = 0;
    sprintf( token.string, "%d", abs( value ) );
    token.type = TT_NUMBER;
    token.subtype = TT_INTEGER | TT_LONG | TT_DECIMAL;
    Parse_UnreadSourceToken( source, &token );
    if( value < 0 ) Parse_UnreadSignToken( source );
    return true;
}

/*
===============
Parse_Directive_evalfloat
===============
*/
static S32 Parse_Directive_evalfloat( Source_t* source )
{
    F64 value;
    Token_t token;
    
    if( !Parse_Evaluate( source, NULL, &value, false ) ) return false;
    token.line = source->scriptstack->line;
    token.whitespace_p = source->scriptstack->script_p;
    token.endwhitespace_p = source->scriptstack->script_p;
    token.linescrossed = 0;
    sprintf( token.string, "%1.2f", fabs( value ) );
    token.type = TT_NUMBER;
    token.subtype = TT_FLOAT | TT_LONG | TT_DECIMAL;
    Parse_UnreadSourceToken( source, &token );
    if( value < 0 ) Parse_UnreadSignToken( source );
    return true;
}

/*
===============
Parse_DollarDirective_evalint
===============
*/
static S32 Parse_DollarDirective_evalint( Source_t* source )
{
    S64 value;
    Token_t token;
    
    if( !Parse_DollarEvaluate( source, &value, NULL, true ) ) return false;
    //
    token.line = source->scriptstack->line;
    token.whitespace_p = source->scriptstack->script_p;
    token.endwhitespace_p = source->scriptstack->script_p;
    token.linescrossed = 0;
    sprintf( token.string, "%d", abs( value ) );
    token.type = TT_NUMBER;
    token.subtype = TT_INTEGER | TT_LONG | TT_DECIMAL;
    token.intvalue = value;
    token.floatvalue = value;
    Parse_UnreadSourceToken( source, &token );
    if( value < 0 ) Parse_UnreadSignToken( source );
    return true;
}

/*
===============
Parse_DollarDirective_evalfloat
===============
*/
static S32 Parse_DollarDirective_evalfloat( Source_t* source )
{
    F64 value;
    Token_t token;
    
    if( !Parse_DollarEvaluate( source, NULL, &value, false ) ) return false;
    token.line = source->scriptstack->line;
    token.whitespace_p = source->scriptstack->script_p;
    token.endwhitespace_p = source->scriptstack->script_p;
    token.linescrossed = 0;
    sprintf( token.string, "%1.2f", fabs( value ) );
    token.type = TT_NUMBER;
    token.subtype = TT_FLOAT | TT_LONG | TT_DECIMAL;
    token.intvalue = ( U64 ) value;
    token.floatvalue = value;
    Parse_UnreadSourceToken( source, &token );
    if( value < 0 ) Parse_UnreadSignToken( source );
    return true;
}

/*
===============
Parse_ReadDollarDirective
===============
*/
directive_t DollarDirectives[20] =
{
    {"evalint", Parse_DollarDirective_evalint},
    {"evalfloat", Parse_DollarDirective_evalfloat},
    {NULL, NULL}
};

static S32 Parse_ReadDollarDirective( Source_t* source )
{
    Token_t token;
    S32 i;
    
    //read the directive name
    if( !Parse_ReadSourceToken( source, &token ) )
    {
        Parse_SourceError( source, "found $ without name" );
        return false;
    }
    //directive name must be on the same line
    if( token.linescrossed > 0 )
    {
        Parse_UnreadSourceToken( source, &token );
        Parse_SourceError( source, "found $ at end of line" );
        return false;
    }
    //if if is a name
    if( token.type == TT_NAME )
    {
        //find the precompiler directive
        for( i = 0; DollarDirectives[i].name; i++ )
        {
            if( !strcmp( DollarDirectives[i].name, token.string ) )
            {
                return DollarDirectives[i].func( source );
            }
        }
    }
    Parse_UnreadSourceToken( source, &token );
    Parse_SourceError( source, "unknown precompiler directive %s", token.string );
    return false;
}

/*
===============
Parse_Directive_if_def
===============
*/
static S32 Parse_Directive_if_def( Source_t* source, S32 type )
{
    Token_t token;
    Define_t* d;
    S32 skip;
    
    if( !Parse_ReadLine( source, &token ) )
    {
        Parse_SourceError( source, "#ifdef without name" );
        return false;
    }
    if( token.type != TT_NAME )
    {
        Parse_UnreadSourceToken( source, &token );
        Parse_SourceError( source, "expected name after #ifdef, found %s", token.string );
        return false;
    }
    d = Parse_FindHashedDefine( source->definehash, token.string );
    skip = ( type == INDENT_IFDEF ) == ( d == NULL );
    Parse_PushIndent( source, type, skip );
    return true;
}

/*
===============
Parse_Directive_ifdef
===============
*/
static S32 Parse_Directive_ifdef( Source_t* source )
{
    return Parse_Directive_if_def( source, INDENT_IFDEF );
}

/*
===============
Parse_Directive_ifndef
===============
*/
static S32 Parse_Directive_ifndef( Source_t* source )
{
    return Parse_Directive_if_def( source, INDENT_IFNDEF );
}

/*
===============
Parse_Directive_else
===============
*/
static S32 Parse_Directive_else( Source_t* source )
{
    S32 type, skip;
    
    Parse_PopIndent( source, &type, &skip );
    if( !type )
    {
        Parse_SourceError( source, "misplaced #else" );
        return false;
    }
    if( type == INDENT_ELSE )
    {
        Parse_SourceError( source, "#else after #else" );
        return false;
    }
    Parse_PushIndent( source, INDENT_ELSE, !skip );
    return true;
}

/*
===============
Parse_Directive_endif
===============
*/
static S32 Parse_Directive_endif( Source_t* source )
{
    S32 type, skip;
    
    Parse_PopIndent( source, &type, &skip );
    if( !type )
    {
        Parse_SourceError( source, "misplaced #endif" );
        return false;
    }
    return true;
}

/*
===============
Parse_CheckTokenString
===============
*/
static S32 Parse_CheckTokenString( Source_t* source, UTF8* string )
{
    Token_t tok;
    
    if( !Parse_ReadToken( source, &tok ) ) return false;
    //if the token is available
    if( !strcmp( tok.string, string ) ) return true;
    //
    Parse_UnreadSourceToken( source, &tok );
    return false;
}

/*
===============
Parse_Directive_define
===============
*/
static S32 Parse_Directive_define( Source_t* source )
{
    Token_t token, *t, *last;
    Define_t* define;
    
    if( source->skip > 0 ) return true;
    //
    if( !Parse_ReadLine( source, &token ) )
    {
        Parse_SourceError( source, "#define without name" );
        return false;
    }
    if( token.type != TT_NAME )
    {
        Parse_UnreadSourceToken( source, &token );
        Parse_SourceError( source, "expected name after #define, found %s", token.string );
        return false;
    }
    //check if the define already exists
    define = Parse_FindHashedDefine( source->definehash, token.string );
    if( define )
    {
        if( define->flags & DEFINE_FIXED )
        {
            Parse_SourceError( source, "can't redefine %s", token.string );
            return false;
        }
        Parse_SourceWarning( source, "redefinition of %s", token.string );
        //unread the define name before executing the #undef directive
        Parse_UnreadSourceToken( source, &token );
        if( !Parse_Directive_undef( source ) )
            return false;
    }
    //allocate define
    define = ( Define_t* ) Z_Malloc( sizeof( Define_t ) + strlen( token.string ) + 1 );
    ::memset( define, 0, sizeof( Define_t ) );
    define->name = ( UTF8* ) define + sizeof( Define_t );
    strcpy( define->name, token.string );
    //add the define to the source
    Parse_AddDefineToHash( define, source->definehash );
    //if nothing is defined, just return
    if( !Parse_ReadLine( source, &token ) ) return true;
    //if it is a define with parameters
    if( !Parse_WhiteSpaceBeforeToken( &token ) && !strcmp( token.string, "(" ) )
    {
        //read the define parameters
        last = NULL;
        if( !Parse_CheckTokenString( source, ")" ) )
        {
            while( 1 )
            {
                if( !Parse_ReadLine( source, &token ) )
                {
                    Parse_SourceError( source, "expected define parameter" );
                    return false;
                }
                //if it isn't a name
                if( token.type != TT_NAME )
                {
                    Parse_SourceError( source, "invalid define parameter" );
                    return false;
                }
                //
                if( Parse_FindDefineParm( define, token.string ) >= 0 )
                {
                    Parse_SourceError( source, "two the same define parameters" );
                    return false;
                }
                //add the define parm
                t = Parse_CopyToken( &token );
                Parse_ClearTokenWhiteSpace( t );
                t->next = NULL;
                if( last ) last->next = t;
                else define->parms = t;
                last = t;
                define->numparms++;
                //read next token
                if( !Parse_ReadLine( source, &token ) )
                {
                    Parse_SourceError( source, "define parameters not terminated" );
                    return false;
                }
                //
                if( !strcmp( token.string, ")" ) ) break;
                //then it must be a comma
                if( strcmp( token.string, "," ) )
                {
                    Parse_SourceError( source, "define not terminated" );
                    return false;
                }
            }
        }
        if( !Parse_ReadLine( source, &token ) ) return true;
    }
    //read the defined stuff
    last = NULL;
    do
    {
        t = Parse_CopyToken( &token );
        if( t->type == TT_NAME && !strcmp( t->string, define->name ) )
        {
            Parse_SourceError( source, "recursive define (removed recursion)" );
            continue;
        }
        Parse_ClearTokenWhiteSpace( t );
        t->next = NULL;
        if( last ) last->next = t;
        else define->tokens = t;
        last = t;
    }
    while( Parse_ReadLine( source, &token ) );
    //
    if( last )
    {
        //check for merge operators at the beginning or end
        if( !strcmp( define->tokens->string, "##" ) ||
                !strcmp( last->string, "##" ) )
        {
            Parse_SourceError( source, "define with misplaced ##" );
            return false;
        }
    }
    return true;
}

/*
===============
Parse_ReadDirective
===============
*/
directive_t Directives[20] =
{
    {"if", Parse_Directive_if},
    {"ifdef", Parse_Directive_ifdef},
    {"ifndef", Parse_Directive_ifndef},
    {"elif", Parse_Directive_elif},
    {"else", Parse_Directive_else},
    {"endif", Parse_Directive_endif},
    {"include", Parse_Directive_include},
    {"define", Parse_Directive_define},
    {"undef", Parse_Directive_undef},
    {"line", Parse_Directive_line},
    {"error", Parse_Directive_error},
    {"pragma", Parse_Directive_pragma},
    {"eval", Parse_Directive_eval},
    {"evalfloat", Parse_Directive_evalfloat},
    {NULL, NULL}
};

static S32 Parse_ReadDirective( Source_t* source )
{
    Token_t token;
    S32 i;
    
    //read the directive name
    if( !Parse_ReadSourceToken( source, &token ) )
    {
        Parse_SourceError( source, "found # without name" );
        return false;
    }
    //directive name must be on the same line
    if( token.linescrossed > 0 )
    {
        Parse_UnreadSourceToken( source, &token );
        Parse_SourceError( source, "found # at end of line" );
        return false;
    }
    //if if is a name
    if( token.type == TT_NAME )
    {
        //find the precompiler directive
        for( i = 0; Directives[i].name; i++ )
        {
            if( !strcmp( Directives[i].name, token.string ) )
            {
                return Directives[i].func( source );
            }
        }
    }
    Parse_SourceError( source, "unknown precompiler directive %s", token.string );
    return false;
}

/*
===============
Parse_UnreadToken
===============
*/
static void Parse_UnreadToken( Source_t* source, Token_t* token )
{
    Parse_UnreadSourceToken( source, token );
}

/*
===============
Parse_ReadEnumeration

It is assumed that the 'enum' token has already been consumed
This is fairly basic: it doesn't catch some fairly obvious errors like nested
enums, and enumerated names conflict with #define parameters
===============
*/
static bool Parse_ReadEnumeration( Source_t* source )
{
    Token_t newtoken;
    S32 value;
    
    if( !Parse_ReadToken( source, &newtoken ) )
        return false;
        
    if( newtoken.type != TT_PUNCTUATION || newtoken.subtype != P_BRACEOPEN )
    {
        Parse_SourceError( source, "Found %s when expecting {\n",
                           newtoken.string );
        return false;
    }
    
    for( value = 0;; value++ )
    {
        Token_t name;
        
        // read the name
        if( !Parse_ReadToken( source, &name ) )
            break;
            
        // it's ok for the enum to end immediately
        if( name.type == TT_PUNCTUATION && name.subtype == P_BRACECLOSE )
        {
            if( !Parse_ReadToken( source, &name ) )
                break;
                
            // ignore trailing semicolon
            if( name.type != TT_PUNCTUATION || name.subtype != P_SEMICOLON )
                Parse_UnreadToken( source, &name );
                
            return true;
        }
        
        // ... but not for it to do anything else
        if( name.type != TT_NAME )
        {
            Parse_SourceError( source, "Found %s when expecting identifier\n",
                               name.string );
            return false;
        }
        
        if( !Parse_ReadToken( source, &newtoken ) )
            break;
            
        if( newtoken.type != TT_PUNCTUATION )
        {
            Parse_SourceError( source, "Found %s when expecting , or = or }\n",
                               newtoken.string );
            return false;
        }
        
        if( newtoken.subtype == P_ASSIGN )
        {
            S32 neg = 1;
            
            if( !Parse_ReadToken( source, &newtoken ) )
                break;
                
            // Parse_ReadToken doesn't seem to read negative numbers, so we do it
            // ourselves
            if( newtoken.type == TT_PUNCTUATION && newtoken.subtype == P_SUB )
            {
                neg = -1;
                
                // the next token should be the number
                if( !Parse_ReadToken( source, &newtoken ) )
                    break;
            }
            
            if( newtoken.type != TT_NUMBER || !( newtoken.subtype & TT_INTEGER ) )
            {
                Parse_SourceError( source, "Found %s when expecting integer\n",
                                   newtoken.string );
                return false;
            }
            
            // this is somewhat silly, but cheap to check
            if( neg == -1 && ( newtoken.subtype & TT_UNSIGNED ) )
            {
                Parse_SourceWarning( source, "Value in enumeration is negative and "
                                     "unsigned\n" );
            }
            
            // set the new define value
            value = newtoken.intvalue * neg;
            
            if( !Parse_ReadToken( source, &newtoken ) )
                break;
        }
        
        if( newtoken.type != TT_PUNCTUATION || ( newtoken.subtype != P_COMMA &&
                newtoken.subtype != P_BRACECLOSE ) )
        {
            Parse_SourceError( source, "Found %s when expecting , or }\n",
                               newtoken.string );
            return false;
        }
        
        if( !Parse_AddDefineToSourceFromString( source, va( "%s %d\n", name.string,
                                                value ) ) )
        {
            Parse_SourceWarning( source, "Couldn't add define to source: %s = %d\n",
                                 name.string, value );
            return false;
        }
        
        if( newtoken.subtype == P_BRACECLOSE )
        {
            if( !Parse_ReadToken( source, &name ) )
                break;
                
            // ignore trailing semicolon
            if( name.type != TT_PUNCTUATION || name.subtype != P_SEMICOLON )
                Parse_UnreadToken( source, &name );
                
            return true;
        }
    }
    
    // got here if a ReadToken returned false
    return false;
}

/*
===============
Parse_ReadToken
===============
*/
static S32 Parse_ReadToken( Source_t* source, Token_t* token )
{
    Define_t* define;
    
    while( 1 )
    {
        if( !Parse_ReadSourceToken( source, token ) ) return false;
        //check for precompiler directives
        if( token->type == TT_PUNCTUATION && *token->string == '#' )
        {
            {
                //read the precompiler directive
                if( !Parse_ReadDirective( source ) ) return false;
                continue;
            }
        }
        if( token->type == TT_PUNCTUATION && *token->string == '$' )
        {
            {
                //read the precompiler directive
                if( !Parse_ReadDollarDirective( source ) ) return false;
                continue;
            }
        }
        if( token->type == TT_NAME && !Q_stricmp( token->string, "enum" ) )
        {
            if( !Parse_ReadEnumeration( source ) )
                return false;
            continue;
        }
        // recursively concatenate strings that are behind each other still resolving defines
        if( token->type == TT_STRING )
        {
            Token_t newtoken;
            if( Parse_ReadToken( source, &newtoken ) )
            {
                if( newtoken.type == TT_STRING )
                {
                    token->string[strlen( token->string ) - 1] = '\0';
                    if( strlen( token->string ) + strlen( newtoken.string + 1 ) + 1 >= MAX_TOKEN_CHARS )
                    {
                        Parse_SourceError( source, "string longer than MAX_TOKEN_CHARS %d\n", MAX_TOKEN_CHARS );
                        return false;
                    }
                    strcat( token->string, newtoken.string + 1 );
                }
                else
                {
                    Parse_UnreadToken( source, &newtoken );
                }
            }
        }
        //if skipping source because of conditional compilation
        if( source->skip ) continue;
        //if the token is a name
        if( token->type == TT_NAME )
        {
            //check if the name is a define macro
            define = Parse_FindHashedDefine( source->definehash, token->string );
            //if it is a define macro
            if( define )
            {
                //expand the defined macro
                if( !Parse_ExpandDefineIntoSource( source, token, define ) ) return false;
                continue;
            }
        }
        //copy token for unreading
        ::memcpy( &source->token, token, sizeof( Token_t ) );
        //found a token
        return true;
    }
}

/*
===============
Parse_DefineFromString
===============
*/
static Define_t* Parse_DefineFromString( UTF8* string )
{
    Script_t* script;
    Source_t src;
    Token_t* t;
    S32 res, i;
    Define_t* def;
    
    script = Parse_LoadScriptMemory( string, strlen( string ), "*extern" );
    //create a new source
    ::memset( &src, 0, sizeof( Source_t ) );
    strncpy( src.filename, "*extern", MAX_QPATH );
    src.scriptstack = script;
    src.definehash = ( Define_t** )Z_Malloc( DEFINEHASHSIZE * sizeof( Define_t* ) );
    ::memset( src.definehash, 0, DEFINEHASHSIZE * sizeof( Define_t* ) );
    //create a define from the source
    res = Parse_Directive_define( &src );
    //free any tokens if left
    for( t = src.tokens; t; t = src.tokens )
    {
        src.tokens = src.tokens->next;
        Parse_FreeToken( t );
    }
    def = NULL;
    for( i = 0; i < DEFINEHASHSIZE; i++ )
    {
        if( src.definehash[i] )
        {
            def = src.definehash[i];
            break;
        }
    }
    //
    Z_Free( src.definehash );
    //
    Parse_FreeScript( script );
    //if the define was created succesfully
    if( res > 0 ) return def;
    //free the define is created
    if( src.defines ) Parse_FreeDefine( def );
    //
    return NULL;
}

/*
===============
Parse_AddDefineToSourceFromString
===============
*/
static bool Parse_AddDefineToSourceFromString( Source_t* source,
        UTF8* string )
{
    Parse_PushScript( source, Parse_LoadScriptMemory( string, strlen( string ),
                      "*extern" ) );
    return ( bool )Parse_Directive_define( source );
}

/*
===============
Parse_AddGlobalDefine

adds or overrides a global define that will be added to all opened sources
===============
*/
S32 Parse_AddGlobalDefine( UTF8* string )
{
    Define_t* define, *prev, *curr;
    
    define = Parse_DefineFromString( string );
    if( !define )
        return false;
        
    prev = NULL;
    for( curr = globaldefines; curr; curr = curr->next )
    {
        if( !strcmp( curr->name, define->name ) )
        {
            define->next = curr->next;
            Parse_FreeDefine( curr );
            if( prev )
                prev->next = define;
            else
                globaldefines = define;
            break;
        }
        prev = curr;
    }
    if( !curr )
    {
        define->next = globaldefines;
        globaldefines = define;
    }
    
    return true;
}

/*
===============
Parse_CopyDefine
===============
*/
static Define_t* Parse_CopyDefine( Source_t* source, Define_t* define )
{
    Define_t* newdefine;
    Token_t* token, *newtoken, *lasttoken;
    
    newdefine = ( Define_t* ) Z_Malloc( sizeof( Define_t ) + strlen( define->name ) + 1 );
    //copy the define name
    newdefine->name = ( UTF8* ) newdefine + sizeof( Define_t );
    strcpy( newdefine->name, define->name );
    newdefine->flags = define->flags;
    newdefine->builtin = define->builtin;
    newdefine->numparms = define->numparms;
    //the define is not linked
    newdefine->next = NULL;
    newdefine->hashnext = NULL;
    //copy the define tokens
    newdefine->tokens = NULL;
    for( lasttoken = NULL, token = define->tokens; token; token = token->next )
    {
        newtoken = Parse_CopyToken( token );
        newtoken->next = NULL;
        if( lasttoken ) lasttoken->next = newtoken;
        else newdefine->tokens = newtoken;
        lasttoken = newtoken;
    }
    //copy the define parameters
    newdefine->parms = NULL;
    for( lasttoken = NULL, token = define->parms; token; token = token->next )
    {
        newtoken = Parse_CopyToken( token );
        newtoken->next = NULL;
        if( lasttoken ) lasttoken->next = newtoken;
        else newdefine->parms = newtoken;
        lasttoken = newtoken;
    }
    return newdefine;
}

/*
===============
Parse_AddGlobalDefinesToSource
===============
*/
static void Parse_AddGlobalDefinesToSource( Source_t* source )
{
    Define_t* define, *newdefine;
    
    for( define = globaldefines; define; define = define->next )
    {
        newdefine = Parse_CopyDefine( source, define );
        Parse_AddDefineToHash( newdefine, source->definehash );
    }
}

/*
===============
Parse_LoadSourceFile
===============
*/
static Source_t* Parse_LoadSourceFile( StringEntry filename )
{
    Source_t* source;
    Script_t* script;
    
    script = Parse_LoadScriptFile( filename );
    if( !script ) return NULL;
    
    script->next = NULL;
    
    source = ( Source_t* ) Z_Malloc( sizeof( Source_t ) );
    ::memset( source, 0, sizeof( Source_t ) );
    
    strncpy( source->filename, filename, MAX_QPATH );
    source->scriptstack = script;
    source->tokens = NULL;
    source->defines = NULL;
    source->indentstack = NULL;
    source->skip = 0;
    
    source->definehash = ( Define_t** )Z_Malloc( DEFINEHASHSIZE * sizeof( Define_t* ) );
    ::memset( source->definehash, 0, DEFINEHASHSIZE * sizeof( Define_t* ) );
    Parse_AddGlobalDefinesToSource( source );
    return source;
}

/*
===============
Parse_FreeSource
===============
*/
static void Parse_FreeSource( Source_t* source )
{
    Script_t* script;
    Token_t* token;
    Define_t* define;
    Indent_t* indent;
    S32 i;
    
    //Parse_PrintDefineHashTable(source->definehash);
    //free all the scripts
    while( source->scriptstack )
    {
        script = source->scriptstack;
        source->scriptstack = source->scriptstack->next;
        Parse_FreeScript( script );
    }
    //free all the tokens
    while( source->tokens )
    {
        token = source->tokens;
        source->tokens = source->tokens->next;
        Parse_FreeToken( token );
    }
    for( i = 0; i < DEFINEHASHSIZE; i++ )
    {
        while( source->definehash[i] )
        {
            define = source->definehash[i];
            source->definehash[i] = source->definehash[i]->hashnext;
            Parse_FreeDefine( define );
        }
    }
    //free all indents
    while( source->indentstack )
    {
        indent = source->indentstack;
        source->indentstack = source->indentstack->next;
        Z_Free( indent );
    }
    //
    if( source->definehash ) Z_Free( source->definehash );
    //free the source itself
    Z_Free( source );
}

#define MAX_SOURCEFILES   64

Source_t* sourceFiles[MAX_SOURCEFILES];

/*
===============
Parse_LoadSourceHandle
===============
*/
S32 Parse_LoadSourceHandle( StringEntry filename )
{
    Source_t* source;
    S32 i;
    
    for( i = 1; i < MAX_SOURCEFILES; i++ )
    {
        if( !sourceFiles[i] )
            break;
    }
    if( i >= MAX_SOURCEFILES )
        return 0;
    source = Parse_LoadSourceFile( filename );
    if( !source )
        return 0;
    sourceFiles[i] = source;
    return i;
}

/*
===============
Parse_FreeSourceHandle
===============
*/
S32 Parse_FreeSourceHandle( S32 handle )
{
    if( handle < 1 || handle >= MAX_SOURCEFILES )
        return false;
    if( !sourceFiles[handle] )
        return false;
        
    Parse_FreeSource( sourceFiles[handle] );
    sourceFiles[handle] = NULL;
    return true;
}

/*
===============
Parse_ReadTokenHandle
===============
*/
S32 Parse_ReadTokenHandle( S32 handle, Token_t* pc_token )
{
    Token_t token;
    S32 ret;
    
    if( handle < 1 || handle >= MAX_SOURCEFILES )
        return 0;
    if( !sourceFiles[handle] )
        return 0;
        
    ret = Parse_ReadToken( sourceFiles[handle], &token );
    strcpy( pc_token->string, token.string );
    pc_token->type = token.type;
    pc_token->subtype = token.subtype;
    pc_token->intvalue = token.intvalue;
    pc_token->floatvalue = token.floatvalue;
    if( pc_token->type == TT_STRING )
        Parse_StripDoubleQuotes( pc_token->string );
    return ret;
}

/*
===============
Parse_SourceFileAndLine
===============
*/
S32 Parse_SourceFileAndLine( S32 handle, UTF8* filename, S32* line )
{
    if( handle < 1 || handle >= MAX_SOURCEFILES )
        return false;
    if( !sourceFiles[handle] )
        return false;
        
    strcpy( filename, sourceFiles[handle]->filename );
    if( sourceFiles[handle]->scriptstack )
        *line = sourceFiles[handle]->scriptstack->line;
    else
        *line = 0;
    return true;
}

void Parse_RemoveAllGlobalDefines( void )
{
    Define_t* define;
    
    for( define = globaldefines; define; define = globaldefines )
    {
        globaldefines = globaldefines->next;
        Parse_FreeDefine( define );
    }
    
    globaldefines = NULL;
}

void Parse_UnreadLastTokenHandle( S32 handle )
{
    if( handle < 1 || handle >= MAX_SOURCEFILES )
    {
        return;
    }
    
    if( !sourceFiles[handle] )
    {
        return;
    }
    
    Parse_UnreadSourceToken( sourceFiles[handle], &sourceFiles[handle]->token );
}
