////////////////////////////////////////////////////////////////////////////////////////
// Copyright(C) 1999 - 2005 id Software, Inc.
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
// File name:   r_glimp.cpp
// Version:     v1.01
// Created:
// Compilers:   Visual Studio 2017, gcc 7.3.0
// Description:
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#include <OWLib/precompiled.h>

S32 qglMajorVersion, qglMinorVersion;
S32 qglesMajorVersion, qglesMinorVersion;

typedef enum
{
    RSERR_OK,
    
    RSERR_INVALID_FULLSCREEN,
    RSERR_INVALID_MODE,
    
    RSERR_UNKNOWN
} rserr_t;

SDL_Window* SDL_window = NULL;
static SDL_GLContext SDL_glContext = NULL;

cvar_t* r_allowSoftwareGL; // Don't abort out if a hardware visual can't be obtained
cvar_t* r_allowResize; // make window resizable
cvar_t* r_centerWindow;
cvar_t* r_sdlDriver;

void ( APIENTRYP qglActiveTextureARB )( U32 texture );
void ( APIENTRYP qglClientActiveTextureARB )( U32 texture );
void ( APIENTRYP qglMultiTexCoord2fARB )( U32 target, F32 s, F32 t );
void ( APIENTRYP qglLockArraysEXT )( S32 first, S32 count );
void ( APIENTRYP qglUnlockArraysEXT )( void );

#define GLE(ret, name, ...) name##proc * qgl##name;
QGL_1_1_PROCS;
QGL_1_1_FIXED_FUNCTION_PROCS;
QGL_DESKTOP_1_1_PROCS;
QGL_DESKTOP_1_1_FIXED_FUNCTION_PROCS;
QGL_ES_1_1_PROCS;
QGL_ES_1_1_FIXED_FUNCTION_PROCS;
QGL_1_3_PROCS;
QGL_1_5_PROCS;
QGL_2_0_PROCS;
QGL_3_0_PROCS;
QGL_4_0_PROCS;
QGL_ARB_occlusion_query_PROCS;
QGL_ARB_framebuffer_object_PROCS;
QGL_ARB_vertex_array_object_PROCS;
QGL_EXT_direct_state_access_PROCS;
#undef GLE

/*
===============
GLimp_Shutdown
===============
*/
void GLimp_Shutdown( void )
{
    IN_Shutdown();
    
    SDL_QuitSubSystem( SDL_INIT_VIDEO );
}

/*
===============
GLimp_Minimize

Minimize the game so that user is back at the desktop
===============
*/
void GLimp_Minimize( void )
{
    SDL_MinimizeWindow( SDL_window );
}


/*
===============
GLimp_LogComment
===============
*/
void GLimp_LogComment( StringEntry comment )
{
}

/*
===============
GLimp_CompareModes
===============
*/
static S32 GLimp_CompareModes( const void* a, const void* b )
{
    const F32 ASPECT_EPSILON = 0.001f;
    SDL_Rect* modeA = ( SDL_Rect* )a;
    SDL_Rect* modeB = ( SDL_Rect* )b;
    F32 aspectA = ( F32 )modeA->w / ( F32 )modeA->h;
    F32 aspectB = ( F32 )modeB->w / ( F32 )modeB->h;
    S32 areaA = modeA->w * modeA->h;
    S32 areaB = modeB->w * modeB->h;
    F32 aspectDiffA = fabs( aspectA - displayAspect );
    F32 aspectDiffB = fabs( aspectB - displayAspect );
    F32 aspectDiffsDiff = aspectDiffA - aspectDiffB;
    
    if( aspectDiffsDiff > ASPECT_EPSILON )
    {
        return 1;
    }
    else if( aspectDiffsDiff < -ASPECT_EPSILON )
    {
        return -1;
    }
    else
    {
        return areaA - areaB;
    }
}


