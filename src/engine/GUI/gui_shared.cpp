////////////////////////////////////////////////////////////////////////////////////////
// Copyright(C) 1999 - 2005 Id Software, Inc.
// Copyright(C) 2000 - 2006 Tim Angus
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
// File name:   gui_shared.cpp
// Version:     v1.00
// Created:
// Compilers:   Visual Studio 2015
// Description:
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#ifdef CGAMEDLL
#include <sgame/sg_precompiled.h>
#else
#include <OWLib/precompiled.h>
#endif // !GAMEDLL

#define SCROLL_TIME_START         500
#define SCROLL_TIME_ADJUST        150
#define SCROLL_TIME_ADJUSTOFFSET  40
#define SCROLL_TIME_FLOOR         20

typedef struct scrollInfo_s
{
    S32 nextScrollTime;
    S32 nextAdjustTime;
    S32 adjustValue;
    S32 scrollKey;
    F32 xStart;
    F32 yStart;
    itemDef_t* item;
    bool scrollDir;
}
scrollInfo_t;

static scrollInfo_t scrollInfo;

// prevent compiler warnings
void voidFunction( void* var )
{
    return;
}

bool voidFunction2( itemDef_t* var1, S32 var2 )
{
    return false;
}

static CaptureFunc* captureFunc = voidFunction;
static S32 captureFuncExpiry = 0;
static void* captureData = NULL;
static itemDef_t* itemCapture = NULL;   // item that has the mouse captured ( if any )

displayContextDef_t* DC = NULL;

bool g_waitingForKey = false;
bool g_editingField = false;

itemDef_t* g_bindItem = NULL;
itemDef_t* g_editItem = NULL;

menuDef_t Menus[MAX_MENUS];      // defined menus
S32 menuCount = 0;               // how many

menuDef_t* menuStack[MAX_OPEN_MENUS];
S32 openMenuCount = 0;

#define DOUBLE_CLICK_DELAY 300
static S32 lastListBoxClickTime = 0;

void Item_RunScript( itemDef_t* item, StringEntry s );
void Item_SetupKeywordHash( void );
void Menu_SetupKeywordHash( void );
S32 BindingIDFromName( StringEntry name );
bool Item_Bind_HandleKey( itemDef_t* item, S32 key, bool down );
itemDef_t* Menu_SetPrevCursorItem( menuDef_t* menu );
itemDef_t* Menu_SetNextCursorItem( menuDef_t* menu );
static bool Menu_OverActiveItem( menuDef_t* menu, F32 x, F32 y );

void trap_R_AddLightToScene( const vec3_t org, F32 intensity, F32 r, F32 g, F32 b );
/*
===============
UI_InstallCaptureFunc
===============
*/
void UI_InstallCaptureFunc( CaptureFunc* f, void* data, S32 timeout )
{
    captureFunc = f;
    captureData = data;
    
    if( timeout > 0 )
        captureFuncExpiry = DC->realTime + timeout;
    else
        captureFuncExpiry = 0;
}

/*
===============
UI_RemoveCaptureFunc
===============
*/
void UI_RemoveCaptureFunc( void )
{
    captureFunc = voidFunction;
    captureData = NULL;
    captureFuncExpiry = 0;
}

#ifdef CGAMEDLL
#define MEM_POOL_SIZE  128 * 1024
#else
#define MEM_POOL_SIZE  1024 * 1024
#endif

static UTF8   UI_memoryPool[MEM_POOL_SIZE];
static S32    allocPoint, outOfMemory;
// Hacked new memory pool for hud
static UTF8   UI_hudmemoryPool[MEM_POOL_SIZE];
static S32    hudallocPoint, hudoutOfMemory;

/*
===============
UI_ResetHUDMemory
===============
*/
void UI_ResetHUDMemory( void )
{
    hudallocPoint = 0;
    hudoutOfMemory = false;
}

/*
===============
UI_Alloc
===============
*/
void* UI_Alloc( S32 size )
{
    UTF8*  p;
    
    if( allocPoint + size > MEM_POOL_SIZE )
    {
        outOfMemory = true;
        
        if( DC->Print )
            DC->Print( "GUI_Alloc: Failure. Out of memory!\n" );
        //DC->trap_Print(S_COLOR_YELLOW"WARNING: UI Out of Memory!\n");
        return NULL;
    }
    
    p = &UI_memoryPool[ allocPoint ];
    
    allocPoint += ( size + 15 ) & ~15;
    
    return p;
}

/*
===============
UI_HUDAlloc
===============
*/
void* UI_HUDAlloc( S32 size )
{
    UTF8*  p;
    
    if( hudallocPoint + size > MEM_POOL_SIZE )
    {
        DC->Print( "GUI_HUDAlloc: Out of memory! Reinititing hud Memory!!\n" );
        hudoutOfMemory = true;
        UI_ResetHUDMemory();
    }
    
    p = &UI_hudmemoryPool[ hudallocPoint ];
    
    hudallocPoint += ( size + 15 ) & ~15;
    
    return p;
}

/*
===============
UI_InitMemory
===============
*/
void UI_InitMemory( void )
{
    allocPoint = 0;
    outOfMemory = false;
    hudallocPoint = 0;
    hudoutOfMemory = false;
}

bool UI_OutOfMemory( )
{
    return outOfMemory;
}





#define HASH_TABLE_SIZE 2048
/*
================
return a hash value for the string
================
*/
static S64 hashForString( StringEntry str )
{
    S32   i;
    S64  hash;
    UTF8  letter;
    
    hash = 0;
    i = 0;
    
    while( str[i] != '\0' )
    {
        letter = tolower( str[i] );
        hash += ( S64 )( letter ) * ( i + 119 );
        i++;
    }
    
    hash &= ( HASH_TABLE_SIZE - 1 );
    return hash;
}

typedef struct stringDef_s
{
    struct stringDef_s* next;
    StringEntry str;
}

stringDef_t;

static S32 strPoolIndex = 0;
static UTF8 strPool[STRING_POOL_SIZE];

static S32 strHandleCount = 0;
static stringDef_t* strHandle[HASH_TABLE_SIZE];


StringEntry String_Alloc( StringEntry p )
{
    S32 len;
    S64 hash;
    stringDef_t* str, *last;
    static StringEntry staticNULL = "";
    
    if( p == NULL )
        return NULL;
        
    if( *p == 0 )
        return staticNULL;
        
    hash = hashForString( p );
    
    str = strHandle[hash];
    
    while( str )
    {
        if( strcmp( p, str->str ) == 0 )
            return str->str;
            
        str = str->next;
    }
    
    len = strlen( p );
    
    if( len + strPoolIndex + 1 < STRING_POOL_SIZE )
    {
        S32 ph = strPoolIndex;
        strcpy( &strPool[strPoolIndex], p );
        strPoolIndex += len + 1;
        
        str = strHandle[hash];
        last = str;
        
        while( str && str->next )
        {
            last = str;
            str = str->next;
        }
        
        str = ( stringDef_t* ) UI_Alloc( sizeof( stringDef_t ) );
        str->next = NULL;
        str->str = &strPool[ph];
        
        if( last )
            last->next = str;
        else
            strHandle[hash] = str;
            
        return &strPool[ph];
    }
    
    return NULL;
}

void String_Report( void )
{
    F32 f;
    Com_Printf( "Memory/String Pool Info\n" );
    Com_Printf( "----------------\n" );
    f = strPoolIndex;
    f /= STRING_POOL_SIZE;
    f *= 100;
    Com_Printf( "String Pool is %.1f%% full, %i bytes out of %i used.\n", f, strPoolIndex, STRING_POOL_SIZE );
    f = allocPoint;
    f /= MEM_POOL_SIZE;
    f *= 100;
    Com_Printf( "Memory Pool is %.1f%% full, %i bytes out of %i used.\n", f, allocPoint, MEM_POOL_SIZE );
}

/*
=================
String_Init
=================
*/
void String_Init( void )
{
    S32 i;
    
    for( i = 0; i < HASH_TABLE_SIZE; i++ )
        strHandle[ i ] = 0;
        
    strHandleCount = 0;
    
    strPoolIndex = 0;
    
    menuCount = 0;
    
    openMenuCount = 0;
    
    UI_InitMemory( );
    
    Item_SetupKeywordHash( );
    
    Menu_SetupKeywordHash( );
    
    if( DC && DC->getBindingBuf )
        Controls_GetConfig( );
}

/*
=================
PC_SourceWarning
=================
*/
void PC_SourceWarning( S32 handle, UTF8* format, ... )
{
    S32 line;
    UTF8 filename[128];
    va_list argptr;
    static UTF8 string[4096];
    
    va_start( argptr, format );
    Q_vsnprintf( string, sizeof( string ), format, argptr );
    va_end( argptr );
    
    filename[0] = '\0';
    line = 0;
    trap_PC_SourceFileAndLine( handle, filename, &line );
    
    Com_Printf( S_COLOR_YELLOW "WARNING: %s, line %d: %s\n", filename, line, string );
}

/*
=================
PC_SourceError
=================
*/
void PC_SourceError( S32 handle, UTF8* format, ... )
{
    S32 line;
    UTF8 filename[128];
    va_list argptr;
    static UTF8 string[4096];
    
    va_start( argptr, format );
    Q_vsnprintf( string, sizeof( string ), format, argptr );
    va_end( argptr );
    
    filename[0] = '\0';
    line = 0;
    trap_PC_SourceFileAndLine( handle, filename, &line );
    
    Com_Printf( S_COLOR_RED "ERROR: %s, line %d: %s\n", filename, line, string );
}

/*
=================
LerpColor
=================
*/
void LerpColor( vec4_t a, vec4_t b, vec4_t c, F32 t )
{
    S32 i;
    
    // lerp and clamp each component
    
    for( i = 0; i < 4; i++ )
    {
        c[i] = a[i] + t * ( b[i] - a[i] );
        
        if( c[i] < 0 )
            c[i] = 0;
        else if( c[i] > 1.0 )
            c[i] = 1.0;
    }
}

/*
=================
Float_Parse
=================
*/
bool Float_Parse( UTF8** p, F32* f )
{
    UTF8*  token;
    token = COM_ParseExt( p, false );
    
    if( token && token[0] != 0 )
    {
        *f = atof( token );
        return true;
    }
    else
        return false;
}

#define MAX_EXPR_ELEMENTS 32

typedef enum
{
    EXPR_OPERATOR,
    EXPR_VALUE
}
exprType_t;

typedef struct exprToken_s
{
    exprType_t  type;
    union
    {
        UTF8  op;
        F32 val;
    } u;
}
exprToken_t;

typedef struct exprList_s
{
    exprToken_t l[ MAX_EXPR_ELEMENTS ];
    S32 f, b;
}
exprList_t;

/*
=================
OpPrec

Return a value reflecting operator precedence
=================
*/
static ID_INLINE S32 OpPrec( UTF8 op )
{
    switch( op )
    {
        case '*':
            return 4;
            
        case '/':
            return 3;
            
        case '+':
            return 2;
            
        case '-':
            return 1;
            
        case '(':
            return 0;
            
        default:
            return -1;
    }
}

/*
=================
PC_Expression_Parse
=================
*/
static bool PC_Expression_Parse( S32 handle, F32* f )
{
    pc_token_t  token;
    S32         unmatchedParentheses = 0;
    exprList_t  stack, fifo;
    exprToken_t value;
    bool    expectingNumber = true;
    
#define FULL( a ) ( a.b >= ( MAX_EXPR_ELEMENTS - 1 ) )
#define EMPTY( a ) ( a.f > a.b )
    
#define PUSH_VAL( a, v ) \
  { \
    if( FULL( a ) ) \
      return false; \
    a.b++; \
    a.l[ a.b ].type = EXPR_VALUE; \
    a.l[ a.b ].u.val = v; \
  }
    
#define PUSH_OP( a, o ) \
  { \
    if( FULL( a ) ) \
      return false; \
    a.b++; \
    a.l[ a.b ].type = EXPR_OPERATOR; \
    a.l[ a.b ].u.op = o; \
  }
    
#define POP_STACK( a ) \
  { \
    if( EMPTY( a ) ) \
      return false; \
    value = a.l[ a.b ]; \
    a.b--; \
  }
    
#define PEEK_STACK_OP( a )  ( a.l[ a.b ].u.op )
#define PEEK_STACK_VAL( a ) ( a.l[ a.b ].u.val )
    
#define POP_FIFO( a ) \
  { \
    if( EMPTY( a ) ) \
      return false; \
    value = a.l[ a.f ]; \
    a.f++; \
  }
    
    stack.f = fifo.f = 0;
    stack.b = fifo.b = -1;
    
    while( trap_PC_ReadToken( handle, &token ) )
    {
        if( !unmatchedParentheses && token.string[ 0 ] == ')' )
            break;
            
        // Special case to catch negative numbers
        if( expectingNumber && token.string[ 0 ] == '-' )
        {
            if( !trap_PC_ReadToken( handle, &token ) )
                return false;
                
            token.floatvalue = -token.floatvalue;
        }
        
        if( token.type == TT_NUMBER )
        {
            if( !expectingNumber )
                return false;
                
            expectingNumber = !expectingNumber;
            
            PUSH_VAL( fifo, token.floatvalue );
        }
        else
        {
            switch( token.string[ 0 ] )
            {
                case '(':
                    unmatchedParentheses++;
                    PUSH_OP( stack, '(' );
                    break;
                    
                case ')':
                    unmatchedParentheses--;
                    
                    if( unmatchedParentheses < 0 )
                        return false;
                        
                    while( !EMPTY( stack ) && PEEK_STACK_OP( stack ) != '(' )
                    {
                        POP_STACK( stack );
                        PUSH_OP( fifo, value.u.op );
                    }
                    
                    // Pop the '('
                    POP_STACK( stack );
                    
                    break;
                    
                case '*':
                case '/':
                case '+':
                case '-':
                    if( expectingNumber )
                        return false;
                        
                    expectingNumber = !expectingNumber;
                    
                    if( EMPTY( stack ) )
                    {
                        PUSH_OP( stack, token.string[ 0 ] );
                    }
                    else
                    {
                        while( !EMPTY( stack ) && OpPrec( token.string[ 0 ] ) < OpPrec( PEEK_STACK_OP( stack ) ) )
                        {
                            POP_STACK( stack );
                            PUSH_OP( fifo, value.u.op );
                        }
                        
                        PUSH_OP( stack, token.string[ 0 ] );
                    }
                    
                    break;
                    
                default:
                    // Unknown token
                    return false;
            }
        }
    }
    
    while( !EMPTY( stack ) )
    {
        POP_STACK( stack );
        PUSH_OP( fifo, value.u.op );
    }
    
    while( !EMPTY( fifo ) )
    {
        POP_FIFO( fifo );
        
        if( value.type == EXPR_VALUE )
        {
            PUSH_VAL( stack, value.u.val );
        }
        else if( value.type == EXPR_OPERATOR )
        {
            UTF8 op = value.u.op;
            F32 operand1, operand2, result;
            
            POP_STACK( stack );
            operand2 = value.u.val;
            POP_STACK( stack );
            operand1 = value.u.val;
            
            switch( op )
            {
                case '*':
                    result = operand1 * operand2;
                    break;
                    
                case '/':
                    result = operand1 / operand2;
                    break;
                    
                case '+':
                    result = operand1 + operand2;
                    break;
                    
                case '-':
                    result = operand1 - operand2;
                    break;
                    
                default:
                    Com_Error( ERR_FATAL, "Unknown operator '%c' in postfix string", op );
                    return false;
            }
            
            PUSH_VAL( stack, result );
        }
    }
    
    POP_STACK( stack );
    
    *f = value.u.val;
    
    return true;
    
#undef FULL
#undef EMPTY
#undef PUSH_VAL
#undef PUSH_OP
#undef POP_STACK
#undef PEEK_STACK_OP
#undef PEEK_STACK_VAL
#undef POP_FIFO
}

/*
=================
PC_Float_Parse
=================
*/
bool PC_Float_Parse( S32 handle, F32* f )
{
    pc_token_t token;
    S32 negative = false;
    
    if( !trap_PC_ReadToken( handle, &token ) )
        return false;
        
    if( token.string[ 0 ] == '(' )
        return PC_Expression_Parse( handle, f );
        
    if( token.string[0] == '-' )
    {
        if( !trap_PC_ReadToken( handle, &token ) )
            return false;
            
        negative = true;
    }
    
    if( token.type != TT_NUMBER )
    {
        PC_SourceError( handle, "expected float but found %s\n", token.string );
        return false;
    }
    
    if( negative )
        * f = -token.floatvalue;
    else
        *f = token.floatvalue;
        
    return true;
}

/*
=================
Color_Parse
=================
*/
bool Color_Parse( UTF8** p, vec4_t* c )
{
    S32 i;
    F32 f;
    
    for( i = 0; i < 4; i++ )
    {
        if( !Float_Parse( p, &f ) )
            return false;
            
        ( *c )[i] = f;
    }
    
    return true;
}

/*
=================
PC_Color_Parse
=================
*/
bool PC_Color_Parse( S32 handle, vec4_t* c )
{
    S32 i;
    F32 f;
    
    for( i = 0; i < 4; i++ )
    {
        if( !PC_Float_Parse( handle, &f ) )
            return false;
            
        ( *c )[i] = f;
    }
    
    return true;
}

/*
=================
Int_Parse
=================
*/
bool Int_Parse( UTF8** p, S32* i )
{
    UTF8*  token;
    token = COM_ParseExt( p, false );
    
    if( token && token[0] != 0 )
    {
        *i = atoi( token );
        return true;
    }
    else
        return false;
}

/*
=================
PC_Int_Parse
=================
*/
bool PC_Int_Parse( S32 handle, S32* i )
{
    pc_token_t token;
    S32 negative = false;
    
    if( !trap_PC_ReadToken( handle, &token ) )
        return false;
        
    if( token.string[ 0 ] == '(' )
    {
        F32 f;
        
        if( PC_Expression_Parse( handle, &f ) )
        {
            *i = ( S32 )f;
            return true;
        }
        else
            return false;
    }
    
    if( token.string[0] == '-' )
    {
        if( !trap_PC_ReadToken( handle, &token ) )
            return false;
            
        negative = true;
    }
    
    if( token.type != TT_NUMBER )
    {
        PC_SourceError( handle, "expected integer but found %s\n", token.string );
        return false;
    }
    
    *i = token.intvalue;
    
    if( negative )
        * i = - *i;
        
    return true;
}

/*
=================
Rect_Parse
=================
*/
bool Rect_Parse( UTF8** p, rectDef_t* r )
{
    if( Float_Parse( p, &r->x ) )
    {
        if( Float_Parse( p, &r->y ) )
        {
            if( Float_Parse( p, &r->w ) )
            {
                if( Float_Parse( p, &r->h ) )
                    return true;
            }
        }
    }
    
    return false;
}

/*
=================
PC_Rect_Parse
=================
*/
bool PC_Rect_Parse( S32 handle, rectDef_t* r )
{
    if( PC_Float_Parse( handle, &r->x ) )
    {
        if( PC_Float_Parse( handle, &r->y ) )
        {
            if( PC_Float_Parse( handle, &r->w ) )
            {
                if( PC_Float_Parse( handle, &r->h ) )
                    return true;
            }
        }
    }
    
    return false;
}

/*
=================
String_Parse
=================
*/
bool String_Parse( UTF8** p, StringEntry* out )
{
    UTF8* token;
    
    token = COM_ParseExt( p, false );
    
    if( token && token[0] != 0 )
    {
        *( out ) = String_Alloc( token );
        return true;
    }
    
    return false;
}

/*
=================
PC_String_Parse
=================
*/
bool PC_String_Parse( S32 handle, StringEntry* out )
{
    pc_token_t token;
    
    if( !trap_PC_ReadToken( handle, &token ) )
        return false;
        
    *( out ) = String_Alloc( token.string );
    
    return true;
}

/*
=================
PC_Script_Parse
=================
*/
bool PC_Script_Parse( S32 handle, StringEntry* out )
{
    UTF8 script[1024];
    pc_token_t token;
    
    ::memset( script, 0, sizeof( script ) );
    // scripts start with { and have ; separated command lists.. commands are command, arg..
    // basically we want everything between the { } as it will be interpreted at run time
    
    if( !trap_PC_ReadToken( handle, &token ) )
        return false;
        
    if( Q_stricmp( token.string, "{" ) != 0 )
        return false;
        
    while( 1 )
    {
        if( !trap_PC_ReadToken( handle, &token ) )
            return false;
            
        if( Q_stricmp( token.string, "}" ) == 0 )
        {
            *out = String_Alloc( script );
            return true;
        }
        
        if( token.string[1] != '\0' )
            Q_strcat( script, 1024, va( "\"%s\"", token.string ) );
        else
            Q_strcat( script, 1024, token.string );
            
        Q_strcat( script, 1024, " " );
    }
    
    return false;
}

// display, window, menu, item code
//

/*
==================
Init_Display

Initializes the display with a structure to all the drawing routines
==================
*/
void Init_Display( displayContextDef_t* dc )
{
    DC = dc;
}



// type and style painting

void GradientBar_Paint( rectDef_t* rect, vec4_t color )
{
    // gradient bar takes two paints
    DC->setColor( color );
    DC->drawHandlePic( rect->x, rect->y, rect->w, rect->h, DC->Assets.gradientBar );
    DC->setColor( NULL );
}


/*
==================
Window_Init

Initializes a window structure ( windowDef_t ) with defaults

==================
*/
void Window_Init( WinDow* w )
{
    ::memset( w, 0, sizeof( windowDef_t ) );
    w->borderSize = 1;
    w->foreColor[0] = w->foreColor[1] = w->foreColor[2] = w->foreColor[3] = 1.0;
    w->cinematic = -1;
}

void Fade( S32* flags, F32* f, F32 clamp, S32* nextTime, S32 offsetTime, bool bFlags, F32 fadeAmount )
{
    if( *flags & ( WINDOW_FADINGOUT | WINDOW_FADINGIN ) )
    {
        if( DC->realTime > *nextTime )
        {
            *nextTime = DC->realTime + offsetTime;
            
            if( *flags & WINDOW_FADINGOUT )
            {
                *f -= fadeAmount;
                
                if( bFlags && *f <= 0.0 )
                    *flags &= ~( WINDOW_FADINGOUT | WINDOW_VISIBLE );
            }
            else
            {
                *f += fadeAmount;
                
                if( *f >= clamp )
                {
                    *f = clamp;
                    
                    if( bFlags )
                        *flags &= ~WINDOW_FADINGIN;
                }
            }
        }
    }
}



static void Window_Paint( WinDow* w, F32 fadeAmount, F32 fadeClamp, F32 fadeCycle )
{
    vec4_t color;
    rectDef_t fillRect = w->rect;
    
    
    if( DC->getCVarValue( "gui_developer" ) )
    {
        color[0] = color[1] = color[2] = color[3] = 1;
        DC->drawRect( w->rect.x, w->rect.y, w->rect.w, w->rect.h, 1, color );
    }
    
    if( w == NULL || ( w->style == 0 && w->border == 0 ) )
        return;
        
    if( w->border != 0 )
    {
        fillRect.x += w->borderSize;
        fillRect.y += w->borderSize;
        fillRect.w -= w->borderSize + 1;
        fillRect.h -= w->borderSize + 1;
    }
    
    if( w->style == WINDOW_STYLE_FILLED )
    {
        // box, but possible a shader that needs filled
        
        if( w->background )
        {
            Fade( &w->flags, &w->backColor[3], fadeClamp, &w->nextTime, fadeCycle, true, fadeAmount );
            DC->setColor( w->backColor );
            DC->drawHandlePic( fillRect.x, fillRect.y, fillRect.w, fillRect.h, w->background );
            DC->setColor( NULL );
        }
        else if( w->border == WINDOW_BORDER_ROUNDED )
            DC->fillRoundedRect( fillRect.x, fillRect.y, fillRect.w, fillRect.h, w->borderSize, w->backColor );
        else
            DC->fillRect( fillRect.x, fillRect.y, fillRect.w, fillRect.h, w->backColor );
    }
    else if( w->style == WINDOW_STYLE_GRADIENT )
    {
        GradientBar_Paint( &fillRect, w->backColor );
        // gradient bar
    }
    else if( w->style == WINDOW_STYLE_SHADER )
    {
        if( w->flags & WINDOW_FORECOLORSET )
            DC->setColor( w->foreColor );
            
        DC->drawHandlePic( fillRect.x, fillRect.y, fillRect.w, fillRect.h, w->background );
        DC->setColor( NULL );
    }
    else if( w->style == WINDOW_STYLE_CINEMATIC )
    {
        if( w->cinematic == -1 )
        {
            w->cinematic = DC->playCinematic( w->cinematicName, fillRect.x, fillRect.y, fillRect.w, fillRect.h );
            
            if( w->cinematic == -1 )
                w->cinematic = -2;
        }
        
        if( w->cinematic >= 0 )
        {
            DC->runCinematicFrame( w->cinematic );
            DC->drawCinematic( w->cinematic, fillRect.x, fillRect.y, fillRect.w, fillRect.h );
        }
    }
}

static void Border_Paint( WinDow* w )
{
    if( w == NULL || ( w->style == 0 && w->border == 0 ) )
        return;
        
    if( w->border == WINDOW_BORDER_FULL )
    {
        // full
        DC->drawRect( w->rect.x, w->rect.y, w->rect.w, w->rect.h, w->borderSize, w->borderColor );
    }
    else if( w->border == WINDOW_BORDER_HORZ )
    {
        // top/bottom
        DC->setColor( w->borderColor );
        DC->drawTopBottom( w->rect.x, w->rect.y, w->rect.w, w->rect.h, w->borderSize );
        DC->setColor( NULL );
    }
    else if( w->border == WINDOW_BORDER_VERT )
    {
        // left right
        DC->setColor( w->borderColor );
        DC->drawSides( w->rect.x, w->rect.y, w->rect.w, w->rect.h, w->borderSize );
        DC->setColor( NULL );
    }
    else if( w->border == WINDOW_BORDER_KCGRADIENT )
    {
        // this is just two gradient bars along each horz edge
        rectDef_t r = w->rect;
        r.h = w->borderSize;
        GradientBar_Paint( &r, w->borderColor );
        r.y = w->rect.y + w->rect.h - 1;
        GradientBar_Paint( &r, w->borderColor );
    }
    else if( w->border == WINDOW_BORDER_ROUNDED )
    {
        // full rounded
        DC->drawRoundedRect( w->rect.x, w->rect.y, w->rect.w, w->rect.h, w->borderSize, w->borderColor );
    }
}


void Item_SetScreenCoords( itemDef_t* item, F32 x, F32 y )
{
    if( item == NULL )
        return;
        
    if( item->window.border != 0 )
    {
        x += item->window.borderSize;
        y += item->window.borderSize;
    }
    
    item->window.rect.x = x + item->window.rectClient.x;
    item->window.rect.y = y + item->window.rectClient.y;
    item->window.rect.w = item->window.rectClient.w;
    item->window.rect.h = item->window.rectClient.h;
    
    // force the text rects to recompute
    item->textRect.w = 0;
    item->textRect.h = 0;
}

// FIXME: consolidate this with nearby stuff
void Item_UpdatePosition( itemDef_t* item )
{
    F32 x, y;
    menuDef_t* menu;
    
    if( item == NULL || item->parent == NULL )
        return;
        
    menu = ( menuDef_t* )item->parent;
    
    x = menu->window.rect.x;
    y = menu->window.rect.y;
    
    if( menu->window.border != 0 )
    {
        x += menu->window.borderSize;
        y += menu->window.borderSize;
    }
    
    Item_SetScreenCoords( item, x, y );
    
}

// menus
void Menu_UpdatePosition( menuDef_t* menu )
{
    S32 i;
    F32 x, y;
    
    if( menu == NULL )
        return;
        
    x = menu->window.rect.x;
    y = menu->window.rect.y;
    
    if( menu->window.border != 0 )
    {
        x += menu->window.borderSize;
        y += menu->window.borderSize;
    }
    
    for( i = 0; i < menu->itemCount; i++ )
        Item_SetScreenCoords( menu->items[i], x, y );
}

static void Menu_AspectiseRect( S32 bias, rectangle* rect )
{
    switch( bias )
    {
        case ALIGN_LEFT:
            rect->x *= DC->aspectScale;
            rect->w *= DC->aspectScale;
            break;
            
        case ALIGN_CENTER:
            rect->x = ( rect->x * DC->aspectScale ) +
                      ( 320.0f - ( 320.0f * DC->aspectScale ) );
            rect->w *= DC->aspectScale;
            break;
            
        case ALIGN_RIGHT:
            rect->x = 640.0f - ( ( 640.0f - rect->x ) * DC->aspectScale );
            rect->w *= DC->aspectScale;
            break;
            
        default:
        
        case ASPECT_NONE:
            break;
    }
}

void Menu_AspectCompensate( menuDef_t* menu )
{
    S32 i;
    
    if( menu->window.aspectBias != ASPECT_NONE )
    {
        Menu_AspectiseRect( menu->window.aspectBias, &menu->window.rect );
        
        for( i = 0; i < menu->itemCount; i++ )
        {
            menu->items[ i ]->window.rectClient.x *= DC->aspectScale;
            menu->items[ i ]->window.rectClient.w *= DC->aspectScale;
            menu->items[ i ]->textalignx *= DC->aspectScale;
        }
    }
    else
    {
        for( i = 0; i < menu->itemCount; i++ )
        {
            Menu_AspectiseRect( menu->items[ i ]->window.aspectBias,
                                &menu->items[ i ]->window.rectClient );
                                
            if( menu->items[ i ]->window.aspectBias != ASPECT_NONE )
                menu->items[ i ]->textalignx *= DC->aspectScale;
        }
    }
}

void Menu_PostParse( menuDef_t* menu )
{
    if( menu == NULL )
        return;
        
    if( menu->fullScreen )
    {
        menu->window.rect.x = 0;
        menu->window.rect.y = 0;
        menu->window.rect.w = 640;
        menu->window.rect.h = 480;
    }
    
    Menu_AspectCompensate( menu );
    Menu_UpdatePosition( menu );
}

itemDef_t* Menu_ClearFocus( menuDef_t* menu )
{
    S32 i;
    itemDef_t* ret = NULL;
    
    if( menu == NULL )
        return NULL;
        
    for( i = 0; i < menu->itemCount; i++ )
    {
        if( menu->items[i]->window.flags & WINDOW_HASFOCUS )
            ret = menu->items[i];
            
        menu->items[i]->window.flags &= ~WINDOW_HASFOCUS;
        
        if( menu->items[i]->leaveFocus )
            Item_RunScript( menu->items[i], menu->items[i]->leaveFocus );
    }
    
    return ret;
}

