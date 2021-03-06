////////////////////////////////////////////////////////////////////////////////////////
// Copyright(C) 1999 - 2005 Id Software, Inc.
// Copyright(C) 2000 - 2013 Darklegion Development
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
// File name:   r_init.cpp
// Version:     v1.01
// Created:
// Compilers:   Visual Studio 2017, gcc 7.3.0
// Description: functions that are not called every frame
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#include <OWLib/precompiled.h>

glconfig_t glConfig;
glRefConfig_t glRefConfig;
bool textureFilterAnisotropic = false;
S32 maxAnisotropy = 0;
F32 displayAspect = 0.0f;

glstate_t glState;

static void GfxInfo_f( void );
static void GfxMemInfo_f( void );

cvar_t* r_glCoreProfile;
cvar_t* r_glMajorVersion;
cvar_t* r_glMinorVersion;
cvar_t* r_glDebugProfile;
cvar_t*	r_flareSize;
cvar_t*	r_flareFade;
cvar_t*	r_flareCoeff;

cvar_t*	r_railWidth;
cvar_t*	r_railCoreWidth;
cvar_t*	r_railSegmentLength;

cvar_t*	r_verbose;
cvar_t*	r_ignore;

cvar_t* r_displayRefresh;
cvar_t*	r_detailTextures;

cvar_t*	r_znear;
cvar_t*	r_zproj;
cvar_t*	r_stereoSeparation;

cvar_t*	r_skipBackEnd;

cvar_t*	r_stereoEnabled;
cvar_t*	r_anaglyphMode;

cvar_t*	r_greyscale;

cvar_t*	r_ignorehwgamma;
cvar_t*	r_measureOverdraw;

cvar_t*	r_inGameVideo;
cvar_t*	r_fastsky;
cvar_t*	r_drawSun;
cvar_t*	r_dlightBacks;

cvar_t*	r_lodbias;
cvar_t*	r_lodscale;

cvar_t*	r_norefresh;
cvar_t*	r_drawentities;
cvar_t*	r_drawworld;
cvar_t*	r_speeds;
cvar_t*	r_fullbright;
cvar_t*	r_novis;
cvar_t*	r_nocull;
cvar_t*	r_facePlaneCull;
cvar_t*	r_showcluster;
cvar_t*	r_nocurves;

cvar_t*	r_allowExtensions;

cvar_t*	r_ext_compressed_textures;
cvar_t*	r_ext_multitexture;
cvar_t*	r_ext_compiled_vertex_array;
cvar_t*	r_ext_texture_env_add;
cvar_t*	r_ext_texture_filter_anisotropic;
cvar_t*	r_ext_max_anisotropy;

cvar_t* r_ext_framebuffer_object;
cvar_t* r_ext_framebuffer_blit;
cvar_t* r_ext_texture_float;
cvar_t* r_ext_framebuffer_multisample;
cvar_t* r_arb_seamless_cube_map;
cvar_t* r_arb_vertex_array_object;
cvar_t* r_ext_direct_state_access;

cvar_t* r_cameraExposure;

cvar_t* r_floatLightmap;
cvar_t* r_postProcess;

cvar_t* r_toneMap;
cvar_t* r_forceToneMap;
cvar_t* r_forceToneMapMin;
cvar_t* r_forceToneMapAvg;
cvar_t* r_forceToneMapMax;

cvar_t* r_autoExposure;
cvar_t* r_forceAutoExposure;
cvar_t* r_forceAutoExposureMin;
cvar_t* r_forceAutoExposureMax;

cvar_t* r_depthPrepass;
cvar_t* r_ssao;

cvar_t* r_normalMapping;
cvar_t* r_specularMapping;
cvar_t* r_deluxeMapping;
cvar_t* r_parallaxMapping;
cvar_t* r_cubeMapping;
cvar_t* r_horizonFade;
cvar_t* r_deluxeSpecular;
cvar_t* r_pbr;
cvar_t* r_baseNormalX;
cvar_t* r_baseNormalY;
cvar_t* r_baseParallax;
cvar_t* r_baseSpecular;
cvar_t* r_baseGloss;
cvar_t* r_glossType;
cvar_t* r_mergeLightmaps;
cvar_t* r_dlightMode;
cvar_t* r_pshadowDist;
cvar_t* r_imageUpsample;
cvar_t* r_imageUpsampleMaxSize;
cvar_t* r_imageUpsampleType;
cvar_t* r_genNormalMaps;
cvar_t* r_forceSun;
cvar_t* r_forceSunLightScale;
cvar_t* r_forceSunAmbientScale;
cvar_t* r_proceduralSun;
cvar_t* r_proceduralSunScale;
cvar_t* r_sunlightMode;
cvar_t* r_sunlightSpecular;
cvar_t* r_drawSunRays;
cvar_t* r_sunShadows;
cvar_t* r_shadowFilter;
cvar_t* r_shadowBlur;
cvar_t* r_shadowMapSize;
cvar_t* r_shadowCascadeZNear;
cvar_t* r_shadowCascadeZFar;
cvar_t* r_shadowCascadeZBias;
cvar_t* r_ignoreDstAlpha;
cvar_t* r_floatfix;
cvar_t*	r_ignoreGLErrors;
cvar_t*	r_logFile;

cvar_t*	r_stencilbits;
cvar_t*	r_depthbits;
cvar_t*	r_colorbits;
cvar_t*	r_alphabits;
cvar_t*	r_texturebits;
cvar_t* r_ext_multisample;

cvar_t*	r_drawBuffer;
cvar_t*	r_lightmap;
cvar_t*	r_vertexLight;
cvar_t*	r_uiFullScreen;
cvar_t*	r_shadows;
cvar_t*	r_flares;
cvar_t*	r_mode;
cvar_t*	r_nobind;
cvar_t*	r_singleShader;
cvar_t*	r_roundImagesDown;
cvar_t*	r_colorMipLevels;
cvar_t*	r_picmip;
cvar_t*	r_showtris;
cvar_t*	r_showsky;
cvar_t*	r_shownormals;
cvar_t*	r_finish;
cvar_t*	r_clear;
cvar_t*	r_swapInterval;
cvar_t*	r_textureMode;
cvar_t*	r_offsetFactor;
cvar_t*	r_offsetUnits;
cvar_t*	r_intensity;
cvar_t*	r_lockpvs;
cvar_t*	r_noportals;
cvar_t*	r_portalOnly;

cvar_t*	r_subdivisions;
cvar_t*	r_lodCurveError;

cvar_t*	r_fullscreen;
cvar_t*  r_noborder;

cvar_t*	r_width;
cvar_t*	r_height;
cvar_t*	r_pixelAspect;

cvar_t*	r_overBrightBits;
cvar_t*	r_mapOverBrightBits;

cvar_t*	r_debugSurface;
cvar_t*	r_simpleMipMaps;

cvar_t*	r_showImages;

cvar_t*	r_ambientScale;
cvar_t*	r_directedScale;
cvar_t*	r_debugLight;
cvar_t*	r_debugSort;
cvar_t*	r_printShaders;
cvar_t*	r_saveFontData;

cvar_t*	r_marksOnTriangleMeshes;

cvar_t*	r_aviMotionJpegQuality;
cvar_t*	r_screenshotJpegQuality;
cvar_t* r_testvar;
cvar_t*	r_lensflare;
cvar_t* r_bloom;
cvar_t* r_bloomPasses;
cvar_t* r_bloomDarkenPower;
cvar_t* r_bloomScale;
cvar_t* r_volumelight;
cvar_t* r_volumeLightStrength;
cvar_t*	r_anamorphic;
cvar_t* r_anamorphicStrength;
cvar_t* r_bloomRaysDecay;
cvar_t* r_bloomRaysWeight;
cvar_t* r_bloomRaysDensity;
cvar_t* r_ssgi;
cvar_t* r_ssgiWidth;
cvar_t* r_ssgiSamples;
cvar_t*	r_darkexpand;
cvar_t* r_truehdr;
cvar_t* r_dof;
cvar_t* r_esharpening;
cvar_t* r_esharpening2;
cvar_t* r_multipost;
cvar_t* r_textureClean;
cvar_t* r_textureCleanSigma;
cvar_t* r_textureCleanBSigma;
cvar_t* r_textureCleanMSize;
cvar_t* r_imageBasedLighting;
cvar_t* r_sss;
cvar_t* r_trueAnaglyph;
cvar_t* r_trueAnaglyphSeparation;
cvar_t* r_trueAnaglyphRed;
cvar_t* r_trueAnaglyphGreen;
cvar_t* r_trueAnaglyphBlue;
cvar_t* r_vibrancy;
cvar_t* r_tesselation;
cvar_t* r_tesselationLevel;
cvar_t* r_tesselationAlpha;
cvar_t* r_fxaa;
cvar_t* r_texturedetail;
cvar_t* r_texturedetailStrength;
cvar_t* r_rbm;
cvar_t* r_rbmStrength;
cvar_t* r_screenblur;
cvar_t* r_brightness;
cvar_t* r_contrast;
cvar_t* r_gamma;
cvar_t* r_hbao;
cvar_t* r_compressTextures;
cvar_t* r_cubemapCullRange;
cvar_t* r_cubemapCullFalloffMult;
cvar_t* r_blinnPhong;
cvar_t* r_ao;
cvar_t* r_env;
cvar_t* r_deferredLighting;
cvar_t* r_ssr;
cvar_t* r_ssrStrength;
cvar_t* r_sse;
cvar_t* r_sseStrength;
cvar_t*  r_testshader;
cvar_t*  r_testshaderValue1;
cvar_t*  r_testshaderValue2;
cvar_t*  r_testshaderValue3;
cvar_t*  r_testshaderValue4;
cvar_t*  r_testshaderValue5;
cvar_t*  r_testshaderValue6;
cvar_t*  r_testshaderValue7;
cvar_t*  r_testshaderValue8;
cvar_t*  r_testshaderValue9;
cvar_t*  r_ssdo;
cvar_t*  r_ssdoBaseRadius;
cvar_t*  r_ssdoMaxOcclusionDist;
cvar_t*	r_showdepth;
cvar_t*	r_shadowMaxDepthError;
cvar_t* r_testvalue0;
cvar_t* r_testvalue1;
cvar_t*	r_shadowSolidityValue;
cvar_t* r_shadowBlurPasses;