/*
===============
GLimp_DetectAvailableModes
===============
*/
static void GLimp_DetectAvailableModes( void )
{
    S32 i, j;
    UTF8 buf[ MAX_STRING_CHARS ] = { 0 };
    S32 numSDLModes;
    SDL_Rect* modes;
    S32 numModes = 0;
    
    SDL_DisplayMode windowMode;
    S32 display = SDL_GetWindowDisplayIndex( SDL_window );
    if( display < 0 )
    {
        CL_RefPrintf( PRINT_WARNING, "Couldn't get window display index, no resolutions detected: %s\n", SDL_GetError() );
        return;
    }
    numSDLModes = SDL_GetNumDisplayModes( display );
    
    if( SDL_GetWindowDisplayMode( SDL_window, &windowMode ) < 0 || numSDLModes <= 0 )
    {
        CL_RefPrintf( PRINT_WARNING, "Couldn't get window display mode, no resolutions detected: %s\n", SDL_GetError() );
        return;
    }
    
    modes = ( SDL_Rect* )SDL_calloc( ( size_t )numSDLModes, sizeof( SDL_Rect ) );
    if( !modes )
    {
        Com_Error( ERR_FATAL, "Out of memory" );
    }
    
    for( i = 0; i < numSDLModes; i++ )
    {
        SDL_DisplayMode mode;
        
        if( SDL_GetDisplayMode( display, i, &mode ) < 0 )
            continue;
            
        if( !mode.w || !mode.h )
        {
            CL_RefPrintf( PRINT_ALL, "Display supports any resolution\n" );
            SDL_free( modes );
            return;
        }
        
        if( windowMode.format != mode.format )
            continue;
            
        // SDL can give the same resolution with different refresh rates.
        // Only list resolution once.
        for( j = 0; j < numModes; j++ )
        {
            if( mode.w == modes[ j ].w && mode.h == modes[ j ].h )
                break;
        }
        
        if( j != numModes )
            continue;
            
        modes[ numModes ].w = mode.w;
        modes[ numModes ].h = mode.h;
        numModes++;
    }
    
    if( numModes > 1 )
        qsort( modes, numModes, sizeof( SDL_Rect ), GLimp_CompareModes );
        
    for( i = 0; i < numModes; i++ )
    {
        StringEntry newModeString = va( "%ux%u ", modes[ i ].w, modes[ i ].h );
        
        if( strlen( newModeString ) < ( S32 )sizeof( buf ) - strlen( buf ) )
        {
            Q_strcat( buf, sizeof( buf ), newModeString );
        }
        else
        {
            CL_RefPrintf( PRINT_WARNING, "Skipping mode %ux%u, buffer too small\n", modes[ i ].w, modes[ i ].h );
        }
    }
    
    if( *buf )
    {
        buf[ strlen( buf ) - 1 ] = 0;
        CL_RefPrintf( PRINT_ALL, "Available modes: '%s'\n", buf );
        cvarSystem->Set( "r_availableModes", buf );
    }
    SDL_free( modes );
}