bool IsVisible( S32 flags )
{
    return ( flags & WINDOW_VISIBLE && !( flags & WINDOW_FADINGOUT ) );
}

bool Rect_ContainsPoint( rectDef_t* rect, F32 x, F32 y )
{
    if( rect )
    {
        if( x > rect->x && x < rect->x + rect->w && y > rect->y && y < rect->y + rect->h )
            return true;
    }
    
    return false;
}

S32 Menu_ItemsMatchingGroup( menuDef_t* menu, StringEntry name )
{
    S32 i;
    S32 count = 0;
    
    for( i = 0; i < menu->itemCount; i++ )
    {
        if( Q_stricmp( menu->items[i]->window.name, name ) == 0 ||
                ( menu->items[i]->window.group && Q_stricmp( menu->items[i]->window.group, name ) == 0 ) )
        {
            count++;
        }
    }
    
    return count;
}

itemDef_t* Menu_GetMatchingItemByNumber( menuDef_t* menu, S32 index, StringEntry name )
{
    S32 i;
    S32 count = 0;
    
    for( i = 0; i < menu->itemCount; i++ )
    {
        if( Q_stricmp( menu->items[i]->window.name, name ) == 0 ||
                ( menu->items[i]->window.group && Q_stricmp( menu->items[i]->window.group, name ) == 0 ) )
        {
            if( count == index )
                return menu->items[i];
                
            count++;
        }
    }
    
    return NULL;
}



void Script_SetColor( itemDef_t* item, UTF8** args )
{
    StringEntry name;
    S32 i;
    F32 f;
    vec4_t* out;
    // expecting type of color to set and 4 args for the color
    
    if( String_Parse( args, &name ) )
    {
        out = NULL;
        
        if( Q_stricmp( name, "backcolor" ) == 0 )
        {
            out = &item->window.backColor;
            item->window.flags |= WINDOW_BACKCOLORSET;
        }
        else if( Q_stricmp( name, "forecolor" ) == 0 )
        {
            out = &item->window.foreColor;
            item->window.flags |= WINDOW_FORECOLORSET;
        }
        else if( Q_stricmp( name, "bordercolor" ) == 0 )
            out = &item->window.borderColor;
            
        if( out )
        {
            for( i = 0; i < 4; i++ )
            {
                if( !Float_Parse( args, &f ) )
                    return;
                    
                ( *out )[i] = f;
            }
        }
    }
}

void Script_SetAsset( itemDef_t* item, UTF8** args )
{
    StringEntry name;
    // expecting name to set asset to
    
    if( String_Parse( args, &name ) )
    {
        // check for a model
        
        if( item->type == ITEM_TYPE_MODEL )
        {}
        
    }
    
}

void Script_SetBackground( itemDef_t* item, UTF8** args )
{
    StringEntry name;
    // expecting name to set asset to
    
    if( String_Parse( args, &name ) )
        item->window.background = DC->registerShaderNoMip( name );
}




itemDef_t* Menu_FindItemByName( menuDef_t* menu, StringEntry p )
{
    S32 i;
    
    if( menu == NULL || p == NULL )
        return NULL;
        
    for( i = 0; i < menu->itemCount; i++ )
    {
        if( Q_stricmp( p, menu->items[i]->window.name ) == 0 )
            return menu->items[i];
    }
    
    return NULL;
}

void Script_SetItemColor( itemDef_t* item, UTF8** args )
{
    StringEntry itemname;
    StringEntry name;
    vec4_t color;
    S32 i;
    vec4_t* out;
    // expecting type of color to set and 4 args for the color
    
    if( String_Parse( args, &itemname ) && String_Parse( args, &name ) )
    {
        itemDef_t* item2;
        S32 j;
        S32 count = Menu_ItemsMatchingGroup( ( menuDef_t* )item->parent, itemname );
        
        if( !Color_Parse( args, &color ) )
            return;
            
        for( j = 0; j < count; j++ )
        {
            item2 = Menu_GetMatchingItemByNumber( ( menuDef_t* )item->parent, j, itemname );
            
            if( item2 != NULL )
            {
                out = NULL;
                
                if( Q_stricmp( name, "backcolor" ) == 0 )
                    out = &item2->window.backColor;
                else if( Q_stricmp( name, "forecolor" ) == 0 )
                {
                    out = &item2->window.foreColor;
                    item2->window.flags |= WINDOW_FORECOLORSET;
                }
                else if( Q_stricmp( name, "bordercolor" ) == 0 )
                    out = &item2->window.borderColor;
                    
                if( out )
                {
                    for( i = 0; i < 4; i++ )
                        ( *out )[i] = color[i];
                }
            }
        }
    }
}


void Menu_ShowItemByName( menuDef_t* menu, StringEntry p, bool bShow )
{
    itemDef_t* item;
    S32 i;
    S32 count = Menu_ItemsMatchingGroup( menu, p );
    
    for( i = 0; i < count; i++ )
    {
        item = Menu_GetMatchingItemByNumber( menu, i, p );
        
        if( item != NULL )
        {
            if( bShow )
                item->window.flags |= WINDOW_VISIBLE;
            else
            {
                item->window.flags &= ~WINDOW_VISIBLE;
                // stop cinematics playing in the window
                
                if( item->window.cinematic >= 0 )
                {
                    DC->stopCinematic( item->window.cinematic );
                    item->window.cinematic = -1;
                }
            }
        }
    }
}

void Menu_FadeItemByName( menuDef_t* menu, StringEntry p, bool fadeOut )
{
    itemDef_t* item;
    S32 i;
    S32 count = Menu_ItemsMatchingGroup( menu, p );
    
    for( i = 0; i < count; i++ )
    {
        item = Menu_GetMatchingItemByNumber( menu, i, p );
        
        if( item != NULL )
        {
            if( fadeOut )
            {
                item->window.flags |= ( WINDOW_FADINGOUT | WINDOW_VISIBLE );
                item->window.flags &= ~WINDOW_FADINGIN;
            }
            else
            {
                item->window.flags |= ( WINDOW_VISIBLE | WINDOW_FADINGIN );
                item->window.flags &= ~WINDOW_FADINGOUT;
            }
        }
    }
}

menuDef_t* Menus_FindByName( StringEntry p )
{
    S32 i;
    
    for( i = 0; i < menuCount; i++ )
    {
        if( Q_stricmp( Menus[i].window.name, p ) == 0 )
            return & Menus[i];
    }
    
    return NULL;
}

static void Menu_RunCloseScript( menuDef_t* menu )
{
    if( menu && menu->window.flags & WINDOW_VISIBLE && menu->onClose )
    {
        itemDef_t item;
        item.parent = menu;
        Item_RunScript( &item, menu->onClose );
    }
}

static void Menus_Close( menuDef_t* menu )
{
    if( menu != NULL )
    {
        Menu_RunCloseScript( menu );
        menu->window.flags &= ~( WINDOW_VISIBLE | WINDOW_HASFOCUS );
        
        if( openMenuCount > 0 )
            openMenuCount--;
            
        if( openMenuCount > 0 )
            Menus_Activate( menuStack[ openMenuCount - 1 ] );
    }
}

void Menus_CloseByName( StringEntry p )
{
    Menus_Close( Menus_FindByName( p ) );
}

void Menus_CloseAll( bool force )
{
    S32 i;
    
    // Close any menus on the stack first
    if( openMenuCount > 0 )
    {
        for( i = openMenuCount; i > 0; i-- )
            Menus_Close( menuStack[ i ] );
            
        openMenuCount = 0;
    }
    
    for( i = 0; i < menuCount; i++ )
        if( !( Menus[i].window.flags & WINDOW_DONTCLOSEALL ) || force )
            Menus_Close( &Menus[ i ] );
    if( force )
    {
        openMenuCount = 0;
        g_editingField = false;
        g_waitingForKey = false;
        g_editItem = NULL;
    }
}


void Script_Show( itemDef_t* item, UTF8** args )
{
    StringEntry name;
    
    if( String_Parse( args, &name ) )
        Menu_ShowItemByName( ( menuDef_t* )item->parent, name, true );
}

void Script_Hide( itemDef_t* item, UTF8** args )
{
    StringEntry name;
    
    if( String_Parse( args, &name ) )
        Menu_ShowItemByName( ( menuDef_t* )item->parent, name, false );
}

void Script_FadeIn( itemDef_t* item, UTF8** args )
{
    StringEntry name;
    
    if( String_Parse( args, &name ) )
        Menu_FadeItemByName( ( menuDef_t* )item->parent, name, false );
}

void Script_FadeOut( itemDef_t* item, UTF8** args )
{
    StringEntry name;
    
    if( String_Parse( args, &name ) )
        Menu_FadeItemByName( ( menuDef_t* )item->parent, name, true );
}



void Script_Open( itemDef_t* item, UTF8** args )
{
    StringEntry name;
    
    if( String_Parse( args, &name ) )
        Menus_ActivateByName( name );
}

void Script_ConditionalOpen( itemDef_t* item, UTF8** args )
{
    StringEntry cvar;
    StringEntry name1;
    StringEntry name2;
    F32           val;
    
    if( String_Parse( args, &cvar ) && String_Parse( args, &name1 ) && String_Parse( args, &name2 ) )
    {
        val = DC->getCVarValue( cvar );
        
        if( val == 0.f )
            Menus_ActivateByName( name2 );
        else
            Menus_ActivateByName( name1 );
    }
}

void Script_Close( itemDef_t* item, UTF8** args )
{
    StringEntry name;
    
    if( String_Parse( args, &name ) )
        Menus_CloseByName( name );
}

void Menu_TransitionItemByName( menuDef_t* menu, StringEntry p, rectDef_t rectFrom, rectDef_t rectTo,
                                S32 time, F32 amt )
{
    itemDef_t* item;
    S32 i;
    S32 count = Menu_ItemsMatchingGroup( menu, p );
    
    for( i = 0; i < count; i++ )
    {
        item = Menu_GetMatchingItemByNumber( menu, i, p );
        
        if( item != NULL )
        {
            item->window.flags |= ( WINDOW_INTRANSITION | WINDOW_VISIBLE );
            item->window.offsetTime = time;
            memcpy( &item->window.rectClient, &rectFrom, sizeof( rectDef_t ) );
            memcpy( &item->window.rectEffects, &rectTo, sizeof( rectDef_t ) );
            item->window.rectEffects2.x = fabs( rectTo.x - rectFrom.x ) / amt;
            item->window.rectEffects2.y = fabs( rectTo.y - rectFrom.y ) / amt;
            item->window.rectEffects2.w = fabs( rectTo.w - rectFrom.w ) / amt;
            item->window.rectEffects2.h = fabs( rectTo.h - rectFrom.h ) / amt;
            Item_UpdatePosition( item );
        }
    }
}


void Script_Transition( itemDef_t* item, UTF8** args )
{
    StringEntry name;
    rectDef_t rectFrom, rectTo;
    S32 time;
    F32 amt;
    
    if( String_Parse( args, &name ) )
    {
        if( Rect_Parse( args, &rectFrom ) && Rect_Parse( args, &rectTo ) &&
                Int_Parse( args, &time ) && Float_Parse( args, &amt ) )
        {
            Menu_TransitionItemByName( ( menuDef_t* )item->parent, name, rectFrom, rectTo, time, amt );
        }
    }
}


void Menu_OrbitItemByName( menuDef_t* menu, StringEntry p, F32 x, F32 y, F32 cx, F32 cy, S32 time )
{
    itemDef_t* item;
    S32 i;
    S32 count = Menu_ItemsMatchingGroup( menu, p );
    
    for( i = 0; i < count; i++ )
    {
        item = Menu_GetMatchingItemByNumber( menu, i, p );
        
        if( item != NULL )
        {
            item->window.flags |= ( WINDOW_ORBITING | WINDOW_VISIBLE );
            item->window.offsetTime = time;
            item->window.rectEffects.x = cx;
            item->window.rectEffects.y = cy;
            item->window.rectClient.x = x;
            item->window.rectClient.y = y;
            Item_UpdatePosition( item );
        }
    }
}


void Script_Orbit( itemDef_t* item, UTF8** args )
{
    StringEntry name;
    F32 cx, cy, x, y;
    S32 time;
    
    if( String_Parse( args, &name ) )
    {
        if( Float_Parse( args, &x ) && Float_Parse( args, &y ) &&
                Float_Parse( args, &cx ) && Float_Parse( args, &cy ) && Int_Parse( args, &time ) )
        {
            Menu_OrbitItemByName( ( menuDef_t* )item->parent, name, x, y, cx, cy, time );
        }
    }
}



void Script_SetFocus( itemDef_t* item, UTF8** args )
{
    StringEntry name;
    itemDef_t* focusItem;
    
    if( String_Parse( args, &name ) )
    {
        focusItem = Menu_FindItemByName( ( menuDef_t* )item->parent, name );
        
        if( focusItem && !( focusItem->window.flags & WINDOW_DECORATION ) )
        {
            Menu_ClearFocus( ( menuDef_t* )item->parent );
            focusItem->window.flags |= WINDOW_HASFOCUS;
            
            if( focusItem->onFocus )
                Item_RunScript( focusItem, focusItem->onFocus );
                
            // Edit fields get activated too
            if( focusItem->type == ITEM_TYPE_EDITFIELD || focusItem->type == ITEM_TYPE_NUMERICFIELD )
            {
                g_editingField = true;
                g_editItem = focusItem;
            }
            
            if( DC->Assets.itemFocusSound )
                DC->startLocalSound( DC->Assets.itemFocusSound, CHAN_LOCAL_SOUND );
        }
    }
}

void Script_Reset( itemDef_t* item, UTF8** args )
{
    StringEntry name;
    itemDef_t* resetItem;
    
    if( String_Parse( args, &name ) )
    {
        resetItem = Menu_FindItemByName( ( menuDef_t* )item->parent, name );
        
        if( resetItem )
        {
            if( resetItem->type == ITEM_TYPE_LISTBOX )
            {
                listBoxDef_t* listPtr = ( listBoxDef_t* )resetItem->typeData;
                resetItem->cursorPos = 0;
                listPtr->startPos = 0;
                DC->feederSelection( resetItem->special, 0 );
            }
        }
    }
}

void Script_SetPlayerModel( itemDef_t* item, UTF8** args )
{
    StringEntry name;
    
    if( String_Parse( args, &name ) )
        DC->setCVar( "team_model", name );
}

void Script_SetPlayerHead( itemDef_t* item, UTF8** args )
{
    StringEntry name;
    
    if( String_Parse( args, &name ) )
        DC->setCVar( "team_headmodel", name );
}

void Script_SetCvar( itemDef_t* item, UTF8** args )
{
    StringEntry cvar, val;
    
    if( String_Parse( args, &cvar ) && String_Parse( args, &val ) )
        DC->setCVar( cvar, val );
        
}

void Script_Exec( itemDef_t* item, UTF8** args )
{
    StringEntry val;
    
    if( String_Parse( args, &val ) )
        DC->executeText( EXEC_APPEND, va( "%s ; ", val ) );
}

void Script_Play( itemDef_t* item, UTF8** args )
{
    StringEntry val;
    
    if( String_Parse( args, &val ) )
        DC->startLocalSound( DC->registerSound( val, false ), CHAN_LOCAL_SOUND );
}

void Script_playLooped( itemDef_t* item, UTF8** args )
{
    StringEntry val;
    
    if( String_Parse( args, &val ) )
    {
        DC->stopBackgroundTrack();
        DC->startBackgroundTrack( val, val, 0 );
    }
}

static bool UI_Text_Emoticon( StringEntry s, bool* escaped,
                              S32* length, qhandle_t* h, S32* width )
{
    UTF8 name[ MAX_EMOTICON_NAME_LEN ] = {""};
    StringEntry p = s;
    S32 i = 0;
    S32 j = 0;
    
    if( *p != '[' )
        return false;
    p++;
    
    *escaped = false;
    if( *p == '[' )
    {
        *escaped = true;
        p++;
    }
    
    while( *p && i < ( MAX_EMOTICON_NAME_LEN - 1 ) )
    {
        if( *p == ']' )
        {
            for( j = 0; j < DC->Assets.emoticonCount; j++ )
            {
                if( !Q_stricmp( DC->Assets.emoticons[ j ], name ) )
                {
                    if( *escaped )
                    {
                        *length = 1;
                        return true;
                    }
                    if( h )
                        *h = DC->Assets.emoticonShaders[ j ];
                    if( width )
                        *width = DC->Assets.emoticonWidths[ j ];
                    *length = i + 2;
                    return true;
                }
            }
            return false;
        }
        name[ i++ ] = *p;
        name[ i ] = '\0';
        p++;
    }
    return false;
}


F32 UI_Text_Width( StringEntry text, F32 scale, S32 limit )
{
    S32 count, len;
    F32 out;
    glyphInfo_t* glyph;
    F32 useScale;
    StringEntry s = text;
    fontInfo_t* font = &DC->Assets.textFont;
    S32 emoticonLen;
    bool emoticonEscaped;
    F32 emoticonW;
    S32 emoticons = 0;
    
    if( scale <= DC->getCVarValue( "gui_smallFont" ) )
        font = &DC->Assets.smallFont;
    else if( scale >= DC->getCVarValue( "gui_bigFont" ) )
        font = &DC->Assets.bigFont;
        
    useScale = scale * font->glyphScale;
    emoticonW = UI_Text_Height( "[", scale, 0 ) * DC->aspectScale;
    out = 0;
    
    if( text )
    {
        len = Q_PrintStrlen( text );
        
        if( limit > 0 && len > limit )
            len = limit;
            
        count = 0;
        
        while( s && *s && count < len )
        {
            glyph = &font->glyphs[( U8 ) * s];
            
            if( Q_IsColorString( s ) )
            {
                s += 2;
                continue;
            }
            else if( UI_Text_Emoticon( s, &emoticonEscaped, &emoticonLen, NULL, NULL ) )
            {
                if( emoticonEscaped )
                {
                    s++;
                }
                else
                {
                    s += emoticonLen;
                    emoticons++;
                    continue;
                }
            }
            out += ( glyph->xSkip * DC->aspectScale );
            s++;
            count++;
        }
    }
    
    return ( out * useScale ) + ( emoticons * emoticonW );
}

F32 UI_Text_Height( StringEntry text, F32 scale, S32 limit )
{
    S32 len, count;
    F32 max;
    glyphInfo_t* glyph;
    F32 useScale;
    StringEntry s = text;
    fontInfo_t* font = &DC->Assets.textFont;
    
    if( scale <= DC->getCVarValue( "gui_smallFont" ) )
        font = &DC->Assets.smallFont;
    else if( scale >= DC->getCVarValue( "gui_bigFont" ) )
        font = &DC->Assets.bigFont;
        
    useScale = scale * font->glyphScale;
    max = 0;
    
    if( text )
    {
        len = strlen( text );
        
        if( limit > 0 && len > limit )
            len = limit;
            
        count = 0;
        
        while( s && *s && count < len )
        {
            if( Q_IsColorString( s ) )
            {
                s += 2;
                continue;
            }
            else
            {
                glyph = &font->glyphs[( U8 ) * s];
                
                if( max < glyph->height )
                    max = glyph->height;
                    
                s++;
                count++;
            }
        }
    }
    
    return max * useScale;
}

F32 UI_Text_EmWidth( F32 scale )
{
    return UI_Text_Width( "M", scale, 0 );
}

F32 UI_Text_EmHeight( F32 scale )
{
    return UI_Text_Height( "M", scale, 0 );
}


/*
================
UI_AdjustFrom640

Adjusted for resolution and screen aspect ratio
================
*/
void UI_AdjustFrom640( F32* x, F32* y, F32* w, F32* h )
{
    *x *= DC->xscale;
    *y *= DC->yscale;
    *w *= DC->xscale;
    *h *= DC->yscale;
}

static void UI_Text_PaintChar( F32 x, F32 y, F32 width, F32 height,
                               F32 scale, F32 s, F32 t, F32 s2, F32 t2, qhandle_t hShader )
{
    F32 w, h;
    w = width * scale;
    h = height * scale;
    UI_AdjustFrom640( &x, &y, &w, &h );
    DC->drawStretchPic( x, y, w, h, s, t, s2, t2, hShader );
}

void UI_Text_Paint_Limit( F32* maxX, F32 x, F32 y, F32 scale,
                          vec4_t color, StringEntry text, F32 adjust, S32 limit )
{
    S32         len, count;
    vec4_t      newColor;
    glyphInfo_t* glyph;
    S32 emoticonLen = 0;
    qhandle_t emoticonHandle = 0;
    F32 emoticonH, emoticonW;
    bool emoticonEscaped;
    S32 emoticonWidth;
    
    emoticonH = UI_Text_Height( "[", scale, 0 );
    emoticonW = emoticonH * DC->aspectScale;
    
    if( text )
    {
        StringEntry s = text;
        F32 max = *maxX;
        F32 useScale;
        fontInfo_t* font = &DC->Assets.textFont;
        
        memcpy( &newColor[0], &color[0], sizeof( vec4_t ) );
        
        if( scale <= DC->getCVarValue( "gui_smallFont" ) )
            font = &DC->Assets.smallFont;
        else if( scale > DC->getCVarValue( "gui_bigFont" ) )
            font = &DC->Assets.bigFont;
            
        useScale = scale * font->glyphScale;
        
        DC->setColor( color );
        
        len = strlen( text );
        
        if( limit > 0 && len > limit )
            len = limit;
            
        count = 0;
        
        while( s && *s && count < len )
        {
            F32 width, height, skip, yadj;
            
            glyph = &font->glyphs[( U8 ) * s ];
            width = glyph->imageWidth * DC->aspectScale;
            height = glyph->imageHeight;
            skip = glyph->xSkip * DC->aspectScale;
            yadj = useScale * glyph->top;
            
            if( Q_IsColorString( s ) )
            {
                memcpy( newColor, g_color_table[ ColorIndex( *( s + 1 ) ) ], sizeof( newColor ) );
                newColor[ 3 ] = color[ 3 ];
                DC->setColor( newColor );
                s += 2;
                continue;
            }
            else if( UI_Text_Emoticon( s, &emoticonEscaped, &emoticonLen,
                                       &emoticonHandle, &emoticonWidth ) )
            {
                if( emoticonEscaped )
                {
                    s++;
                }
                else
                {
                    s += emoticonLen;
                    DC->setColor( NULL );
                    DC->drawHandlePic( x, y - yadj, ( emoticonW * emoticonWidth ),
                                       emoticonH, emoticonHandle );
                    DC->setColor( newColor );
                    x += ( emoticonW * emoticonWidth );
                    continue;
                }
            }
            
            if( UI_Text_Width( s, useScale, 1 ) + x > max )
            {
                *maxX = 0;
                break;
            }
            
            UI_Text_PaintChar( x, y - yadj,
                               width,
                               height,
                               useScale,
                               glyph->s,
                               glyph->t,
                               glyph->s2,
                               glyph->t2,
                               glyph->glyph );
            x += ( skip * useScale ) + adjust;
            *maxX = x;
            count++;
            s++;
        }
        
        DC->setColor( NULL );
    }
}

void UI_Text_Paint( F32 x, F32 y, F32 scale, vec4_t color, StringEntry text,
                    F32 adjust, S32 limit, S32 style )
{
    S32 len, count;
    vec4_t newColor;
    glyphInfo_t* glyph;
    F32 useScale;
    fontInfo_t* font = &DC->Assets.textFont;
    S32 emoticonLen = 0;
    qhandle_t emoticonHandle = 0;
    F32 emoticonH, emoticonW;
    bool emoticonEscaped;
    S32 emoticonWidth;
    
    if( scale <= DC->getCVarValue( "gui_smallFont" ) )
        font = &DC->Assets.smallFont;
    else if( scale >= DC->getCVarValue( "gui_bigFont" ) )
        font = &DC->Assets.bigFont;
        
    emoticonH = UI_Text_Height( "[", scale, 0 );
    emoticonW = emoticonH * DC->aspectScale;
    useScale = scale * font->glyphScale;
    
    if( text )
    {
        StringEntry s = text;
        DC->setColor( color );
        memcpy( &newColor[0], &color[0], sizeof( vec4_t ) );
        len = strlen( text );
        
        if( limit > 0 && len > limit )
            len = limit;
            
        count = 0;
        
        while( s && *s && count < len )
        {
            F32 width, height, skip, yadj;
            
            glyph = &font->glyphs[( U8 ) * s];
            width = glyph->imageWidth * DC->aspectScale;
            height = glyph->imageHeight;
            skip = glyph->xSkip * DC->aspectScale;
            yadj = useScale * glyph->top;
            
            if( Q_IsColorString( s ) )
            {
                memcpy( newColor, g_color_table[ColorIndex( *( s + 1 ) )], sizeof( newColor ) );
                newColor[3] = color[3];
                DC->setColor( newColor );
                s += 2;
                continue;
            }
            else if( UI_Text_Emoticon( s, &emoticonEscaped, &emoticonLen,
                                       &emoticonHandle, &emoticonWidth ) )
            {
                if( emoticonEscaped )
                {
                    s++;
                }
                else
                {
                    DC->setColor( NULL );
                    DC->drawHandlePic( x, y - yadj, ( emoticonW * emoticonWidth ),
                                       emoticonH, emoticonHandle );
                    DC->setColor( newColor );
                    x += ( emoticonW * emoticonWidth );
                    s += emoticonLen;
                    continue;
                }
            }
            
            if( style == ITEM_TEXTSTYLE_SHADOWED || style == ITEM_TEXTSTYLE_SHADOWEDMORE )
            {
                S32 ofs = style == ITEM_TEXTSTYLE_SHADOWED ? 1 : 2;
                colorBlack[3] = newColor[3];
                DC->setColor( colorBlack );
                UI_Text_PaintChar( x + ofs, y - yadj + ofs,
                                   width,
                                   height,
                                   useScale,
                                   glyph->s,
                                   glyph->t,
                                   glyph->s2,
                                   glyph->t2,
                                   glyph->glyph );
                DC->setColor( newColor );
                colorBlack[3] = 1.0;
            }
            else if( style == ITEM_TEXTSTYLE_NEON )
            {
                vec4_t glow, outer, inner, white;
                
                glow[ 0 ] = newColor[ 0 ] * 0.5;
                glow[ 1 ] = newColor[ 1 ] * 0.5;
                glow[ 2 ] = newColor[ 2 ] * 0.5;
                glow[ 3 ] = newColor[ 3 ] * 0.2;
                
                outer[ 0 ] = newColor[ 0 ];
                outer[ 1 ] = newColor[ 1 ];
                outer[ 2 ] = newColor[ 2 ];
                outer[ 3 ] = newColor[ 3 ];
                
                inner[ 0 ] = newColor[ 0 ] * 1.5 > 1.0f ? 1.0f : newColor[ 0 ] * 1.5;
                inner[ 1 ] = newColor[ 1 ] * 1.5 > 1.0f ? 1.0f : newColor[ 1 ] * 1.5;
                inner[ 2 ] = newColor[ 2 ] * 1.5 > 1.0f ? 1.0f : newColor[ 2 ] * 1.5;
                inner[ 3 ] = newColor[ 3 ];
                
                white[ 0 ] = white[ 1 ] = white[ 2 ] = white[ 3 ] = 1.0f;
                
                DC->setColor( glow );
                UI_Text_PaintChar( x - 1.5, y - yadj - 1.5,
                                   width + 3,
                                   height + 3,
                                   useScale,
                                   glyph->s,
                                   glyph->t,
                                   glyph->s2,
                                   glyph->t2,
                                   glyph->glyph );
                                   
                DC->setColor( outer );
                UI_Text_PaintChar( x - 1, y - yadj - 1,
                                   width + 2,
                                   height + 2,
                                   useScale,
                                   glyph->s,
                                   glyph->t,
                                   glyph->s2,
                                   glyph->t2,
                                   glyph->glyph );
                                   
                DC->setColor( inner );
                UI_Text_PaintChar( x - 0.5, y - yadj - 0.5,
                                   width + 1,
                                   height + 1,
                                   useScale,
                                   glyph->s,
                                   glyph->t,
                                   glyph->s2,
                                   glyph->t2,
                                   glyph->glyph );
                                   
                DC->setColor( white );
            }
            
            UI_Text_PaintChar( x, y - yadj,
                               width,
                               height,
                               useScale,
                               glyph->s,
                               glyph->t,
                               glyph->s2,
                               glyph->t2,
                               glyph->glyph );
                               
            x += ( skip * useScale ) + adjust;
            s++;
            count++;
        }
        DC->setColor( NULL );
    }
}