cvar_t*	r_maxpolys;
S32		max_polys;
cvar_t*	r_maxpolyverts;
S32		max_polyverts;

/*
** InitOpenGL
**
** This function is responsible for initializing a valid OpenGL subsystem.  This
** is done by calling GLimp_Init (which gives us a working OGL subsystem) then
** setting variables, checking GL constants, and reporting the gfx system config
** to the user.
*/
static void InitOpenGL( void )
{
    UTF8 renderer_buffer[1024];
    
    //
    // initialize OS specific portions of the renderer
    //
    // GLimp_Init directly or indirectly references the following cvars:
    //		- r_fullscreen
    //		- r_mode
    //		- r_(color|depth|stencil)bits
    //		- r_ignorehwgamma
    //		- r_gamma
    //
    
    if( glConfig.vidWidth == 0 )
    {
        S32		temp;
        
        GLimp_Init( false );
        GLimp_InitExtraExtensions();
        
        strcpy( renderer_buffer, glConfig.renderer_string );
        Q_strlwr( renderer_buffer );
        
        // OpenGL driver constants
        glGetIntegerv( GL_MAX_TEXTURE_SIZE, &temp );
        glConfig.maxTextureSize = temp;
        
        // stubbed or broken drivers may have reported 0...
        if( glConfig.maxTextureSize <= 0 )
        {
            glConfig.maxTextureSize = 0;
        }
    }
    
    // set default state
    GL_SetDefaultState();
}

/*
==================
GL_CheckErrors
==================
*/
void GL_CheckErrs( StringEntry file, S32 line )
{
    S32		err;
    UTF8	s[64];
    
    err = glGetError();
    if( err == GL_NO_ERROR )
    {
        return;
    }
    if( r_ignoreGLErrors->integer )
    {
        return;
    }
    switch( err )
    {
        case GL_INVALID_ENUM:
            strcpy( s, "GL_INVALID_ENUM" );
            break;
        case GL_INVALID_VALUE:
            strcpy( s, "GL_INVALID_VALUE" );
            break;
        case GL_INVALID_OPERATION:
            strcpy( s, "GL_INVALID_OPERATION" );
            break;
        case GL_STACK_OVERFLOW:
            strcpy( s, "GL_STACK_OVERFLOW" );
            break;
        case GL_STACK_UNDERFLOW:
            strcpy( s, "GL_STACK_UNDERFLOW" );
            break;
        case GL_OUT_OF_MEMORY:
            strcpy( s, "GL_OUT_OF_MEMORY" );
            break;
        default:
            Com_sprintf( s, sizeof( s ), "%i", err );
            break;
    }
    
    Com_Error( ERR_FATAL, "GL_CheckErrors: %s in %s at line %d", s, file, line );
}

/*
** R_GetModeInfo
*/
typedef struct vidmode_s
{
    StringEntry description;
    S32        width, height;
    F32      pixelAspect; // pixel width / height
} vidmode_t;

//Dushan
static const vidmode_t r_vidModes[] =
{
    { " 320x240  (4:3)",   320,  240, 1 },
    { " 320x200  (16:10)", 320,  200, 1 },
    { " 640x480  (4:3)",   640,  480, 1 },
    { " 640x400  (16:10)", 640,  400, 1 },
    { " 720x480  (3:2)",   720,  480, 1 },
    { " 800x480  (5:3)",   800,  480, 1 },
    { " 800x600  (4:3)",   800,  600, 1 },
    { " 852x480  (16:9)",  853,  480, 1 },
    { "1024x768  (4:3)",   1024, 768, 1 },
    { "1152x768  (3:2)",   1152, 758, 1 },
    { "1152x864  (4:3)",   1152, 864, 1 },
    { "1280x720  (16:9)",  1280, 720, 1 },
    { "1280x768  (5:4)",   1280, 768, 1 },
    { "1280x800  (16:10)", 1280, 800, 1 },
    { "1280x854  (3:2)",   1280, 854, 1 },
    { "1280x1024 (5:4) ",  1280, 1024, 1 },
    { "1366x768  (16:9)",  1366, 768, 1 },
    { "1440x900  (16:10)", 1440, 900, 1 },
    { "1440x960  (3:2)",   1440, 960, 1 },
    { "1600x900  (16:9)",  1600, 900, 1 },
    { "1680x1050 (16:10)", 1680, 1050, 1 },
    { "1600x1200 (4:3)",   1600, 1200, 1 },
    { "1920x1080 (16:9)",  1920, 1080, 1 },
    { "1920x1200 (16:10)", 1920, 1200, 1 },
    { "2048x1080 (17:9)",  2048, 1080, 1 },
    { "2048x1536 (4:3)",   2048, 1536, 1 },
    { "2560x1080 (21:9)",  2560, 1080, 1 },
    { "2560x1600 (16:10)", 2560, 1600, 1 },
    { "2560x2048 (5:4)",   2560, 2048, 1 },
    { "2880x1920 (3:2)",   2880, 1920, 1 },
    { "3440x1440 (21:9)",  3440, 1440, 1 },
    { "3840x2400 (16:10)", 3840, 2400, 1 },
    { "5120x4096 (5:4)",   5120, 4096, 1 },
    { "7680x4800 (16:10)", 7680, 4800, 1 },
};
static const S32 s_numVidModes = ARRAY_LEN( r_vidModes );

bool R_GetModeInfo( S32* width, S32* height, F32* windowAspect, S32 mode )
{
    const vidmode_t* vm;
    
    if( mode < -2 )
    {
        return false;
    }
    
    if( mode >= s_numVidModes )
    {
        return false;
    }
    
    if( mode == -2 )
    {
        // Must set width and height to display size before calling this function!
        *windowAspect = ( F32 ) * width / *height;
    }
    else if( mode == -1 )
    {
        //*width = r_customWidth->integer;
        //*height = r_customheight->integer;
        //*windowAspect = r_customaspect->value;
    }
    else
    {
        vm = &r_vidModes[ mode ];
        
        *width = vm->width;
        *height = vm->height;
        *windowAspect = ( F32 ) vm->width / ( vm->height * vm->pixelAspect );
    }
    
    return true;
}

/*
** R_ModeList_f
*/
static void R_ModeList_f( void )
{
    S32 i;
    
    CL_RefPrintf( PRINT_ALL, "\n" );
    
    for( i = 0; i < s_numVidModes; i++ )
    {
        CL_RefPrintf( PRINT_ALL, "Mode %-2d: %s\n", i, r_vidModes[ i ].description );
    }
    
    CL_RefPrintf( PRINT_ALL, "\n" );
}


/*
==============================================================================

						SCREEN SHOTS

NOTE TTimo
some thoughts about the screenshots system:
screenshots get written in fs_homepath + fs_gamedir
vanilla q3 .. baseq3/screenshots/ *.tga
team arena .. missionpack/screenshots/ *.tga

two commands: "screenshot" and "screenshotJPEG"
we use statics to store a count and start writing the first screenshot/screenshot????.tga (.jpg) available
(with fileSystem->FileExists / fileSystem->FOpenFileWrite calls)
FIXME: the statics don't get a reinit between fs_game changes

==============================================================================
*/

/*
==================
RB_ReadPixels

Reads an image but takes care of alignment issues for reading RGB images.

Reads a minimum offset for where the RGB data starts in the image from
integer stored at pointer offset. When the function has returned the actual
offset was written back to address offset. This address will always have an
alignment of packAlign to ensure efficient copying.

Stores the length of padding after a line of pixels to address padlen

Return value must be freed with Hunk_FreeTempMemory()
==================
*/