/*
===============
GLimp_GetProcAddresses

Get addresses for OpenGL functions.
===============
*/
static bool GLimp_GetProcAddresses( bool fixedFunction )
{
    bool success = true;
    StringEntry version;
    
#ifdef __SDL_NOGETPROCADDR__
#define GLE( ret, name, ... ) qgl##name = gl#name;
#else
#define GLE( ret, name, ... ) qgl##name = (name##proc *) SDL_GL_GetProcAddress("gl" #name); \
	if ( qgl##name == NULL ) { \
		CL_RefPrintf( PRINT_ALL, "ERROR: Missing OpenGL function %s\n", "gl" #name ); \
		success = false; \
	}
#endif
    
    // OpenGL 1.0 and OpenGL ES 1.0
    GLE( const U8*, GetString, U32 name )
    
    if( !qglGetString )
    {
        Com_Error( ERR_FATAL, "glGetString is NULL" );
    }
    
    version = ( StringEntry )qglGetString( GL_VERSION );
    
    if( !version )
    {
        Com_Error( ERR_FATAL, "GL_VERSION is NULL\n" );
    }
    
    if( Q_stricmpn( "OpenGL ES", version, 9 ) == 0 )
    {
        UTF8 profile[6]; // ES, ES-CM, or ES-CL
        
        sscanf( version, "OpenGL %5s %d.%d", profile, &qglesMajorVersion, &qglesMinorVersion );
        // common lite profile (no floating point) is not supported
        if( Q_stricmp( profile, "ES-CL" ) == 0 )
        {
            qglesMajorVersion = 0;
            qglesMinorVersion = 0;
        }
    }
    else
    {
        sscanf( version, "%d.%d", &qglMajorVersion, &qglMinorVersion );
    }
    
    if( fixedFunction )
    {
        if( QGL_VERSION_ATLEAST( 1, 2 ) )
        {
            QGL_1_1_PROCS;
            QGL_1_1_FIXED_FUNCTION_PROCS;
            QGL_DESKTOP_1_1_PROCS;
            QGL_DESKTOP_1_1_FIXED_FUNCTION_PROCS;
        }
        else if( qglesMajorVersion == 1 && qglesMinorVersion >= 1 )
        {
            // OpenGL ES 1.1 (2.0 is not backward compatible)
            QGL_1_1_PROCS;
            QGL_1_1_FIXED_FUNCTION_PROCS;
            QGL_ES_1_1_PROCS;
            QGL_ES_1_1_FIXED_FUNCTION_PROCS;
            QGL_1_3_PROCS;
            
            // error so this doesn't segfault due to NULL desktop GL functions being used
            Com_Error( ERR_FATAL, "Unsupported OpenGL Version: %s\n", version );
        }
        else
        {
            Com_Error( ERR_FATAL, "Unsupported OpenGL Version (%s), OpenGL 1.2 is required\n", version );
        }
    }
    else
    {
        if( QGL_VERSION_ATLEAST( 2, 0 ) )
        {
            QGL_1_1_PROCS;
            QGL_DESKTOP_1_1_PROCS;
            QGL_1_3_PROCS;
            QGL_1_5_PROCS;
            QGL_2_0_PROCS;
        }
        else if( QGLES_VERSION_ATLEAST( 2, 0 ) )
        {
            QGL_1_1_PROCS;
            QGL_ES_1_1_PROCS;
            QGL_1_3_PROCS;
            QGL_1_5_PROCS;
            QGL_2_0_PROCS;
            QGL_ARB_occlusion_query_PROCS;
            
            // error so this doesn't segfault due to NULL desktop GL functions being used
            Com_Error( ERR_FATAL, "Unsupported OpenGL Version: %s\n", version );
        }
        else
        {
            Com_Error( ERR_FATAL, "Unsupported OpenGL Version (%s), OpenGL 2.0 is required\n", version );
        }
    }
    
    if( QGL_VERSION_ATLEAST( 3, 0 ) || QGLES_VERSION_ATLEAST( 3, 0 ) )
    {
        QGL_3_0_PROCS;
    }
    
    if( QGL_VERSION_ATLEAST( 4, 0 ) || QGLES_VERSION_ATLEAST( 4, 0 ) )
    {
        QGL_4_0_PROCS;
    }
    
#undef GLE
    
    return success;
}

/*
===============
GLimp_ClearProcAddresses

Clear addresses for OpenGL functions.
===============
*/
static void GLimp_ClearProcAddresses( void )
{
#define GLE( ret, name, ... ) qgl##name = NULL;

    qglMajorVersion = 0;
    qglMinorVersion = 0;
    qglesMajorVersion = 0;
    qglesMinorVersion = 0;
    
    QGL_1_1_PROCS;
    QGL_1_1_FIXED_FUNCTION_PROCS;
    QGL_DESKTOP_1_1_PROCS;
    QGL_DESKTOP_1_1_FIXED_FUNCTION_PROCS;
    QGL_ES_1_1_PROCS;
    QGL_ES_1_1_FIXED_FUNCTION_PROCS;
    QGL_1_3_PROCS;
    QGL_1_5_PROCS;
    QGL_2_0_PROCS;
    QGL_3_0_PROCS;
    QGL_4_0_PROCS;
    QGL_ARB_occlusion_query_PROCS;
    QGL_ARB_framebuffer_object_PROCS;
    QGL_ARB_vertex_array_object_PROCS;
    QGL_EXT_direct_state_access_PROCS;
    
    qglActiveTextureARB = NULL;
    qglClientActiveTextureARB = NULL;
    qglMultiTexCoord2fARB = NULL;
    
    qglLockArraysEXT = NULL;
    qglUnlockArraysEXT = NULL;
    
#undef GLE
}

/*
===============
GLimp_SetMode
===============
*/
static S32 GLimp_SetMode( S32 mode, bool fullscreen, bool noborder, bool fixedFunction )
{
    StringEntry glstring;
    S32 perChannelColorBits;
    S32 colorBits, depthBits, stencilBits;
    S32 samples;
    S32 i = 0;
    SDL_Surface* icon = NULL;
    U32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL;
    SDL_DisplayMode desktopMode;
    S32 display = 0;
    S32 x = SDL_WINDOWPOS_UNDEFINED, y = SDL_WINDOWPOS_UNDEFINED;
    
    CL_RefPrintf( PRINT_ALL, "Initializing OpenGL display\n" );
    
    if( r_allowResize->integer )
    {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    
    // If a window exists, note its display index
    if( SDL_window != NULL )
    {
        display = SDL_GetWindowDisplayIndex( SDL_window );
        if( display < 0 )
        {
            CL_RefPrintf( PRINT_DEVELOPER, "SDL_GetWindowDisplayIndex() failed: %s\n", SDL_GetError() );
        }
    }
    
    if( display >= 0 && SDL_GetDesktopDisplayMode( display, &desktopMode ) == 0 )
    {
        displayAspect = ( F32 )desktopMode.w / ( F32 )desktopMode.h;
        
        CL_RefPrintf( PRINT_ALL, "Display aspect: %.3f\n", displayAspect );
    }
    else
    {
        ::memset( &desktopMode, 0, sizeof( SDL_DisplayMode ) );
        
        CL_RefPrintf( PRINT_ALL, "Cannot determine display aspect, assuming 1.333\n" );
    }
    
    CL_RefPrintf( PRINT_ALL, "...setting mode %d:", mode );
    
    if( mode == -2 )
    {
        // use desktop video resolution
        if( desktopMode.h > 0 )
        {
            glConfig.vidWidth = desktopMode.w;
            glConfig.vidHeight = desktopMode.h;
        }
        else
        {
            glConfig.vidWidth = 640;
            glConfig.vidHeight = 480;
            CL_RefPrintf( PRINT_ALL, "Cannot determine display resolution, assuming 640x480\n" );
        }
        
        glConfig.windowAspect = ( F32 )glConfig.vidWidth / ( F32 )glConfig.vidHeight;
    }
    else if( !R_GetModeInfo( &glConfig.vidWidth, &glConfig.vidHeight, &glConfig.windowAspect, mode ) )
    {
        CL_RefPrintf( PRINT_ALL, " invalid mode\n" );
        return RSERR_INVALID_MODE;
    }
    CL_RefPrintf( PRINT_ALL, " %d %d\n", glConfig.vidWidth, glConfig.vidHeight );
    
    // Center window
    if( r_centerWindow->integer && !fullscreen )
    {
        x = ( desktopMode.w / 2 ) - ( glConfig.vidWidth / 2 );
        y = ( desktopMode.h / 2 ) - ( glConfig.vidHeight / 2 );
    }
    
    // Destroy existing state if it exists
    if( SDL_glContext != NULL )
    {
        GLimp_ClearProcAddresses();
        SDL_GL_DeleteContext( SDL_glContext );
        SDL_glContext = NULL;
    }
    
    if( SDL_window != NULL )
    {
        SDL_GetWindowPosition( SDL_window, &x, &y );
        CL_RefPrintf( PRINT_DEVELOPER, "Existing window at %dx%d before being destroyed\n", x, y );
        SDL_DestroyWindow( SDL_window );
        SDL_window = NULL;
    }
    
    if( fullscreen )
    {
        flags |= SDL_WINDOW_FULLSCREEN;
        glConfig.isFullscreen = true;
    }
    else
    {
        if( noborder )
        {
            flags |= SDL_WINDOW_BORDERLESS;
        }
        
        glConfig.isFullscreen = false;
    }
    
    colorBits = r_colorbits->value;
    if( ( !colorBits ) || ( colorBits >= 32 ) )
    {
        colorBits = 24;
    }
    
    if( !r_depthbits->value )
    {
        depthBits = 24;
    }
    else
    {
        depthBits = r_depthbits->value;
    }
    
    stencilBits = r_stencilbits->value;
    samples = r_ext_multisample->value;
    
    for( i = 0; i < 16; i++ )
    {
        S32 testColorBits, testDepthBits, testStencilBits;
        S32 realColorBits[3];
        
        // 0 - default
        // 1 - minus colorBits
        // 2 - minus depthBits
        // 3 - minus stencil
        if( ( i % 4 ) == 0 && i )
        {
            // one pass, reduce
            switch( i / 4 )
            {
                case 2:
                    if( colorBits == 24 )
                    {
                        colorBits = 16;
                    }
                    break;
                case 1:
                    if( depthBits == 24 )
                    {
                        depthBits = 16;
                    }
                    else if( depthBits == 16 )
                    {
                        depthBits = 8;
                    }
                case 3:
                    if( stencilBits == 24 )
                    {
                        stencilBits = 16;
                    }
                    else if( stencilBits == 16 )
                    {
                        stencilBits = 8;
                    }
            }
        }
        
        testColorBits = colorBits;
        testDepthBits = depthBits;
        testStencilBits = stencilBits;
        
        if( ( i % 4 ) == 3 )
        {
            // reduce colorBits
            if( testColorBits == 24 )
            {
                testColorBits = 16;
            }
        }
        
        if( ( i % 4 ) == 2 )
        {
            // reduce depthBits
            if( testDepthBits == 24 )
            {
                testDepthBits = 16;
            }
            else if( testDepthBits == 16 )
            {
                testDepthBits = 8;
            }
        }
        
        if( ( i % 4 ) == 1 )
        {
            // reduce stencilBits
            if( testStencilBits == 24 )
            {
                testStencilBits = 16;
            }
            else if( testStencilBits == 16 )
            {
                testStencilBits = 8;
            }
            else
            {
                testStencilBits = 0;
            }
        }
        
        if( testColorBits == 24 )
        {
            perChannelColorBits = 8;
        }
        else
        {
            perChannelColorBits = 4;
        }
        
        SDL_GL_SetAttribute( SDL_GL_RED_SIZE, perChannelColorBits );
        SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, perChannelColorBits );
        SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, perChannelColorBits );
        SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, testDepthBits );
        SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, testStencilBits );
        
        SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, samples ? 1 : 0 );
        SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, samples );
        
        if( r_stereoEnabled->integer )
        {
            glConfig.stereoEnabled = true;
            SDL_GL_SetAttribute( SDL_GL_STEREO, 1 );
        }
        else
        {
            glConfig.stereoEnabled = false;
            SDL_GL_SetAttribute( SDL_GL_STEREO, 0 );
        }
        
        SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
        
        
        if( ( SDL_window = SDL_CreateWindow( CLIENT_WINDOW_TITLE, x, y, glConfig.vidWidth, glConfig.vidHeight, flags ) ) == NULL )
        {
            CL_RefPrintf( PRINT_DEVELOPER, "SDL_CreateWindow failed: %s\n", SDL_GetError() );
            continue;
        }
        
        if( fullscreen )
        {
            SDL_DisplayMode mode;
            
            switch( testColorBits )
            {
                case 16:
                    mode.format = SDL_PIXELFORMAT_RGB565;
                    break;
                case 24:
                    mode.format = SDL_PIXELFORMAT_RGB24;
                    break;
                default:
                    CL_RefPrintf( PRINT_DEVELOPER, "testColorBits is %d, can't fullscreen\n", testColorBits );
                    continue;
            }
            
            mode.w = glConfig.vidWidth;
            mode.h = glConfig.vidHeight;
            mode.refresh_rate = glConfig.displayFrequency = cvarSystem->VariableIntegerValue( "r_displayRefresh" );
            mode.driverdata = NULL;
            
            if( SDL_SetWindowDisplayMode( SDL_window, &mode ) < 0 )
            {
                CL_RefPrintf( PRINT_DEVELOPER, "SDL_SetWindowDisplayMode failed: %s\n", SDL_GetError() );
                continue;
            }
        }
        
        SDL_SetWindowIcon( SDL_window, icon );
        
        if( !fixedFunction )
        {
            S32 profileMask, majorVersion, minorVersion;
            
            SDL_GL_GetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, &profileMask );
            SDL_GL_GetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, &majorVersion );
            SDL_GL_GetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, &minorVersion );
            
            CL_RefPrintf( PRINT_ALL, "Trying to get an OpenGL 4.0 core context\n" );
            
            SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
            SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 4 );
            SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );
            
            if( ( SDL_glContext = SDL_GL_CreateContext( SDL_window ) ) == NULL )
            {
                CL_RefPrintf( PRINT_ALL, "SDL_GL_CreateContext failed: %s\n", SDL_GetError() );
                CL_RefPrintf( PRINT_ALL, "Reverting to default context\n" );
                
                SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, profileMask );
                SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, majorVersion );
                SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, minorVersion );
            }
            else
            {
                StringEntry renderer;
                
                CL_RefPrintf( PRINT_ALL, "SDL_GL_CreateContext succeeded.\n" );
                
                if( GLimp_GetProcAddresses( fixedFunction ) )
                {
                    renderer = ( StringEntry )qglGetString( GL_RENDERER );
                }
                else
                {
                    CL_RefPrintf( PRINT_ALL, "GLimp_GetProcAdvedresses() failed for OpenGL 3.2 core context\n" );
                    renderer = NULL;
                }
                
                if( !renderer || ( strstr( renderer, "Software Renderer" ) || strstr( renderer, "Software Rasterizer" ) ) )
                {
                    if( renderer )
                    {
                        CL_RefPrintf( PRINT_ALL, "GL_RENDERER is %s, rejecting context\n", renderer );
                    }
                    
                    GLimp_ClearProcAddresses();
                    SDL_GL_DeleteContext( SDL_glContext );
                    SDL_glContext = NULL;
                    
                    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, profileMask );
                    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, majorVersion );
                    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, minorVersion );
                }
            }
        }
        else
        {
            SDL_glContext = NULL;
        }
        
        if( !SDL_glContext )
        {
            if( ( SDL_glContext = SDL_GL_CreateContext( SDL_window ) ) == NULL )
            {
                CL_RefPrintf( PRINT_DEVELOPER, "SDL_GL_CreateContext failed: %s\n", SDL_GetError() );
                SDL_DestroyWindow( SDL_window );
                SDL_window = NULL;
                continue;
            }
            
            if( !GLimp_GetProcAddresses( fixedFunction ) )
            {
                CL_RefPrintf( PRINT_ALL, "GLimp_GetProcAddresses() failed\n" );
                GLimp_ClearProcAddresses();
                SDL_GL_DeleteContext( SDL_glContext );
                SDL_glContext = NULL;
                SDL_DestroyWindow( SDL_window );
                SDL_window = NULL;
                continue;
            }
        }
        
        qglClearColor( 0, 0, 0, 1 );
        qglClear( GL_COLOR_BUFFER_BIT );
        SDL_GL_SwapWindow( SDL_window );
        
        if( SDL_GL_SetSwapInterval( r_swapInterval->integer ) == -1 )
        {
            CL_RefPrintf( PRINT_DEVELOPER, "SDL_GL_SetSwapInterval failed: %s\n", SDL_GetError() );
        }
        
        SDL_GL_GetAttribute( SDL_GL_RED_SIZE, &realColorBits[0] );
        SDL_GL_GetAttribute( SDL_GL_GREEN_SIZE, &realColorBits[1] );
        SDL_GL_GetAttribute( SDL_GL_BLUE_SIZE, &realColorBits[2] );
        SDL_GL_GetAttribute( SDL_GL_DEPTH_SIZE, &glConfig.depthBits );
        SDL_GL_GetAttribute( SDL_GL_STENCIL_SIZE, &glConfig.stencilBits );
        
        glConfig.colorBits = realColorBits[0] + realColorBits[1] + realColorBits[2];
        
        CL_RefPrintf( PRINT_ALL, "Using %d color bits, %d depth, %d stencil display.\n", glConfig.colorBits, glConfig.depthBits, glConfig.stencilBits );
        break;
    }
    
    SDL_FreeSurface( icon );
    
    if( !SDL_window )
    {
        CL_RefPrintf( PRINT_ALL, "Couldn't get a visual\n" );
        return RSERR_INVALID_MODE;
    }
    
    GLimp_DetectAvailableModes();
    
    glstring = ( UTF8* )qglGetString( GL_RENDERER );
    CL_RefPrintf( PRINT_ALL, "GL_RENDERER: %s\n", glstring );
    
    return RSERR_OK;
}