//FIXME: merge this with Text_Paint, somehow
void UI_Text_PaintWithCursor( F32 x, F32 y, F32 scale, vec4_t color, StringEntry text,
                              S32 cursorPos, UTF8 cursor, S32 limit, S32 style )
{
    S32 len, count;
    vec4_t newColor;
    glyphInfo_t* glyph, *glyph2;
    F32 yadj;
    F32 useScale;
    fontInfo_t* font = &DC->Assets.textFont;
    
    if( scale <= DC->getCVarValue( "gui_smallFont" ) )
        font = &DC->Assets.smallFont;
    else if( scale >= DC->getCVarValue( "gui_bigFont" ) )
        font = &DC->Assets.bigFont;
        
    useScale = scale * font->glyphScale;
    
    if( text )
    {
        F32 width2, height2, skip2;
        StringEntry s = text;
        DC->setColor( color );
        memcpy( &newColor[0], &color[0], sizeof( vec4_t ) );
        len = strlen( text );
        
        if( limit > 0 && len > limit )
            len = limit;
            
        count = 0;
        glyph2 = &font->glyphs[( U8 ) cursor];
        width2 = glyph2->imageWidth * DC->aspectScale;
        height2 = glyph2->imageHeight;
        skip2 = glyph2->xSkip * DC->aspectScale;
        
        while( s && *s && count < len )
        {
            F32 width, height, skip;
            glyph = &font->glyphs[( U8 ) * s];
            width = glyph->imageWidth * DC->aspectScale;
            height = glyph->imageHeight;
            skip = glyph->xSkip * DC->aspectScale;
            
            yadj = useScale * glyph->top;
            
            if( Q_IsColorString( s ) )
            {
                memcpy( newColor, g_color_table[ColorIndex( *( s + 1 ) )], sizeof( newColor ) );
                newColor[3] = color[3];
                DC->setColor( newColor );
            }
            
            if( style == ITEM_TEXTSTYLE_SHADOWED || style == ITEM_TEXTSTYLE_SHADOWEDMORE )
            {
                S32 ofs = style == ITEM_TEXTSTYLE_SHADOWED ? 1 : 2;
                colorBlack[3] = newColor[3];
                DC->setColor( colorBlack );
                UI_Text_PaintChar( x + ofs, y - yadj + ofs,
                                   width,
                                   height,
                                   useScale,
                                   glyph->s,
                                   glyph->t,
                                   glyph->s2,
                                   glyph->t2,
                                   glyph->glyph );
                colorBlack[3] = 1.0;
                DC->setColor( newColor );
            }
            else if( style == ITEM_TEXTSTYLE_NEON )
            {
                vec4_t glow, outer, inner, white;
                
                glow[ 0 ] = newColor[ 0 ] * 0.5;
                glow[ 1 ] = newColor[ 1 ] * 0.5;
                glow[ 2 ] = newColor[ 2 ] * 0.5;
                glow[ 3 ] = newColor[ 3 ] * 0.2;
                
                outer[ 0 ] = newColor[ 0 ];
                outer[ 1 ] = newColor[ 1 ];
                outer[ 2 ] = newColor[ 2 ];
                outer[ 3 ] = newColor[ 3 ];
                
                inner[ 0 ] = newColor[ 0 ] * 1.5 > 1.0f ? 1.0f : newColor[ 0 ] * 1.5;
                inner[ 1 ] = newColor[ 1 ] * 1.5 > 1.0f ? 1.0f : newColor[ 1 ] * 1.5;
                inner[ 2 ] = newColor[ 2 ] * 1.5 > 1.0f ? 1.0f : newColor[ 2 ] * 1.5;
                inner[ 3 ] = newColor[ 3 ];
                
                white[ 0 ] = white[ 1 ] = white[ 2 ] = white[ 3 ] = 1.0f;
                
                DC->setColor( glow );
                UI_Text_PaintChar( x - 1.5, y - yadj - 1.5,
                                   width + 3,
                                   height + 3,
                                   useScale,
                                   glyph->s,
                                   glyph->t,
                                   glyph->s2,
                                   glyph->t2,
                                   glyph->glyph );
                                   
                DC->setColor( outer );
                UI_Text_PaintChar( x - 1, y - yadj - 1,
                                   width + 2,
                                   height + 2,
                                   useScale,
                                   glyph->s,
                                   glyph->t,
                                   glyph->s2,
                                   glyph->t2,
                                   glyph->glyph );
                                   
                DC->setColor( inner );
                UI_Text_PaintChar( x - 0.5, y - yadj - 0.5,
                                   width + 1,
                                   height + 1,
                                   useScale,
                                   glyph->s,
                                   glyph->t,
                                   glyph->s2,
                                   glyph->t2,
                                   glyph->glyph );
                                   
                DC->setColor( white );
            }
            
            UI_Text_PaintChar( x, y - yadj,
                               width,
                               height,
                               useScale,
                               glyph->s,
                               glyph->t,
                               glyph->s2,
                               glyph->t2,
                               glyph->glyph );
                               
            yadj = useScale * glyph2->top;
            
            if( count == cursorPos && !( ( DC->realTime / BLINK_DIVISOR ) & 1 ) )
            {
                UI_Text_PaintChar( x, y - yadj,
                                   width2,
                                   height2,
                                   useScale,
                                   glyph2->s,
                                   glyph2->t,
                                   glyph2->s2,
                                   glyph2->t2,
                                   glyph2->glyph );
            }
            
            x += ( skip * useScale );
            s++;
            count++;
        }
        
        // need to paint cursor at end of text
        if( cursorPos == len && !( ( DC->realTime / BLINK_DIVISOR ) & 1 ) )
        {
            yadj = useScale * glyph2->top;
            UI_Text_PaintChar( x, y - yadj,
                               width2,
                               height2,
                               useScale,
                               glyph2->s,
                               glyph2->t,
                               glyph2->s2,
                               glyph2->t2,
                               glyph2->glyph );
                               
        }
        
        DC->setColor( NULL );
    }
}

commandDef_t commandList[] =
{
    {"fadein", &Script_FadeIn},                   // group/name
    {"fadeout", &Script_FadeOut},                 // group/name
    {"show", &Script_Show},                       // group/name
    {"hide", &Script_Hide},                       // group/name
    {"setcolor", &Script_SetColor},               // works on this
    {"open", &Script_Open},                       // menu
    {"conditionalopen", &Script_ConditionalOpen}, // menu
    {"close", &Script_Close},                     // menu
    {"setasset", &Script_SetAsset},               // works on this
    {"setbackground", &Script_SetBackground},     // works on this
    {"setitemcolor", &Script_SetItemColor},       // group/name
    {"setfocus", &Script_SetFocus},               // sets this background color to team color
    {"reset", &Script_Reset},                     // resets the state of the item argument
    {"setplayermodel", &Script_SetPlayerModel},   // sets this background color to team color
    {"setplayerhead", &Script_SetPlayerHead},     // sets this background color to team color
    {"transition", &Script_Transition},           // group/name
    {"setcvar", &Script_SetCvar},           // group/name
    {"exec", &Script_Exec},           // group/name
    {"play", &Script_Play},           // group/name
    {"playlooped", &Script_playLooped},           // group/name
    {"orbit", &Script_Orbit},                      // group/name
};

S32 scriptCommandCount = sizeof( commandList ) / sizeof( commandDef_t );


void Item_RunScript( itemDef_t* item, StringEntry s )
{
    UTF8 script[1024], *p;
    S32 i;
    bool bRan;
    ::memset( script, 0, sizeof( script ) );
    
    if( item && s && s[0] )
    {
        Q_strcat( script, 1024, s );
        p = script;
        
        while( 1 )
        {
            StringEntry command;
            // expect command then arguments, ; ends command, NULL ends script
            
            if( !String_Parse( &p, &command ) )
                return;
                
            if( command[0] == ';' && command[1] == '\0' )
                continue;
                
            bRan = false;
            
            for( i = 0; i < scriptCommandCount; i++ )
            {
                if( Q_stricmp( command, commandList[i].name ) == 0 )
                {
                    ( commandList[i].handler( item, &p ) );
                    bRan = true;
                    break;
                }
            }
            
            // not in our auto list, pass to handler
            if( !bRan )
                DC->runScript( &p );
        }
    }
}


bool Item_EnableShowViaCvar( itemDef_t* item, S32 flag )
{
    UTF8 script[1024], *p;
    ::memset( script, 0, sizeof( script ) );
    
    if( item && item->enableCvar && *item->enableCvar && item->cvarTest && *item->cvarTest )
    {
        UTF8 buff[1024];
        DC->getCVarString( item->cvarTest, buff, sizeof( buff ) );
        
        Q_strcat( script, 1024, item->enableCvar );
        p = script;
        
        while( 1 )
        {
            StringEntry val;
            // expect value then ; or NULL, NULL ends list
            
            if( !String_Parse( &p, &val ) )
                return ( item->cvarFlags & flag ) ? false : true;
                
            if( val[0] == ';' && val[1] == '\0' )
                continue;
                
            // enable it if any of the values are true
            if( item->cvarFlags & flag )
            {
                if( Q_stricmp( buff, val ) == 0 )
                    return true;
            }
            else
            {
                // disable it if any of the values are true
                
                if( Q_stricmp( buff, val ) == 0 )
                    return false;
            }
            
        }
        
        return ( item->cvarFlags & flag ) ? false : true;
    }
    
    return true;
}


// will optionaly set focus to this item
bool Item_SetFocus( itemDef_t* item, F32 x, F32 y )
{
    S32 i;
    itemDef_t* oldFocus;
    sfxHandle_t* sfx = &DC->Assets.itemFocusSound;
    bool playSound = false;
    menuDef_t* parent;
    // sanity check, non-null, not a decoration and does not already have the focus
    
    if( item == NULL || item->window.flags & WINDOW_DECORATION ||
            item->window.flags & WINDOW_HASFOCUS || !( item->window.flags & WINDOW_VISIBLE ) )
    {
        return false;
    }
    
    parent = ( menuDef_t* )item->parent;
    
    // items can be enabled and disabled based on cvars
    
    if( item->cvarFlags & ( CVAR_ENABLE | CVAR_DISABLE ) && !Item_EnableShowViaCvar( item, CVAR_ENABLE ) )
        return false;
        
    if( item->cvarFlags & ( CVAR_SHOW | CVAR_HIDE ) && !Item_EnableShowViaCvar( item, CVAR_SHOW ) )
        return false;
        
    oldFocus = Menu_ClearFocus( ( menuDef_t* )item->parent );
    
    if( item->type == ITEM_TYPE_TEXT )
    {
        rectDef_t r;
        r = item->textRect;
        r.y -= r.h;
        
        if( Rect_ContainsPoint( &r, x, y ) )
        {
            item->window.flags |= WINDOW_HASFOCUS;
            
            if( item->focusSound )
                sfx = &item->focusSound;
                
            playSound = true;
        }
        else
        {
            if( oldFocus )
            {
                oldFocus->window.flags |= WINDOW_HASFOCUS;
                
                if( oldFocus->onFocus )
                    Item_RunScript( oldFocus, oldFocus->onFocus );
            }
        }
    }
    else
    {
        item->window.flags |= WINDOW_HASFOCUS;
        
        if( item->onFocus )
            Item_RunScript( item, item->onFocus );
            
        if( item->focusSound )
            sfx = &item->focusSound;
            
        playSound = true;
    }
    
    if( playSound && sfx )
        DC->startLocalSound( *sfx, CHAN_LOCAL_SOUND );
        
    for( i = 0; i < parent->itemCount; i++ )
    {
        if( parent->items[i] == item )
        {
            parent->cursorItem = i;
            break;
        }
    }
    
    return true;
}

S32 Item_ListBox_MaxScroll( itemDef_t* item )
{
    listBoxDef_t* listPtr = ( listBoxDef_t* )item->typeData;
    S32 count = DC->feederCount( item->special );
    S32 max;
    
    if( item->window.flags & WINDOW_HORIZONTAL )
        max = count - ( item->window.rect.w / listPtr->elementWidth ) + 1;
    else
        max = count - ( item->window.rect.h / listPtr->elementHeight ) + 1;
        
    if( max < 0 )
        return 0;
        
    return max;
}

S32 Item_ListBox_ThumbPosition( itemDef_t* item )
{
    F32 max, pos, size;
    listBoxDef_t* listPtr = ( listBoxDef_t* )item->typeData;
    
    max = Item_ListBox_MaxScroll( item );
    
    if( item->window.flags & WINDOW_HORIZONTAL )
    {
        size = item->window.rect.w - ( SCROLLBAR_WIDTH * 2 ) - 2;
        
        if( max > 0 )
            pos = ( size - SCROLLBAR_WIDTH ) / ( F32 ) max;
        else
            pos = 0;
            
        pos *= listPtr->startPos;
        return item->window.rect.x + 1 + SCROLLBAR_WIDTH + pos;
    }
    else
    {
        size = item->window.rect.h - ( SCROLLBAR_HEIGHT * 2 ) - 2;
        
        if( max > 0 )
            pos = ( size - SCROLLBAR_HEIGHT ) / ( F32 ) max;
        else
            pos = 0;
            
        pos *= listPtr->startPos;
        return item->window.rect.y + 1 + SCROLLBAR_HEIGHT + pos;
    }
}

S32 Item_ListBox_ThumbDrawPosition( itemDef_t* item )
{
    S32 min, max;
    
    if( itemCapture == item )
    {
        if( item->window.flags & WINDOW_HORIZONTAL )
        {
            min = item->window.rect.x + SCROLLBAR_WIDTH + 1;
            max = item->window.rect.x + item->window.rect.w - 2 * SCROLLBAR_WIDTH - 1;
            
            if( DC->cursorx >= min + SCROLLBAR_WIDTH / 2 && DC->cursorx <= max + SCROLLBAR_WIDTH / 2 )
                return DC->cursorx - SCROLLBAR_WIDTH / 2;
            else
                return Item_ListBox_ThumbPosition( item );
        }
        else
        {
            min = item->window.rect.y + SCROLLBAR_HEIGHT + 1;
            max = item->window.rect.y + item->window.rect.h - 2 * SCROLLBAR_HEIGHT - 1;
            
            if( DC->cursory >= min + SCROLLBAR_HEIGHT / 2 && DC->cursory <= max + SCROLLBAR_HEIGHT / 2 )
                return DC->cursory - SCROLLBAR_HEIGHT / 2;
            else
                return Item_ListBox_ThumbPosition( item );
        }
    }
    else
        return Item_ListBox_ThumbPosition( item );
}

F32 Item_Slider_ThumbPosition( itemDef_t* item )
{
    F32 value, range, x;
    editFieldDef_t* editDef = ( editFieldDef_t* )item->typeData;
    
    if( item->text )
        x = item->textRect.x + item->textRect.w + ITEM_VALUE_OFFSET;
    else
        x = item->window.rect.x;
        
    if( editDef == NULL && item->cvar )
        return x;
        
    value = DC->getCVarValue( item->cvar );
    
    if( value < editDef->minVal )
        value = editDef->minVal;
    else if( value > editDef->maxVal )
        value = editDef->maxVal;
        
    range = editDef->maxVal - editDef->minVal;
    value -= editDef->minVal;
    value /= range;
    value *= SLIDER_WIDTH;
    x += value;
    
    return x;
}

static F32 Item_Slider_VScale( itemDef_t* item )
{
    if( SLIDER_THUMB_HEIGHT > item->window.rect.h )
        return item->window.rect.h / SLIDER_THUMB_HEIGHT;
    else
        return 1.0f;
}

S32 Item_Slider_OverSlider( itemDef_t* item, F32 x, F32 y )
{
    rectDef_t r;
    F32 vScale = Item_Slider_VScale( item );
    
    r.x = Item_Slider_ThumbPosition( item ) - ( SLIDER_THUMB_WIDTH / 2 );
    r.y = item->textRect.y - item->textRect.h +
          ( ( item->textRect.h - ( SLIDER_THUMB_HEIGHT * vScale ) ) / 2.0f );
    r.w = SLIDER_THUMB_WIDTH;
    r.h = SLIDER_THUMB_HEIGHT * vScale;
    
    if( Rect_ContainsPoint( &r, x, y ) )
        return WINDOW_LB_THUMB;
        
    return 0;
}

S32 Item_ListBox_OverLB( itemDef_t* item, F32 x, F32 y )
{
    rectDef_t r;
    listBoxDef_t* listPtr;
    S32 thumbstart;
    S32 count;
    
    count = DC->feederCount( item->special );
    listPtr = ( listBoxDef_t* )item->typeData;
    
    if( item->window.flags & WINDOW_HORIZONTAL )
    {
        // check if on left arrow
        r.x = item->window.rect.x;
        r.y = item->window.rect.y + item->window.rect.h - SCROLLBAR_HEIGHT;
        r.w = SCROLLBAR_WIDTH;
        r.h = SCROLLBAR_HEIGHT;
        
        if( Rect_ContainsPoint( &r, x, y ) )
            return WINDOW_LB_LEFTARROW;
            
        // check if on right arrow
        r.x = item->window.rect.x + item->window.rect.w - SCROLLBAR_WIDTH;
        
        if( Rect_ContainsPoint( &r, x, y ) )
            return WINDOW_LB_RIGHTARROW;
            
        // check if on thumb
        thumbstart = Item_ListBox_ThumbPosition( item );
        
        r.x = thumbstart;
        
        if( Rect_ContainsPoint( &r, x, y ) )
            return WINDOW_LB_THUMB;
            
        r.x = item->window.rect.x + SCROLLBAR_WIDTH;
        r.w = thumbstart - r.x;
        
        if( Rect_ContainsPoint( &r, x, y ) )
            return WINDOW_LB_PGUP;
            
        r.x = thumbstart + SCROLLBAR_WIDTH;
        r.w = item->window.rect.x + item->window.rect.w - SCROLLBAR_WIDTH;
        
        if( Rect_ContainsPoint( &r, x, y ) )
            return WINDOW_LB_PGDN;
    }
    else
    {
        r.x = item->window.rect.x + item->window.rect.w - SCROLLBAR_WIDTH;
        r.y = item->window.rect.y;
        r.w = SCROLLBAR_WIDTH;
        r.h = SCROLLBAR_HEIGHT;
        
        if( Rect_ContainsPoint( &r, x, y ) )
            return WINDOW_LB_LEFTARROW;
            
        r.y = item->window.rect.y + item->window.rect.h - SCROLLBAR_HEIGHT;
        
        if( Rect_ContainsPoint( &r, x, y ) )
            return WINDOW_LB_RIGHTARROW;
            
        thumbstart = Item_ListBox_ThumbPosition( item );
        r.y = thumbstart;
        
        if( Rect_ContainsPoint( &r, x, y ) )
            return WINDOW_LB_THUMB;
            
        r.y = item->window.rect.y + SCROLLBAR_HEIGHT;
        r.h = thumbstart - r.y;
        
        if( Rect_ContainsPoint( &r, x, y ) )
            return WINDOW_LB_PGUP;
            
        r.y = thumbstart + SCROLLBAR_HEIGHT;
        r.h = item->window.rect.y + item->window.rect.h - SCROLLBAR_HEIGHT;
        
        if( Rect_ContainsPoint( &r, x, y ) )
            return WINDOW_LB_PGDN;
    }
    
    return 0;
}


void Item_ListBox_MouseEnter( itemDef_t* item, F32 x, F32 y )
{
    rectDef_t r;
    listBoxDef_t* listPtr = ( listBoxDef_t* )item->typeData;
    
    item->window.flags &= ~( WINDOW_LB_LEFTARROW | WINDOW_LB_RIGHTARROW | WINDOW_LB_THUMB |
                             WINDOW_LB_PGUP | WINDOW_LB_PGDN );
    item->window.flags |= Item_ListBox_OverLB( item, x, y );
    
    if( item->window.flags & WINDOW_HORIZONTAL )
    {
        if( !( item->window.flags & ( WINDOW_LB_LEFTARROW | WINDOW_LB_RIGHTARROW | WINDOW_LB_THUMB |
                                      WINDOW_LB_PGUP | WINDOW_LB_PGDN ) ) )
        {
            // check for selection hit as we have exausted buttons and thumb
            
            if( listPtr->elementStyle == LISTBOX_IMAGE )
            {
                r.x = item->window.rect.x;
                r.y = item->window.rect.y;
                r.h = item->window.rect.h - SCROLLBAR_HEIGHT;
                r.w = item->window.rect.w - listPtr->drawPadding;
                
                if( Rect_ContainsPoint( &r, x, y ) )
                {
                    listPtr->cursorPos = ( S32 )( ( x - r.x ) / listPtr->elementWidth )  + listPtr->startPos;
                    
                    if( listPtr->cursorPos >= listPtr->endPos )
                        listPtr->cursorPos = listPtr->endPos;
                }
            }
        }
    }
    else if( !( item->window.flags & ( WINDOW_LB_LEFTARROW | WINDOW_LB_RIGHTARROW |
                                       WINDOW_LB_THUMB | WINDOW_LB_PGUP | WINDOW_LB_PGDN ) ) )
    {
        r.x = item->window.rect.x;
        r.y = item->window.rect.y;
        r.w = item->window.rect.w - SCROLLBAR_WIDTH;
        r.h = item->window.rect.h - listPtr->drawPadding;
        
        if( Rect_ContainsPoint( &r, x, y ) )
        {
            listPtr->cursorPos = ( S32 )( ( y - 2 - r.y ) / listPtr->elementHeight )  + listPtr->startPos;
            
            if( listPtr->cursorPos > listPtr->endPos )
                listPtr->cursorPos = listPtr->endPos;
        }
    }
}

void Item_MouseEnter( itemDef_t* item, F32 x, F32 y )
{
    rectDef_t r;
    
    if( item )
    {
        r = item->textRect;
        r.y -= r.h;
        // in the text rect?
        
        // items can be enabled and disabled based on cvars
        
        if( item->cvarFlags & ( CVAR_ENABLE | CVAR_DISABLE ) && !Item_EnableShowViaCvar( item, CVAR_ENABLE ) )
            return;
            
        if( item->cvarFlags & ( CVAR_SHOW | CVAR_HIDE ) && !Item_EnableShowViaCvar( item, CVAR_SHOW ) )
            return;
            
        if( Rect_ContainsPoint( &r, x, y ) )
        {
            if( !( item->window.flags & WINDOW_MOUSEOVERTEXT ) )
            {
                Item_RunScript( item, item->mouseEnterText );
                item->window.flags |= WINDOW_MOUSEOVERTEXT;
            }
            
            if( !( item->window.flags & WINDOW_MOUSEOVER ) )
            {
                Item_RunScript( item, item->mouseEnter );
                item->window.flags |= WINDOW_MOUSEOVER;
            }
            
        }
        else
        {
            // not in the text rect
            
            if( item->window.flags & WINDOW_MOUSEOVERTEXT )
            {
                // if we were
                Item_RunScript( item, item->mouseExitText );
                item->window.flags &= ~WINDOW_MOUSEOVERTEXT;
            }
            
            if( !( item->window.flags & WINDOW_MOUSEOVER ) )
            {
                Item_RunScript( item, item->mouseEnter );
                item->window.flags |= WINDOW_MOUSEOVER;
            }
            
            if( item->type == ITEM_TYPE_LISTBOX )
                Item_ListBox_MouseEnter( item, x, y );
        }
    }
}

void Item_MouseLeave( itemDef_t* item )
{
    if( item )
    {
        if( item->window.flags & WINDOW_MOUSEOVERTEXT )
        {
            Item_RunScript( item, item->mouseExitText );
            item->window.flags &= ~WINDOW_MOUSEOVERTEXT;
        }
        
        Item_RunScript( item, item->mouseExit );
        item->window.flags &= ~( WINDOW_LB_RIGHTARROW | WINDOW_LB_LEFTARROW );
    }
}

itemDef_t* Menu_HitTest( menuDef_t* menu, F32 x, F32 y )
{
    S32 i;
    
    for( i = 0; i < menu->itemCount; i++ )
    {
        if( Rect_ContainsPoint( &menu->items[i]->window.rect, x, y ) )
            return menu->items[i];
    }
    
    return NULL;
}

void Item_SetMouseOver( itemDef_t* item, bool focus )
{
    if( item )
    {
        if( focus )
            item->window.flags |= WINDOW_MOUSEOVER;
        else
            item->window.flags &= ~WINDOW_MOUSEOVER;
    }
}


bool Item_OwnerDraw_HandleKey( itemDef_t* item, S32 key )
{
    if( item && DC->ownerDrawHandleKey )
        return DC->ownerDrawHandleKey( item->window.ownerDraw, item->window.ownerDrawFlags, &item->special, key );
        
    return false;
}

bool Item_ListBox_HandleKey( itemDef_t* item, S32 key, bool down, bool force )
{
    listBoxDef_t* listPtr = ( listBoxDef_t* )item->typeData;
    S32 count = DC->feederCount( item->special );
    S32 max, viewmax;
    
    if( force || ( Rect_ContainsPoint( &item->window.rect, DC->cursorx, DC->cursory ) &&
                   item->window.flags & WINDOW_HASFOCUS ) )
    {
        max = Item_ListBox_MaxScroll( item );
        
        if( item->window.flags & WINDOW_HORIZONTAL )
        {
            viewmax = ( item->window.rect.w / listPtr->elementWidth );
            
            if( key == K_LEFTARROW || key == K_KP_LEFTARROW )
            {
                if( !listPtr->notselectable )
                {
                    listPtr->cursorPos--;
                    
                    if( listPtr->cursorPos < 0 )
                        listPtr->cursorPos = 0;
                        
                    if( listPtr->cursorPos < listPtr->startPos )
                        listPtr->startPos = listPtr->cursorPos;
                        
                    if( listPtr->cursorPos >= listPtr->startPos + viewmax )
                        listPtr->startPos = listPtr->cursorPos - viewmax + 1;
                        
                    item->cursorPos = listPtr->cursorPos;
                    DC->feederSelection( item->special, item->cursorPos );
                }
                else
                {
                    listPtr->startPos--;
                    
                    if( listPtr->startPos < 0 )
                        listPtr->startPos = 0;
                }
                
                return true;
            }
            
            if( key == K_RIGHTARROW || key == K_KP_RIGHTARROW )
            {
                if( !listPtr->notselectable )
                {
                    listPtr->cursorPos++;
                    
                    if( listPtr->cursorPos < listPtr->startPos )
                        listPtr->startPos = listPtr->cursorPos;
                        
                    if( listPtr->cursorPos >= count )
                        listPtr->cursorPos = count - 1;
                        
                    if( listPtr->cursorPos >= listPtr->startPos + viewmax )
                        listPtr->startPos = listPtr->cursorPos - viewmax + 1;
                        
                    item->cursorPos = listPtr->cursorPos;
                    DC->feederSelection( item->special, item->cursorPos );
                }
                else
                {
                    listPtr->startPos++;
                    
                    if( listPtr->startPos >= count )
                        listPtr->startPos = count - 1;
                }
                
                return true;
            }
        }
        else
        {
            viewmax = ( item->window.rect.h / listPtr->elementHeight );
            
            if( key == K_UPARROW || key == K_KP_UPARROW )
            {
                if( !listPtr->notselectable )
                {
                    listPtr->cursorPos--;
                    
                    if( listPtr->cursorPos < 0 )
                        listPtr->cursorPos = 0;
                        
                    if( listPtr->cursorPos < listPtr->startPos )
                        listPtr->startPos = listPtr->cursorPos;
                        
                    if( listPtr->cursorPos >= listPtr->startPos + viewmax )
                        listPtr->startPos = listPtr->cursorPos - viewmax + 1;
                        
                    item->cursorPos = listPtr->cursorPos;
                    DC->feederSelection( item->special, item->cursorPos );
                }
                else
                {
                    listPtr->startPos--;
                    
                    if( listPtr->startPos < 0 )
                        listPtr->startPos = 0;
                }
                
                return true;
            }
            
            if( key == K_DOWNARROW || key == K_KP_DOWNARROW )
            {
                if( !listPtr->notselectable )
                {
                    listPtr->cursorPos++;
                    
                    if( listPtr->cursorPos < listPtr->startPos )
                        listPtr->startPos = listPtr->cursorPos;
                        
                    if( listPtr->cursorPos >= count )
                        listPtr->cursorPos = count - 1;
                        
                    if( listPtr->cursorPos >= listPtr->startPos + viewmax )
                        listPtr->startPos = listPtr->cursorPos - viewmax + 1;
                        
                    item->cursorPos = listPtr->cursorPos;
                    DC->feederSelection( item->special, item->cursorPos );
                }
                else
                {
                    listPtr->startPos++;
                    
                    if( listPtr->startPos > max )
                        listPtr->startPos = max;
                }
                
                return true;
            }
        }
        
        // mouse hit
        if( key == K_MOUSE1 || key == K_MOUSE2 )
        {
            if( item->window.flags & WINDOW_LB_LEFTARROW )
            {
                listPtr->startPos--;
                
                if( listPtr->startPos < 0 )
                    listPtr->startPos = 0;
            }
            else if( item->window.flags & WINDOW_LB_RIGHTARROW )
            {
                // one down
                listPtr->startPos++;
                
                if( listPtr->startPos > max )
                    listPtr->startPos = max;
            }
            else if( item->window.flags & WINDOW_LB_PGUP )
            {
                // page up
                listPtr->startPos -= viewmax;
                
                if( listPtr->startPos < 0 )
                    listPtr->startPos = 0;
            }
            else if( item->window.flags & WINDOW_LB_PGDN )
            {
                // page down
                listPtr->startPos += viewmax;
                
                if( listPtr->startPos > max )
                    listPtr->startPos = max;
            }
            else if( item->window.flags & WINDOW_LB_THUMB )
            {
                // Display_SetCaptureItem(item);
            }
            else
            {
                // select an item
                
                if( item->cursorPos != listPtr->cursorPos )
                {
                    item->cursorPos = listPtr->cursorPos;
                    DC->feederSelection( item->special, item->cursorPos );
                }
                
                if( DC->realTime < lastListBoxClickTime && listPtr->doubleClick )
                    Item_RunScript( item, listPtr->doubleClick );
                    
                lastListBoxClickTime = DC->realTime + DOUBLE_CLICK_DELAY;
            }
            
            return true;
        }
        
        // Scroll wheel
        if( key == K_MWHEELUP )
        {
            listPtr->startPos--;
            
            if( listPtr->startPos < 0 )
                listPtr->startPos = 0;
                
            return true;
        }
        
        if( key == K_MWHEELDOWN )
        {
            listPtr->startPos++;
            
            if( listPtr->startPos > max )
                listPtr->startPos = max;
                
            return true;
        }
        
        // Invoke the doubleClick handler when enter is pressed
        if( key == K_ENTER )
        {
            if( listPtr->doubleClick )
                Item_RunScript( item, listPtr->doubleClick );
                
            return true;
        }
        
        if( key == K_HOME || key == K_KP_HOME )
        {
            // home
            listPtr->startPos = 0;
            return true;
        }
        
        if( key == K_END || key == K_KP_END )
        {
            // end
            listPtr->startPos = max;
            return true;
        }
        
        if( key == K_PGUP || key == K_KP_PGUP )
        {
            // page up
            
            if( !listPtr->notselectable )
            {
                listPtr->cursorPos -= viewmax;
                
                if( listPtr->cursorPos < 0 )
                    listPtr->cursorPos = 0;
                    
                if( listPtr->cursorPos < listPtr->startPos )
                    listPtr->startPos = listPtr->cursorPos;
                    
                if( listPtr->cursorPos >= listPtr->startPos + viewmax )
                    listPtr->startPos = listPtr->cursorPos - viewmax + 1;
                    
                item->cursorPos = listPtr->cursorPos;
                DC->feederSelection( item->special, item->cursorPos );
            }
            else
            {
                listPtr->startPos -= viewmax;
                
                if( listPtr->startPos < 0 )
                    listPtr->startPos = 0;
            }
            
            return true;
        }
        
        if( key == K_PGDN || key == K_KP_PGDN )
        {
            // page down
            
            if( !listPtr->notselectable )
            {
                listPtr->cursorPos += viewmax;
                
                if( listPtr->cursorPos < listPtr->startPos )
                    listPtr->startPos = listPtr->cursorPos;
                    
                if( listPtr->cursorPos >= count )
                    listPtr->cursorPos = count - 1;
                    
                if( listPtr->cursorPos >= listPtr->startPos + viewmax )
                    listPtr->startPos = listPtr->cursorPos - viewmax + 1;
                    
                item->cursorPos = listPtr->cursorPos;
                DC->feederSelection( item->special, item->cursorPos );
            }
            else
            {
                listPtr->startPos += viewmax;
                
                if( listPtr->startPos > max )
                    listPtr->startPos = max;
            }
            
            return true;
        }
    }
    
    return false;
}