U8* RB_ReadPixels( S32 x, S32 y, S32 width, S32 height, U64* offset, S32* padlen )
{
    U8* buffer, *bufstart;
    S32 padwidth, linelen;
    S32 packAlign;
    
    glGetIntegerv( GL_PACK_ALIGNMENT, &packAlign );
    
    linelen = width * 3;
    padwidth = PAD( linelen, packAlign );
    
    // Allocate a few more bytes so that we can choose an alignment we like
    buffer = ( U8* )Hunk_AllocateTempMemory( padwidth * height + *offset + packAlign - 1 );
    
    bufstart = ( U8* )PADP( ( intptr_t ) buffer + *offset, packAlign );
    
    glReadPixels( x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, bufstart );
    
    *offset = bufstart - buffer;
    *padlen = padwidth - linelen;
    
    return buffer;
}

/*
==================
RB_TakeScreenshot
==================
*/
void RB_TakeScreenshot( S32 x, S32 y, S32 width, S32 height, UTF8* fileName )
{
    U8* allbuf, *buffer;
    U8* srcptr, *destptr;
    U8* endline, *endmem;
    U8 temp;
    
    S32 linelen, padlen;
    U64 offset = 18, memcount;
    
    allbuf = RB_ReadPixels( x, y, width, height, &offset, &padlen );
    buffer = allbuf + offset - 18;
    
    ::memset( buffer, 0, 18 );
    buffer[2] = 2;		// uncompressed type
    buffer[12] = width & 255;
    buffer[13] = width >> 8;
    buffer[14] = height & 255;
    buffer[15] = height >> 8;
    buffer[16] = 24;	// pixel size
    
    // swap rgb to bgr and remove padding from line endings
    linelen = width * 3;
    
    srcptr = destptr = allbuf + offset;
    endmem = srcptr + ( linelen + padlen ) * height;
    
    while( srcptr < endmem )
    {
        endline = srcptr + linelen;
        
        while( srcptr < endline )
        {
            temp = srcptr[0];
            *destptr++ = srcptr[2];
            *destptr++ = srcptr[1];
            *destptr++ = temp;
            
            srcptr += 3;
        }
        
        // Skip the pad
        srcptr += padlen;
    }
    
    memcount = linelen * height;
    
    fileSystem->WriteFile( fileName, buffer, memcount + 18 );
    
    Hunk_FreeTempMemory( allbuf );
}

/*
==================
RB_TakeScreenshotJPEG
==================
*/

void RB_TakeScreenshotJPEG( S32 x, S32 y, S32 width, S32 height, UTF8* fileName )
{
    U8* buffer;
    U64 offset = 0, memcount;
    S32 padlen;
    
    buffer = RB_ReadPixels( x, y, width, height, &offset, &padlen );
    memcount = ( width * 3 + padlen ) * height;
    
    RE_SaveJPG( fileName, r_screenshotJpegQuality->integer, width, height, buffer + offset, padlen );
    Hunk_FreeTempMemory( buffer );
}

/*
==================
RB_TakeScreenshotCmd
==================
*/
const void* RB_TakeScreenshotCmd( const void* data )
{
    const screenshotCommand_t*	cmd;
    
    cmd = ( const screenshotCommand_t* )data;
    
    // finish any 2D drawing if needed
    if( tess.numIndexes )
        RB_EndSurface();
        
    if( cmd->jpeg )
        RB_TakeScreenshotJPEG( cmd->x, cmd->y, cmd->width, cmd->height, cmd->fileName );
    else
        RB_TakeScreenshot( cmd->x, cmd->y, cmd->width, cmd->height, cmd->fileName );
        
    return ( const void* )( cmd + 1 );
}

/*
==================
R_TakeScreenshot
==================
*/
void R_TakeScreenshot( S32 x, S32 y, S32 width, S32 height, UTF8* name, bool jpeg )
{
    static UTF8	fileName[MAX_OSPATH]; // bad things if two screenshots per frame?
    screenshotCommand_t*	cmd;
    
    cmd = ( screenshotCommand_t* )R_GetCommandBuffer( sizeof( *cmd ) );
    if( !cmd )
    {
        return;
    }
    cmd->commandId = RC_SCREENSHOT;
    
    cmd->x = x;
    cmd->y = y;
    cmd->width = width;
    cmd->height = height;
    Q_strncpyz( fileName, name, sizeof( fileName ) );
    cmd->fileName = fileName;
    cmd->jpeg = jpeg;
}

/*
==================
R_ScreenshotFilename
==================
*/
void R_ScreenshotFilename( S32 lastNumber, UTF8* fileName )
{
    S32		a, b, c, d;
    
    if( lastNumber < 0 || lastNumber > 9999 )
    {
        Com_sprintf( fileName, MAX_OSPATH, "screenshots/shot9999.tga" );
        return;
    }
    
    a = lastNumber / 1000;
    lastNumber -= a * 1000;
    b = lastNumber / 100;
    lastNumber -= b * 100;
    c = lastNumber / 10;
    lastNumber -= c * 10;
    d = lastNumber;
    
    Com_sprintf( fileName, MAX_OSPATH, "screenshots/shot%i%i%i%i.tga"
                 , a, b, c, d );
}

/*
==================
R_ScreenshotFilename
==================
*/
void R_ScreenshotFilenameJPEG( S32 lastNumber, UTF8* fileName )
{
    S32		a, b, c, d;
    
    if( lastNumber < 0 || lastNumber > 9999 )
    {
        Com_sprintf( fileName, MAX_OSPATH, "screenshots/shot9999.jpg" );
        return;
    }
    
    a = lastNumber / 1000;
    lastNumber -= a * 1000;
    b = lastNumber / 100;
    lastNumber -= b * 100;
    c = lastNumber / 10;
    lastNumber -= c * 10;
    d = lastNumber;
    
    Com_sprintf( fileName, MAX_OSPATH, "screenshots/shot%i%i%i%i.jpg"
                 , a, b, c, d );
}

/*
====================
R_LevelShot

levelshots are specialized 128*128 thumbnails for
the menu system, sampled down from full screen distorted images
====================
*/
void R_LevelShot( void )
{
    UTF8		checkname[MAX_OSPATH];
    U8*		buffer;
    U8*		source, *allsource;
    U8*		src, *dst;
    U64			offset = 0;
    S32			padlen;
    S32			x, y;
    S32			r, g, b;
    F32		xScale, yScale;
    S32			xx, yy;
    
    Com_sprintf( checkname, sizeof( checkname ), "levelshots/%s.tga", tr.world->baseName );
    
    allsource = RB_ReadPixels( 0, 0, glConfig.vidWidth, glConfig.vidHeight, &offset, &padlen );
    source = allsource + offset;
    
    buffer = ( U8* )Hunk_AllocateTempMemory( 128 * 128 * 3 + 18 );
    ::memset( buffer, 0, 18 );
    buffer[2] = 2;		// uncompressed type
    buffer[12] = 128;
    buffer[14] = 128;
    buffer[16] = 24;	// pixel size
    
    // resample from source
    xScale = glConfig.vidWidth / 512.0f;
    yScale = glConfig.vidHeight / 384.0f;
    for( y = 0 ; y < 128 ; y++ )
    {
        for( x = 0 ; x < 128 ; x++ )
        {
            r = g = b = 0;
            for( yy = 0 ; yy < 3 ; yy++ )
            {
                for( xx = 0 ; xx < 4 ; xx++ )
                {
                    src = source + ( 3 * glConfig.vidWidth + padlen ) * ( S32 )( ( y * 3 + yy ) * yScale ) +
                          3 * ( S32 )( ( x * 4 + xx ) * xScale );
                    r += src[0];
                    g += src[1];
                    b += src[2];
                }
            }
            dst = buffer + 18 + 3 * ( y * 128 + x );
            dst[0] = b / 12;
            dst[1] = g / 12;
            dst[2] = r / 12;
        }
    }
    
    fileSystem->WriteFile( checkname, buffer, 128 * 128 * 3 + 18 );
    
    Hunk_FreeTempMemory( buffer );
    Hunk_FreeTempMemory( allsource );
    
    CL_RefPrintf( PRINT_ALL, "Wrote %s\n", checkname );
}

/*
==================
R_ScreenShot_f

screenshot
screenshot [silent]
screenshot [levelshot]
screenshot [filename]

Doesn't print the pacifier message if there is a second arg
==================
*/
void R_ScreenShot_f( void )
{
    UTF8	checkname[MAX_OSPATH];
    static	S32	lastNumber = -1;
    bool	silent;
    
    if( !strcmp( Cmd_Argv( 1 ), "levelshot" ) )
    {
        R_LevelShot();
        return;
    }
    
    if( !strcmp( Cmd_Argv( 1 ), "silent" ) )
    {
        silent = true;
    }
    else
    {
        silent = false;
    }
    
    if( Cmd_Argc() == 2 && !silent )
    {
        // explicit filename
        Com_sprintf( checkname, MAX_OSPATH, "screenshots/%s.tga", Cmd_Argv( 1 ) );
    }
    else
    {
        // scan for a free filename
        
        // if we have saved a previous screenshot, don't scan
        // again, because recording demo avis can involve
        // thousands of shots
        if( lastNumber == -1 )
        {
            lastNumber = 0;
        }
        // scan for a free number
        for( ; lastNumber <= 9999 ; lastNumber++ )
        {
            R_ScreenshotFilename( lastNumber, checkname );
            
            if( !fileSystem->FileExists( checkname ) )
            {
                break; // file doesn't exist
            }
        }
        
        if( lastNumber >= 9999 )
        {
            CL_RefPrintf( PRINT_ALL, "ScreenShot: Couldn't create a file\n" );
            return;
        }
        
        lastNumber++;
    }
    
    R_TakeScreenshot( 0, 0, glConfig.vidWidth, glConfig.vidHeight, checkname, false );
    
    if( !silent )
    {
        CL_RefPrintf( PRINT_ALL, "Wrote %s\n", checkname );
    }
}