/*
===============
GLimp_StartDriverAndSetMode
===============
*/
static bool GLimp_StartDriverAndSetMode( S32 mode, bool fullscreen, bool noborder, bool gl3Core )
{
    rserr_t err;
    
    if( !SDL_WasInit( SDL_INIT_VIDEO ) )
    {
        StringEntry driverName;
        
        if( SDL_Init( SDL_INIT_VIDEO ) == -1 )
        {
            CL_RefPrintf( PRINT_ALL, "SDL_Init( SDL_INIT_VIDEO ) FAILED (%s)\n", SDL_GetError() );
            return false;
        }
        
        driverName = SDL_GetCurrentVideoDriver();
        CL_RefPrintf( PRINT_ALL, "SDL using driver \"%s\"\n", driverName );
        cvarSystem->Set( "r_sdlDriver", driverName );
    }
    
    if( fullscreen && cvarSystem->VariableIntegerValue( "in_nograb" ) )
    {
        CL_RefPrintf( PRINT_ALL, "Fullscreen not allowed with in_nograb 1\n" );
        cvarSystem->Set( "r_fullscreen", "0" );
        r_fullscreen->modified = false;
        fullscreen = false;
    }
    
    err = ( rserr_t )GLimp_SetMode( mode, fullscreen, noborder, gl3Core );
    
    switch( err )
    {
        case RSERR_INVALID_FULLSCREEN:
            CL_RefPrintf( PRINT_ALL, "...WARNING: fullscreen unavailable in this mode\n" );
            return false;
            
        case RSERR_INVALID_MODE:
            CL_RefPrintf( PRINT_ALL, "...WARNING: could not set the given mode (%d)\n", mode );
            return false;
            
        default:
            break;
    }
    
    return true;
}