bool Item_YesNo_HandleKey( itemDef_t* item, S32 key )
{
    if( Rect_ContainsPoint( &item->window.rect, DC->cursorx, DC->cursory ) &&
            item->window.flags & WINDOW_HASFOCUS && item->cvar )
    {
        if( key == K_MOUSE1 || key == K_ENTER || key == K_MOUSE2 || key == K_MOUSE3 )
        {
            DC->setCVar( item->cvar, va( "%i", !DC->getCVarValue( item->cvar ) ) );
            return true;
        }
    }
    
    return false;
    
}

S32 Item_Multi_CountSettings( itemDef_t* item )
{
    multiDef_t* multiPtr = ( multiDef_t* )item->typeData;
    
    if( multiPtr == NULL )
        return 0;
        
    return multiPtr->count;
}

S32 Item_Multi_FindCvarByValue( itemDef_t* item )
{
    UTF8 buff[1024];
    F32 value = 0;
    S32 i;
    multiDef_t* multiPtr = ( multiDef_t* )item->typeData;
    
    if( multiPtr )
    {
        if( multiPtr->strDef )
            DC->getCVarString( item->cvar, buff, sizeof( buff ) );
        else
            value = DC->getCVarValue( item->cvar );
            
        for( i = 0; i < multiPtr->count; i++ )
        {
            if( multiPtr->strDef )
            {
                if( Q_stricmp( buff, multiPtr->cvarStr[i] ) == 0 )
                    return i;
            }
            else
            {
                if( multiPtr->cvarValue[i] == value )
                    return i;
            }
        }
    }
    
    return 0;
}

StringEntry Item_Multi_Setting( itemDef_t* item )
{
    UTF8 buff[1024];
    F32 value = 0;
    S32 i;
    multiDef_t* multiPtr = ( multiDef_t* )item->typeData;
    
    if( multiPtr )
    {
        if( multiPtr->strDef )
            DC->getCVarString( item->cvar, buff, sizeof( buff ) );
        else
            value = DC->getCVarValue( item->cvar );
            
        for( i = 0; i < multiPtr->count; i++ )
        {
            if( multiPtr->strDef )
            {
                if( Q_stricmp( buff, multiPtr->cvarStr[i] ) == 0 )
                    return multiPtr->cvarList[i];
            }
            else
            {
                if( multiPtr->cvarValue[i] == value )
                    return multiPtr->cvarList[i];
            }
        }
    }
    
    return "";
}

bool Item_Combobox_HandleKey( itemDef_t* item, S32 key )
{
    comboBoxDef_t* comboPtr = ( comboBoxDef_t* )item->typeData;
    bool mouseOver = Rect_ContainsPoint( &item->window.rect, DC->cursorx, DC->cursory );
    S32 count = DC->feederCount( item->special );
    
    if( comboPtr )
    {
        if( item->window.flags & WINDOW_HASFOCUS )
        {
            if( ( mouseOver && key == K_MOUSE1 ) ||
                    key == K_ENTER || key == K_RIGHTARROW || key == K_DOWNARROW )
            {
                if( count > 0 )
                    comboPtr->cursorPos = ( comboPtr->cursorPos + 1 ) % count;
                    
                DC->feederSelection( item->special, comboPtr->cursorPos );
                
                return true;
            }
            else if( ( mouseOver && key == K_MOUSE2 ) ||
                     key == K_LEFTARROW || key == K_UPARROW )
            {
                if( count > 0 )
                    comboPtr->cursorPos = ( count + comboPtr->cursorPos - 1 ) % count;
                    
                DC->feederSelection( item->special, comboPtr->cursorPos );
                
                return true;
            }
        }
    }
    
    return false;
}

bool Item_Multi_HandleKey( itemDef_t* item, S32 key )
{
    multiDef_t* multiPtr = ( multiDef_t* )item->typeData;
    bool mouseOver = Rect_ContainsPoint( &item->window.rect, DC->cursorx, DC->cursory );
    S32 max = Item_Multi_CountSettings( item );
    bool changed = false;
    
    if( multiPtr )
    {
        if( item->window.flags & WINDOW_HASFOCUS && item->cvar && max > 0 )
        {
            S32 current;
            
            if( ( mouseOver && key == K_MOUSE1 ) ||
                    key == K_ENTER || key == K_RIGHTARROW || key == K_DOWNARROW )
            {
                current = ( Item_Multi_FindCvarByValue( item ) + 1 ) % max;
                changed = true;
            }
            else if( ( mouseOver && key == K_MOUSE2 ) ||
                     key == K_LEFTARROW || key == K_UPARROW )
            {
                current = ( Item_Multi_FindCvarByValue( item ) + max - 1 ) % max;
                changed = true;
            }
            
            if( changed )
            {
                if( multiPtr->strDef )
                    DC->setCVar( item->cvar, multiPtr->cvarStr[current] );
                else
                {
                    F32 value = multiPtr->cvarValue[current];
                    
                    if( ( ( F32 )( ( S32 ) value ) ) == value )
                        DC->setCVar( item->cvar, va( "%i", ( S32 ) value ) );
                    else
                        DC->setCVar( item->cvar, va( "%f", value ) );
                }
                
                return true;
            }
        }
    }
    
    return false;
}

#define MIN_FIELD_WIDTH   10
#define EDIT_CURSOR_WIDTH 10

static void Item_TextField_CalcPaintOffset( itemDef_t* item, UTF8* buff )
{
    editFieldDef_t*  editPtr = ( editFieldDef_t* )item->typeData;
    
    if( item->cursorPos < editPtr->paintOffset )
        editPtr->paintOffset = item->cursorPos;
    else
    {
        // If there is a maximum field width
        
        if( editPtr->maxFieldWidth > 0 )
        {
            // If the cursor is at the end of the string, maximise the amount of the
            // string that's visible
            
            if( buff[ item->cursorPos + 1 ] == '\0' )
            {
                while( UI_Text_Width( &buff[ editPtr->paintOffset ], item->textscale, 0 ) <=
                        ( editPtr->maxFieldWidth - EDIT_CURSOR_WIDTH ) && editPtr->paintOffset > 0 )
                    editPtr->paintOffset--;
            }
            
            buff[ item->cursorPos + 1 ] = '\0';
            
            // Shift paintOffset so that the cursor is visible
            
            while( UI_Text_Width( &buff[ editPtr->paintOffset ], item->textscale, 0 ) >
                    ( editPtr->maxFieldWidth - EDIT_CURSOR_WIDTH ) )
                editPtr->paintOffset++;
        }
    }
}

bool Item_TextField_HandleKey( itemDef_t* item, S32 key )
{
    UTF8 buff[1024];
    S32 len;
    itemDef_t* newItem = NULL;
    editFieldDef_t* editPtr = ( editFieldDef_t* )item->typeData;
    bool releaseFocus = true;
    
    if( item->cvar )
    {
        ::memset( buff, 0, sizeof( buff ) );
        DC->getCVarString( item->cvar, buff, sizeof( buff ) );
        len = strlen( buff );
        
        if( len < item->cursorPos )
            item->cursorPos = len;
            
        if( editPtr->maxChars && len > editPtr->maxChars )
            len = editPtr->maxChars;
            
        if( key & K_CHAR_FLAG )
        {
            key &= ~K_CHAR_FLAG;
            
            if( key == 'h' - 'a' + 1 )
            {
                // ctrl-h is backspace
                
                if( item->cursorPos > 0 )
                {
                    memmove( &buff[item->cursorPos - 1], &buff[item->cursorPos], len + 1 - item->cursorPos );
                    item->cursorPos--;
                }
                
                DC->setCVar( item->cvar, buff );
            }
            else if( ( key < 32 && key >= 0 ) || !item->cvar )
            {
                // Ignore any non printable UTF8s
                releaseFocus = false;
                goto exit;
            }
            else if( item->type == ITEM_TYPE_NUMERICFIELD && ( key < '0' || key > '9' ) )
            {
                // Ignore non-numeric UTF8acters
                releaseFocus = false;
                goto exit;
            }
            else
            {
                if( !DC->getOverstrikeMode() )
                {
                    if( ( len == MAX_EDITFIELD - 1 ) || ( editPtr->maxChars && len >= editPtr->maxChars ) )
                    {
                        // Reached maximum field length
                        releaseFocus = false;
                        goto exit;
                    }
                    
                    memmove( &buff[item->cursorPos + 1], &buff[item->cursorPos], len + 1 - item->cursorPos );
                }
                else
                {
                    // Reached maximum field length
                    
                    if( editPtr->maxChars && item->cursorPos >= editPtr->maxChars )
                        releaseFocus = false;
                        
                    goto exit;
                }
                
                buff[ item->cursorPos ] = key;
                
                DC->setCVar( item->cvar, buff );
                
                if( item->cursorPos < len + 1 )
                    item->cursorPos++;
            }
        }
        else
        {
            switch( key )
            {
                case K_DEL:
                case K_KP_DEL:
                    if( item->cursorPos < len )
                    {
                        memmove( buff + item->cursorPos, buff + item->cursorPos + 1, len - item->cursorPos );
                        DC->setCVar( item->cvar, buff );
                    }
                    
                    break;
                    
                case K_RIGHTARROW:
                case K_KP_RIGHTARROW:
                    if( item->cursorPos < len )
                        item->cursorPos++;
                        
                    break;
                    
                case K_LEFTARROW:
                case K_KP_LEFTARROW:
                    if( item->cursorPos > 0 )
                        item->cursorPos--;
                        
                    break;
                    
                case K_HOME:
                case K_KP_HOME:
                    item->cursorPos = 0;
                    
                    break;
                    
                case K_END:
                case K_KP_END:
                    item->cursorPos = len;
                    
                    break;
                    
                case K_INS:
                case K_KP_INS:
                    DC->setOverstrikeMode( !DC->getOverstrikeMode() );
                    
                    break;
                    
                case K_TAB:
                case K_DOWNARROW:
                case K_KP_DOWNARROW:
                case K_UPARROW:
                case K_KP_UPARROW:
                    newItem = Menu_SetNextCursorItem( ( menuDef_t* )item->parent );
                    
                    if( newItem && ( newItem->type == ITEM_TYPE_EDITFIELD || newItem->type == ITEM_TYPE_NUMERICFIELD ) )
                        g_editItem = newItem;
                    else
                    {
                        releaseFocus = true;
                        goto exit;
                    }
                    
                    break;
                    
                case K_ENTER:
                case K_KP_ENTER:
                case K_ESCAPE:
                case K_MOUSE1:
                case K_MOUSE2:
                case K_MOUSE3:
                case K_MOUSE4:
                    releaseFocus = true;
                    goto exit;
                    
                default:
                    break;
            }
        }
        
        releaseFocus = false;
    }
    
exit:
    Item_TextField_CalcPaintOffset( item, buff );
    
    return !releaseFocus;
}

static void Scroll_ListBox_AutoFunc( void* p )
{
    scrollInfo_t* si = ( scrollInfo_t* )p;
    
    if( DC->realTime > si->nextScrollTime )
    {
        // need to scroll which is done by simulating a click to the item
        // this is done a bit sideways as the autoscroll "knows" that the item is a listbox
        // so it calls it directly
        Item_ListBox_HandleKey( si->item, si->scrollKey, true, false );
        si->nextScrollTime = DC->realTime + si->adjustValue;
    }
    
    if( DC->realTime > si->nextAdjustTime )
    {
        si->nextAdjustTime = DC->realTime + SCROLL_TIME_ADJUST;
        
        if( si->adjustValue > SCROLL_TIME_FLOOR )
            si->adjustValue -= SCROLL_TIME_ADJUSTOFFSET;
    }
}

static void Scroll_ListBox_ThumbFunc( void* p )
{
    scrollInfo_t* si = ( scrollInfo_t* )p;
    rectDef_t r;
    S32 pos, max;
    
    listBoxDef_t* listPtr = ( listBoxDef_t* )si->item->typeData;
    
    if( si->item->window.flags & WINDOW_HORIZONTAL )
    {
        if( DC->cursorx == si->xStart )
            return;
            
        r.x = si->item->window.rect.x + SCROLLBAR_WIDTH + 1;
        r.y = si->item->window.rect.y + si->item->window.rect.h - SCROLLBAR_HEIGHT - 1;
        r.w = si->item->window.rect.w - ( SCROLLBAR_WIDTH * 2 ) - 2;
        r.h = SCROLLBAR_HEIGHT;
        max = Item_ListBox_MaxScroll( si->item );
        //
        pos = ( DC->cursorx - r.x - SCROLLBAR_WIDTH / 2 ) * max / ( r.w - SCROLLBAR_WIDTH );
        
        if( pos < 0 )
            pos = 0;
        else if( pos > max )
            pos = max;
            
        listPtr->startPos = pos;
        si->xStart = DC->cursorx;
    }
    else if( DC->cursory != si->yStart )
    {
        r.x = si->item->window.rect.x + si->item->window.rect.w - SCROLLBAR_WIDTH - 1;
        r.y = si->item->window.rect.y + SCROLLBAR_HEIGHT + 1;
        r.w = SCROLLBAR_WIDTH;
        r.h = si->item->window.rect.h - ( SCROLLBAR_HEIGHT * 2 ) - 2;
        max = Item_ListBox_MaxScroll( si->item );
        //
        pos = ( DC->cursory - r.y - SCROLLBAR_HEIGHT / 2 ) * max / ( r.h - SCROLLBAR_HEIGHT );
        
        if( pos < 0 )
            pos = 0;
        else if( pos > max )
            pos = max;
            
        listPtr->startPos = pos;
        si->yStart = DC->cursory;
    }
    
    if( DC->realTime > si->nextScrollTime )
    {
        // need to scroll which is done by simulating a click to the item
        // this is done a bit sideways as the autoscroll "knows" that the item is a listbox
        // so it calls it directly
        Item_ListBox_HandleKey( si->item, si->scrollKey, true, false );
        si->nextScrollTime = DC->realTime + si->adjustValue;
    }
    
    if( DC->realTime > si->nextAdjustTime )
    {
        si->nextAdjustTime = DC->realTime + SCROLL_TIME_ADJUST;
        
        if( si->adjustValue > SCROLL_TIME_FLOOR )
            si->adjustValue -= SCROLL_TIME_ADJUSTOFFSET;
    }
}

static void Scroll_Slider_ThumbFunc( void* p )
{
    F32 x, value, cursorx;
    scrollInfo_t* si = ( scrollInfo_t* )p;
    editFieldDef_t* editDef = ( editFieldDef_t* )si->item->typeData;
    
    if( si->item->text )
        x = si->item->textRect.x + si->item->textRect.w + ITEM_VALUE_OFFSET;
    else
        x = si->item->window.rect.x;
        
    cursorx = DC->cursorx;
    
    if( cursorx < x )
        cursorx = x;
    else if( cursorx > x + SLIDER_WIDTH )
        cursorx = x + SLIDER_WIDTH;
        
    value = cursorx - x;
    value /= SLIDER_WIDTH;
    value *= ( editDef->maxVal - editDef->minVal );
    value += editDef->minVal;
    DC->setCVar( si->item->cvar, va( "%f", value ) );
}

void Item_StartCapture( itemDef_t* item, S32 key )
{
    S32 flags;
    
    // Don't allow captureFunc to be overridden
    
    if( captureFunc != voidFunction )
        return;
        
    switch( item->type )
    {
        case ITEM_TYPE_EDITFIELD:
        case ITEM_TYPE_NUMERICFIELD:
        case ITEM_TYPE_LISTBOX:
        {
            flags = Item_ListBox_OverLB( item, DC->cursorx, DC->cursory );
            
            if( flags & ( WINDOW_LB_LEFTARROW | WINDOW_LB_RIGHTARROW ) )
            {
                scrollInfo.nextScrollTime = DC->realTime + SCROLL_TIME_START;
                scrollInfo.nextAdjustTime = DC->realTime + SCROLL_TIME_ADJUST;
                scrollInfo.adjustValue = SCROLL_TIME_START;
                scrollInfo.scrollKey = key;
                scrollInfo.scrollDir = ( flags & WINDOW_LB_LEFTARROW ) ? true : false;
                scrollInfo.item = item;
                UI_InstallCaptureFunc( Scroll_ListBox_AutoFunc, &scrollInfo, 0 );
                itemCapture = item;
            }
            else if( flags & WINDOW_LB_THUMB )
            {
                scrollInfo.scrollKey = key;
                scrollInfo.item = item;
                scrollInfo.xStart = DC->cursorx;
                scrollInfo.yStart = DC->cursory;
                UI_InstallCaptureFunc( Scroll_ListBox_ThumbFunc, &scrollInfo, 0 );
                itemCapture = item;
            }
            
            break;
        }
        
        case ITEM_TYPE_SLIDER:
        {
            flags = Item_Slider_OverSlider( item, DC->cursorx, DC->cursory );
            
            if( flags & WINDOW_LB_THUMB )
            {
                scrollInfo.scrollKey = key;
                scrollInfo.item = item;
                scrollInfo.xStart = DC->cursorx;
                scrollInfo.yStart = DC->cursory;
                UI_InstallCaptureFunc( Scroll_Slider_ThumbFunc, &scrollInfo, 0 );
                itemCapture = item;
            }
            
            break;
        }
    }
}

void Item_StopCapture( itemDef_t* item )
{
}

bool Item_Slider_HandleKey( itemDef_t* item, S32 key, bool down )
{
    F32 x, value, width, work;
    
    if( item->window.flags & WINDOW_HASFOCUS && item->cvar &&
            Rect_ContainsPoint( &item->window.rect, DC->cursorx, DC->cursory ) )
    {
        if( key == K_MOUSE1 || key == K_ENTER || key == K_MOUSE2 || key == K_MOUSE3 )
        {
            editFieldDef_t* editDef = ( editFieldDef_t* )item->typeData;
            
            if( editDef )
            {
                rectDef_t testRect;
                width = SLIDER_WIDTH;
                
                if( item->text )
                    x = item->textRect.x + item->textRect.w + ITEM_VALUE_OFFSET;
                else
                    x = item->window.rect.x;
                    
                testRect = item->window.rect;
                testRect.x = x;
                value = ( F32 )SLIDER_THUMB_WIDTH / 2;
                testRect.x -= value;
                testRect.w = ( SLIDER_WIDTH + ( F32 )SLIDER_THUMB_WIDTH / 2 );
                
                if( Rect_ContainsPoint( &testRect, DC->cursorx, DC->cursory ) )
                {
                    work = DC->cursorx - x;
                    value = work / width;
                    value *= ( editDef->maxVal - editDef->minVal );
                    // vm fuckage
                    // value = (((F32)(DC->cursorx - x)/ SLIDER_WIDTH) * (editDef->maxVal - editDef->minVal));
                    value += editDef->minVal;
                    DC->setCVar( item->cvar, va( "%f", value ) );
                    return true;
                }
            }
        }
    }
    
    return false;
}


bool Item_HandleKey( itemDef_t* item, S32 key, bool down )
{
    if( itemCapture )
    {
        Item_StopCapture( itemCapture );
        itemCapture = NULL;
        UI_RemoveCaptureFunc( );
    }
    else
    {
        if( down && ( key == K_MOUSE1 || key == K_MOUSE2 || key == K_MOUSE3 ) )
            Item_StartCapture( item, key );
    }
    
    if( !down )
        return false;
        
    switch( item->type )
    {
        case ITEM_TYPE_BUTTON:
            return false;
            break;
            
        case ITEM_TYPE_RADIOBUTTON:
            return false;
            break;
            
        case ITEM_TYPE_CHECKBOX:
            return false;
            break;
            
        case ITEM_TYPE_EDITFIELD:
        case ITEM_TYPE_NUMERICFIELD:
            //return Item_TextField_HandleKey(item, key);
            return false;
            break;
            
        case ITEM_TYPE_COMBO:
            return Item_Combobox_HandleKey( item, key );
            break;
            
        case ITEM_TYPE_LISTBOX:
            return Item_ListBox_HandleKey( item, key, down, false );
            break;
            
        case ITEM_TYPE_YESNO:
            return Item_YesNo_HandleKey( item, key );
            break;
            
        case ITEM_TYPE_MULTI:
            return Item_Multi_HandleKey( item, key );
            break;
            
        case ITEM_TYPE_OWNERDRAW:
            return Item_OwnerDraw_HandleKey( item, key );
            break;
            
        case ITEM_TYPE_BIND:
            return Item_Bind_HandleKey( item, key, down );
            break;
            
        case ITEM_TYPE_SLIDER:
            return Item_Slider_HandleKey( item, key, down );
            break;
            //case ITEM_TYPE_IMAGE:
            //  Item_Image_Paint(item);
            //  break;
            
        default:
            return false;
            break;
    }
    
    //return false;
}

void Item_Action( itemDef_t* item )
{
    if( item )
        Item_RunScript( item, item->action );
}

itemDef_t* Menu_SetPrevCursorItem( menuDef_t* menu )
{
    bool wrapped = false;
    S32 oldCursor = menu->cursorItem;
    
    if( menu->cursorItem < 0 )
    {
        menu->cursorItem = menu->itemCount - 1;
        wrapped = true;
    }
    
    while( menu->cursorItem > -1 )
    {
        menu->cursorItem--;
        
        if( menu->cursorItem < 0 && !wrapped )
        {
            wrapped = true;
            menu->cursorItem = menu->itemCount - 1;
        }
        
        if( Item_SetFocus( menu->items[menu->cursorItem], DC->cursorx, DC->cursory ) )
        {
            Menu_HandleMouseMove( menu, menu->items[menu->cursorItem]->window.rect.x + 1,
                                  menu->items[menu->cursorItem]->window.rect.y + 1 );
            return menu->items[menu->cursorItem];
        }
    }
    
    menu->cursorItem = oldCursor;
    return NULL;
    
}

itemDef_t* Menu_SetNextCursorItem( menuDef_t* menu )
{
    bool wrapped = false;
    S32 oldCursor = menu->cursorItem;
    
    
    if( menu->cursorItem == -1 )
    {
        menu->cursorItem = 0;
        wrapped = true;
    }
    
    while( menu->cursorItem < menu->itemCount )
    {
        menu->cursorItem++;
        
        if( menu->cursorItem >= menu->itemCount && !wrapped )
        {
            wrapped = true;
            menu->cursorItem = 0;
        }
        
        if( Item_SetFocus( menu->items[menu->cursorItem], DC->cursorx, DC->cursory ) )
        {
            Menu_HandleMouseMove( menu, menu->items[menu->cursorItem]->window.rect.x + 1,
                                  menu->items[menu->cursorItem]->window.rect.y + 1 );
            return menu->items[menu->cursorItem];
        }
        
    }
    
    menu->cursorItem = oldCursor;
    return NULL;
}

static void Window_CloseCinematic( windowDef_t* window )
{
    if( window->style == WINDOW_STYLE_CINEMATIC && window->cinematic >= 0 )
    {
        DC->stopCinematic( window->cinematic );
        window->cinematic = -1;
    }
}

static void Menu_CloseCinematics( menuDef_t* menu )
{
    if( menu )
    {
        S32 i;
        Window_CloseCinematic( &menu->window );
        
        for( i = 0; i < menu->itemCount; i++ )
        {
            Window_CloseCinematic( &menu->items[i]->window );
            
            if( menu->items[i]->type == ITEM_TYPE_OWNERDRAW )
                DC->stopCinematic( 0 - menu->items[i]->window.ownerDraw );
        }
    }
}

static void Display_CloseCinematics( void )
{
    S32 i;
    
    for( i = 0; i < menuCount; i++ )
        Menu_CloseCinematics( &Menus[i] );
}

void Menus_Activate( menuDef_t* menu )
{
    S32 i;
    bool onTopOfMenuStack = false;
    
    if( openMenuCount > 0 && menuStack[ openMenuCount - 1 ] == menu )
        onTopOfMenuStack = true;
        
    menu->window.flags |= ( WINDOW_HASFOCUS | WINDOW_VISIBLE );
    
    // If being opened for the first time
    if( !onTopOfMenuStack )
    {
        if( menu->onOpen )
        {
            itemDef_t item;
            item.parent = menu;
            Item_RunScript( &item, menu->onOpen );
        }
        
        if( menu->soundName && *menu->soundName )
            DC->startBackgroundTrack( menu->soundName, menu->soundName, 0 );
            
        Display_CloseCinematics( );
        
        Menu_HandleMouseMove( menu, DC->cursorx, DC->cursory ); // force the item under the cursor to focus
        
        for( i = 0; i < menu->itemCount; i++ ) // reset selection in listboxes when opened
        {
            if( menu->items[ i ]->type == ITEM_TYPE_LISTBOX )
            {
                listBoxDef_t* listPtr = ( listBoxDef_t* )menu->items[ i ]->typeData;
                menu->items[ i ]->cursorPos = 0;
                listPtr->startPos = 0;
                DC->feederSelection( menu->items[ i ]->special, 0 );
            }
            else if( menu->items[ i ]->type == ITEM_TYPE_COMBO )
            {
                comboBoxDef_t* comboPtr = ( comboBoxDef_t* )menu->items[ i ]->typeData;
                
                comboPtr->cursorPos = DC->feederInitialise( menu->items[ i ]->special );
            }
            
        }
        
        if( openMenuCount < MAX_OPEN_MENUS )
            menuStack[ openMenuCount++ ] = menu;
    }
}

S32 Display_VisibleMenuCount( void )
{
    S32 i, count;
    count = 0;
    
    for( i = 0; i < menuCount; i++ )
    {
        if( Menus[i].window.flags & ( WINDOW_FORCED | WINDOW_VISIBLE ) )
            count++;
    }
    
    return count;
}

void Menus_HandleOOBClick( menuDef_t* menu, S32 key, bool down )
{
    if( menu )
    {
        S32 i;
        // basically the behaviour we are looking for is if there are windows in the stack.. see if
        // the cursor is within any of them.. if not close them otherwise activate them and pass the
        // key on.. force a mouse move to activate focus and script stuff
        
        if( down && menu->window.flags & WINDOW_OOB_CLICK )
            Menus_Close( menu );
            
        for( i = 0; i < menuCount; i++ )
        {
            if( Menu_OverActiveItem( &Menus[i], DC->cursorx, DC->cursory ) )
            {
                Menus_Close( menu );
                Menus_Activate( &Menus[i] );
                Menu_HandleMouseMove( &Menus[i], DC->cursorx, DC->cursory );
                Menu_HandleKey( &Menus[i], key, down );
            }
        }
        
        if( Display_VisibleMenuCount() == 0 )
        {
            if( DC->Pause )
                DC->Pause( false );
        }
        
        Display_CloseCinematics();
    }
}

static rectDef_t* Item_CorrectedTextRect( itemDef_t* item )
{
    static rectDef_t rect;
    ::memset( &rect, 0, sizeof( rectDef_t ) );
    
    if( item )
    {
        rect = item->textRect;
        
        if( rect.w )
            rect.y -= rect.h;
    }
    
    return &rect;
}

void Menu_HandleKey( menuDef_t* menu, S32 key, bool down )
{
    S32 i;
    itemDef_t* item = NULL;
    bool inHandler = false;
    
    // KTW: Draggable Windows
    if( key == K_MOUSE1 && down && Rect_ContainsPoint( &menu->window.rect, DC->cursorx, DC->cursory ) &&
            menu->window.style && menu->window.border )
        menu->window.flags |= WINDOW_DRAG;
    else
        menu->window.flags &= ~WINDOW_DRAG;
        
    inHandler = true;
    
    if( g_waitingForKey && down )
    {
        Item_Bind_HandleKey( g_bindItem, key, down );
        inHandler = false;
        return;
    }
    
    if( g_editingField && down )
    {
        if( !Item_TextField_HandleKey( g_editItem, key ) )
        {
            g_editingField = false;
            Item_RunScript( g_editItem, g_editItem->onTextEntry );
            g_editItem = NULL;
            inHandler = false;
            return;
        }
        else if( key == K_MOUSE1 || key == K_MOUSE2 || key == K_MOUSE3 )
        {
            g_editingField = false;
            Item_RunScript( g_editItem, g_editItem->onTextEntry );
            g_editItem = NULL;
            Display_MouseMove( NULL, DC->cursorx, DC->cursory );
        }
        else if( key == K_TAB || key == K_UPARROW || key == K_DOWNARROW )
            return;
    }
    
    if( menu == NULL )
    {
        inHandler = false;
        return;
    }
    
    // see if the mouse is within the window bounds and if so is this a mouse click
    if( down && !( menu->window.flags & WINDOW_POPUP ) &&
            !Rect_ContainsPoint( &menu->window.rect, DC->cursorx, DC->cursory ) )
    {
        static bool inHandleKey = false;
        
        if( !inHandleKey && ( key == K_MOUSE1 || key == K_MOUSE2 || key == K_MOUSE3 ) )
        {
            inHandleKey = true;
            Menus_HandleOOBClick( menu, key, down );
            inHandleKey = false;
            inHandler = false;
            return;
        }
    }
    
    // get the item with focus
    for( i = 0; i < menu->itemCount; i++ )
    {
        if( menu->items[i]->window.flags & WINDOW_HASFOCUS )
            item = menu->items[i];
    }
    
    if( item != NULL )
    {
        if( Item_HandleKey( item, key, down ) )
        {
            Item_Action( item );
            inHandler = false;
            return;
        }
    }
    
    if( !down )
    {
        inHandler = false;
        return;
    }
    
    // default handling
    switch( key )
    {
        case K_F11:
            DC->executeText( EXEC_APPEND, "screenshotJPEG\n" );
            break;
            
        case K_KP_UPARROW:
        case K_UPARROW:
            Menu_SetPrevCursorItem( menu );
            break;
            
        case K_ESCAPE:
            if( !g_waitingForKey && menu->onESC )
            {
                itemDef_t it;
                it.parent = menu;
                Item_RunScript( &it, menu->onESC );
            }
            
            break;
            
        case K_TAB:
        case K_KP_DOWNARROW:
        case K_DOWNARROW:
            Menu_SetNextCursorItem( menu );
            break;
            
        case K_MOUSE1:
        case K_MOUSE2:
            if( item )
            {
                if( item->type == ITEM_TYPE_TEXT )
                {
                    if( Rect_ContainsPoint( Item_CorrectedTextRect( item ), DC->cursorx, DC->cursory ) )
                        Item_Action( item );
                }
                else if( item->type == ITEM_TYPE_EDITFIELD || item->type == ITEM_TYPE_NUMERICFIELD )
                {
                    if( Rect_ContainsPoint( &item->window.rect, DC->cursorx, DC->cursory ) )
                    {
                        UTF8 buffer[ MAX_STRING_CHARS ] = { 0 };
                        
                        if( item->cvar )
                            DC->getCVarString( item->cvar, buffer, sizeof( buffer ) );
                            
                        item->cursorPos = strlen( buffer );
                        
                        Item_TextField_CalcPaintOffset( item, buffer );
                        
                        g_editingField = true;
                        
                        g_editItem = item;
                    }
                }
                else
                {
                    if( Rect_ContainsPoint( &item->window.rect, DC->cursorx, DC->cursory ) )
                        Item_Action( item );
                }
            }
            
            break;
            
        case K_JOY1:
        case K_JOY2:
        case K_JOY3:
        case K_JOY4:
        case K_AUX1:
        case K_AUX2:
        case K_AUX3:
        case K_AUX4:
        case K_AUX5:
        case K_AUX6:
        case K_AUX7:
        case K_AUX8:
        case K_AUX9:
        case K_AUX10:
        case K_AUX11:
        case K_AUX12:
        case K_AUX13:
        case K_AUX14:
        case K_AUX15:
        case K_AUX16:
            break;
            
        case K_KP_ENTER:
        case K_ENTER:
            if( item )
            {
                if( item->type == ITEM_TYPE_EDITFIELD || item->type == ITEM_TYPE_NUMERICFIELD )
                {
                    UTF8 buffer[ MAX_STRING_CHARS ] = { 0 };
                    
                    if( item->cvar )
                        DC->getCVarString( item->cvar, buffer, sizeof( buffer ) );
                        
                    item->cursorPos = strlen( buffer );
                    
                    Item_TextField_CalcPaintOffset( item, buffer );
                    
                    g_editingField = true;
                    
                    g_editItem = item;
                }
                else
                    Item_Action( item );
            }
            
            break;
    }
    
    inHandler = false;
}