void R_ScreenShotJPEG_f( void )
{
    UTF8		checkname[MAX_OSPATH];
    static	S32	lastNumber = -1;
    bool	silent;
    
    if( !strcmp( Cmd_Argv( 1 ), "levelshot" ) )
    {
        R_LevelShot();
        return;
    }
    
    if( !strcmp( Cmd_Argv( 1 ), "silent" ) )
    {
        silent = true;
    }
    else
    {
        silent = false;
    }
    
    if( Cmd_Argc() == 2 && !silent )
    {
        // explicit filename
        Com_sprintf( checkname, MAX_OSPATH, "screenshots/%s.jpg", Cmd_Argv( 1 ) );
    }
    else
    {
        // scan for a free filename
        
        // if we have saved a previous screenshot, don't scan
        // again, because recording demo avis can involve
        // thousands of shots
        if( lastNumber == -1 )
        {
            lastNumber = 0;
        }
        // scan for a free number
        for( ; lastNumber <= 9999 ; lastNumber++ )
        {
            R_ScreenshotFilenameJPEG( lastNumber, checkname );
            
            if( !fileSystem->FileExists( checkname ) )
            {
                break; // file doesn't exist
            }
        }
        
        if( lastNumber == 10000 )
        {
            CL_RefPrintf( PRINT_ALL, "ScreenShot: Couldn't create a file\n" );
            return;
        }
        
        lastNumber++;
    }
    
    R_TakeScreenshot( 0, 0, glConfig.vidWidth, glConfig.vidHeight, checkname, true );
    
    if( !silent )
    {
        CL_RefPrintf( PRINT_ALL, "Wrote %s\n", checkname );
    }
}

//============================================================================

/*
==================
R_ExportCubemaps
==================
*/
void R_ExportCubemaps( void )
{
    exportCubemapsCommand_t*	cmd;
    
    cmd = ( exportCubemapsCommand_t* )R_GetCommandBuffer( sizeof( *cmd ) );
    if( !cmd )
    {
        return;
    }
    cmd->commandId = RC_EXPORT_CUBEMAPS;
}


/*
==================
R_ExportCubemaps_f
==================
*/
void R_ExportCubemaps_f( void )
{
    R_ExportCubemaps();
}

//============================================================================

/*
==================
RB_TakeVideoFrameCmd
==================
*/
const void* RB_TakeVideoFrameCmd( const void* data )
{
    const videoFrameCommand_t*	cmd;
    U8*				cBuf;
    U64				memcount, linelen;
    S32				padwidth, avipadwidth, padlen, avipadlen;
    S32 packAlign;
    
    // finish any 2D drawing if needed
    if( tess.numIndexes )
        RB_EndSurface();
        
    cmd = ( const videoFrameCommand_t* )data;
    
    glGetIntegerv( GL_PACK_ALIGNMENT, &packAlign );
    
    linelen = cmd->width * 3;
    
    // Alignment stuff for glReadPixels
    padwidth = PAD( linelen, packAlign );
    padlen = padwidth - linelen;
    // AVI line padding
    avipadwidth = PAD( linelen, AVI_LINE_PADDING );
    avipadlen = avipadwidth - linelen;
    
    cBuf = ( U8* )PADP( cmd->captureBuffer, packAlign );
    
    glReadPixels( 0, 0, cmd->width, cmd->height, GL_RGB,
                  GL_UNSIGNED_BYTE, cBuf );
                  
    memcount = padwidth * cmd->height;
    
    if( cmd->motionJpeg )
    {
        memcount = RE_SaveJPGToBuffer( cmd->encodeBuffer, linelen * cmd->height,
                                       r_aviMotionJpegQuality->integer,
                                       cmd->width, cmd->height, cBuf, padlen );
        CL_WriteAVIVideoFrame( cmd->encodeBuffer, memcount );
    }
    else
    {
        U8* lineend, *memend;
        U8* srcptr, *destptr;
        
        srcptr = cBuf;
        destptr = cmd->encodeBuffer;
        memend = srcptr + memcount;
        
        // swap R and B and remove line paddings
        while( srcptr < memend )
        {
            lineend = srcptr + linelen;
            while( srcptr < lineend )
            {
                *destptr++ = srcptr[2];
                *destptr++ = srcptr[1];
                *destptr++ = srcptr[0];
                srcptr += 3;
            }
            
            ::memset( destptr, '\0', avipadlen );
            destptr += avipadlen;
            
            srcptr += padlen;
        }
        
        CL_WriteAVIVideoFrame( cmd->encodeBuffer, avipadwidth * cmd->height );
    }
    
    return ( const void* )( cmd + 1 );
}

//============================================================================

/*
** GL_SetDefaultState
*/
void GL_SetDefaultState( void )
{
    S32 i;
    
    glClearDepth( 1.0f );
    
    glCullFace( GL_FRONT );
    
    GL_BindNullTextures();
    
    if( glRefConfig.framebufferObject )
        GL_BindNullFramebuffers();
        
    GL_TextureMode( r_textureMode->string );
    
    GL_CheckErrors();
    
    //glShadeModel( GL_SMOOTH );
    glDepthFunc( GL_LEQUAL );
    
    //
    // make sure our GL state vector is set correctly
    //
    glState.glStateBits = GLS_DEPTHTEST_DISABLE | GLS_DEPTHMASK_TRUE;
    glState.storedGlState = 0;
    glState.faceCulling = CT_TWO_SIDED;
    glState.faceCullFront = true;
    
    GL_BindNullProgram();
    
    if( glRefConfig.vertexArrayObject )
        qglBindVertexArray( 0 );
        
    qglBindBuffer( GL_ARRAY_BUFFER, 0 );
    qglBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
    glState.currentVao = NULL;
    glState.vertexAttribsEnabled = 0;
    
    qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
    qglDepthMask( GL_TRUE );
    qglDisable( GL_DEPTH_TEST );
    qglEnable( GL_SCISSOR_TEST );
    qglDisable( GL_CULL_FACE );
    qglDisable( GL_BLEND );
    
    if( glRefConfig.seamlessCubeMap )
        qglEnable( GL_TEXTURE_CUBE_MAP_SEAMLESS );
        
    // GL_POLYGON_OFFSET_FILL will be glEnable()d when this is used
    glPolygonOffset( r_offsetFactor->value, r_offsetUnits->value );
    
    glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );	// FIXME: get color of sky
}

/*
================
R_PrintLongString

Workaround for CL_RefPrintf's 1024 characters buffer limit.
================
*/
void R_PrintLongString( StringEntry string )
{
    UTF8 buffer[1024];
    StringEntry p;
    S32 size = strlen( string );
    
    p = string;
    while( size > 0 )
    {
        Q_strncpyz( buffer, p, sizeof( buffer ) );
        CL_RefPrintf( PRINT_ALL, "%s", buffer );
        p += 1023;
        size -= 1023;
    }
}