/*
===============
GLimp_HaveExtension
===============
*/
static bool GLimp_HaveExtension( StringEntry ext )
{
    StringEntry ptr = Q_stristr( glConfig.extensions_string, ext );
    
    if( ptr == NULL )
    {
        return false;
    }
    
    ptr += strlen( ext );
    return ( ( *ptr == ' ' ) || ( *ptr == '\0' ) ); // verify it's complete string.
}

/*
===============
GLimp_InitExtensions
===============
*/
static void GLimp_InitExtensions( void )
{
    if( !r_allowExtensions->integer )
    {
        CL_RefPrintf( PRINT_ALL, "* IGNORING OPENGL EXTENSIONS *\n" );
        return;
    }
    
    CL_RefPrintf( PRINT_ALL, "Initializing OpenGL extensions\n" );
    
    glConfig.textureCompression = TC_NONE;
    
    // GL_EXT_texture_compression_s3tc
    if( GLimp_HaveExtension( "GL_ARB_texture_compression" ) && GLimp_HaveExtension( "GL_EXT_texture_compression_s3tc" ) )
    {
        if( r_ext_compressed_textures->value )
        {
            glConfig.textureCompression = TC_S3TC_ARB;
            CL_RefPrintf( PRINT_ALL, "...using GL_EXT_texture_compression_s3tc\n" );
        }
        else
        {
            CL_RefPrintf( PRINT_ALL, "...ignoring GL_EXT_texture_compression_s3tc\n" );
        }
    }
    else
    {
        CL_RefPrintf( PRINT_ALL, "...GL_EXT_texture_compression_s3tc not found\n" );
    }
    
    // GL_S3_s3tc ... legacy extension before GL_EXT_texture_compression_s3tc.
    if( glConfig.textureCompression == TC_NONE )
    {
        if( GLimp_HaveExtension( "GL_S3_s3tc" ) )
        {
            if( r_ext_compressed_textures->value )
            {
                glConfig.textureCompression = TC_S3TC;
                CL_RefPrintf( PRINT_ALL, "...using GL_S3_s3tc\n" );
            }
            else
            {
                CL_RefPrintf( PRINT_ALL, "...ignoring GL_S3_s3tc\n" );
            }
        }
        else
        {
            CL_RefPrintf( PRINT_ALL, "...GL_S3_s3tc not found\n" );
        }
    }
    
    // GL_EXT_texture_env_add
    glConfig.textureEnvAddAvailable = false;
    if( GLimp_HaveExtension( "EXT_texture_env_add" ) )
    {
        if( r_ext_texture_env_add->integer )
        {
            glConfig.textureEnvAddAvailable = true;
            CL_RefPrintf( PRINT_ALL, "...using GL_EXT_texture_env_add\n" );
        }
        else
        {
            glConfig.textureEnvAddAvailable = false;
            CL_RefPrintf( PRINT_ALL, "...ignoring GL_EXT_texture_env_add\n" );
        }
    }
    else
    {
        CL_RefPrintf( PRINT_ALL, "...GL_EXT_texture_env_add not found\n" );
    }
    
    // GL_ARB_multitexture
    qglMultiTexCoord2fARB = NULL;
    qglActiveTextureARB = NULL;
    qglClientActiveTextureARB = NULL;
    if( GLimp_HaveExtension( "GL_ARB_multitexture" ) )
    {
        if( r_ext_multitexture->value )
        {
            qglMultiTexCoord2fARB = ( PFNGLMULTITEXCOORD2FARBPROC )SDL_GL_GetProcAddress( "glMultiTexCoord2fARB" );
            qglActiveTextureARB = ( PFNGLACTIVETEXTUREARBPROC )SDL_GL_GetProcAddress( "glActiveTextureARB" );
            qglClientActiveTextureARB = ( PFNGLCLIENTACTIVETEXTUREARBPROC )SDL_GL_GetProcAddress( "glClientActiveTextureARB" );
            
            if( qglActiveTextureARB )
            {
                S32 glint = 0;
                qglGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB, &glint );
                glConfig.numTextureUnits = ( S32 )glint;
                
                if( glConfig.numTextureUnits > 1 )
                {
                    CL_RefPrintf( PRINT_ALL, "...using GL_ARB_multitexture\n" );
                }
                else
                {
                    qglMultiTexCoord2fARB = NULL;
                    qglActiveTextureARB = NULL;
                    qglClientActiveTextureARB = NULL;
                    CL_RefPrintf( PRINT_ALL, "...not using GL_ARB_multitexture, < 2 texture units\n" );
                }
            }
        }
        else
        {
            CL_RefPrintf( PRINT_ALL, "...ignoring GL_ARB_multitexture\n" );
        }
    }
    else
    {
        CL_RefPrintf( PRINT_ALL, "...GL_ARB_multitexture not found\n" );
    }
    
    
    // GL_EXT_compiled_vertex_array
    if( GLimp_HaveExtension( "GL_EXT_compiled_vertex_array" ) )
    {
        if( r_ext_compiled_vertex_array->value )
        {
            CL_RefPrintf( PRINT_ALL, "...using GL_EXT_compiled_vertex_array\n" );
            qglLockArraysEXT = ( void ( APIENTRY* )( S32, S32 ) ) SDL_GL_GetProcAddress( "glLockArraysEXT" );
            qglUnlockArraysEXT = ( void ( APIENTRY* )( void ) ) SDL_GL_GetProcAddress( "glUnlockArraysEXT" );
            if( !qglLockArraysEXT || !qglUnlockArraysEXT )
            {
                Com_Error( ERR_FATAL, "bad getprocaddress" );
            }
        }
        else
        {
            CL_RefPrintf( PRINT_ALL, "...ignoring GL_EXT_compiled_vertex_array\n" );
        }
    }
    else
    {
        CL_RefPrintf( PRINT_ALL, "...GL_EXT_compiled_vertex_array not found\n" );
    }
    
    glConfig.textureFilterAnisotropic = false;
    if( GLimp_HaveExtension( "GL_EXT_texture_filter_anisotropic" ) )
    {
        if( r_ext_texture_filter_anisotropic->integer )
        {
            qglGetIntegerv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, ( S32* )&glConfig.maxAnisotropy );
            if( glConfig.maxAnisotropy <= 0 )
            {
                CL_RefPrintf( PRINT_ALL, "...GL_EXT_texture_filter_anisotropic not properly supported!\n" );
                glConfig.maxAnisotropy = 0;
            }
            else
            {
                CL_RefPrintf( PRINT_ALL, "...using GL_EXT_texture_filter_anisotropic (max: %i)\n", glConfig.maxAnisotropy );
                glConfig.textureFilterAnisotropic = true;
            }
        }
        else
        {
            CL_RefPrintf( PRINT_ALL, "...ignoring GL_EXT_texture_filter_anisotropic\n" );
        }
    }
    else
    {
        CL_RefPrintf( PRINT_ALL, "...GL_EXT_texture_filter_anisotropic not found\n" );
    }
}