void ToWindowCoords( F32* x, F32* y, windowDef_t* window )
{
    if( window->border != 0 )
    {
        *x += window->borderSize;
        *y += window->borderSize;
    }
    
    *x += window->rect.x;
    *y += window->rect.y;
}

void Rect_ToWindowCoords( rectDef_t* rect, windowDef_t* window )
{
    ToWindowCoords( &rect->x, &rect->y, window );
}

void Item_SetTextExtents( itemDef_t* item, S32* width, S32* height, StringEntry text )
{
    StringEntry textPtr = ( text ) ? text : item->text;
    
    if( textPtr == NULL )
        return;
        
    *width = item->textRect.w;
    *height = item->textRect.h;
    
    // keeps us from computing the widths and heights more than once
    
    if( *width == 0 || ( item->type == ITEM_TYPE_OWNERDRAW && item->textalignment == ALIGN_CENTER ) )
    {
        S32 originalWidth;
        
        if( item->type == ITEM_TYPE_EDITFIELD && item->textalignment == ALIGN_CENTER && item->cvar )
        {
            //FIXME: this will only be called once?
            UTF8 buff[256];
            DC->getCVarString( item->cvar, buff, 256 );
            originalWidth = UI_Text_Width( item->text, item->textscale, 0 ) +
                            UI_Text_Width( buff, item->textscale, 0 );
        }
        else
            originalWidth = UI_Text_Width( item->text, item->textscale, 0 );
            
        *width = UI_Text_Width( textPtr, item->textscale, 0 );
        *height = UI_Text_Height( textPtr, item->textscale, 0 );
        item->textRect.w = *width;
        item->textRect.h = *height;
        
        if( item->textvalignment == VALIGN_BOTTOM )
            item->textRect.y = item->textaligny + item->window.rect.h;
        else if( item->textvalignment == VALIGN_CENTER )
            item->textRect.y = item->textaligny + ( ( *height + item->window.rect.h ) / 2.0f );
        else if( item->textvalignment == VALIGN_TOP )
            item->textRect.y = item->textaligny + *height;
            
        if( item->textalignment == ALIGN_LEFT )
            item->textRect.x = item->textalignx;
        else if( item->textalignment == ALIGN_CENTER )
            item->textRect.x = item->textalignx + ( ( item->window.rect.w - originalWidth ) / 2.0f );
        else if( item->textalignment == ALIGN_RIGHT )
            item->textRect.x = item->textalignx + item->window.rect.w - originalWidth;
            
        ToWindowCoords( &item->textRect.x, &item->textRect.y, &item->window );
    }
}

void Item_TextColor( itemDef_t* item, vec4_t* newColor )
{
    vec4_t lowLight;
    menuDef_t* parent = ( menuDef_t* )item->parent;
    
    Fade( &item->window.flags, &item->window.foreColor[3], parent->fadeClamp,
          &item->window.nextTime, parent->fadeCycle, true, parent->fadeAmount );
          
    if( item->window.flags & WINDOW_HASFOCUS )
        memcpy( newColor, &parent->focusColor, sizeof( vec4_t ) );
    else if( item->textStyle == ITEM_TEXTSTYLE_BLINK && !( ( DC->realTime / BLINK_DIVISOR ) & 1 ) )
    {
        lowLight[0] = 0.8 * item->window.foreColor[0];
        lowLight[1] = 0.8 * item->window.foreColor[1];
        lowLight[2] = 0.8 * item->window.foreColor[2];
        lowLight[3] = 0.8 * item->window.foreColor[3];
        LerpColor( item->window.foreColor, lowLight, *newColor, 0.5 + 0.5 * sin( DC->realTime / PULSE_DIVISOR ) );
    }
    else
    {
        memcpy( newColor, &item->window.foreColor, sizeof( vec4_t ) );
        // items can be enabled and disabled based on cvars
    }
    
    if( item->enableCvar != NULL && *item->enableCvar && item->cvarTest != NULL && *item->cvarTest )
    {
        if( item->cvarFlags & ( CVAR_ENABLE | CVAR_DISABLE ) && !Item_EnableShowViaCvar( item, CVAR_ENABLE ) )
            memcpy( newColor, &parent->disableColor, sizeof( vec4_t ) );
    }
}

static StringEntry Item_Text_Wrap( StringEntry text, F32 scale, F32 width )
{
    static UTF8   out[ 8192 ] = "";
    UTF8*          paint = out;
    UTF8          c[ 3 ] = "^7";
    StringEntry    p = text;
    StringEntry    eol;
    StringEntry    q = NULL, qMinus1 = NULL;
    U32  testLength;
    U32  i;
    S32           emoticonLen;
    bool      emoticonEscaped;
    
    if( strlen( text ) >= sizeof( out ) )
        return NULL;
        
    *paint = '\0';
    
    while( *p )
    {
        // Skip leading whitespace
        
        while( *p )
        {
            if( Q_IsColorString( p ) )
            {
                c[ 0 ] = p[ 0 ];
                c[ 1 ] = p[ 1 ];
                p += 2;
            }
            else if( *p != '\n' && isspace( *p ) )
                p++;
            else
                break;
        }
        
        if( !*p )
            break;
            
        Q_strcat( paint, out + sizeof( out ) - paint, c );
        
        testLength = 1;
        
        eol = p;
        
        q = p + 1;
        
        while( Q_IsColorString( q ) )
        {
            c[ 0 ] = q[ 0 ];
            c[ 1 ] = q[ 1 ];
            q += 2;
        }
        
        while( UI_Text_Width( p, scale, testLength ) < width )
        {
            if( testLength >= strlen( p ) )
            {
                eol = p + strlen( p );
                break;
            }
            
            // Point q at the end of the current testLength
            q = p;
            
            for( i = 0; i < testLength; )
            {
                // Skip color escapes
                while( Q_IsColorString( q ) )
                {
                    c[ 0 ] = q[ 0 ];
                    c[ 1 ] = q[ 1 ];
                    q += 2;
                }
                while( UI_Text_Emoticon( q, &emoticonEscaped, &emoticonLen, NULL, NULL ) )
                {
                    if( emoticonEscaped )
                        q++;
                    else
                        q += emoticonLen;
                }
                
                qMinus1 = q;
                q++;
                i++;
            }
            
            // Some color escapes might still be present
            while( Q_IsColorString( q ) )
            {
                c[ 0 ] = q[ 0 ];
                c[ 1 ] = q[ 1 ];
                q += 2;
            }
            while( UI_Text_Emoticon( q, &emoticonEscaped, &emoticonLen, NULL, NULL ) )
            {
                if( emoticonEscaped )
                    q++;
                else
                    q += emoticonLen;
            }
            
            // Manual line break
            if( *q == '\n' )
            {
                eol = q + 1;
                break;
            }
            
            if( !isspace( *qMinus1 ) && isspace( *q ) )
                eol = q;
                
            testLength++;
        }
        
        // No split has taken place, so just split mid-word
        if( eol == p )
            eol = q;
            
        paint = out + strlen( out );
        
        // Copy text
        strncpy( paint, p, eol - p );
        
        paint[ eol - p ] = '\0';
        
        // Add a \n if it's not there already
        if( out[ strlen( out ) - 1 ] != '\n' )
        {
            Q_strcat( out, sizeof( out ), "\n " );
            Q_strcat( out, sizeof( out ), c );
        }
        else
            c[ 0 ] = '\0';
            
        paint = out + strlen( out );
        
        p = eol;
    }
    
    return out;
}

#define MAX_WRAP_CACHE  16
#define MAX_WRAP_LINES  128
#define MAX_WRAP_TEXT   512

typedef struct
{
    UTF8      text[ MAX_WRAP_TEXT* MAX_WRAP_LINES ];  //FIXME: augment with hash?
    rectDef_t rect;
    F32     scale;
    UTF8      lines[ MAX_WRAP_LINES ][ MAX_WRAP_TEXT ];
    F32     lineCoords[ MAX_WRAP_LINES ][ 2 ];
    S32       numLines;
}

wrapCache_t;

static wrapCache_t  wrapCache[ MAX_WRAP_CACHE ];
static S32          cacheWriteIndex   = 0;
static S32          cacheReadIndex    = 0;
static S32          cacheReadLineNum  = 0;

static void UI_CreateCacheEntry( StringEntry text, rectDef_t* rect, F32 scale )
{
    wrapCache_t* cacheEntry = &wrapCache[ cacheWriteIndex ];
    
    Q_strncpyz( cacheEntry->text, text, sizeof( cacheEntry->text ) );
    cacheEntry->rect.x    = rect->x;
    cacheEntry->rect.y    = rect->y;
    cacheEntry->rect.w    = rect->w;
    cacheEntry->rect.h    = rect->h;
    cacheEntry->scale     = scale;
    cacheEntry->numLines  = 0;
}

static void UI_AddCacheEntryLine( StringEntry text, F32 x, F32 y )
{
    wrapCache_t* cacheEntry = &wrapCache[ cacheWriteIndex ];
    
    if( cacheEntry->numLines >= MAX_WRAP_LINES )
        return;
        
    Q_strncpyz( cacheEntry->lines[ cacheEntry->numLines ], text,
                sizeof( cacheEntry->lines[ 0 ] ) );
                
    cacheEntry->lineCoords[ cacheEntry->numLines ][ 0 ] = x;
    
    cacheEntry->lineCoords[ cacheEntry->numLines ][ 1 ] = y;
    
    cacheEntry->numLines++;
}

static void UI_FinishCacheEntry( void )
{
    cacheWriteIndex = ( cacheWriteIndex + 1 ) % MAX_WRAP_CACHE;
}

static bool UI_CheckWrapCache( StringEntry text, rectDef_t* rect, F32 scale )
{
    S32 i;
    
    for( i = 0; i < MAX_WRAP_CACHE; i++ )
    {
        wrapCache_t* cacheEntry = &wrapCache[ i ];
        
        if( Q_stricmp( text, cacheEntry->text ) )
            continue;
            
        if( rect->x != cacheEntry->rect.x ||
                rect->y != cacheEntry->rect.y ||
                rect->w != cacheEntry->rect.w ||
                rect->h != cacheEntry->rect.h )
            continue;
            
        if( cacheEntry->scale != scale )
            continue;
            
        // This is a match
        cacheReadIndex = i;
        
        cacheReadLineNum = 0;
        
        return true;
    }
    
    // No match - wrap isn't cached
    return false;
}

static bool UI_NextWrapLine( StringEntry* text, F32* x, F32* y )
{
    wrapCache_t* cacheEntry = &wrapCache[ cacheReadIndex ];
    
    if( cacheReadLineNum >= cacheEntry->numLines )
        return false;
        
    *text = cacheEntry->lines[ cacheReadLineNum ];
    
    *x    = cacheEntry->lineCoords[ cacheReadLineNum ][ 0 ];
    
    *y    = cacheEntry->lineCoords[ cacheReadLineNum ][ 1 ];
    
    cacheReadLineNum++;
    
    return true;
}

void Item_Text_Wrapped_Paint( itemDef_t* item )
{
    UTF8        text[ 1024 ];
    StringEntry  p, textPtr;
    F32       x, y, w, h;
    vec4_t      color;
    
    if( item->text == NULL )
    {
        if( item->cvar == NULL )
            return;
        else
        {
            DC->getCVarString( item->cvar, text, sizeof( text ) );
            textPtr = text;
        }
    }
    else
        textPtr = item->text;
        
    if( *textPtr == '\0' )
        return;
        
    Item_TextColor( item, &color );
    
    // Check if this block is cached
    if( ( bool )DC->getCVarValue( "gui_textWrapCache" ) &&
            UI_CheckWrapCache( textPtr, &item->window.rect, item->textscale ) )
    {
        while( UI_NextWrapLine( &p, &x, &y ) )
        {
            UI_Text_Paint( x, y, item->textscale, color,
                           p, 0, 0, item->textStyle );
        }
    }
    else
    {
        UTF8        buff[ 4096 ];
        F32       fontHeight    = UI_Text_EmHeight( item->textscale );
        const F32 lineSpacing   = fontHeight * 0.4f;
        F32       lineHeight    = fontHeight + lineSpacing;
        F32       textHeight;
        S32         textLength;
        S32         paintLines, totalLines, lineNum = 0;
        F32       paintY;
        S32         i;
        
        UI_CreateCacheEntry( textPtr, &item->window.rect, item->textscale );
        
        x = item->window.rect.x + item->textalignx;
        y = item->window.rect.y + item->textaligny;
        w = item->window.rect.w - ( 2.0f * item->textalignx );
        h = item->window.rect.h - ( 2.0f * item->textaligny );
        
        textPtr = Item_Text_Wrap( textPtr, item->textscale, w );
        textLength = strlen( textPtr );
        
        // Count lines
        totalLines = 0;
        
        for( i = 0; i < textLength; i++ )
        {
            if( textPtr[ i ] == '\n' )
                totalLines++;
        }
        
        paintLines = ( S32 )floor( ( h + lineSpacing ) / lineHeight );
        
        if( paintLines > totalLines )
            paintLines = totalLines;
            
        textHeight = ( paintLines * lineHeight ) - lineSpacing;
        
        switch( item->textvalignment )
        {
            default:
            
            case VALIGN_BOTTOM:
                paintY = y + ( h - textHeight );
                break;
                
            case VALIGN_CENTER:
                paintY = y + ( ( h - textHeight ) / 2.0f );
                break;
                
            case VALIGN_TOP:
                paintY = y;
                break;
        }
        
        p = textPtr;
        
        for( i = 0, lineNum = 0; i < textLength && lineNum < paintLines; i++ )
        {
            S32 lineLength = &textPtr[ i ] - p;
            
            if( lineLength >= sizeof( buff ) - 1 )
                break;
                
            if( textPtr[ i ] == '\n' || textPtr[ i ] == '\0' )
            {
                itemDef_t   lineItem;
                S32         width, height;
                
                strncpy( buff, p, lineLength );
                buff[ lineLength ] = '\0';
                p = &textPtr[ i + 1 ];
                
                lineItem.type               = ITEM_TYPE_TEXT;
                lineItem.textscale          = item->textscale;
                lineItem.textStyle          = item->textStyle;
                lineItem.text               = buff;
                lineItem.textalignment      = item->textalignment;
                lineItem.textvalignment     = VALIGN_TOP;
                lineItem.textalignx         = 0.0f;
                lineItem.textaligny         = 0.0f;
                
                lineItem.textRect.w         = 0.0f;
                lineItem.textRect.h         = 0.0f;
                lineItem.window.rect.x      = x;
                lineItem.window.rect.y      = paintY + ( lineNum * lineHeight );
                lineItem.window.rect.w      = w;
                lineItem.window.rect.h      = lineHeight;
                lineItem.window.border      = item->window.border;
                lineItem.window.borderSize  = item->window.borderSize;
                
                if( DC->getCVarValue( "gui_developer" ) )
                {
                    vec4_t color;
                    color[ 0 ] = color[ 2 ] = color[ 3 ] = 1.0f;
                    color[ 1 ] = 0.0f;
                    DC->drawRect( lineItem.window.rect.x, lineItem.window.rect.y,
                                  lineItem.window.rect.w, lineItem.window.rect.h, 1, color );
                }
                
                Item_SetTextExtents( &lineItem, &width, &height, buff );
                UI_Text_Paint( lineItem.textRect.x, lineItem.textRect.y,
                               lineItem.textscale, color, buff, 0, 0,
                               lineItem.textStyle );
                UI_AddCacheEntryLine( buff, lineItem.textRect.x, lineItem.textRect.y );
                
                lineNum++;
            }
        }
        
        UI_FinishCacheEntry( );
    }
}

/*
==============
UI_DrawTextBlock
==============
*/
void UI_DrawTextBlock( rectDef_t* rect, F32 text_x, F32 text_y, vec4_t color,
                       F32 scale, S32 textalign, S32 textvalign,
                       S32 textStyle, StringEntry text )
{
    static menuDef_t  dummyParent;
    static itemDef_t  textItem;
    
    textItem.text = text;
    
    textItem.parent = &dummyParent;
    memcpy( textItem.window.foreColor, color, sizeof( vec4_t ) );
    textItem.window.flags = 0;
    
    textItem.window.rect.x = rect->x;
    textItem.window.rect.y = rect->y;
    textItem.window.rect.w = rect->w;
    textItem.window.rect.h = rect->h;
    textItem.window.border = 0;
    textItem.window.borderSize = 0.0f;
    textItem.textRect.x = 0.0f;
    textItem.textRect.y = 0.0f;
    textItem.textRect.w = 0.0f;
    textItem.textRect.h = 0.0f;
    textItem.textalignment = textalign;
    textItem.textvalignment = textvalign;
    textItem.textalignx = text_x;
    textItem.textaligny = text_y;
    textItem.textscale = scale;
    textItem.textStyle = textStyle;
    
    // Utilise existing wrap code
    Item_Text_Wrapped_Paint( &textItem );
}

void Item_Text_Paint( itemDef_t* item )
{
    UTF8 text[1024];
    StringEntry textPtr;
    S32 height, width;
    vec4_t color;
    
    if( item->window.flags & WINDOW_WRAPPED )
    {
        Item_Text_Wrapped_Paint( item );
        return;
    }
    
    if( item->text == NULL )
    {
        if( item->cvar == NULL )
            return;
        else
        {
            DC->getCVarString( item->cvar, text, sizeof( text ) );
            textPtr = text;
        }
    }
    else
        textPtr = item->text;
        
    // this needs to go here as it sets extents for cvar types as well
    Item_SetTextExtents( item, &width, &height, textPtr );
    
    if( *textPtr == '\0' )
        return;
        
        
    Item_TextColor( item, &color );
    
    UI_Text_Paint( item->textRect.x, item->textRect.y, item->textscale, color, textPtr, 0, 0, item->textStyle );
}



void Item_TextField_Paint( itemDef_t* item )
{
    UTF8            buff[1024];
    vec4_t          newColor;
    S32             offset = ( item->text && *item->text ) ? ITEM_VALUE_OFFSET : 0;
    menuDef_t*       parent = ( menuDef_t* )item->parent;
    editFieldDef_t*  editPtr = ( editFieldDef_t* )item->typeData;
    UTF8            cursor = DC->getOverstrikeMode() ? '|' : '_';
    bool        editing = ( item->window.flags & WINDOW_HASFOCUS && g_editingField );
    const S32       cursorWidth = editing ? EDIT_CURSOR_WIDTH : 0;
    
    //FIXME: causes duplicate printing if item->text is not set (NULL)
    Item_Text_Paint( item );
    
    buff[0] = '\0';
    
    if( item->cvar )
        DC->getCVarString( item->cvar, buff, sizeof( buff ) );
        
    // maxFieldWidth hasn't been set, so use the item's rect
    if( editPtr->maxFieldWidth == 0 )
    {
        editPtr->maxFieldWidth = item->window.rect.w -
                                 ( item->textRect.w + offset + ( item->textRect.x - item->window.rect.x ) );
                                 
        if( editPtr->maxFieldWidth < MIN_FIELD_WIDTH )
            editPtr->maxFieldWidth = MIN_FIELD_WIDTH;
    }
    
    if( !editing )
        editPtr->paintOffset = 0;
        
    // Shorten string to max viewable
    while( UI_Text_Width( buff + editPtr->paintOffset, item->textscale, 0 ) >
            ( editPtr->maxFieldWidth - cursorWidth ) && strlen( buff ) > 0 )
        buff[ strlen( buff ) - 1 ] = '\0';
        
    parent = ( menuDef_t* )item->parent;
    
    if( item->window.flags & WINDOW_HASFOCUS )
        memcpy( newColor, &parent->focusColor, sizeof( vec4_t ) );
    else
        memcpy( &newColor, &item->window.foreColor, sizeof( vec4_t ) );
        
    if( editing )
    {
        UI_Text_PaintWithCursor( item->textRect.x + item->textRect.w + offset,
                                 item->textRect.y, item->textscale, newColor,
                                 buff + editPtr->paintOffset,
                                 item->cursorPos - editPtr->paintOffset,
                                 cursor, editPtr->maxPaintChars, item->textStyle );
    }
    else
    {
        UI_Text_Paint( item->textRect.x + item->textRect.w + offset,
                       item->textRect.y, item->textscale, newColor,
                       buff + editPtr->paintOffset, 0,
                       editPtr->maxPaintChars, item->textStyle );
    }
    
}

void Item_YesNo_Paint( itemDef_t* item )
{
    vec4_t newColor;
    F32 value;
    S32 offset;
    menuDef_t* parent = ( menuDef_t* )item->parent;
    
    value = ( item->cvar ) ? DC->getCVarValue( item->cvar ) : 0;
    
    if( item->window.flags & WINDOW_HASFOCUS )
        memcpy( newColor, &parent->focusColor, sizeof( vec4_t ) );
    else
        memcpy( &newColor, &item->window.foreColor, sizeof( vec4_t ) );
        
    offset = ( item->text && *item->text ) ? ITEM_VALUE_OFFSET : 0;
    
    if( item->text )
    {
        Item_Text_Paint( item );
        UI_Text_Paint( item->textRect.x + item->textRect.w + offset, item->textRect.y, item->textscale,
                       newColor, ( value != 0 ) ? "Yes" : "No", 0, 0, item->textStyle );
    }
    else
        UI_Text_Paint( item->textRect.x, item->textRect.y, item->textscale, newColor, ( value != 0 ) ? "Yes" : "No", 0, 0, item->textStyle );
}

void Item_Multi_Paint( itemDef_t* item )
{
    vec4_t newColor;
    StringEntry text = "";
    menuDef_t* parent = ( menuDef_t* )item->parent;
    
    if( item->window.flags & WINDOW_HASFOCUS )
        memcpy( newColor, &parent->focusColor, sizeof( vec4_t ) );
    else
        memcpy( &newColor, &item->window.foreColor, sizeof( vec4_t ) );
        
    text = Item_Multi_Setting( item );
    
    if( item->text )
    {
        Item_Text_Paint( item );
        UI_Text_Paint( item->textRect.x + item->textRect.w + ITEM_VALUE_OFFSET, item->textRect.y,
                       item->textscale, newColor, text, 0, 0, item->textStyle );
    }
    else
        UI_Text_Paint( item->textRect.x, item->textRect.y, item->textscale, newColor, text, 0, 0, item->textStyle );
}

void Item_Combobox_Paint( itemDef_t* item )
{
    vec4_t newColor;
    StringEntry text = "";
    menuDef_t* parent = ( menuDef_t* )item->parent;
    comboBoxDef_t* comboPtr = ( comboBoxDef_t* )item->typeData;
    
    if( item->window.flags & WINDOW_HASFOCUS )
        memcpy( newColor, &parent->focusColor, sizeof( vec4_t ) );
    else
        memcpy( &newColor, &item->window.foreColor, sizeof( vec4_t ) );
        
    if( comboPtr )
        text = DC->feederItemText( item->special, comboPtr->cursorPos, 0, NULL );
        
    if( item->text )
    {
        Item_Text_Paint( item );
        UI_Text_Paint( item->textRect.x + item->textRect.w + ITEM_VALUE_OFFSET, item->textRect.y,
                       item->textscale, newColor, text, 0, 0, item->textStyle );
    }
    else
        UI_Text_Paint( item->textRect.x, item->textRect.y, item->textscale, newColor, text, 0, 0, item->textStyle );
}


typedef struct
{
    UTF8*  command;
    S32   id;
    S32   defaultbind1;
    S32   defaultbind2;
    S32   bind1;
    S32   bind2;
}
bind_t;

typedef struct
{
    UTF8* name;
    F32 defaultvalue;
    F32 value;
}
configcvar_t;


static bind_t g_bindings[] =
{
    { "+scores",      K_TAB,         -1, -1, -1 },
    { "+button2",     K_ENTER,       -1, -1, -1 },
    { "+speed",       K_SHIFT,       -1, -1, -1 },
    { "+button6",     'x',           -1, -1, -1 }, // human sprinting
    { "+forward",     K_UPARROW,     -1, -1, -1 },
    { "+back",        K_DOWNARROW,   -1, -1, -1 },
    { "+moveleft",    ',',           -1, -1, -1 },
    { "+moveright",   '.',           -1, -1, -1 },
    { "+moveup",      K_SPACE,       -1, -1, -1 },
    { "+movedown",    'c',           -1, -1, -1 },
    { "+left",        K_LEFTARROW,   -1, -1, -1 },
    { "+right",       K_RIGHTARROW,  -1, -1, -1 },
    { "+strafe",      K_ALT,         -1, -1, -1 },
    { "+lookup",      K_PGDN,        -1, -1, -1 },
    { "+lookdown",    K_DEL,         -1, -1, -1 },
    { "+mlook",       '/',           -1, -1, -1 },
    { "centerview",   K_END,         -1, -1, -1 },
    { "+zoom",        -1,            -1, -1, -1 },
    { "weapon 1",     '1',           -1, -1, -1 },
    { "weapon 2",     '2',           -1, -1, -1 },
    { "weapon 3",     '3',           -1, -1, -1 },
    { "weapon 4",     '4',           -1, -1, -1 },
    { "weapon 5",     '5',           -1, -1, -1 },
    { "weapon 6",     '6',           -1, -1, -1 },
    { "weapon 7",     '7',           -1, -1, -1 },
    { "weapon 8",     '8',           -1, -1, -1 },
    { "weapon 9",     '9',           -1, -1, -1 },
    { "weapon 10",    '0',           -1, -1, -1 },
    { "weapon 11",    -1,            -1, -1, -1 },
    { "weapon 12",    -1,            -1, -1, -1 },
    { "weapon 13",    -1,            -1, -1, -1 },
    { "+attack",      K_MOUSE1,      -1, -1, -1 },
    { "+button5",     K_MOUSE2,      -1, -1, -1 }, // secondary attack
    { "reload",       'r',           -1, -1, -1 }, // reload
    { "buy ammo",     'b',           -1, -1, -1 }, // buy ammo
    { "itemact medkit", 'm',         -1, -1, -1 }, // use medkit
    { "+button7",     'q',           -1, -1, -1 }, // buildable use
    { "deconstruct",  'e',           -1, -1, -1 }, // buildable destroy
    { "weapprev",     '[',           -1, -1, -1 },
    { "weapnext",     ']',           -1, -1, -1 },
    { "+button3",     K_MOUSE3,      -1, -1, -1 },
    { "+button4",     K_MOUSE4,      -1, -1, -1 },
    { "vote yes",     K_F1,          -1, -1, -1 },
    { "vote no",      K_F2,          -1, -1, -1 },
    { "teamvote yes", K_F3,          -1, -1, -1 },
    { "teamvote no",  K_F4,          -1, -1, -1 },
    { "scoresUp",      K_KP_PGUP,    -1, -1, -1 },
    { "scoresDown",    K_KP_PGDN,    -1, -1, -1 },
    { "messagemode",  -1,            -1, -1, -1 },
    { "messagemode2", -1,            -1, -1, -1 },
    { "messagemode3", -1,            -1, -1, -1 },
    { "messagemode4", -1,            -1, -1, -1 },
    { "messagemode5", -1,            -1, -1, -1 },
    { "messagemode6", -1,            -1, -1, -1 },
    { "prompt",       -1,            -1, -1, -1 },
    { "squadmark",    'k',           -1, -1, -1 },
};


static const S32 g_bindCount = sizeof( g_bindings ) / sizeof( bind_t );

/*
=================
Controls_GetKeyAssignment
=================
*/
static void Controls_GetKeyAssignment( UTF8* command, S32* twokeys )
{
    S32   count;
    S32   j;
    UTF8  b[256];
    
    twokeys[0] = twokeys[1] = -1;
    count = 0;
    
    for( j = 0; j < 256; j++ )
    {
        DC->getBindingBuf( j, b, 256 );
        
        if( *b == 0 )
            continue;
            
        if( !Q_stricmp( b, command ) )
        {
            twokeys[count] = j;
            count++;
            
            if( count == 2 )
                break;
        }
    }
}

/*
=================
Controls_GetConfig
=================
*/
void Controls_GetConfig( void )
{
    S32   i;
    S32   twokeys[ 2 ];
    
    // iterate each command, get its numeric binding
    
    for( i = 0; i < g_bindCount; i++ )
    {
        Controls_GetKeyAssignment( g_bindings[ i ].command, twokeys );
        
        g_bindings[ i ].bind1 = twokeys[ 0 ];
        g_bindings[ i ].bind2 = twokeys[ 1 ];
    }
}