/*
================
GfxInfo_f
================
*/
void GfxInfo_f( void )
{
    StringEntry enablestrings[] =
    {
        "disabled",
        "enabled"
    };
    StringEntry fsstrings[] =
    {
        "windowed",
        "fullscreen"
    };
    
    CL_RefPrintf( PRINT_ALL, "\nGL_VENDOR: %s\n", glConfig.vendor_string );
    CL_RefPrintf( PRINT_ALL, "GL_RENDERER: %s\n", glConfig.renderer_string );
    CL_RefPrintf( PRINT_ALL, "GL_VERSION: %s\n", glConfig.version_string );
    CL_RefPrintf( PRINT_ALL, "GL_EXTENSIONS: " );
    if( qglGetStringi )
    {
        GLint numExtensions;
        S32 i;
        
        qglGetIntegerv( GL_NUM_EXTENSIONS, &numExtensions );
        for( i = 0; i < numExtensions; i++ )
        {
            CL_RefPrintf( PRINT_ALL, "%s ", qglGetStringi( GL_EXTENSIONS, i ) );
        }
    }
    else
    {
        R_PrintLongString( glConfig.extensions_string );
    }
    CL_RefPrintf( PRINT_ALL, "GL_MAX_TEXTURE_SIZE: %d\n", glConfig.maxTextureSize );
    CL_RefPrintf( PRINT_ALL, "GL_MAX_TEXTURE_UNITS_ARB: %d\n", glConfig.numTextureUnits );
    CL_RefPrintf( PRINT_ALL, "\nPIXELFORMAT: color(%d-bits) Z(%d-bit) stencil(%d-bits)\n", glConfig.colorBits, glConfig.depthBits, glConfig.stencilBits );
    CL_RefPrintf( PRINT_ALL, "MODE: %d, %d x %d %s hz:", r_mode->integer, glConfig.vidWidth, glConfig.vidHeight, fsstrings[r_fullscreen->integer == 1] );
    if( glConfig.displayFrequency )
    {
        CL_RefPrintf( PRINT_ALL, "%d\n", glConfig.displayFrequency );
    }
    else
    {
        CL_RefPrintf( PRINT_ALL, "N/A\n" );
    }
    if( glConfig.deviceSupportsGamma )
    {
        CL_RefPrintf( PRINT_ALL, "GAMMA: hardware w/ %d overbright bits\n", tr.overbrightBits );
    }
    else
    {
        CL_RefPrintf( PRINT_ALL, "GAMMA: software w/ %d overbright bits\n", tr.overbrightBits );
    }
    
    CL_RefPrintf( PRINT_ALL, "texturemode: %s\n", r_textureMode->string );
    CL_RefPrintf( PRINT_ALL, "picmip: %d\n", r_picmip->integer );
    CL_RefPrintf( PRINT_ALL, "texture bits: %d\n", r_texturebits->integer );
    CL_RefPrintf( PRINT_ALL, "compiled vertex arrays: %s\n", enablestrings[qglLockArraysEXT != 0 ] );
    CL_RefPrintf( PRINT_ALL, "texenv add: %s\n", enablestrings[glConfig.textureEnvAddAvailable != 0] );
    CL_RefPrintf( PRINT_ALL, "compressed textures: %s\n", enablestrings[glConfig.textureCompression != TC_NONE] );
    
    if( r_vertexLight->integer )
    {
        CL_RefPrintf( PRINT_ALL, "HACK: using vertex lightmap approximation\n" );
    }
    
    if( r_finish->integer )
    {
        CL_RefPrintf( PRINT_ALL, "Forcing glFinish\n" );
    }
}