#define R_MODE_FALLBACK 3 // 640 * 480

/*
===============
GLimp_Init

This routine is responsible for initializing the OS specific portions
of OpenGL
===============
*/
void GLimp_Init( bool fixedFunction )
{
    CL_RefPrintf( PRINT_DEVELOPER, "Glimp_Init( )\n" );
    
    r_allowSoftwareGL = cvarSystem->Get( "r_allowSoftwareGL", "0", CVAR_LATCH );
    r_sdlDriver = cvarSystem->Get( "r_sdlDriver", "", CVAR_ROM );
    r_allowResize = cvarSystem->Get( "r_allowResize", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_centerWindow = cvarSystem->Get( "r_centerWindow", "0", CVAR_ARCHIVE | CVAR_LATCH );
    
    if( cvarSystem->VariableIntegerValue( "com_abnormalExit" ) )
    {
        cvarSystem->Set( "r_mode", va( "%d", R_MODE_FALLBACK ) );
        cvarSystem->Set( "r_fullscreen", "0" );
        cvarSystem->Set( "r_centerWindow", "0" );
        cvarSystem->Set( "com_abnormalExit", "0" );
    }
    
    Sys_GLimpInit();
    
    // Create the window and set up the context
    if( GLimp_StartDriverAndSetMode( r_mode->integer, r_fullscreen->integer, r_noborder->integer, fixedFunction ) )
    {
        goto success;
    }
    
    // Try again, this time in a platform specific "safe mode"
    Sys_GLimpSafeInit();
    
    if( GLimp_StartDriverAndSetMode( r_mode->integer, r_fullscreen->integer, false, fixedFunction ) )
    {
        goto success;
    }
    
    // Finally, try the default screen resolution
    if( r_mode->integer != R_MODE_FALLBACK )
    {
        CL_RefPrintf( PRINT_ALL, "Setting r_mode %d failed, falling back on r_mode %d\n", r_mode->integer, R_MODE_FALLBACK );
        
        if( GLimp_StartDriverAndSetMode( R_MODE_FALLBACK, false, false, fixedFunction ) )
        {
            goto success;
        }
    }
    
    // Nothing worked, give up
    Com_Error( ERR_FATAL, "GLimp_Init() - could not load OpenGL subsystem" );
    
success:
    // These values force the UI to disable driver selection
    glConfig.driverType = GLDRV_ICD;
    glConfig.hardwareType = GLHW_GENERIC;
    
    // Only using SDL_SetWindowBrightness to determine if hardware gamma is supported
    glConfig.deviceSupportsGamma = !r_ignorehwgamma->integer && SDL_SetWindowBrightness( SDL_window, 1.0f ) >= 0;
    
    // get our config strings
    Q_strncpyz( glConfig.vendor_string, ( UTF8* ) qglGetString( GL_VENDOR ), sizeof( glConfig.vendor_string ) );
    Q_strncpyz( glConfig.renderer_string, ( UTF8* ) qglGetString( GL_RENDERER ), sizeof( glConfig.renderer_string ) );
    if( *glConfig.renderer_string && glConfig.renderer_string[strlen( glConfig.renderer_string ) - 1] == '\n' )
        glConfig.renderer_string[strlen( glConfig.renderer_string ) - 1] = 0;
    Q_strncpyz( glConfig.version_string, ( UTF8* ) qglGetString( GL_VERSION ), sizeof( glConfig.version_string ) );
    
    // manually create extension list if using OpenGL 4
    if( qglGetStringi )
    {
        S32 i, numExtensions, extensionLength, listLength;
        StringEntry extension;
        
        qglGetIntegerv( GL_NUM_EXTENSIONS, &numExtensions );
        listLength = 0;
        
        for( i = 0; i < numExtensions; i++ )
        {
            extension = ( UTF8* ) qglGetStringi( GL_EXTENSIONS, i );
            extensionLength = strlen( extension );
            
            if( ( listLength + extensionLength + 1 ) >= sizeof( glConfig.extensions_string ) )
                break;
                
            if( i > 0 )
            {
                Q_strcat( glConfig.extensions_string, sizeof( glConfig.extensions_string ), " " );
                listLength++;
            }
            
            Q_strcat( glConfig.extensions_string, sizeof( glConfig.extensions_string ), extension );
            listLength += extensionLength;
        }
    }
    else
    {
        Q_strncpyz( glConfig.extensions_string, ( UTF8* ) qglGetString( GL_EXTENSIONS ), sizeof( glConfig.extensions_string ) );
    }
    
    // initialize extensions
    GLimp_InitExtensions();
    
    cvarSystem->Get( "r_availableModes", "", CVAR_ROM );
    
    // This depends on SDL_INIT_VIDEO, hence having it here
    IN_Init( SDL_window );
}

/*
===============
GLimp_EndFrame

Responsible for doing a swapbuffers
===============
*/
void GLimp_EndFrame( void )
{
    // don't flip if drawing to front buffer
    if( Q_stricmp( r_drawBuffer->string, "GL_FRONT" ) != 0 )
    {
        SDL_GL_SwapWindow( SDL_window );
    }
    
    if( r_fullscreen->modified )
    {
        S32 fullscreen;
        bool needToToggle;
        bool sdlToggled = false;
        
        // Find out the current state
        fullscreen = !!( SDL_GetWindowFlags( SDL_window ) & SDL_WINDOW_FULLSCREEN );
        
        if( r_fullscreen->integer && cvarSystem->VariableIntegerValue( "in_nograb" ) )
        {
            CL_RefPrintf( PRINT_ALL, "Fullscreen not allowed with in_nograb 1\n" );
            cvarSystem->Set( "r_fullscreen", "0" );
            r_fullscreen->modified = false;
        }
        
        // Is the state we want different from the current state?
        needToToggle = !!r_fullscreen->integer != fullscreen;
        
        if( needToToggle )
        {
            sdlToggled = SDL_SetWindowFullscreen( SDL_window, r_fullscreen->integer ) >= 0;
            
            // SDL_WM_ToggleFullScreen didn't work, so do it the slow way
            if( !sdlToggled )
            {
                Cbuf_ExecuteText( EXEC_APPEND, "vid_restart\n" );
            }
            
            IN_Restart();
        }
        
        r_fullscreen->modified = false;
    }
}