/*
=================
Controls_SetConfig
=================
*/
void Controls_SetConfig( bool restart )
{
    S32   i;
    
    // iterate each command, get its numeric binding
    
    for( i = 0; i < g_bindCount; i++ )
    {
        if( g_bindings[i].bind1 != -1 )
        {
            DC->setBinding( g_bindings[i].bind1, g_bindings[i].command );
            
            if( g_bindings[i].bind2 != -1 )
                DC->setBinding( g_bindings[i].bind2, g_bindings[i].command );
        }
    }
    
    DC->executeText( EXEC_APPEND, "in_restart\n" );
}

/*
=================
Controls_SetDefaults
=================
*/
void Controls_SetDefaults( void )
{
    S32 i;
    
    // iterate each command, set its default binding
    
    for( i = 0; i < g_bindCount; i++ )
    {
        g_bindings[i].bind1 = g_bindings[i].defaultbind1;
        g_bindings[i].bind2 = g_bindings[i].defaultbind2;
    }
}

S32 BindingIDFromName( StringEntry name )
{
    S32 i;
    
    for( i = 0; i < g_bindCount; i++ )
    {
        if( Q_stricmp( name, g_bindings[i].command ) == 0 )
            return i;
    }
    
    return -1;
}

UTF8 g_nameBind1[32];
UTF8 g_nameBind2[32];

void BindingFromName( StringEntry cvar )
{
    S32 i, b1, b2;
    
    // iterate each command, set its default binding
    
    for( i = 0; i < g_bindCount; i++ )
    {
        if( Q_stricmp( cvar, g_bindings[i].command ) == 0 )
        {
            b1 = g_bindings[i].bind1;
            
            if( b1 == -1 )
                break;
                
            DC->keynumToStringBuf( b1, g_nameBind1, 32 );
            Q_strupr( g_nameBind1 );
            
            b2 = g_bindings[i].bind2;
            
            if( b2 != -1 )
            {
                DC->keynumToStringBuf( b2, g_nameBind2, 32 );
                Q_strupr( g_nameBind2 );
                strcat( g_nameBind1, " or " );
                strcat( g_nameBind1, g_nameBind2 );
            }
            
            return;
        }
    }
    
    strcpy( g_nameBind1, "???" );
}

void Item_Slider_Paint( itemDef_t* item )
{
    vec4_t newColor;
    F32 x, y, value;
    menuDef_t* parent = ( menuDef_t* )item->parent;
    F32 vScale = Item_Slider_VScale( item );
    
    value = ( item->cvar ) ? DC->getCVarValue( item->cvar ) : 0;
    
    if( item->window.flags & WINDOW_HASFOCUS )
        memcpy( newColor, &parent->focusColor, sizeof( vec4_t ) );
    else
        memcpy( &newColor, &item->window.foreColor, sizeof( vec4_t ) );
        
    if( item->text )
    {
        Item_Text_Paint( item );
        x = item->textRect.x + item->textRect.w + ITEM_VALUE_OFFSET;
        y = item->textRect.y - item->textRect.h +
            ( ( item->textRect.h - ( SLIDER_HEIGHT * vScale ) ) / 2.0f );
    }
    else
    {
        x = item->window.rect.x;
        y = item->window.rect.y;
    }
    
    DC->setColor( newColor );
    DC->drawHandlePic( x, y, SLIDER_WIDTH, SLIDER_HEIGHT * vScale, DC->Assets.sliderBar );
    
    y = item->textRect.y - item->textRect.h +
        ( ( item->textRect.h - ( SLIDER_THUMB_HEIGHT * vScale ) ) / 2.0f );
        
    x = Item_Slider_ThumbPosition( item );
    DC->drawHandlePic( x - ( SLIDER_THUMB_WIDTH / 2 ), y,
                       SLIDER_THUMB_WIDTH, SLIDER_THUMB_HEIGHT * vScale, DC->Assets.sliderThumb );
                       
}

void Item_Bind_Paint( itemDef_t* item )
{
    vec4_t newColor, lowLight;
    F32 value;
    S32 maxChars = 0;
    menuDef_t* parent = ( menuDef_t* )item->parent;
    editFieldDef_t* editPtr = ( editFieldDef_t* )item->typeData;
    
    if( editPtr )
        maxChars = editPtr->maxPaintChars;
        
    value = ( item->cvar ) ? DC->getCVarValue( item->cvar ) : 0;
    
    if( item->window.flags & WINDOW_HASFOCUS )
    {
        if( g_bindItem == item )
        {
            lowLight[0] = 0.8f * parent->focusColor[0];
            lowLight[1] = 0.8f * parent->focusColor[1];
            lowLight[2] = 0.8f * parent->focusColor[2];
            lowLight[3] = 0.8f * parent->focusColor[3];
            
            LerpColor( parent->focusColor, lowLight, newColor,
                       0.5 + 0.5 * sin( DC->realTime / PULSE_DIVISOR ) );
        }
        else
            memcpy( &newColor, &parent->focusColor, sizeof( vec4_t ) );
    }
    else
        memcpy( &newColor, &item->window.foreColor, sizeof( vec4_t ) );
        
    if( item->text )
    {
        Item_Text_Paint( item );
        
        if( g_bindItem == item && g_waitingForKey )
        {
            UI_Text_Paint( item->textRect.x + item->textRect.w + ITEM_VALUE_OFFSET, item->textRect.y,
                           item->textscale, newColor, "Press key", 0, maxChars, item->textStyle );
        }
        else
        {
            BindingFromName( item->cvar );
            UI_Text_Paint( item->textRect.x + item->textRect.w + ITEM_VALUE_OFFSET, item->textRect.y,
                           item->textscale, newColor, g_nameBind1, 0, maxChars, item->textStyle );
        }
    }
    else
        UI_Text_Paint( item->textRect.x, item->textRect.y, item->textscale, newColor,
                       ( value != 0 ) ? "FIXME" : "FIXME", 0, maxChars, item->textStyle );
}

bool Display_KeyBindPending( void )
{
    return g_waitingForKey;
}

bool Item_Bind_HandleKey( itemDef_t* item, S32 key, bool down )
{
    S32     id;
    S32     i;
    
    if( Rect_ContainsPoint( &item->window.rect, DC->cursorx, DC->cursory ) && !g_waitingForKey )
    {
        if( down && ( key == K_MOUSE1 || key == K_ENTER ) )
        {
            g_waitingForKey = true;
            g_bindItem = item;
        }
        
        return true;
    }
    else
    {
        if( !g_waitingForKey || g_bindItem == NULL )
            return true;
            
        if( key & K_CHAR_FLAG )
            return true;
            
        switch( key )
        {
            case K_ESCAPE:
                g_waitingForKey = false;
                return true;
                
            case K_BACKSPACE:
                id = BindingIDFromName( item->cvar );
                
                if( id != -1 )
                {
                    g_bindings[id].bind1 = -1;
                    g_bindings[id].bind2 = -1;
                }
                
                Controls_SetConfig( true );
                g_waitingForKey = false;
                g_bindItem = NULL;
                return true;
                
            case '`':
                return true;
        }
    }
    
    if( key != -1 )
    {
        for( i = 0; i < g_bindCount; i++ )
        {
            if( g_bindings[i].bind2 == key )
                g_bindings[i].bind2 = -1;
                
            if( g_bindings[i].bind1 == key )
            {
                g_bindings[i].bind1 = g_bindings[i].bind2;
                g_bindings[i].bind2 = -1;
            }
        }
    }
    
    
    id = BindingIDFromName( item->cvar );
    
    if( id != -1 )
    {
        if( key == -1 )
        {
            if( g_bindings[id].bind1 != -1 )
            {
                DC->setBinding( g_bindings[id].bind1, "" );
                g_bindings[id].bind1 = -1;
            }
            
            if( g_bindings[id].bind2 != -1 )
            {
                DC->setBinding( g_bindings[id].bind2, "" );
                g_bindings[id].bind2 = -1;
            }
        }
        else if( g_bindings[id].bind1 == -1 )
            g_bindings[id].bind1 = key;
        else if( g_bindings[id].bind1 != key && g_bindings[id].bind2 == -1 )
            g_bindings[id].bind2 = key;
        else
        {
            DC->setBinding( g_bindings[id].bind1, "" );
            DC->setBinding( g_bindings[id].bind2, "" );
            g_bindings[id].bind1 = key;
            g_bindings[id].bind2 = -1;
        }
    }
    
    Controls_SetConfig( true );
    g_waitingForKey = false;
    
    return true;
}



void AdjustFrom640( F32* x, F32* y, F32* w, F32* h )
{
    //*x = *x * DC->scale + DC->bias;
    *x *= DC->xscale;
    *y *= DC->yscale;
    *w *= DC->xscale;
    *h *= DC->yscale;
}

void Item_Model_Paint( itemDef_t* item )
{
    F32 x, y, w, h;
    refdef_t refdef;
    refEntity_t   ent;
    vec3_t      mins, maxs, origin;
    vec3_t      angles;
    modelDef_t* modelPtr = ( modelDef_t* )item->typeData;
    
    qhandle_t hModel;
    S32 backLerpWhole;
//  vec3_t axis[3];

    if( modelPtr == NULL )
        return;
        
    if( !item->asset )
        return;
        
    hModel = item->asset;
    
    
    // setup the refdef
    ::memset( &refdef, 0, sizeof( refdef ) );
    
    refdef.rdflags = RDF_NOWORLDMODEL;
    
    AxisClear( refdef.viewaxis );
    
    x = item->window.rect.x + 1;
    y = item->window.rect.y + 1;
    w = item->window.rect.w - 2;
    h = item->window.rect.h - 2;
    
    AdjustFrom640( &x, &y, &w, &h );
    
    refdef.x = x;
    refdef.y = y;
    refdef.width = w;
    refdef.height = h;
    
    DC->modelBounds( hModel, mins, maxs );
    
    origin[2] = -0.5 * ( mins[2] + maxs[2] );
    origin[1] = 0.5 * ( mins[1] + maxs[1] );
    
    // calculate distance so the model nearly fills the box
    if( true )
    {
        F32 len = 0.5 * ( maxs[2] - mins[2] );
        origin[0] = len / 0.268;  // len / tan( fov/2 )
        //origin[0] = len / tan(w/2);
    }
    else
        origin[0] = item->textscale;
        
    refdef.fov_x = ( modelPtr->fov_x ) ? modelPtr->fov_x : w;
    refdef.fov_y = ( modelPtr->fov_y ) ? modelPtr->fov_y : h;
    
    //refdef.fov_x = (S32)((F32)refdef.width / 640.0f * 90.0f);
    //xx = refdef.width / tan( refdef.fov_x / 360 * M_PI );
    //refdef.fov_y = atan2( refdef.height, xx );
    //refdef.fov_y *= ( 360 / M_PI );
    
    DC->clearScene();
    
    refdef.time = DC->realTime;
    
    // add the model
    
    ::memset( &ent, 0, sizeof( ent ) );
    
    //adjust = 5.0 * sin( (F32)uis.realtime / 500 );
    //adjust = 360 % (S32)((F32)uis.realtime / 1000);
    //VectorSet( angles, 0, 0, 1 );
    
    // use item storage to track
    
    if( modelPtr->rotationSpeed )
    {
        if( DC->realTime > item->window.nextTime )
        {
            item->window.nextTime = DC->realTime + modelPtr->rotationSpeed;
            modelPtr->angle = ( S32 )( modelPtr->angle + 1 ) % 360;
        }
    }
    
    if( VectorLengthSquared( modelPtr->axis ) )
    {
        VectorNormalize( modelPtr->axis );
        angles[0] = AngleNormalize360( modelPtr->axis[0] * modelPtr->angle );
        angles[1] = AngleNormalize360( modelPtr->axis[1] * modelPtr->angle );
        angles[2] = AngleNormalize360( modelPtr->axis[2] * modelPtr->angle );
        AnglesToAxis( angles, ent.axis );
    }
    else
    {
        VectorSet( angles, 0, modelPtr->angle, 0 );
        AnglesToAxis( angles, ent.axis );
    }
    
    ent.hModel = hModel;
    
    
    if( modelPtr->frameTime )	// don't advance on the first frame
        modelPtr->backlerp += ( ( ( DC->realTime - modelPtr->frameTime ) / 1000.0f ) * ( F32 )modelPtr->fps );
        
    if( modelPtr->backlerp > 1 )
    {
        backLerpWhole = floor( modelPtr->backlerp );
        
        modelPtr->frame += ( backLerpWhole );
        if( ( modelPtr->frame - modelPtr->startframe ) > modelPtr->numframes )
            modelPtr->frame = modelPtr->startframe + modelPtr->frame % modelPtr->numframes;	// todo: ignoring loopframes
            
        modelPtr->oldframe += ( backLerpWhole );
        if( ( modelPtr->oldframe - modelPtr->startframe ) > modelPtr->numframes )
            modelPtr->oldframe = modelPtr->startframe + modelPtr->oldframe % modelPtr->numframes;	// todo: ignoring loopframes
            
        modelPtr->backlerp = modelPtr->backlerp - backLerpWhole;
    }
    
    modelPtr->frameTime = DC->realTime;
    
    ent.frame		= modelPtr->frame;
    ent.oldframe	= modelPtr->oldframe;
    ent.backlerp	= 1.0f - modelPtr->backlerp;
    
    VectorCopy( origin, ent.origin );
    VectorCopy( origin, ent.lightingOrigin );
    ent.renderfx = RF_LIGHTING_ORIGIN | RF_NOSHADOW;
    VectorCopy( ent.origin, ent.oldorigin );
    
    DC->addRefEntityToScene( &ent );
    
    // add an accent light
    origin[0] -= 100;	// + = behind, - = in front
    origin[1] += 100;	// + = left, - = right
    origin[2] += 100;	// + = above, - = below
    trap_R_AddLightToScene( origin, 500, 1.0, 1.0, 1.0 );
    
    origin[0] -= 100;
    origin[1] -= 100;
    origin[2] -= 100;
    trap_R_AddLightToScene( origin, 500, 1.0, 0.0, 0.0 );
    
    DC->renderScene( &refdef );
    
}


void Item_Image_Paint( itemDef_t* item )
{
    if( item == NULL )
        return;
        
    DC->drawHandlePic( item->window.rect.x + 1, item->window.rect.y + 1,
                       item->window.rect.w - 2, item->window.rect.h - 2, item->asset );
}

void Item_ListBox_Paint( itemDef_t* item )
{
    F32 x, y, size, thumb;
    S32 i, count;
    qhandle_t image;
    qhandle_t optionalImage;
    listBoxDef_t* listPtr = ( listBoxDef_t* )item->typeData;
    menuDef_t* menu = ( menuDef_t* )item->parent;
    F32 one, two;
    
    if( menu->window.aspectBias != ASPECT_NONE || item->window.aspectBias != ASPECT_NONE )
    {
        one = 1.0f * DC->aspectScale;
        two = 2.0f * DC->aspectScale;
    }
    else
    {
        one = 1.0f;
        two = 2.0f;
    }
    
    // the listbox is horizontal or vertical and has a fixed size scroll bar going either direction
    // elements are enumerated from the DC and either text or image handles are acquired from the DC as well
    // textscale is used to size the text, textalignx and textaligny are used to size image elements
    // there is no clipping available so only the last completely visible item is painted
    count = DC->feederCount( item->special );
    
    // default is vertical if horizontal flag is not here
    if( item->window.flags & WINDOW_HORIZONTAL )
    {
        //FIXME: unmaintained cruft?
        
        if( !listPtr->noscrollbar )
        {
            // draw scrollbar in bottom of the window
            // bar
            x = item->window.rect.x + 1;
            y = item->window.rect.y + item->window.rect.h - SCROLLBAR_HEIGHT - 1;
            DC->drawHandlePic( x, y, SCROLLBAR_WIDTH, SCROLLBAR_HEIGHT, DC->Assets.scrollBarArrowLeft );
            x += SCROLLBAR_WIDTH - 1;
            size = item->window.rect.w - ( SCROLLBAR_WIDTH * 2 );
            DC->drawHandlePic( x, y, size + 1, SCROLLBAR_HEIGHT, DC->Assets.scrollBar );
            x += size - 1;
            DC->drawHandlePic( x, y, SCROLLBAR_WIDTH, SCROLLBAR_HEIGHT, DC->Assets.scrollBarArrowRight );
            // thumb
            thumb = Item_ListBox_ThumbDrawPosition( item );//Item_ListBox_ThumbPosition(item);
            
            if( thumb > x - SCROLLBAR_WIDTH - 1 )
                thumb = x - SCROLLBAR_WIDTH - 1;
                
            DC->drawHandlePic( thumb, y, SCROLLBAR_WIDTH, SCROLLBAR_HEIGHT, DC->Assets.scrollBarThumb );
            //
            listPtr->endPos = listPtr->startPos;
        }
        
        size = item->window.rect.w - 2;
        // items
        // size contains max available space
        
        if( listPtr->elementStyle == LISTBOX_IMAGE )
        {
            // fit = 0;
            x = item->window.rect.x + 1;
            y = item->window.rect.y + 1;
            
            for( i = listPtr->startPos; i < count; i++ )
            {
                // always draw at least one
                // which may overdraw the box if it is too small for the element
                image = DC->feederItemImage( item->special, i );
                
                if( image )
                    DC->drawHandlePic( x + 1, y + 1, listPtr->elementWidth - 2, listPtr->elementHeight - 2, image );
                    
                if( i == item->cursorPos )
                {
                    DC->drawRect( x, y, listPtr->elementWidth - 1, listPtr->elementHeight - 1,
                                  item->window.borderSize, item->window.borderColor );
                }
                
                listPtr->endPos++;
                size -= listPtr->elementWidth;
                
                if( size < listPtr->elementWidth )
                {
                    listPtr->drawPadding = size; //listPtr->elementWidth - size;
                    break;
                }
                
                x += listPtr->elementWidth;
                // fit++;
            }
        }
    }
    else
    {
        if( !listPtr->noscrollbar )
        {
            // draw scrollbar to right side of the window
            x = item->window.rect.x + item->window.rect.w - SCROLLBAR_WIDTH - one;
            y = item->window.rect.y + 1;
            DC->drawHandlePic( x, y, SCROLLBAR_WIDTH, SCROLLBAR_HEIGHT, DC->Assets.scrollBarArrowUp );
            y += SCROLLBAR_HEIGHT - 1;
            
            listPtr->endPos = listPtr->startPos;
            size = item->window.rect.h - ( SCROLLBAR_HEIGHT * 2 );
            DC->drawHandlePic( x, y, SCROLLBAR_WIDTH, size + 1, DC->Assets.scrollBar );
            y += size - 1;
            DC->drawHandlePic( x, y, SCROLLBAR_WIDTH, SCROLLBAR_HEIGHT, DC->Assets.scrollBarArrowDown );
            // thumb
            thumb = Item_ListBox_ThumbDrawPosition( item );//Item_ListBox_ThumbPosition(item);
            
            if( thumb > y - SCROLLBAR_HEIGHT - 1 )
                thumb = y - SCROLLBAR_HEIGHT - 1;
                
            DC->drawHandlePic( x, thumb, SCROLLBAR_WIDTH, SCROLLBAR_HEIGHT, DC->Assets.scrollBarThumb );
        }
        
        // adjust size for item painting
        size = item->window.rect.h - 2;
        
        if( listPtr->elementStyle == LISTBOX_IMAGE )
        {
            // fit = 0;
            x = item->window.rect.x + one;
            y = item->window.rect.y + 1;
            
            for( i = listPtr->startPos; i < count; i++ )
            {
                // always draw at least one
                // which may overdraw the box if it is too small for the element
                image = DC->feederItemImage( item->special, i );
                
                if( image )
                    DC->drawHandlePic( x + one, y + 1, listPtr->elementWidth - two, listPtr->elementHeight - 2, image );
                    
                if( i == item->cursorPos )
                {
                    DC->drawRect( x, y, listPtr->elementWidth - one, listPtr->elementHeight - 1,
                                  item->window.borderSize, item->window.borderColor );
                }
                
                listPtr->endPos++;
                size -= listPtr->elementWidth;
                
                if( size < listPtr->elementHeight )
                {
                    listPtr->drawPadding = listPtr->elementHeight - size;
                    break;
                }
                
                y += listPtr->elementHeight;
                // fit++;
            }
        }
        else
        {
            F32 m = UI_Text_EmHeight( item->textscale );
            x = item->window.rect.x + one;
            y = item->window.rect.y + 1;
            
            for( i = listPtr->startPos; i < count; i++ )
            {
                UTF8 text[ MAX_STRING_CHARS ];
                // always draw at least one
                // which may overdraw the box if it is too small for the element
                
                if( listPtr->numColumns > 0 )
                {
                    S32 j;
                    
                    for( j = 0; j < listPtr->numColumns; j++ )
                    {
                        F32 columnPos;
                        F32 width, height;
                        
                        if( menu->window.aspectBias != ASPECT_NONE || item->window.aspectBias != ASPECT_NONE )
                        {
                            columnPos = ( listPtr->columnInfo[ j ].pos + 4.0f ) * DC->aspectScale;
                            width = listPtr->columnInfo[ j ].width * DC->aspectScale;
                        }
                        else
                        {
                            columnPos = ( listPtr->columnInfo[ j ].pos + 4.0f );
                            width = listPtr->columnInfo[ j ].width;
                        }
                        
                        height = listPtr->columnInfo[ j ].width;
                        
                        Q_strncpyz( text, DC->feederItemText( item->special, i, j, &optionalImage ), sizeof( text ) );
                        
                        if( optionalImage >= 0 )
                        {
                            DC->drawHandlePic( x + columnPos, y + ( ( listPtr->elementHeight - height ) / 2.0f ),
                                               width, height, optionalImage );
                        }
                        else if( text[ 0 ] )
                        {
                            S32 alignOffset = 0.0f, tw;
                            
                            tw = UI_Text_Width( text, item->textscale, 0 );
                            
                            // Shorten the string if it's too long
                            
                            while( tw > width && strlen( text ) > 0 )
                            {
                                text[ strlen( text ) - 1 ] = '\0';
                                tw = UI_Text_Width( text, item->textscale, 0 );
                            }
                            
                            switch( listPtr->columnInfo[ j ].align )
                            {
                                case ALIGN_LEFT:
                                    alignOffset = 0.0f;
                                    break;
                                    
                                case ALIGN_RIGHT:
                                    alignOffset = width - tw;
                                    break;
                                    
                                case ALIGN_CENTER:
                                    alignOffset = ( width / 2.0f ) - ( tw / 2.0f );
                                    break;
                                    
                                default:
                                    alignOffset = 0.0f;
                            }
                            
                            UI_Text_Paint( x + columnPos + alignOffset,
                                           y + m + ( ( listPtr->elementHeight - m ) / 2.0f ),
                                           item->textscale, item->window.foreColor, text, 0,
                                           0, item->textStyle );
                        }
                    }
                }
                else
                {
                    F32 offset;
                    
                    if( menu->window.aspectBias != ASPECT_NONE || item->window.aspectBias != ASPECT_NONE )
                        offset = 4.0f * DC->aspectScale;
                    else
                        offset = 4.0f;
                        
                    Q_strncpyz( text, DC->feederItemText( item->special, i, 0, &optionalImage ), sizeof( text ) );
                    
                    if( optionalImage >= 0 )
                        DC->drawHandlePic( x + offset, y, listPtr->elementHeight, listPtr->elementHeight, optionalImage );
                    else if( text[ 0 ] )
                    {
                        UI_Text_Paint( x + offset, y + m + ( ( listPtr->elementHeight - m ) / 2.0f ),
                                       item->textscale, item->window.foreColor, text, 0,
                                       0, item->textStyle );
                    }
                }
                
                if( i == item->cursorPos )
                {
                    DC->fillRect( x, y, item->window.rect.w - SCROLLBAR_WIDTH - ( two * item->window.borderSize ),
                                  listPtr->elementHeight, item->window.outlineColor );
                }
                
                listPtr->endPos++;
                size -= listPtr->elementHeight;
                
                if( size < listPtr->elementHeight )
                {
                    listPtr->drawPadding = listPtr->elementHeight - size;
                    break;
                }
                
                y += listPtr->elementHeight;
                // fit++;
            }
        }
    }
    
    // FIXME: hacky fix to off-by-one bug
    listPtr->endPos--;
}

void Item_OwnerDraw_Paint( itemDef_t* item )
{
    menuDef_t* parent;
    StringEntry text;
    
    if( item == NULL )
        return;
        
    parent = ( menuDef_t* )item->parent;
    
    if( DC->ownerDrawItem )
    {
        vec4_t color, lowLight;
        menuDef_t* parent = ( menuDef_t* )item->parent;
        Fade( &item->window.flags, &item->window.foreColor[3], parent->fadeClamp, &item->window.nextTime,
              parent->fadeCycle, true, parent->fadeAmount );
        memcpy( &color, &item->window.foreColor, sizeof( color ) );
        
        if( item->numColors > 0 && DC->getValue )
        {
            // if the value is within one of the ranges then set color to that, otherwise leave at default
            S32 i;
            F32 f = DC->getValue( item->window.ownerDraw );
            
            for( i = 0; i < item->numColors; i++ )
            {
                if( f >= item->colorRanges[i].low && f <= item->colorRanges[i].high )
                {
                    memcpy( &color, &item->colorRanges[i].color, sizeof( color ) );
                    break;
                }
            }
        }
        
        if( item->window.flags & WINDOW_HASFOCUS )
            memcpy( color, &parent->focusColor, sizeof( vec4_t ) );
        else if( item->textStyle == ITEM_TEXTSTYLE_BLINK && !( ( DC->realTime / BLINK_DIVISOR ) & 1 ) )
        {
            lowLight[0] = 0.8 * item->window.foreColor[0];
            lowLight[1] = 0.8 * item->window.foreColor[1];
            lowLight[2] = 0.8 * item->window.foreColor[2];
            lowLight[3] = 0.8 * item->window.foreColor[3];
            LerpColor( item->window.foreColor, lowLight, color, 0.5 + 0.5 * sin( DC->realTime / PULSE_DIVISOR ) );
        }
        
        if( item->cvarFlags & ( CVAR_ENABLE | CVAR_DISABLE ) && !Item_EnableShowViaCvar( item, CVAR_ENABLE ) )
            ::memcpy( color, parent->disableColor, sizeof( vec4_t ) );
            
        if( DC->ownerDrawText && ( text = DC->ownerDrawText( item->window.ownerDraw ) ) )
        {
            if( item->text && *item->text )
            {
                Item_Text_Paint( item );
                
                UI_Text_Paint( item->textRect.x + item->textRect.w + ITEM_VALUE_OFFSET,
                               item->textRect.y, item->textscale,
                               color, text, 0, 0, item->textStyle );
            }
            else
            {
                item->text = text;
                Item_Text_Paint( item );
                item->text = NULL;
            }
        }
        else
        {
            DC->ownerDrawItem( item->window.rect.x, item->window.rect.y,
                               item->window.rect.w, item->window.rect.h,
                               item->textalignx, item->textaligny,
                               item->window.ownerDraw, item->window.ownerDrawFlags,
                               item->alignment, item->textalignment, item->textvalignment,
                               item->special, item->textscale, color, item->window.backColor,
                               item->window.background, item->textStyle );
        }
    }
}


void Item_Paint( itemDef_t* item )
{
    vec4_t red;
    menuDef_t* parent = ( menuDef_t* )item->parent;
    red[0] = red[3] = 1;
    red[1] = red[2] = 0;
    
    if( item == NULL )
        return;
        
    if( item->window.flags & WINDOW_ORBITING )
    {
        if( DC->realTime > item->window.nextTime )
        {
            F32 rx, ry, a, c, s, w, h;
            
            item->window.nextTime = DC->realTime + item->window.offsetTime;
            // translate
            w = item->window.rectClient.w / 2;
            h = item->window.rectClient.h / 2;
            rx = item->window.rectClient.x + w - item->window.rectEffects.x;
            ry = item->window.rectClient.y + h - item->window.rectEffects.y;
            a = 3 * M_PI / 180;
            c = cos( a );
            s = sin( a );
            item->window.rectClient.x = ( rx * c - ry * s ) + item->window.rectEffects.x - w;
            item->window.rectClient.y = ( rx * s + ry * c ) + item->window.rectEffects.y - h;
            Item_UpdatePosition( item );
            
        }
    }
    
    
    if( item->window.flags & WINDOW_INTRANSITION )
    {
        if( DC->realTime > item->window.nextTime )
        {
            S32 done = 0;
            item->window.nextTime = DC->realTime + item->window.offsetTime;
            // transition the x,y
            
            if( item->window.rectClient.x == item->window.rectEffects.x )
                done++;
            else
            {
                if( item->window.rectClient.x < item->window.rectEffects.x )
                {
                    item->window.rectClient.x += item->window.rectEffects2.x;
                    
                    if( item->window.rectClient.x > item->window.rectEffects.x )
                    {
                        item->window.rectClient.x = item->window.rectEffects.x;
                        done++;
                    }
                }
                else
                {
                    item->window.rectClient.x -= item->window.rectEffects2.x;
                    
                    if( item->window.rectClient.x < item->window.rectEffects.x )
                    {
                        item->window.rectClient.x = item->window.rectEffects.x;
                        done++;
                    }
                }
            }
            
            if( item->window.rectClient.y == item->window.rectEffects.y )
                done++;
            else
            {
                if( item->window.rectClient.y < item->window.rectEffects.y )
                {
                    item->window.rectClient.y += item->window.rectEffects2.y;
                    
                    if( item->window.rectClient.y > item->window.rectEffects.y )
                    {
                        item->window.rectClient.y = item->window.rectEffects.y;
                        done++;
                    }
                }
                else
                {
                    item->window.rectClient.y -= item->window.rectEffects2.y;
                    
                    if( item->window.rectClient.y < item->window.rectEffects.y )
                    {
                        item->window.rectClient.y = item->window.rectEffects.y;
                        done++;
                    }
                }
            }
            
            if( item->window.rectClient.w == item->window.rectEffects.w )
                done++;
            else
            {
                if( item->window.rectClient.w < item->window.rectEffects.w )
                {
                    item->window.rectClient.w += item->window.rectEffects2.w;
                    
                    if( item->window.rectClient.w > item->window.rectEffects.w )
                    {
                        item->window.rectClient.w = item->window.rectEffects.w;
                        done++;
                    }
                }
                else
                {
                    item->window.rectClient.w -= item->window.rectEffects2.w;
                    
                    if( item->window.rectClient.w < item->window.rectEffects.w )
                    {
                        item->window.rectClient.w = item->window.rectEffects.w;
                        done++;
                    }
                }
            }
            
            if( item->window.rectClient.h == item->window.rectEffects.h )
                done++;
            else
            {
                if( item->window.rectClient.h < item->window.rectEffects.h )
                {
                    item->window.rectClient.h += item->window.rectEffects2.h;
                    
                    if( item->window.rectClient.h > item->window.rectEffects.h )
                    {
                        item->window.rectClient.h = item->window.rectEffects.h;
                        done++;
                    }
                }
                else
                {
                    item->window.rectClient.h -= item->window.rectEffects2.h;
                    
                    if( item->window.rectClient.h < item->window.rectEffects.h )
                    {
                        item->window.rectClient.h = item->window.rectEffects.h;
                        done++;
                    }
                }
            }
            
            Item_UpdatePosition( item );
            
            if( done == 4 )
                item->window.flags &= ~WINDOW_INTRANSITION;
                
        }
    }
    
    if( item->window.ownerDrawFlags && DC->ownerDrawVisible )
    {
        if( !DC->ownerDrawVisible( item->window.ownerDrawFlags ) )
            item->window.flags &= ~WINDOW_VISIBLE;
        else
            item->window.flags |= WINDOW_VISIBLE;
    }
    
    if( item->cvarFlags & ( CVAR_SHOW | CVAR_HIDE ) )
    {
        if( !Item_EnableShowViaCvar( item, CVAR_SHOW ) )
            return;
    }
    
    if( item->window.flags & WINDOW_TIMEDVISIBLE )
    {
    }
    
    if( !( item->window.flags & WINDOW_VISIBLE ) )
        return;
        
    Window_Paint( &item->window, parent->fadeAmount , parent->fadeClamp, parent->fadeCycle );
    
    if( DC->getCVarValue( "gui_developer" ) )
    {
        vec4_t color;
        rectDef_t* r = Item_CorrectedTextRect( item );
        color[1] = color[3] = 1;
        color[0] = color[2] = 0;
        DC->drawRect( r->x, r->y, r->w, r->h, 1, color );
    }
    
    switch( item->type )
    {
        case ITEM_TYPE_OWNERDRAW:
            Item_OwnerDraw_Paint( item );
            break;
            
        case ITEM_TYPE_TEXT:
        case ITEM_TYPE_BUTTON:
            Item_Text_Paint( item );
            break;
            
        case ITEM_TYPE_RADIOBUTTON:
            break;
            
        case ITEM_TYPE_CHECKBOX:
            break;
            
        case ITEM_TYPE_EDITFIELD:
        case ITEM_TYPE_NUMERICFIELD:
            Item_TextField_Paint( item );
            break;
            
        case ITEM_TYPE_COMBO:
            Item_Combobox_Paint( item );
            break;
            
        case ITEM_TYPE_LISTBOX:
            Item_ListBox_Paint( item );
            break;
            
            //case ITEM_TYPE_IMAGE:
            //  Item_Image_Paint(item);
            //  break;
            
        case ITEM_TYPE_MODEL:
            Item_Model_Paint( item );
            break;
            
        case ITEM_TYPE_YESNO:
            Item_YesNo_Paint( item );
            break;
            
        case ITEM_TYPE_MULTI:
            Item_Multi_Paint( item );
            break;
            
        case ITEM_TYPE_BIND:
            Item_Bind_Paint( item );
            break;
            
        case ITEM_TYPE_SLIDER:
            Item_Slider_Paint( item );
            break;
            
        default:
            break;
    }
    
    Border_Paint( &item->window );
}