/*
===============
R_Register
===============
*/
void R_Register( void )
{
    // OpenGL context selection
    r_glMajorVersion = cvarSystem->Get( "r_glMajorVersion", "4", CVAR_LATCH );
    r_glMinorVersion = cvarSystem->Get( "r_glMinorVersion", "0", CVAR_LATCH );
    r_glCoreProfile = cvarSystem->Get( "r_glCoreProfile", "1", CVAR_LATCH );
    r_glDebugProfile = cvarSystem->Get( "r_glDebugProfile", "", CVAR_LATCH );
    
    //
    // latched and archived variables
    //
    r_allowExtensions = cvarSystem->Get( "r_allowExtensions", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_ext_compressed_textures = cvarSystem->Get( "r_ext_compressed_textures", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_ext_multitexture = cvarSystem->Get( "r_ext_multitexture", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_ext_compiled_vertex_array = cvarSystem->Get( "r_ext_compiled_vertex_array", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_ext_texture_env_add = cvarSystem->Get( "r_ext_texture_env_add", "0", CVAR_ARCHIVE | CVAR_LATCH );
    
    r_ext_framebuffer_object = cvarSystem->Get( "r_ext_framebuffer_object", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_ext_framebuffer_blit = cvarSystem->Get( "r_ext_framebuffer_blit", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_ext_texture_float = cvarSystem->Get( "r_ext_texture_float", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_ext_framebuffer_multisample = cvarSystem->Get( "r_ext_framebuffer_multisample", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_arb_seamless_cube_map = cvarSystem->Get( "r_arb_seamless_cube_map", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_arb_vertex_array_object = cvarSystem->Get( "r_arb_vertex_array_object", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_ext_direct_state_access = cvarSystem->Get( "r_ext_direct_state_access", "1", CVAR_ARCHIVE | CVAR_LATCH );
    
    r_ext_texture_filter_anisotropic = cvarSystem->Get( "r_ext_texture_filter_anisotropic", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_ext_max_anisotropy = cvarSystem->Get( "r_ext_max_anisotropy", "0", CVAR_ARCHIVE | CVAR_LATCH );
    
    r_picmip = cvarSystem->Get( "r_picmip", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_roundImagesDown = cvarSystem->Get( "r_roundImagesDown", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_colorMipLevels = cvarSystem->Get( "r_colorMipLevels", "0", CVAR_LATCH );
    cvarSystem->CheckRange( r_picmip, 0, 16, true );
    r_displayRefresh = cvarSystem->Get( "r_displayRefresh", "0", CVAR_LATCH );
    cvarSystem->CheckRange( r_displayRefresh, 0, 200, true );
    r_detailTextures = cvarSystem->Get( "r_detailtextures", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_texturebits = cvarSystem->Get( "r_texturebits", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_colorbits = cvarSystem->Get( "r_colorbits", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_alphabits = cvarSystem->Get( "r_alphabits", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_stencilbits = cvarSystem->Get( "r_stencilbits", "8", CVAR_ARCHIVE | CVAR_LATCH );
    r_depthbits = cvarSystem->Get( "r_depthbits", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_ext_multisample = cvarSystem->Get( "r_ext_multisample", "0", CVAR_ARCHIVE | CVAR_LATCH );
    cvarSystem->CheckRange( r_ext_multisample, 0, 4, true );
    r_overBrightBits = cvarSystem->Get( "r_overBrightBits", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_ignorehwgamma = cvarSystem->Get( "r_ignorehwgamma", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_mode = cvarSystem->Get( "r_mode", "7", CVAR_ARCHIVE | CVAR_LATCH );
    r_fullscreen = cvarSystem->Get( "r_fullscreen", "0", CVAR_ARCHIVE );
    r_noborder = cvarSystem->Get( "r_noborder", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_width = cvarSystem->Get( "r_width", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_height = cvarSystem->Get( "r_height", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_pixelAspect = cvarSystem->Get( "r_pixelAspect", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_simpleMipMaps = cvarSystem->Get( "r_simpleMipMaps", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_vertexLight = cvarSystem->Get( "r_vertexLight", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_uiFullScreen = cvarSystem->Get( "r_uifullscreen", "0", 0 );
    r_subdivisions = cvarSystem->Get( "r_subdivisions", "4", CVAR_ARCHIVE | CVAR_LATCH );
    r_stereoEnabled = cvarSystem->Get( "r_stereoEnabled", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_greyscale = cvarSystem->Get( "r_greyscale", "0", CVAR_ARCHIVE | CVAR_LATCH );
    cvarSystem->CheckRange( r_greyscale, 0, 1, false );
    
    r_floatLightmap = cvarSystem->Get( "r_floatLightmap", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_postProcess = cvarSystem->Get( "r_postProcess", "0", CVAR_ARCHIVE );
    
    r_toneMap = cvarSystem->Get( "r_toneMap", "1", CVAR_ARCHIVE );
    r_forceToneMap = cvarSystem->Get( "r_forceToneMap", "0", CVAR_CHEAT );
    r_forceToneMapMin = cvarSystem->Get( "r_forceToneMapMin", "-8.0", CVAR_CHEAT );
    r_forceToneMapAvg = cvarSystem->Get( "r_forceToneMapAvg", "-2.0", CVAR_CHEAT );
    r_forceToneMapMax = cvarSystem->Get( "r_forceToneMapMax", "0.0", CVAR_CHEAT );
    
    r_autoExposure = cvarSystem->Get( "r_autoExposure", "1", CVAR_ARCHIVE );
    r_forceAutoExposure = cvarSystem->Get( "r_forceAutoExposure", "0", CVAR_CHEAT );
    r_forceAutoExposureMin = cvarSystem->Get( "r_forceAutoExposureMin", "-2.0", CVAR_CHEAT );
    r_forceAutoExposureMax = cvarSystem->Get( "r_forceAutoExposureMax", "2.0", CVAR_CHEAT );
    
    r_cameraExposure = cvarSystem->Get( "r_cameraExposure", "1", CVAR_CHEAT );
    
    r_depthPrepass = cvarSystem->Get( "r_depthPrepass", "1", CVAR_ARCHIVE );
    r_ssao = cvarSystem->Get( "r_ssao", "0", CVAR_LATCH | CVAR_ARCHIVE );
    
    r_normalMapping = cvarSystem->Get( "r_normalMapping", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_specularMapping = cvarSystem->Get( "r_specularMapping", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_deluxeMapping = cvarSystem->Get( "r_deluxeMapping", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_parallaxMapping = cvarSystem->Get( "r_parallaxMapping", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_cubeMapping = cvarSystem->Get( "r_cubeMapping", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_horizonFade = cvarSystem->Get( "r_horizonFade", "3", CVAR_ARCHIVE | CVAR_LATCH );
    r_deluxeSpecular = cvarSystem->Get( "r_deluxeSpecular", "0.3", CVAR_ARCHIVE | CVAR_LATCH );
    r_pbr = cvarSystem->Get( "r_pbr", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_baseNormalX = cvarSystem->Get( "r_baseNormalX", "1.0", CVAR_ARCHIVE | CVAR_LATCH );
    r_baseNormalY = cvarSystem->Get( "r_baseNormalY", "1.0", CVAR_ARCHIVE | CVAR_LATCH );
    r_baseParallax = cvarSystem->Get( "r_baseParallax", "0.001", CVAR_ARCHIVE | CVAR_LATCH );
    r_baseSpecular = cvarSystem->Get( "r_baseSpecular", "0.04", CVAR_ARCHIVE | CVAR_LATCH );
    r_baseGloss = cvarSystem->Get( "r_baseGloss", "0.3", CVAR_ARCHIVE | CVAR_LATCH );
    r_glossType = cvarSystem->Get( "r_glossType", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_dlightMode = cvarSystem->Get( "r_dlightMode", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_pshadowDist = cvarSystem->Get( "r_pshadowDist", "128", CVAR_ARCHIVE );
    r_mergeLightmaps = cvarSystem->Get( "r_mergeLightmaps", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_imageUpsample = cvarSystem->Get( "r_imageUpsample", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_imageUpsampleMaxSize = cvarSystem->Get( "r_imageUpsampleMaxSize", "1024", CVAR_ARCHIVE | CVAR_LATCH );
    r_imageUpsampleType = cvarSystem->Get( "r_imageUpsampleType", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_genNormalMaps = cvarSystem->Get( "r_genNormalMaps", "0", CVAR_ARCHIVE | CVAR_LATCH );
    
    r_forceSun = cvarSystem->Get( "r_forceSun", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_forceSunLightScale = cvarSystem->Get( "r_forceSunLightScale", "1.0", CVAR_CHEAT );
    r_forceSunAmbientScale = cvarSystem->Get( "r_forceSunAmbientScale", "0.5", CVAR_CHEAT );
    r_volumeLightStrength = cvarSystem->Get( "r_volumeLightStrength", "0.15", CVAR_ARCHIVE );
    r_proceduralSun = cvarSystem->Get( "r_proceduralSun", "1", CVAR_ARCHIVE );
    r_proceduralSunScale = cvarSystem->Get( "r_proceduralSunScale", "1.75", CVAR_ARCHIVE );
    r_drawSunRays = cvarSystem->Get( "r_drawSunRays", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_sunlightMode = cvarSystem->Get( "r_sunlightMode", "4", CVAR_ARCHIVE | CVAR_LATCH );
    r_sunlightSpecular = cvarSystem->Get( "r_sunlightSpecular", "1", CVAR_ARCHIVE );
    
    r_sunShadows = cvarSystem->Get( "r_sunShadows", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_shadowFilter = cvarSystem->Get( "r_shadowFilter", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_shadowBlur = cvarSystem->Get( "r_shadowBlur", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_shadowMapSize = cvarSystem->Get( "r_shadowMapSize", "2048", CVAR_ARCHIVE | CVAR_LATCH );
    r_shadowCascadeZNear = cvarSystem->Get( "r_shadowCascadeZNear", "8", CVAR_ARCHIVE | CVAR_LATCH );
    r_shadowCascadeZFar = cvarSystem->Get( "r_shadowCascadeZFar", "1024", CVAR_ARCHIVE | CVAR_LATCH );
    r_shadowCascadeZBias = cvarSystem->Get( "r_shadowCascadeZBias", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_ignoreDstAlpha = cvarSystem->Get( "r_ignoreDstAlpha", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_testvar = cvarSystem->Get( "r_testvar", "1.0", CVAR_ARCHIVE );
    r_bloom = cvarSystem->Get( "r_bloom", "0", CVAR_ARCHIVE );
    r_bloomPasses = cvarSystem->Get( "r_bloomPasses", "2", CVAR_ARCHIVE );
    r_bloomDarkenPower = cvarSystem->Get( "r_bloomDarkenPower", "0.3", CVAR_ARCHIVE );
    r_bloomScale = cvarSystem->Get( "r_bloomScale", "0.1", CVAR_ARCHIVE );
    r_lensflare = cvarSystem->Get( "r_lensflare", "1", CVAR_ARCHIVE );
    r_volumelight = cvarSystem->Get( "r_volumelight", "1", CVAR_ARCHIVE );
    r_anamorphic = cvarSystem->Get( "r_anamorphic", "1", CVAR_ARCHIVE );
    r_anamorphicStrength = cvarSystem->Get( "r_anamorphicStrength", "0.5", CVAR_ARCHIVE );
    //r_bloomRays = cvarSystem->Get("r_bloomRays", "1", CVAR_ARCHIVE);
    r_bloomRaysDecay = cvarSystem->Get( "r_bloomRaysDecay", "0.975", CVAR_ARCHIVE );
    r_bloomRaysWeight = cvarSystem->Get( "r_bloomRaysWeight", "0.2", CVAR_ARCHIVE );
    r_bloomRaysDensity = cvarSystem->Get( "r_bloomRaysDensity", "1.0", CVAR_ARCHIVE );
    r_ssgi = cvarSystem->Get( "r_ssgi", "4", CVAR_ARCHIVE );
    r_ssgiWidth = cvarSystem->Get( "r_ssgiWidth", "12.0", CVAR_ARCHIVE );
    r_ssgiSamples = cvarSystem->Get( "r_ssgiSamples", "4", CVAR_ARCHIVE );
    r_darkexpand = cvarSystem->Get( "r_darkexpand", "1", CVAR_ARCHIVE );
    r_truehdr = cvarSystem->Get( "r_truehdr", "1", CVAR_ARCHIVE );
    r_dof = cvarSystem->Get( "r_dof", "1", CVAR_ARCHIVE );
    r_esharpening = cvarSystem->Get( "r_esharpening", "1", CVAR_ARCHIVE );
    r_esharpening2 = cvarSystem->Get( "r_esharpening2", "1", CVAR_ARCHIVE );
    r_fxaa = cvarSystem->Get( "r_fxaa", "2", CVAR_ARCHIVE );
    r_multipost = cvarSystem->Get( "r_multipost", "1", CVAR_ARCHIVE );
    r_textureClean = cvarSystem->Get( "r_textureClean", "1", CVAR_ARCHIVE );
    r_textureCleanSigma = cvarSystem->Get( "r_textureCleanSigma", "1.2", CVAR_ARCHIVE );
    r_textureCleanBSigma = cvarSystem->Get( "r_textureCleanBSigma", "0.1", CVAR_ARCHIVE );
    r_textureCleanMSize = cvarSystem->Get( "r_textureCleanMSize", "6.0", CVAR_ARCHIVE );
    r_imageBasedLighting = cvarSystem->Get( "r_imageBasedLighting", "1", CVAR_ARCHIVE );
    r_sss = cvarSystem->Get( "r_sss", "0", CVAR_ARCHIVE );
    r_trueAnaglyph = cvarSystem->Get( "r_trueAnaglyph", "0", CVAR_ARCHIVE );
    r_trueAnaglyphSeparation = cvarSystem->Get( "r_trueAnaglyphSeparation", "8.0", CVAR_ARCHIVE );
    r_trueAnaglyphRed = cvarSystem->Get( "r_trueAnaglyphRed", "0.0", CVAR_ARCHIVE );
    r_trueAnaglyphGreen = cvarSystem->Get( "r_trueAnaglyphGreen", "0.0", CVAR_ARCHIVE );
    r_trueAnaglyphBlue = cvarSystem->Get( "r_trueAnaglyphBlue", "0.0", CVAR_ARCHIVE );
    r_vibrancy = cvarSystem->Get( "r_vibrancy", "0.4", CVAR_ARCHIVE );
    r_tesselation = cvarSystem->Get( "r_tesselation", "0", CVAR_ARCHIVE );
    r_tesselationAlpha = cvarSystem->Get( "r_tesselationAlpha", "1.5", CVAR_ARCHIVE );
    r_tesselationLevel = cvarSystem->Get( "r_tesselationLevel", "3.0", CVAR_ARCHIVE );
    r_texturedetail = cvarSystem->Get( "r_texturedetail", "1", CVAR_ARCHIVE );
    r_texturedetailStrength = cvarSystem->Get( "r_texturedetailStrength", "0.004", CVAR_ARCHIVE );
    r_rbm = cvarSystem->Get( "r_rbm", "0", CVAR_ARCHIVE );
    r_rbmStrength = cvarSystem->Get( "r_rbmStrength", "0.015", CVAR_ARCHIVE );
    r_screenblur = cvarSystem->Get( "r_screenBlur", "0", CVAR_ARCHIVE );
    r_brightness = cvarSystem->Get( "r_brightness", "0.0", CVAR_ARCHIVE );
    r_contrast = cvarSystem->Get( "r_contrast", "1.0", CVAR_ARCHIVE );
    r_gamma = cvarSystem->Get( "r_gamma", "1.0", CVAR_ARCHIVE );
    r_hbao = cvarSystem->Get( "r_hbao", "0", CVAR_ARCHIVE );
    r_deferredLighting = cvarSystem->Get( "r_deferredLighting", "1", CVAR_ARCHIVE );
    r_compressTextures = cvarSystem->Get( "r_compressTextures", "0", CVAR_ARCHIVE );
    r_cubemapCullRange = cvarSystem->Get( "r_cubemapCullRange", "768.0", CVAR_ARCHIVE );
    r_cubemapCullFalloffMult = cvarSystem->Get( "r_cubemapCullFalloffMult", "1.5", CVAR_ARCHIVE );
    r_blinnPhong = cvarSystem->Get( "r_blinnPhong", "1.5", CVAR_ARCHIVE );
    r_ao = cvarSystem->Get( "r_ao", "1", CVAR_ARCHIVE );
    r_env = cvarSystem->Get( "r_env", "1", CVAR_ARCHIVE );
    r_ssr = cvarSystem->Get( "r_ssr", "0", CVAR_ARCHIVE );
    r_ssrStrength = cvarSystem->Get( "r_ssrStrength", "0.02", CVAR_ARCHIVE );
    r_sse = cvarSystem->Get( "r_sse", "0", CVAR_ARCHIVE );
    r_sseStrength = cvarSystem->Get( "r_sseStrength", "0.02", CVAR_ARCHIVE );
    r_testshader = cvarSystem->Get( "r_testshader", "0", CVAR_ARCHIVE );
    r_testshaderValue1 = cvarSystem->Get( "r_testshaderValue1", "1.0", CVAR_ARCHIVE );
    r_testshaderValue2 = cvarSystem->Get( "r_testshaderValue2", "0.0", CVAR_ARCHIVE );
    r_testshaderValue3 = cvarSystem->Get( "r_testshaderValue3", "0.0", CVAR_ARCHIVE );
    r_testshaderValue4 = cvarSystem->Get( "r_testshaderValue4", "1.0", CVAR_ARCHIVE );
    r_testshaderValue5 = cvarSystem->Get( "r_testshaderValue5", "0.0", CVAR_ARCHIVE );
    r_testshaderValue6 = cvarSystem->Get( "r_testshaderValue6", "0.0", CVAR_ARCHIVE );
    r_testshaderValue7 = cvarSystem->Get( "r_testshaderValue7", "1.0", CVAR_ARCHIVE );
    r_testshaderValue8 = cvarSystem->Get( "r_testshaderValue8", "0.0", CVAR_ARCHIVE );
    r_testshaderValue9 = cvarSystem->Get( "r_testshaderValue9", "0.0", CVAR_ARCHIVE );
    
    r_ssdo = cvarSystem->Get( "r_ssdo", "0", CVAR_ARCHIVE );
    r_ssdoBaseRadius = cvarSystem->Get( "r_ssdoBaseRadius", "1.0", CVAR_ARCHIVE );
    r_ssdoMaxOcclusionDist = cvarSystem->Get( "r_ssdoMaxOcclusionDist", "99999.0", CVAR_ARCHIVE );
    r_shadowBlurPasses = cvarSystem->Get( "r_shadowBlurPasses", "1", CVAR_ARCHIVE );
    r_showdepth = cvarSystem->Get( "r_showdepth", "0", CVAR_CHEAT );
    r_shadowMaxDepthError = cvarSystem->Get( "r_shadowMaxDepthError", "0.000000059604644775390625", CVAR_ARCHIVE );
    r_shadowSolidityValue = cvarSystem->Get( "r_shadowSolidityValue", "0.5", CVAR_ARCHIVE );
    r_testvalue0 = cvarSystem->Get( "r_testvalue0", "0.1", CVAR_ARCHIVE );
    r_testvalue1 = cvarSystem->Get( "r_testvalue1", "0.005", CVAR_ARCHIVE );
    
    //
    // temporary latched variables that can only change over a restart
    //
    r_fullbright = cvarSystem->Get( "r_fullbright", "0", CVAR_LATCH | CVAR_CHEAT );
    r_mapOverBrightBits = cvarSystem->Get( "r_mapOverBrightBits", "2", CVAR_LATCH );
    r_intensity = cvarSystem->Get( "r_intensity", "1", CVAR_LATCH );
    r_singleShader = cvarSystem->Get( "r_singleShader", "0", CVAR_CHEAT | CVAR_LATCH );
    
    //
    // archived variables that can change at any time
    //
    r_lodCurveError = cvarSystem->Get( "r_lodCurveError", "250", CVAR_ARCHIVE | CVAR_CHEAT );
    r_lodbias = cvarSystem->Get( "r_lodbias", "0", CVAR_ARCHIVE );
    r_flares = cvarSystem->Get( "r_flares", "0", CVAR_ARCHIVE );
    r_znear = cvarSystem->Get( "r_znear", "4", CVAR_CHEAT );
    cvarSystem->CheckRange( r_znear, 0.001f, 200, false );
    r_zproj = cvarSystem->Get( "r_zproj", "64", CVAR_ARCHIVE );
    r_stereoSeparation = cvarSystem->Get( "r_stereoSeparation", "64", CVAR_ARCHIVE );
    r_ignoreGLErrors = cvarSystem->Get( "r_ignoreGLErrors", "1", CVAR_ARCHIVE );
    r_fastsky = cvarSystem->Get( "r_fastsky", "0", CVAR_ARCHIVE );
    r_inGameVideo = cvarSystem->Get( "r_inGameVideo", "1", CVAR_ARCHIVE );
    r_drawSun = cvarSystem->Get( "r_drawSun", "0", CVAR_ARCHIVE );
    r_dlightBacks = cvarSystem->Get( "r_dlightBacks", "1", CVAR_ARCHIVE );
    r_finish = cvarSystem->Get( "r_finish", "0", CVAR_ARCHIVE );
    r_textureMode = cvarSystem->Get( "r_textureMode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE );
    r_swapInterval = cvarSystem->Get( "r_swapInterval", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_gamma = cvarSystem->Get( "r_gamma", "1", CVAR_ARCHIVE );
    r_facePlaneCull = cvarSystem->Get( "r_facePlaneCull", "1", CVAR_ARCHIVE );
    
    r_railWidth = cvarSystem->Get( "r_railWidth", "16", CVAR_ARCHIVE );
    r_railCoreWidth = cvarSystem->Get( "r_railCoreWidth", "6", CVAR_ARCHIVE );
    r_railSegmentLength = cvarSystem->Get( "r_railSegmentLength", "32", CVAR_ARCHIVE );
    
    r_ambientScale = cvarSystem->Get( "r_ambientScale", "0.6", CVAR_CHEAT );
    r_directedScale = cvarSystem->Get( "r_directedScale", "1", CVAR_CHEAT );
    
    r_anaglyphMode = cvarSystem->Get( "r_anaglyphMode", "0", CVAR_ARCHIVE );
    
    //
    // temporary variables that can change at any time
    //
    r_showImages = cvarSystem->Get( "r_showImages", "0", CVAR_CHEAT | CVAR_TEMP );
    
    r_debugLight = cvarSystem->Get( "r_debuglight", "0", CVAR_TEMP );
    r_debugSort = cvarSystem->Get( "r_debugSort", "0", CVAR_CHEAT );
    r_printShaders = cvarSystem->Get( "r_printShaders", "0", 0 );
    r_saveFontData = cvarSystem->Get( "r_saveFontData", "0", 0 );
    
    r_nocurves = cvarSystem->Get( "r_nocurves", "0", CVAR_CHEAT );
    r_drawworld = cvarSystem->Get( "r_drawworld", "1", CVAR_CHEAT );
    r_lightmap = cvarSystem->Get( "r_lightmap", "0", 0 );
    r_portalOnly = cvarSystem->Get( "r_portalOnly", "0", CVAR_CHEAT );
    
    r_flareSize = cvarSystem->Get( "r_flareSize", "40", CVAR_CHEAT );
    r_flareFade = cvarSystem->Get( "r_flareFade", "7", CVAR_CHEAT );
    r_flareCoeff = cvarSystem->Get( "r_flareCoeff", FLARE_STDCOEFF, CVAR_CHEAT );
    
    r_skipBackEnd = cvarSystem->Get( "r_skipBackEnd", "0", CVAR_CHEAT );
    
    r_measureOverdraw = cvarSystem->Get( "r_measureOverdraw", "0", CVAR_CHEAT );
    r_lodscale = cvarSystem->Get( "r_lodscale", "5", CVAR_CHEAT );
    r_norefresh = cvarSystem->Get( "r_norefresh", "0", CVAR_CHEAT );
    r_drawentities = cvarSystem->Get( "r_drawentities", "1", CVAR_CHEAT );
    r_ignore = cvarSystem->Get( "r_ignore", "1", CVAR_CHEAT );
    r_nocull = cvarSystem->Get( "r_nocull", "0", CVAR_CHEAT );
    r_novis = cvarSystem->Get( "r_novis", "0", CVAR_CHEAT );
    r_showcluster = cvarSystem->Get( "r_showcluster", "0", CVAR_CHEAT );
    r_speeds = cvarSystem->Get( "r_speeds", "0", CVAR_CHEAT );
    r_verbose = cvarSystem->Get( "r_verbose", "0", CVAR_CHEAT );
    r_logFile = cvarSystem->Get( "r_logFile", "0", CVAR_CHEAT );
    r_debugSurface = cvarSystem->Get( "r_debugSurface", "0", CVAR_CHEAT );
    r_nobind = cvarSystem->Get( "r_nobind", "0", CVAR_CHEAT );
    r_showtris = cvarSystem->Get( "r_showtris", "0", CVAR_CHEAT );
    r_showsky = cvarSystem->Get( "r_showsky", "0", CVAR_CHEAT );
    r_shownormals = cvarSystem->Get( "r_shownormals", "0", CVAR_CHEAT );
    r_clear = cvarSystem->Get( "r_clear", "0", CVAR_CHEAT );
    r_offsetFactor = cvarSystem->Get( "r_offsetfactor", "-1", CVAR_CHEAT );
    r_offsetUnits = cvarSystem->Get( "r_offsetunits", "-2", CVAR_CHEAT );
    r_drawBuffer = cvarSystem->Get( "r_drawBuffer", "GL_BACK", CVAR_CHEAT );
    r_lockpvs = cvarSystem->Get( "r_lockpvs", "0", CVAR_CHEAT );
    r_noportals = cvarSystem->Get( "r_noportals", "0", CVAR_CHEAT );
    r_shadows = cvarSystem->Get( "cg_shadows", "0", 0 );
    
    r_marksOnTriangleMeshes = cvarSystem->Get( "r_marksOnTriangleMeshes", "0", CVAR_ARCHIVE );
    
    r_aviMotionJpegQuality = cvarSystem->Get( "r_aviMotionJpegQuality", "90", CVAR_ARCHIVE );
    r_screenshotJpegQuality = cvarSystem->Get( "r_screenshotJpegQuality", "90", CVAR_ARCHIVE );
    
    r_maxpolys = cvarSystem->Get( "r_maxpolys", va( "%d", MAX_POLYS ), 0 );
    r_maxpolyverts = cvarSystem->Get( "r_maxpolyverts", va( "%d", MAX_POLYVERTS ), 0 );
    
    r_floatfix = cvarSystem->Get( "r_floatfix", "0", 0 );
    
    // make sure all the commands added here are also
    // removed in R_Shutdown
    Cmd_AddCommand( "imagelist", R_ImageList_f );
    Cmd_AddCommand( "shaderlist", R_ShaderList_f );
    Cmd_AddCommand( "skinlist", R_SkinList_f );
    Cmd_AddCommand( "modellist", R_Modellist_f );
    Cmd_AddCommand( "modelist", R_ModeList_f );
    Cmd_AddCommand( "vaolist", R_VaoList_f ); //Dushan - was missing
    Cmd_AddCommand( "screenshot", R_ScreenShot_f );
    Cmd_AddCommand( "screenshotJPEG", R_ScreenShotJPEG_f );
    Cmd_AddCommand( "gfxinfo", GfxInfo_f );
    //Cmd_AddCommand( "minimize", GLimp_Minimize );
    Cmd_AddCommand( "exportCubemaps", R_ExportCubemaps_f );
    Cmd_AddCommand( "fbolist", R_FBOList_f );
}

void R_InitQueries( void )
{
    if( !glRefConfig.occlusionQuery )
        return;
        
    if( r_drawSunRays->integer )
        qglGenQueries( ARRAY_LEN( tr.sunFlareQuery ), tr.sunFlareQuery );
}

void R_ShutDownQueries( void )
{
    if( !glRefConfig.occlusionQuery )
        return;
        
    if( r_drawSunRays->integer )
        qglDeleteQueries( ARRAY_LEN( tr.sunFlareQuery ), tr.sunFlareQuery );
}

/*
===============
R_Init
===============
*/
void R_Init( void )
{
    S32	err;
    S32 i;
    U8* ptr;
    
    CL_RefPrintf( PRINT_ALL, "----- R_Init -----\n" );
    
    // clear all our internal state
    ::memset( &tr, 0, sizeof( tr ) );
    ::memset( &backEnd, 0, sizeof( backEnd ) );
    ::memset( &tess, 0, sizeof( tess ) );
    
//	Swap_Init();

    if( ( intptr_t )tess.xyz & 15 )
    {
        CL_RefPrintf( PRINT_WARNING, "tess.xyz not 16 U8 aligned\n" );
    }
    //::memset( tess.constantColor255, 255, sizeof( tess.constantColor255 ) );
    
    //
    // init function tables
    //
    for( i = 0; i < FUNCTABLE_SIZE; i++ )
    {
        tr.sinTable[i]		= sin( DEG2RAD( i * 360.0f / ( ( F32 )( FUNCTABLE_SIZE - 1 ) ) ) );
        tr.squareTable[i]	= ( i < FUNCTABLE_SIZE / 2 ) ? 1.0f : -1.0f;
        tr.sawToothTable[i] = ( F32 )i / FUNCTABLE_SIZE;
        tr.inverseSawToothTable[i] = 1.0f - tr.sawToothTable[i];
        
        if( i < FUNCTABLE_SIZE / 2 )
        {
            if( i < FUNCTABLE_SIZE / 4 )
            {
                tr.triangleTable[i] = ( F32 ) i / ( FUNCTABLE_SIZE / 4 );
            }
            else
            {
                tr.triangleTable[i] = 1.0f - tr.triangleTable[i - FUNCTABLE_SIZE / 4];
            }
        }
        else
        {
            tr.triangleTable[i] = -tr.triangleTable[i - FUNCTABLE_SIZE / 2];
        }
    }
    
    R_InitFogTable();
    
    R_NoiseInit();
    
    R_Register();
    
    max_polys = r_maxpolys->integer;
    if( max_polys < MAX_POLYS )
        max_polys = MAX_POLYS;
        
    max_polyverts = r_maxpolyverts->integer;
    if( max_polyverts < MAX_POLYVERTS )
        max_polyverts = MAX_POLYVERTS;
        
    ptr = ( U8* )Hunk_Alloc( sizeof( *backEndData ) + sizeof( srfPoly_t ) * max_polys + sizeof( polyVert_t ) * max_polyverts, h_low );
    backEndData = ( backEndData_t* ) ptr;
    backEndData->polys = ( srfPoly_t* )( ( UTF8* ) ptr + sizeof( *backEndData ) );
    backEndData->polyVerts = ( polyVert_t* )( ( UTF8* ) ptr + sizeof( *backEndData ) + sizeof( srfPoly_t ) * max_polys );
    R_InitNextFrame();
    
    InitOpenGL();
    
    R_InitImages();
    
    if( glRefConfig.framebufferObject )
    {
        renderSystemLocal.FBOInit();
    }
    
    //Init gpuWorker
    //Dushan - Not completed
    //gpuWorker->Init();
    //gpuWorkerCLManager->InitDevice();
    //gpuWorkerCLManager->InitTexturefromCL();
    
    //Init GLSL
    renderSystemLocal.InitGPUShaders();
    
    R_InitVaos();
    
    R_InitShaders();
    
    R_InitSkins();
    
    R_ModelInit();
    
    R_InitFreeType();
    
    R_InitQueries();
    
    err = glGetError();
    if( err != GL_NO_ERROR )
        CL_RefPrintf( PRINT_ALL, "glGetError() = 0x%x\n", err );
        
    // print info
    GfxInfo_f();
    CL_RefPrintf( PRINT_ALL, "----- finished R_Init -----\n" );
}

/*
===============
idRenderSystemLocal::Shutdown
===============
*/
void idRenderSystemLocal::Shutdown( bool destroyWindow )
{

    CL_RefPrintf( PRINT_ALL, "idRenderSystemLocal::Shutdown( %i )\n", destroyWindow );
    
    Cmd_RemoveCommand( "exportCubemaps" );
    Cmd_RemoveCommand( "gfxinfo" );
    Cmd_RemoveCommand( "gfxmeminfo" );
    Cmd_RemoveCommand( "imagelist" );
    Cmd_RemoveCommand( "minimize" );
    Cmd_RemoveCommand( "modellist" );
    Cmd_RemoveCommand( "screenshot" );
    Cmd_RemoveCommand( "screenshotJPEG" );
    Cmd_RemoveCommand( "shaderlist" );
    Cmd_RemoveCommand( "skinlist" );
    
    if( tr.registered )
    {
        R_IssuePendingRenderCommands();
        R_ShutDownQueries();
        if( glRefConfig.framebufferObject )
        {
            FBOShutdown();
        }
        
        R_DeleteTextures();
        R_ShutdownVaos();
        //
        //gpuWorker->Shutdown();
        ShutdownGPUShaders();
    }
    
    R_DoneFreeType();
    
    // shut down platform specific OpenGL stuff
    if( destroyWindow )
    {
        GLimp_Shutdown();
        
        ::memset( &glConfig, 0, sizeof( glConfig ) );
        ::memset( &glState, 0, sizeof( glState ) );
    }
    
    tr.registered = false;
}


/*
=============
idRenderSystemLocal::EndRegistration

Touch all images to make sure they are resident
=============
*/
void idRenderSystemLocal::EndRegistration( void )
{
    R_IssuePendingRenderCommands();
    if( !Sys_LowPhysicalMemory() )
    {
        RB_ShowImages();
    }
}