void Menu_Init( menuDef_t* menu )
{
    ::memset( menu, 0, sizeof( menuDef_t ) );
    menu->cursorItem = -1;
    menu->fadeAmount = DC->Assets.fadeAmount;
    menu->fadeClamp = DC->Assets.fadeClamp;
    menu->fadeCycle = DC->Assets.fadeCycle;
    Window_Init( &menu->window );
    menu->window.aspectBias = ALIGN_CENTER;
}

itemDef_t* Menu_GetFocusedItem( menuDef_t* menu )
{
    S32 i;
    
    if( menu )
    {
        for( i = 0; i < menu->itemCount; i++ )
        {
            if( menu->items[i]->window.flags & WINDOW_HASFOCUS )
                return menu->items[i];
        }
    }
    
    return NULL;
}

menuDef_t* Menu_GetFocused( void )
{
    S32 i;
    
    for( i = 0; i < menuCount; i++ )
    {
        if( Menus[i].window.flags & WINDOW_HASFOCUS && Menus[i].window.flags & WINDOW_VISIBLE )
            return & Menus[i];
    }
    
    return NULL;
}

void Menu_ScrollFeeder( menuDef_t* menu, S32 feeder, bool down )
{
    if( menu )
    {
        S32 i;
        
        for( i = 0; i < menu->itemCount; i++ )
        {
            if( menu->items[i]->special == feeder )
            {
                Item_ListBox_HandleKey( menu->items[i], ( down ) ? K_DOWNARROW : K_UPARROW, true, true );
                return;
            }
        }
    }
}



void Menu_SetFeederSelection( menuDef_t* menu, S32 feeder, S32 index, StringEntry name )
{
    if( menu == NULL )
    {
        if( name == NULL )
            menu = Menu_GetFocused();
        else
            menu = Menus_FindByName( name );
    }
    
    if( menu )
    {
        S32 i;
        
        for( i = 0; i < menu->itemCount; i++ )
        {
            if( menu->items[i]->special == feeder )
            {
                if( index == 0 )
                {
                    listBoxDef_t* listPtr = ( listBoxDef_t* )menu->items[i]->typeData;
                    listPtr->cursorPos = 0;
                    listPtr->startPos = 0;
                }
                
                menu->items[i]->cursorPos = index;
                DC->feederSelection( menu->items[i]->special, menu->items[i]->cursorPos );
                return;
            }
        }
    }
}

bool Menus_AnyFullScreenVisible( void )
{
    S32 i;
    
    for( i = 0; i < menuCount; i++ )
    {
        if( Menus[i].window.flags & WINDOW_VISIBLE && Menus[i].fullScreen )
            return true;
    }
    
    return false;
}

menuDef_t* Menus_ActivateByName( StringEntry p )
{
    S32 i;
    menuDef_t* m = NULL;
    
    // Activate one menu
    
    for( i = 0; i < menuCount; i++ )
    {
        if( Q_stricmp( Menus[i].window.name, p ) == 0 )
        {
            m = &Menus[i];
            Menus_Activate( m );
            break;
        }
    }
    
    // Defocus the others
    for( i = 0; i < menuCount; i++ )
    {
        if( Q_stricmp( Menus[i].window.name, p ) != 0 )
            Menus[i].window.flags &= ~WINDOW_HASFOCUS;
    }
    
    return m;
}


void Item_Init( itemDef_t* item )
{
    ::memset( item, 0, sizeof( itemDef_t ) );
    item->textscale = 0.55f;
    Window_Init( &item->window );
    item->window.aspectBias = ASPECT_NONE;
}

void Menu_HandleMouseMove( menuDef_t* menu, F32 x, F32 y )
{
    S32 i, pass;
    bool focusSet = false;
    
    itemDef_t* overItem;
    
    if( menu == NULL )
        return;
        
    if( !( menu->window.flags & ( WINDOW_VISIBLE | WINDOW_FORCED ) ) )
        return;
        
    if( itemCapture )
    {
        //Item_MouseMove(itemCapture, x, y);
        return;
    }
    
    if( g_waitingForKey || g_editingField )
        return;
        
    // KTW: Draggable windows
    if( ( menu->window.flags & WINDOW_HASFOCUS ) && ( menu->window.flags & WINDOW_DRAG ) )
    {
        menu->window.rect.x +=  DC->cursordx;
        menu->window.rect.y +=  DC->cursordy;
        
        if( menu->window.rect.x < 0 )
            menu->window.rect.x = 0;
        if( menu->window.rect.x + menu->window.rect.w > SCREEN_WIDTH )
            menu->window.rect.x = SCREEN_WIDTH - menu->window.rect.w;
            
        if( menu->window.rect.y < 0 )
            menu->window.rect.y = 0;
        if( menu->window.rect.y + menu->window.rect.h > SCREEN_HEIGHT )
            menu->window.rect.y = SCREEN_HEIGHT - menu->window.rect.h;
            
        Menu_UpdatePosition( menu );
        
        for( i = 0; i < menu->itemCount; i++ )
            Item_UpdatePosition( menu->items[i] );
    }
    
    // FIXME: this is the whole issue of focus vs. mouse over..
    // need a better overall solution as i don't like going through everything twice
    for( pass = 0; pass < 2; pass++ )
    {
        for( i = 0; i < menu->itemCount; i++ )
        {
            // turn off focus each item
            // menu->items[i].window.flags &= ~WINDOW_HASFOCUS;
            
            if( !( menu->items[i]->window.flags & ( WINDOW_VISIBLE | WINDOW_FORCED ) ) )
                continue;
                
            // items can be enabled and disabled based on cvars
            if( menu->items[i]->cvarFlags & ( CVAR_ENABLE | CVAR_DISABLE ) &&
                    !Item_EnableShowViaCvar( menu->items[i], CVAR_ENABLE ) )
                continue;
                
            if( menu->items[i]->cvarFlags & ( CVAR_SHOW | CVAR_HIDE ) &&
                    !Item_EnableShowViaCvar( menu->items[i], CVAR_SHOW ) )
                continue;
                
                
                
            if( Rect_ContainsPoint( &menu->items[i]->window.rect, x, y ) )
            {
                if( pass == 1 )
                {
                    overItem = menu->items[i];
                    
                    if( overItem->type == ITEM_TYPE_TEXT && overItem->text )
                    {
                        if( !Rect_ContainsPoint( Item_CorrectedTextRect( overItem ), x, y ) )
                            continue;
                    }
                    
                    // if we are over an item
                    if( IsVisible( overItem->window.flags ) )
                    {
                        // different one
                        Item_MouseEnter( overItem, x, y );
                        // Item_SetMouseOver(overItem, true);
                        
                        // if item is not a decoration see if it can take focus
                        
                        if( !focusSet )
                            focusSet = Item_SetFocus( overItem, x, y );
                    }
                }
            }
            else if( menu->items[i]->window.flags & WINDOW_MOUSEOVER )
            {
                Item_MouseLeave( menu->items[i] );
                Item_SetMouseOver( menu->items[i], false );
            }
        }
    }
    
}

void Menu_Paint( menuDef_t* menu, bool forcePaint )
{
    S32 i;
    itemDef_t item;
    UTF8 listened_text[1024];
    
    if( menu == NULL )
        return;
        
    if( !( menu->window.flags & WINDOW_VISIBLE ) &&  !forcePaint )
        return;
        
    if( menu->window.ownerDrawFlags && DC->ownerDrawVisible && !DC->ownerDrawVisible( menu->window.ownerDrawFlags ) )
        return;
        
    if( forcePaint )
        menu->window.flags |= WINDOW_FORCED;
        
    //Executes the text stored in the listened cvar as an UIscript
    if( menu->listenCvar && menu->listenCvar[0] )
    {
        DC->getCVarString( menu->listenCvar, listened_text, sizeof( listened_text ) );
        if( listened_text[0] )
        {
            item.parent = menu;
            Item_RunScript( &item, listened_text );
            DC->setCVar( menu->listenCvar, "" );
        }
    }
    
    // draw the background if necessary
    if( menu->fullScreen )
    {
        // implies a background shader
        // FIXME: make sure we have a default shader if fullscreen is set with no background
        DC->drawHandlePic( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, menu->window.background );
    }
    
    // paint the background and or border
    Window_Paint( &menu->window, menu->fadeAmount, menu->fadeClamp, menu->fadeCycle );
    
    Border_Paint( &menu->window );
    
    for( i = 0; i < menu->itemCount; i++ )
        Item_Paint( menu->items[i] );
        
    if( DC->getCVarValue( "gui_developer" ) )
    {
        vec4_t color;
        color[0] = color[2] = color[3] = 1;
        color[1] = 0;
        DC->drawRect( menu->window.rect.x, menu->window.rect.y, menu->window.rect.w, menu->window.rect.h, 1, color );
    }
}

/*
===============
Item_ValidateTypeData
===============
*/
void Item_ValidateTypeData( itemDef_t* item )
{
    if( item->typeData )
        return;
        
    if( item->type == ITEM_TYPE_LISTBOX )
    {
        item->typeData = UI_Alloc( sizeof( listBoxDef_t ) );
        ::memset( item->typeData, 0, sizeof( listBoxDef_t ) );
    }
    else if( item->type == ITEM_TYPE_COMBO )
    {
        item->typeData = UI_Alloc( sizeof( comboBoxDef_t ) );
        ::memset( item->typeData, 0, sizeof( comboBoxDef_t ) );
    }
    else if( item->type == ITEM_TYPE_EDITFIELD ||
             item->type == ITEM_TYPE_NUMERICFIELD ||
             item->type == ITEM_TYPE_YESNO ||
             item->type == ITEM_TYPE_BIND ||
             item->type == ITEM_TYPE_SLIDER ||
             item->type == ITEM_TYPE_TEXT )
    {
        item->typeData = UI_Alloc( sizeof( editFieldDef_t ) );
        ::memset( item->typeData, 0, sizeof( editFieldDef_t ) );
        
        if( item->type == ITEM_TYPE_EDITFIELD )
        {
            if( !( ( editFieldDef_t* ) item->typeData )->maxPaintChars )
                ( ( editFieldDef_t* ) item->typeData )->maxPaintChars = MAX_EDITFIELD;
        }
    }
    else if( item->type == ITEM_TYPE_MULTI )
        item->typeData = UI_Alloc( sizeof( multiDef_t ) );
    else if( item->type == ITEM_TYPE_MODEL )
        item->typeData = UI_Alloc( sizeof( modelDef_t ) );
}

/*
===============
Keyword Hash
===============
*/

#define KEYWORDHASH_SIZE  512

typedef struct keywordHash_s
{
    UTF8* keyword;
    bool ( *func )( itemDef_t* item, S32 handle );
    
    struct keywordHash_s* next;
}

keywordHash_t;

S32 KeywordHash_Key( UTF8* keyword )
{
    S32 register hash, i;
    
    hash = 0;
    
    for( i = 0; keyword[i] != '\0'; i++ )
    {
        if( keyword[i] >= 'A' && keyword[i] <= 'Z' )
            hash += ( keyword[i] + ( 'a' - 'A' ) ) * ( 119 + i );
        else
            hash += keyword[i] * ( 119 + i );
    }
    
    hash = ( hash ^ ( hash >> 10 ) ^ ( hash >> 20 ) ) & ( KEYWORDHASH_SIZE - 1 );
    return hash;
}

void KeywordHash_Add( keywordHash_t* table[], keywordHash_t* key )
{
    S32 hash;
    
    hash = KeywordHash_Key( key->keyword );
    /*
       if(table[hash])     S32 collision = true;
       */
    key->next = table[hash];
    table[hash] = key;
}

keywordHash_t* KeywordHash_Find( keywordHash_t* table[], UTF8* keyword )
{
    keywordHash_t* key;
    S32 hash;
    
    hash = KeywordHash_Key( keyword );
    
    for( key = table[hash]; key; key = key->next )
    {
        if( !Q_stricmp( key->keyword, keyword ) )
            return key;
    }
    
    return NULL;
}

/*
===============
Item Keyword Parse functions
===============
*/

// name <string>
bool ItemParse_name( itemDef_t* item, S32 handle )
{
    if( !PC_String_Parse( handle, &item->window.name ) )
        return false;
        
    return true;
}

// name <string>
bool ItemParse_focusSound( itemDef_t* item, S32 handle )
{
    StringEntry temp;
    
    if( !PC_String_Parse( handle, &temp ) )
        return false;
        
    item->focusSound = DC->registerSound( temp, false );
    return true;
}


// text <string>
bool ItemParse_text( itemDef_t* item, S32 handle )
{
    if( !PC_String_Parse( handle, &item->text ) )
        return false;
        
    return true;
}

// group <string>
bool ItemParse_group( itemDef_t* item, S32 handle )
{
    if( !PC_String_Parse( handle, &item->window.group ) )
        return false;
        
    return true;
}

// asset_model <string>
bool ItemParse_asset_model( itemDef_t* item, S32 handle )
{
    StringEntry temp;
    modelDef_t* modelPtr;
    Item_ValidateTypeData( item );
    modelPtr = ( modelDef_t* )item->typeData;
    
    if( !PC_String_Parse( handle, &temp ) )
        return false;
        
    item->asset = DC->registerModel( temp );
    modelPtr->angle = rand() % 360;
    return true;
}

// asset_shader <string>
bool ItemParse_asset_shader( itemDef_t* item, S32 handle )
{
    StringEntry temp;
    
    if( !PC_String_Parse( handle, &temp ) )
        return false;
        
    item->asset = DC->registerShaderNoMip( temp );
    return true;
}

// model_origin <number> <number> <number>
bool ItemParse_model_origin( itemDef_t* item, S32 handle )
{
    modelDef_t* modelPtr;
    Item_ValidateTypeData( item );
    modelPtr = ( modelDef_t* )item->typeData;
    
    if( PC_Float_Parse( handle, &modelPtr->origin[0] ) )
    {
        if( PC_Float_Parse( handle, &modelPtr->origin[1] ) )
        {
            if( PC_Float_Parse( handle, &modelPtr->origin[2] ) )
                return true;
        }
    }
    
    return false;
}

// model_fovx <number>
bool ItemParse_model_fovx( itemDef_t* item, S32 handle )
{
    modelDef_t* modelPtr;
    Item_ValidateTypeData( item );
    modelPtr = ( modelDef_t* )item->typeData;
    
    if( !PC_Float_Parse( handle, &modelPtr->fov_x ) )
        return false;
        
    return true;
}

// model_fovy <number>
bool ItemParse_model_fovy( itemDef_t* item, S32 handle )
{
    modelDef_t* modelPtr;
    Item_ValidateTypeData( item );
    modelPtr = ( modelDef_t* )item->typeData;
    
    if( !PC_Float_Parse( handle, &modelPtr->fov_y ) )
        return false;
        
    return true;
}

// model_rotation <integer>
bool ItemParse_model_rotation( itemDef_t* item, S32 handle )
{
    modelDef_t* modelPtr;
    Item_ValidateTypeData( item );
    modelPtr = ( modelDef_t* )item->typeData;
    
    if( !PC_Int_Parse( handle, &modelPtr->rotationSpeed ) )
        return false;
        
    return true;
}

// model_angle <integer>
bool ItemParse_model_angle( itemDef_t* item, S32 handle )
{
    modelDef_t* modelPtr;
    Item_ValidateTypeData( item );
    modelPtr = ( modelDef_t* )item->typeData;
    
    if( !PC_Int_Parse( handle, &modelPtr->angle ) )
        return false;
        
    return true;
}

// model_axis <number> <number> <number> //:://
bool ItemParse_model_axis( itemDef_t* item, S32 handle )
{
    modelDef_t* modelPtr;
    Item_ValidateTypeData( item );
    modelPtr = ( modelDef_t* )item->typeData;
    
    if( !PC_Float_Parse( handle, &modelPtr->axis[0] ) ) return false;
    if( !PC_Float_Parse( handle, &modelPtr->axis[1] ) ) return false;
    if( !PC_Float_Parse( handle, &modelPtr->axis[2] ) ) return false;
    
    return true;
}

// model_animplay <S32(startframe)> <S32(numframes)> <S32(fps)>
bool ItemParse_model_animplay( itemDef_t* item, S32 handle )
{
    modelDef_t* modelPtr;
    Item_ValidateTypeData( item );
    modelPtr = ( modelDef_t* )item->typeData;
    
    modelPtr->animated = 1;
    
    if( !PC_Int_Parse( handle, &modelPtr->startframe ) )	return false;
    if( !PC_Int_Parse( handle, &modelPtr->numframes ) )	return false;
    if( !PC_Int_Parse( handle, &modelPtr->fps ) )			return false;
    
    modelPtr->frame		= modelPtr->startframe + 1;
    modelPtr->oldframe	= modelPtr->startframe;
    modelPtr->backlerp	= 0.0f;
    modelPtr->frameTime = DC->realTime;
    return true;
}

// rect <rectangle>
bool ItemParse_rect( itemDef_t* item, S32 handle )
{
    if( !PC_Rect_Parse( handle, &item->window.rectClient ) )
        return false;
        
    return true;
}

// aspectBias <bias>
bool ItemParse_aspectBias( itemDef_t* item, S32 handle )
{
    if( !PC_Int_Parse( handle, &item->window.aspectBias ) )
        return false;
        
    return true;
}

// style <integer>
bool ItemParse_style( itemDef_t* item, S32 handle )
{
    if( !PC_Int_Parse( handle, &item->window.style ) )
        return false;
        
    return true;
}

// decoration
bool ItemParse_decoration( itemDef_t* item, S32 handle )
{
    item->window.flags |= WINDOW_DECORATION;
    return true;
}

// notselectable
bool ItemParse_notselectable( itemDef_t* item, S32 handle )
{
    listBoxDef_t* listPtr;
    Item_ValidateTypeData( item );
    listPtr = ( listBoxDef_t* )item->typeData;
    
    if( item->type == ITEM_TYPE_LISTBOX && listPtr )
        listPtr->notselectable = true;
        
    return true;
}

// noscrollbar
bool ItemParse_noscrollbar( itemDef_t* item, S32 handle )
{
    listBoxDef_t* listPtr;
    Item_ValidateTypeData( item );
    listPtr = ( listBoxDef_t* )item->typeData;
    
    if( item->type == ITEM_TYPE_LISTBOX && listPtr )
        listPtr->noscrollbar = true;
        
    return true;
}

// auto wrapped
bool ItemParse_wrapped( itemDef_t* item, S32 handle )
{
    item->window.flags |= WINDOW_WRAPPED;
    return true;
}


// horizontalscroll
bool ItemParse_horizontalscroll( itemDef_t* item, S32 handle )
{
    item->window.flags |= WINDOW_HORIZONTAL;
    return true;
}

// type <integer>
bool ItemParse_type( itemDef_t* item, S32 handle )
{
    if( !PC_Int_Parse( handle, &item->type ) )
        return false;
        
    Item_ValidateTypeData( item );
    return true;
}

// elementwidth, used for listbox image elements
// uses textalignx for storage
bool ItemParse_elementwidth( itemDef_t* item, S32 handle )
{
    listBoxDef_t* listPtr;
    
    Item_ValidateTypeData( item );
    listPtr = ( listBoxDef_t* )item->typeData;
    
    if( !PC_Float_Parse( handle, &listPtr->elementWidth ) )
        return false;
        
    return true;
}

// elementheight, used for listbox image elements
// uses textaligny for storage
bool ItemParse_elementheight( itemDef_t* item, S32 handle )
{
    listBoxDef_t* listPtr;
    
    Item_ValidateTypeData( item );
    listPtr = ( listBoxDef_t* )item->typeData;
    
    if( !PC_Float_Parse( handle, &listPtr->elementHeight ) )
        return false;
        
    return true;
}

// feeder <F32>
bool ItemParse_feeder( itemDef_t* item, S32 handle )
{
    if( !PC_Float_Parse( handle, &item->special ) )
        return false;
        
    return true;
}

// elementtype, used to specify what type of elements a listbox contains
// uses textstyle for storage
bool ItemParse_elementtype( itemDef_t* item, S32 handle )
{
    listBoxDef_t* listPtr;
    
    Item_ValidateTypeData( item );
    
    if( !item->typeData )
        return false;
        
    listPtr = ( listBoxDef_t* )item->typeData;
    
    if( !PC_Int_Parse( handle, &listPtr->elementStyle ) )
        return false;
        
    return true;
}

// columns sets a number of columns and an x pos and width per..
bool ItemParse_columns( itemDef_t* item, S32 handle )
{
    S32 num, i;
    listBoxDef_t* listPtr;
    
    Item_ValidateTypeData( item );
    
    if( !item->typeData )
        return false;
        
    listPtr = ( listBoxDef_t* )item->typeData;
    
    if( PC_Int_Parse( handle, &num ) )
    {
        if( num > MAX_LB_COLUMNS )
            num = MAX_LB_COLUMNS;
            
        listPtr->numColumns = num;
        
        for( i = 0; i < num; i++ )
        {
            S32 pos, width, align;
            
            if( PC_Int_Parse( handle, &pos ) &&
                    PC_Int_Parse( handle, &width ) &&
                    PC_Int_Parse( handle, &align ) )
            {
                listPtr->columnInfo[i].pos = pos;
                listPtr->columnInfo[i].width = width;
                listPtr->columnInfo[i].align = align;
            }
            else
                return false;
        }
    }
    else
        return false;
        
    return true;
}

bool ItemParse_border( itemDef_t* item, S32 handle )
{
    if( !PC_Int_Parse( handle, &item->window.border ) )
        return false;
        
    return true;
}

bool ItemParse_bordersize( itemDef_t* item, S32 handle )
{
    if( !PC_Float_Parse( handle, &item->window.borderSize ) )
        return false;
        
    return true;
}

bool ItemParse_visible( itemDef_t* item, S32 handle )
{
    S32 i;
    
    if( !PC_Int_Parse( handle, &i ) )
        return false;
        
    item->window.flags &= ~WINDOW_VISIBLE;
    if( i )
        item->window.flags |= WINDOW_VISIBLE;
        
    return true;
}

bool ItemParse_ownerdraw( itemDef_t* item, S32 handle )
{
    if( !PC_Int_Parse( handle, &item->window.ownerDraw ) )
        return false;
        
    item->type = ITEM_TYPE_OWNERDRAW;
    return true;
}

bool ItemParse_align( itemDef_t* item, S32 handle )
{
    if( !PC_Int_Parse( handle, &item->alignment ) )
        return false;
        
    return true;
}

bool ItemParse_textalign( itemDef_t* item, S32 handle )
{
    if( !PC_Int_Parse( handle, &item->textalignment ) )
        return false;
        
    return true;
}

bool ItemParse_textvalign( itemDef_t* item, S32 handle )
{
    if( !PC_Int_Parse( handle, &item->textvalignment ) )
        return false;
        
    return true;
}

bool ItemParse_textalignx( itemDef_t* item, S32 handle )
{
    if( !PC_Float_Parse( handle, &item->textalignx ) )
        return false;
        
    return true;
}

bool ItemParse_textaligny( itemDef_t* item, S32 handle )
{
    if( !PC_Float_Parse( handle, &item->textaligny ) )
        return false;
        
    return true;
}

bool ItemParse_textscale( itemDef_t* item, S32 handle )
{
    if( !PC_Float_Parse( handle, &item->textscale ) )
        return false;
        
    return true;
}

bool ItemParse_textstyle( itemDef_t* item, S32 handle )
{
    if( !PC_Int_Parse( handle, &item->textStyle ) )
        return false;
        
    return true;
}

bool ItemParse_backcolor( itemDef_t* item, S32 handle )
{
    S32 i;
    F32 f;
    
    for( i = 0; i < 4; i++ )
    {
        if( !PC_Float_Parse( handle, &f ) )
            return false;
            
        item->window.backColor[i]  = f;
    }
    
    return true;
}

bool ItemParse_forecolor( itemDef_t* item, S32 handle )
{
    S32 i;
    F32 f;
    
    for( i = 0; i < 4; i++ )
    {
        if( !PC_Float_Parse( handle, &f ) )
            return false;
            
        item->window.foreColor[i]  = f;
        item->window.flags |= WINDOW_FORECOLORSET;
    }
    
    return true;
}

bool ItemParse_bordercolor( itemDef_t* item, S32 handle )
{
    S32 i;
    F32 f;
    
    for( i = 0; i < 4; i++ )
    {
        if( !PC_Float_Parse( handle, &f ) )
            return false;
            
        item->window.borderColor[i]  = f;
    }
    
    return true;
}

bool ItemParse_outlinecolor( itemDef_t* item, S32 handle )
{
    if( !PC_Color_Parse( handle, &item->window.outlineColor ) )
        return false;
        
    return true;
}

bool ItemParse_background( itemDef_t* item, S32 handle )
{
    StringEntry temp;
    
    if( !PC_String_Parse( handle, &temp ) )
        return false;
        
    item->window.background = DC->registerShaderNoMip( temp );
    return true;
}

bool ItemParse_cinematic( itemDef_t* item, S32 handle )
{
    if( !PC_String_Parse( handle, &item->window.cinematicName ) )
        return false;
        
    return true;
}

bool ItemParse_doubleClick( itemDef_t* item, S32 handle )
{
    listBoxDef_t* listPtr;
    
    Item_ValidateTypeData( item );
    
    if( !item->typeData )
        return false;
        
    listPtr = ( listBoxDef_t* )item->typeData;
    
    if( !PC_Script_Parse( handle, &listPtr->doubleClick ) )
        return false;
        
    return true;
}

bool ItemParse_onFocus( itemDef_t* item, S32 handle )
{
    if( !PC_Script_Parse( handle, &item->onFocus ) )
        return false;
        
    return true;
}

bool ItemParse_leaveFocus( itemDef_t* item, S32 handle )
{
    if( !PC_Script_Parse( handle, &item->leaveFocus ) )
        return false;
        
    return true;
}

bool ItemParse_mouseEnter( itemDef_t* item, S32 handle )
{
    if( !PC_Script_Parse( handle, &item->mouseEnter ) )
        return false;
        
    return true;
}

bool ItemParse_mouseExit( itemDef_t* item, S32 handle )
{
    if( !PC_Script_Parse( handle, &item->mouseExit ) )
        return false;
        
    return true;
}

bool ItemParse_mouseEnterText( itemDef_t* item, S32 handle )
{
    if( !PC_Script_Parse( handle, &item->mouseEnterText ) )
        return false;
        
    return true;
}

bool ItemParse_mouseExitText( itemDef_t* item, S32 handle )
{
    if( !PC_Script_Parse( handle, &item->mouseExitText ) )
        return false;
        
    return true;
}

bool ItemParse_onTextEntry( itemDef_t* item, S32 handle )
{
    if( !PC_Script_Parse( handle, &item->onTextEntry ) )
        return false;
        
    return true;
}

bool ItemParse_action( itemDef_t* item, S32 handle )
{
    if( !PC_Script_Parse( handle, &item->action ) )
        return false;
        
    return true;
}

bool ItemParse_special( itemDef_t* item, S32 handle )
{
    if( !PC_Float_Parse( handle, &item->special ) )
        return false;
        
    return true;
}

bool ItemParse_cvarTest( itemDef_t* item, S32 handle )
{
    if( !PC_String_Parse( handle, &item->cvarTest ) )
        return false;
        
    return true;
}

bool ItemParse_cvar( itemDef_t* item, S32 handle )
{
    editFieldDef_t* editPtr;
    
    Item_ValidateTypeData( item );
    
    if( !PC_String_Parse( handle, &item->cvar ) )
        return false;
        
    if( item->typeData )
    {
        editPtr = ( editFieldDef_t* )item->typeData;
        editPtr->minVal = -1;
        editPtr->maxVal = -1;
        editPtr->defVal = -1;
    }
    
    return true;
}

bool ItemParse_maxChars( itemDef_t* item, S32 handle )
{
    editFieldDef_t* editPtr;
    S32 maxChars;
    
    Item_ValidateTypeData( item );
    
    if( !item->typeData )
        return false;
        
    if( !PC_Int_Parse( handle, &maxChars ) )
        return false;
        
    editPtr = ( editFieldDef_t* )item->typeData;
    editPtr->maxChars = maxChars;
    return true;
}

bool ItemParse_maxPaintChars( itemDef_t* item, S32 handle )
{
    editFieldDef_t* editPtr;
    S32 maxChars;
    
    Item_ValidateTypeData( item );
    
    if( !item->typeData )
        return false;
        
    if( !PC_Int_Parse( handle, &maxChars ) )
        return false;
        
    editPtr = ( editFieldDef_t* )item->typeData;
    editPtr->maxPaintChars = maxChars;
    return true;
}

bool ItemParse_maxFieldWidth( itemDef_t* item, S32 handle )
{
    editFieldDef_t* editPtr;
    S32 maxFieldWidth;
    
    Item_ValidateTypeData( item );
    
    if( !item->typeData )
        return false;
        
    if( !PC_Int_Parse( handle, &maxFieldWidth ) )
        return false;
        
    editPtr = ( editFieldDef_t* )item->typeData;
    
    if( maxFieldWidth < MIN_FIELD_WIDTH )
        editPtr->maxFieldWidth = MIN_FIELD_WIDTH;
    else
        editPtr->maxFieldWidth = maxFieldWidth;
        
    return true;
}



bool ItemParse_cvarFloat( itemDef_t* item, S32 handle )
{
    editFieldDef_t* editPtr;
    
    Item_ValidateTypeData( item );
    
    if( !item->typeData )
        return false;
        
    editPtr = ( editFieldDef_t* )item->typeData;
    
    if( PC_String_Parse( handle, &item->cvar ) &&
            PC_Float_Parse( handle, &editPtr->defVal ) &&
            PC_Float_Parse( handle, &editPtr->minVal ) &&
            PC_Float_Parse( handle, &editPtr->maxVal ) )
        return true;
        
    return false;
}

bool ItemParse_cvarStrList( itemDef_t* item, S32 handle )
{
    pc_token_t token;
    multiDef_t* multiPtr;
    S32 pass;
    
    Item_ValidateTypeData( item );
    
    if( !item->typeData )
        return false;
        
    multiPtr = ( multiDef_t* )item->typeData;
    
    multiPtr->count = 0;
    
    multiPtr->strDef = true;
    
    if( !trap_PC_ReadToken( handle, &token ) )
        return false;
        
    if( *token.string != '{' )
        return false;
        
    pass = 0;
    
    while( 1 )
    {
        if( !trap_PC_ReadToken( handle, &token ) )
        {
            PC_SourceError( handle, "end of file inside menu item\n" );
            return false;
        }
        
        if( *token.string == '}' )
            return true;
            
        if( *token.string == ',' || *token.string == ';' )
            continue;
            
        if( pass == 0 )
        {
            multiPtr->cvarList[multiPtr->count] = String_Alloc( token.string );
            pass = 1;
        }
        else
        {
            multiPtr->cvarStr[multiPtr->count] = String_Alloc( token.string );
            pass = 0;
            multiPtr->count++;
            
            if( multiPtr->count >= MAX_MULTI_CVARS )
                return false;
        }
        
    }
    
    return false;
}

bool ItemParse_cvarFloatList( itemDef_t* item, S32 handle )
{
    pc_token_t token;
    multiDef_t* multiPtr;
    
    Item_ValidateTypeData( item );
    
    if( !item->typeData )
        return false;
        
    multiPtr = ( multiDef_t* )item->typeData;
    
    multiPtr->count = 0;
    
    multiPtr->strDef = false;
    
    if( !trap_PC_ReadToken( handle, &token ) )
        return false;
        
    if( *token.string != '{' )
        return false;
        
    while( 1 )
    {
        if( !trap_PC_ReadToken( handle, &token ) )
        {
            PC_SourceError( handle, "end of file inside menu item\n" );
            return false;
        }
        
        if( *token.string == '}' )
            return true;
            
        if( *token.string == ',' || *token.string == ';' )
            continue;
            
        multiPtr->cvarList[multiPtr->count] = String_Alloc( token.string );
        
        if( !PC_Float_Parse( handle, &multiPtr->cvarValue[multiPtr->count] ) )
            return false;
            
        multiPtr->count++;
        
        if( multiPtr->count >= MAX_MULTI_CVARS )
            return false;
            
    }
    
    return false;
}



bool ItemParse_addColorRange( itemDef_t* item, S32 handle )
{
    colorRangeDef_t color;
    
    if( PC_Float_Parse( handle, &color.low ) &&
            PC_Float_Parse( handle, &color.high ) &&
            PC_Color_Parse( handle, &color.color ) )
    {
        if( item->numColors < MAX_COLOR_RANGES )
        {
            memcpy( &item->colorRanges[item->numColors], &color, sizeof( color ) );
            item->numColors++;
        }
        
        return true;
    }
    
    return false;
}

bool ItemParse_ownerdrawFlag( itemDef_t* item, S32 handle )
{
    S32 i;
    
    if( !PC_Int_Parse( handle, &i ) )
        return false;
        
    item->window.ownerDrawFlags |= i;
    return true;
}

bool ItemParse_enableCvar( itemDef_t* item, S32 handle )
{
    if( PC_Script_Parse( handle, &item->enableCvar ) )
    {
        item->cvarFlags = CVAR_ENABLE;
        return true;
    }
    
    return false;
}

bool ItemParse_disableCvar( itemDef_t* item, S32 handle )
{
    if( PC_Script_Parse( handle, &item->enableCvar ) )
    {
        item->cvarFlags = CVAR_DISABLE;
        return true;
    }
    
    return false;
}

bool ItemParse_showCvar( itemDef_t* item, S32 handle )
{
    if( PC_Script_Parse( handle, &item->enableCvar ) )
    {
        item->cvarFlags = CVAR_SHOW;
        return true;
    }
    
    return false;
}

bool ItemParse_hideCvar( itemDef_t* item, S32 handle )
{
    if( PC_Script_Parse( handle, &item->enableCvar ) )
    {
        item->cvarFlags = CVAR_HIDE;
        return true;
    }
    
    return false;
}


keywordHash_t itemParseKeywords[] =
{
    {"name", ItemParse_name, NULL},
    {"text", ItemParse_text, NULL},
    {"group", ItemParse_group, NULL},
    {"asset_model", ItemParse_asset_model, NULL},
    {"asset_shader", ItemParse_asset_shader, NULL},
    {"model_origin", ItemParse_model_origin, NULL},
    {"model_fovx", ItemParse_model_fovx, NULL},
    {"model_fovy", ItemParse_model_fovy, NULL},
    {"model_rotation", ItemParse_model_rotation, NULL},
    {"model_angle", ItemParse_model_angle, NULL},
    {"model_axis", ItemParse_model_axis, NULL},
    {"model_animplay", ItemParse_model_animplay, NULL},
    {"rect", ItemParse_rect, NULL},
    {"aspectBias", ItemParse_aspectBias, NULL},
    {"style", ItemParse_style, NULL},
    {"decoration", ItemParse_decoration, NULL},
    {"notselectable", ItemParse_notselectable, NULL},
    {"noscrollbar", ItemParse_noscrollbar, NULL},
    {"wrapped", ItemParse_wrapped, NULL},
    {"horizontalscroll", ItemParse_horizontalscroll, NULL},
    {"type", ItemParse_type, NULL},
    {"elementwidth", ItemParse_elementwidth, NULL},
    {"elementheight", ItemParse_elementheight, NULL},
    {"feeder", ItemParse_feeder, NULL},
    {"elementtype", ItemParse_elementtype, NULL},
    {"columns", ItemParse_columns, NULL},
    {"border", ItemParse_border, NULL},
    {"bordersize", ItemParse_bordersize, NULL},
    {"visible", ItemParse_visible, NULL},
    {"ownerdraw", ItemParse_ownerdraw, NULL},
    {"align", ItemParse_align, NULL},
    {"textalign", ItemParse_textalign, NULL},
    {"textvalign", ItemParse_textvalign, NULL},
    {"textalignx", ItemParse_textalignx, NULL},
    {"textaligny", ItemParse_textaligny, NULL},
    {"textscale", ItemParse_textscale, NULL},
    {"textstyle", ItemParse_textstyle, NULL},
    {"backcolor", ItemParse_backcolor, NULL},
    {"forecolor", ItemParse_forecolor, NULL},
    {"bordercolor", ItemParse_bordercolor, NULL},
    {"outlinecolor", ItemParse_outlinecolor, NULL},
    {"background", ItemParse_background, NULL},
    {"onFocus", ItemParse_onFocus, NULL},
    {"leaveFocus", ItemParse_leaveFocus, NULL},
    {"mouseEnter", ItemParse_mouseEnter, NULL},
    {"mouseExit", ItemParse_mouseExit, NULL},
    {"mouseEnterText", ItemParse_mouseEnterText, NULL},
    {"mouseExitText", ItemParse_mouseExitText, NULL},
    {"onTextEntry", ItemParse_onTextEntry, NULL},
    {"action", ItemParse_action, NULL},
    {"special", ItemParse_special, NULL},
    {"cvar", ItemParse_cvar, NULL},
    {"maxChars", ItemParse_maxChars, NULL},
    {"maxPaintChars", ItemParse_maxPaintChars, NULL},
    {"maxFieldWidth", ItemParse_maxFieldWidth, NULL},
    {"focusSound", ItemParse_focusSound, NULL},
    {"cvarFloat", ItemParse_cvarFloat, NULL},
    {"cvarStrList", ItemParse_cvarStrList, NULL},
    {"cvarFloatList", ItemParse_cvarFloatList, NULL},
    {"addColorRange", ItemParse_addColorRange, NULL},
    {"ownerdrawFlag", ItemParse_ownerdrawFlag, NULL},
    {"enableCvar", ItemParse_enableCvar, NULL},
    {"cvarTest", ItemParse_cvarTest, NULL},
    {"disableCvar", ItemParse_disableCvar, NULL},
    {"showCvar", ItemParse_showCvar, NULL},
    {"hideCvar", ItemParse_hideCvar, NULL},
    {"cinematic", ItemParse_cinematic, NULL},
    {"doubleclick", ItemParse_doubleClick, NULL},
    {NULL, voidFunction2, NULL}
};

keywordHash_t* itemParseKeywordHash[KEYWORDHASH_SIZE];

/*
===============
Item_SetupKeywordHash
===============
*/
void Item_SetupKeywordHash( void )
{
    S32 i;
    
    ::memset( itemParseKeywordHash, 0, sizeof( itemParseKeywordHash ) );
    
    for( i = 0; itemParseKeywords[ i ].keyword; i++ )
        KeywordHash_Add( itemParseKeywordHash, &itemParseKeywords[ i ] );
}

/*
===============
Item_Parse
===============
*/
bool Item_Parse( S32 handle, itemDef_t* item )
{
    pc_token_t token;
    keywordHash_t* key;
    
    
    if( !trap_PC_ReadToken( handle, &token ) )
        return false;
        
    if( *token.string != '{' )
        return false;
        
    while( 1 )
    {
        if( !trap_PC_ReadToken( handle, &token ) )
        {
            PC_SourceError( handle, "end of file inside menu item\n" );
            return false;
        }
        
        if( *token.string == '}' )
            return true;
            
        key = KeywordHash_Find( itemParseKeywordHash, token.string );
        
        if( !key )
        {
            PC_SourceError( handle, "unknown menu item keyword %s", token.string );
            continue;
        }
        
        if( !key->func( item, handle ) )
        {
            PC_SourceError( handle, "couldn't parse menu item keyword %s", token.string );
            return false;
        }
    }
    
    return false;
}


// Item_InitControls
// init's special control types
void Item_InitControls( itemDef_t* item )
{
    if( item == NULL )
        return;
        
    if( item->type == ITEM_TYPE_LISTBOX )
    {
        listBoxDef_t* listPtr = ( listBoxDef_t* )item->typeData;
        item->cursorPos = 0;
        
        if( listPtr )
        {
            listPtr->cursorPos = 0;
            listPtr->startPos = 0;
            listPtr->endPos = 0;
            listPtr->cursorPos = 0;
        }
    }
}

/*
===============
Menu Keyword Parse functions
===============
*/

bool MenuParse_font( itemDef_t* item, S32 handle )
{
    menuDef_t* menu = ( menuDef_t* )item;
    
    if( !PC_String_Parse( handle, &menu->font ) )
        return false;
        
    if( !DC->Assets.fontRegistered )
    {
        DC->registerFont( menu->font, 48, &DC->Assets.textFont );
        DC->Assets.fontRegistered = true;
    }
    
    return true;
}

bool MenuParse_name( itemDef_t* item, S32 handle )
{
    menuDef_t* menu = ( menuDef_t* )item;
    
    if( !PC_String_Parse( handle, &menu->window.name ) )
        return false;
        
    return true;
}

bool MenuParse_fullscreen( itemDef_t* item, S32 handle )
{
    menuDef_t* menu = ( menuDef_t* )item;
    union
    {
        bool b;
        S32 i;
    } fullScreen;
    
    if( !PC_Int_Parse( handle, &fullScreen.i ) )
        return false;
    menu->fullScreen = fullScreen.b;
    return true;
}

bool MenuParse_rect( itemDef_t* item, S32 handle )
{
    menuDef_t* menu = ( menuDef_t* )item;
    
    if( !PC_Rect_Parse( handle, &menu->window.rect ) )
        return false;
        
    return true;
}

bool MenuParse_aspectBias( itemDef_t* item, S32 handle )
{
    menuDef_t* menu = ( menuDef_t* )item;
    
    if( !PC_Int_Parse( handle, &menu->window.aspectBias ) )
        return false;
        
    return true;
}

bool MenuParse_style( itemDef_t* item, S32 handle )
{
    menuDef_t* menu = ( menuDef_t* )item;
    
    if( !PC_Int_Parse( handle, &menu->window.style ) )
        return false;
        
    return true;
}

bool MenuParse_visible( itemDef_t* item, S32 handle )
{
    S32 i;
    menuDef_t* menu = ( menuDef_t* )item;
    
    if( !PC_Int_Parse( handle, &i ) )
        return false;
        
    if( i )
        menu->window.flags |= WINDOW_VISIBLE;
        
    return true;
}

bool MenuParse_dontCloseAll( itemDef_t* item, S32 handle )
{
    S32 i;
    menuDef_t* menu = ( menuDef_t* )item;
    
    if( !PC_Int_Parse( handle, &i ) )
        return false;
        
    if( i )
        menu->window.flags |= WINDOW_DONTCLOSEALL;
        
    return true;
}

bool MenuParse_onOpen( itemDef_t* item, S32 handle )
{
    menuDef_t* menu = ( menuDef_t* )item;
    
    if( !PC_Script_Parse( handle, &menu->onOpen ) )
        return false;
        
    return true;
}

bool MenuParse_onClose( itemDef_t* item, S32 handle )
{
    menuDef_t* menu = ( menuDef_t* )item;
    
    if( !PC_Script_Parse( handle, &menu->onClose ) )
        return false;
        
    return true;
}

bool MenuParse_onESC( itemDef_t* item, S32 handle )
{
    menuDef_t* menu = ( menuDef_t* )item;
    
    if( !PC_Script_Parse( handle, &menu->onESC ) )
        return false;
        
    return true;
}



bool MenuParse_border( itemDef_t* item, S32 handle )
{
    menuDef_t* menu = ( menuDef_t* )item;
    
    if( !PC_Int_Parse( handle, &menu->window.border ) )
        return false;
        
    return true;
}

bool MenuParse_borderSize( itemDef_t* item, S32 handle )
{
    menuDef_t* menu = ( menuDef_t* )item;
    
    if( !PC_Float_Parse( handle, &menu->window.borderSize ) )
        return false;
        
    return true;
}

bool MenuParse_backcolor( itemDef_t* item, S32 handle )
{
    S32 i;
    F32 f;
    menuDef_t* menu = ( menuDef_t* )item;
    
    for( i = 0; i < 4; i++ )
    {
        if( !PC_Float_Parse( handle, &f ) )
            return false;
            
        menu->window.backColor[i]  = f;
    }
    
    return true;
}

bool MenuParse_forecolor( itemDef_t* item, S32 handle )
{
    S32 i;
    F32 f;
    menuDef_t* menu = ( menuDef_t* )item;
    
    for( i = 0; i < 4; i++ )
    {
        if( !PC_Float_Parse( handle, &f ) )
            return false;
            
        menu->window.foreColor[i]  = f;
        menu->window.flags |= WINDOW_FORECOLORSET;
    }
    
    return true;
}

bool MenuParse_bordercolor( itemDef_t* item, S32 handle )
{
    S32 i;
    F32 f;
    menuDef_t* menu = ( menuDef_t* )item;
    
    for( i = 0; i < 4; i++ )
    {
        if( !PC_Float_Parse( handle, &f ) )
            return false;
            
        menu->window.borderColor[i]  = f;
    }
    
    return true;
}

bool MenuParse_focuscolor( itemDef_t* item, S32 handle )
{
    S32 i;
    F32 f;
    menuDef_t* menu = ( menuDef_t* )item;
    
    for( i = 0; i < 4; i++ )
    {
        if( !PC_Float_Parse( handle, &f ) )
            return false;
            
        menu->focusColor[i]  = f;
    }
    
    return true;
}

bool MenuParse_disablecolor( itemDef_t* item, S32 handle )
{
    S32 i;
    F32 f;
    menuDef_t* menu = ( menuDef_t* )item;
    
    for( i = 0; i < 4; i++ )
    {
        if( !PC_Float_Parse( handle, &f ) )
            return false;
            
        menu->disableColor[i]  = f;
    }
    
    return true;
}


bool MenuParse_outlinecolor( itemDef_t* item, S32 handle )
{
    menuDef_t* menu = ( menuDef_t* )item;
    
    if( !PC_Color_Parse( handle, &menu->window.outlineColor ) )
        return false;
        
    return true;
}

bool MenuParse_background( itemDef_t* item, S32 handle )
{
    StringEntry buff;
    menuDef_t* menu = ( menuDef_t* )item;
    
    if( !PC_String_Parse( handle, &buff ) )
        return false;
        
    menu->window.background = DC->registerShaderNoMip( buff );
    return true;
}

bool MenuParse_cinematic( itemDef_t* item, S32 handle )
{
    menuDef_t* menu = ( menuDef_t* )item;
    
    if( !PC_String_Parse( handle, &menu->window.cinematicName ) )
        return false;
        
    return true;
}

bool MenuParse_ownerdrawFlag( itemDef_t* item, S32 handle )
{
    S32 i;
    menuDef_t* menu = ( menuDef_t* )item;
    
    if( !PC_Int_Parse( handle, &i ) )
        return false;
        
    menu->window.ownerDrawFlags |= i;
    return true;
}

bool MenuParse_ownerdraw( itemDef_t* item, S32 handle )
{
    menuDef_t* menu = ( menuDef_t* )item;
    
    if( !PC_Int_Parse( handle, &menu->window.ownerDraw ) )
        return false;
        
    return true;
}


// decoration
bool MenuParse_popup( itemDef_t* item, S32 handle )
{
    menuDef_t* menu = ( menuDef_t* )item;
    menu->window.flags |= WINDOW_POPUP;
    return true;
}


bool MenuParse_outOfBounds( itemDef_t* item, S32 handle )
{
    menuDef_t* menu = ( menuDef_t* )item;
    
    menu->window.flags |= WINDOW_OOB_CLICK;
    return true;
}

bool MenuParse_soundLoop( itemDef_t* item, S32 handle )
{
    menuDef_t* menu = ( menuDef_t* )item;
    
    if( !PC_String_Parse( handle, &menu->soundName ) )
        return false;
        
    return true;
}

bool MenuParse_listenTo( itemDef_t* item, S32 handle )
{
    menuDef_t* menu = ( menuDef_t* )item;
    
    if( !PC_String_Parse( handle, &menu->listenCvar ) )
        return false;
        
    return true;
}

bool MenuParse_fadeClamp( itemDef_t* item, S32 handle )
{
    menuDef_t* menu = ( menuDef_t* )item;
    
    if( !PC_Float_Parse( handle, &menu->fadeClamp ) )
        return false;
        
    return true;
}

bool MenuParse_fadeAmount( itemDef_t* item, S32 handle )
{
    menuDef_t* menu = ( menuDef_t* )item;
    
    if( !PC_Float_Parse( handle, &menu->fadeAmount ) )
        return false;
        
    return true;
}


bool MenuParse_fadeCycle( itemDef_t* item, S32 handle )
{
    menuDef_t* menu = ( menuDef_t* )item;
    
    if( !PC_Int_Parse( handle, &menu->fadeCycle ) )
        return false;
        
    return true;
}


bool MenuParse_itemDef( itemDef_t* item, S32 handle )
{
    menuDef_t* menu = ( menuDef_t* )item;
    
    if( menu->itemCount < MAX_MENUITEMS )
    {
        if( DC->hudloading )
            menu->items[menu->itemCount] = ( itemDef_t* )UI_HUDAlloc( sizeof( itemDef_t ) );
        else
            menu->items[menu->itemCount] = ( itemDef_t* )UI_Alloc( sizeof( itemDef_t ) );
        Item_Init( menu->items[menu->itemCount] );
        
        if( !Item_Parse( handle, menu->items[menu->itemCount] ) )
            return false;
            
        Item_InitControls( menu->items[menu->itemCount] );
        menu->items[menu->itemCount++]->parent = menu;
    }
    
    return true;
}

keywordHash_t menuParseKeywords[] =
{
    {"font", MenuParse_font, NULL},
    {"name", MenuParse_name, NULL},
    {"fullscreen", MenuParse_fullscreen, NULL},
    {"rect", MenuParse_rect, NULL},
    {"aspectBias", MenuParse_aspectBias, NULL},
    {"style", MenuParse_style, NULL},
    {"visible", MenuParse_visible, NULL},
    {"dontCloseAll", MenuParse_dontCloseAll, NULL},
    {"onOpen", MenuParse_onOpen, NULL},
    {"onClose", MenuParse_onClose, NULL},
    {"onESC", MenuParse_onESC, NULL},
    {"border", MenuParse_border, NULL},
    {"borderSize", MenuParse_borderSize, NULL},
    {"backcolor", MenuParse_backcolor, NULL},
    {"forecolor", MenuParse_forecolor, NULL},
    {"bordercolor", MenuParse_bordercolor, NULL},
    {"focuscolor", MenuParse_focuscolor, NULL},
    {"disablecolor", MenuParse_disablecolor, NULL},
    {"outlinecolor", MenuParse_outlinecolor, NULL},
    {"background", MenuParse_background, NULL},
    {"ownerdraw", MenuParse_ownerdraw, NULL},
    {"ownerdrawFlag", MenuParse_ownerdrawFlag, NULL},
    {"outOfBoundsClick", MenuParse_outOfBounds, NULL},
    {"soundLoop", MenuParse_soundLoop, NULL},
    {"listento", MenuParse_listenTo, NULL},
    {"itemDef", MenuParse_itemDef, NULL},
    {"cinematic", MenuParse_cinematic, NULL},
    {"popup", MenuParse_popup, NULL},
    {"fadeClamp", MenuParse_fadeClamp, NULL},
    {"fadeCycle", MenuParse_fadeCycle, NULL},
    {"fadeAmount", MenuParse_fadeAmount, NULL},
    {NULL, voidFunction2, NULL}
};

keywordHash_t* menuParseKeywordHash[KEYWORDHASH_SIZE];

/*
===============
Menu_SetupKeywordHash
===============
*/
void Menu_SetupKeywordHash( void )
{
    S32 i;
    
    ::memset( menuParseKeywordHash, 0, sizeof( menuParseKeywordHash ) );
    
    for( i = 0; menuParseKeywords[ i ].keyword; i++ )
        KeywordHash_Add( menuParseKeywordHash, &menuParseKeywords[ i ] );
}

/*
===============
Menu_Parse
===============
*/
bool Menu_Parse( S32 handle, menuDef_t* menu )
{
    pc_token_t token;
    keywordHash_t* key;
    
    if( !trap_PC_ReadToken( handle, &token ) )
        return false;
        
    if( *token.string != '{' )
        return false;
        
    while( 1 )
    {
        ::memset( &token, 0, sizeof( pc_token_t ) );
        
        if( !trap_PC_ReadToken( handle, &token ) )
        {
            PC_SourceError( handle, "end of file inside menu\n" );
            return false;
        }
        
        if( *token.string == '}' )
            return true;
            
        key = KeywordHash_Find( menuParseKeywordHash, token.string );
        
        if( !key )
        {
            PC_SourceError( handle, "unknown menu keyword %s", token.string );
            continue;
        }
        
        if( !key->func( ( itemDef_t* )menu, handle ) )
        {
            PC_SourceError( handle, "couldn't parse menu keyword %s", token.string );
            return false;
        }
    }
    
    return false;
}

/*
===============
Menu_New
===============
*/
void Menu_New( S32 handle )
{
    menuDef_t* menu = &Menus[menuCount];
    
    if( menuCount < MAX_MENUS )
    {
        Menu_Init( menu );
        
        if( Menu_Parse( handle, menu ) )
        {
            Menu_PostParse( menu );
            menuCount++;
        }
    }
}

S32 Menu_Count( void )
{
    return menuCount;
}

void Menu_PaintAll( void )
{
    S32 i;
    
    if( g_editingField || g_waitingForKey )
        DC->setCVar( "gui_hideCursor", "1" );
    else
        DC->setCVar( "gui_hideCursor", "0" );
        
    if( captureFunc != voidFunction )
    {
        if( captureFuncExpiry > 0 && DC->realTime > captureFuncExpiry )
            UI_RemoveCaptureFunc( );
        else
            captureFunc( captureData );
    }
    
    for( i = 0; i < openMenuCount; i++ )
        Menu_Paint( menuStack[i], false );
        
    if( DC->getCVarValue( "gui_developer" ) )
    {
        vec4_t v = {1, 1, 1, 1};
        UI_Text_Paint( 5, 25, .5, v, va( "fps: %f", DC->FPS ), 0, 0, 0 );
    }
}

void Menu_Reset( void )
{
    menuCount = 0;
}

displayContextDef_t* Display_GetContext( void )
{
    return DC;
}

void* Display_CaptureItem( S32 x, S32 y )
{
    S32 i;
    
    for( i = 0; i < menuCount; i++ )
    {
        if( Rect_ContainsPoint( &Menus[i].window.rect, x, y ) )
            return & Menus[i];
    }
    
    return NULL;
}


// FIXME:
bool Display_MouseMove( void* p, F32 x, F32 y )
{
    S32 i;
    menuDef_t* menu = ( menuDef_t* )p;
    
    if( menu == NULL )
    {
        menu = Menu_GetFocused();
        
        if( menu )
        {
            if( menu->window.flags & WINDOW_POPUP )
            {
                Menu_HandleMouseMove( menu, x, y );
                return true;
            }
        }
        
        for( i = 0; i < menuCount; i++ )
            Menu_HandleMouseMove( &Menus[i], x, y );
    }
    else
    {
        menu->window.rect.x += x;
        menu->window.rect.y += y;
        Menu_UpdatePosition( menu );
    }
    
    return true;
    
}

S32 Display_CursorType( S32 x, S32 y )
{
    S32 i;
    
    for( i = 0; i < menuCount; i++ )
    {
        rectDef_t r2;
        r2.x = Menus[i].window.rect.x - 3;
        r2.y = Menus[i].window.rect.y - 3;
        r2.w = r2.h = 7;
        
        if( Rect_ContainsPoint( &r2, x, y ) )
            return CURSOR_SIZER;
    }
    
    return CURSOR_ARROW;
}


void Display_HandleKey( S32 key, bool down, S32 x, S32 y )
{
    menuDef_t* menu = ( menuDef_t* )Display_CaptureItem( x, y );
    
    if( menu == NULL )
        menu = Menu_GetFocused();
        
    if( menu )
        Menu_HandleKey( menu, key, down );
}

static void Window_CacheContents( windowDef_t* window )
{
    if( window )
    {
        if( window->cinematicName )
        {
            S32 cin = DC->playCinematic( window->cinematicName, 0, 0, 0, 0 );
            DC->stopCinematic( cin );
        }
    }
}


static void Item_CacheContents( itemDef_t* item )
{
    if( item )
        Window_CacheContents( &item->window );
        
}

static void Menu_CacheContents( menuDef_t* menu )
{
    if( menu )
    {
        S32 i;
        Window_CacheContents( &menu->window );
        
        for( i = 0; i < menu->itemCount; i++ )
            Item_CacheContents( menu->items[i] );
            
        if( menu->soundName && *menu->soundName )
            DC->registerSound( menu->soundName, false );
    }
    
}

void Display_CacheAll( void )
{
    S32 i;
    
    for( i = 0; i < menuCount; i++ )
        Menu_CacheContents( &Menus[i] );
}


static bool Menu_OverActiveItem( menuDef_t* menu, F32 x, F32 y )
{
    if( menu && menu->window.flags & ( WINDOW_VISIBLE | WINDOW_FORCED ) )
    {
        if( Rect_ContainsPoint( &menu->window.rect, x, y ) )
        {
            S32 i;
            
            for( i = 0; i < menu->itemCount; i++ )
            {
                if( !( menu->items[i]->window.flags & ( WINDOW_VISIBLE | WINDOW_FORCED ) ) )
                    continue;
                    
                if( menu->items[i]->window.flags & WINDOW_DECORATION )
                    continue;
                    
                if( Rect_ContainsPoint( &menu->items[i]->window.rect, x, y ) )
                {
                    itemDef_t* overItem = menu->items[i];
                    
                    if( overItem->type == ITEM_TYPE_TEXT && overItem->text )
                    {
                        if( Rect_ContainsPoint( Item_CorrectedTextRect( overItem ), x, y ) )
                            return true;
                        else
                            continue;
                    }
                    else
                        return true;
                }
            }
            
        }
    }
    
    return false;
}

