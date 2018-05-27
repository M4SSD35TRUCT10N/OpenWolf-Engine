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
// File name:   r_cmds.cpp
// Version:     v1.00
// Created:
// Compilers:   Visual Studio 2015
// Description:
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#include <GPURenderer/r_local.h>

/*
=====================
R_PerformanceCounters
=====================
*/
void R_PerformanceCounters( void )
{
    if( !r_speeds->integer )
    {
        // clear the counters even if we aren't printing
        ::memset( &tr.pc, 0, sizeof( tr.pc ) );
        ::memset( &backEnd.pc, 0, sizeof( backEnd.pc ) );
        return;
    }
    
    if( r_speeds->integer == 1 )
    {
        CL_RefPrintf( PRINT_ALL, "%i/%i/%i shaders/batches/surfs %i leafs %i verts %i/%i tris %.2f mtex %.2f dc\n",
                      backEnd.pc.c_shaders, backEnd.pc.c_surfBatches, backEnd.pc.c_surfaces, tr.pc.c_leafs, backEnd.pc.c_vertexes,
                      backEnd.pc.c_indexes / 3, backEnd.pc.c_totalIndexes / 3,
                      R_SumOfUsedImages() / ( 1000000.0f ), backEnd.pc.c_overDraw / ( F32 )( glConfig.vidWidth * glConfig.vidHeight ) );
    }
    else if( r_speeds->integer == 2 )
    {
        CL_RefPrintf( PRINT_ALL, "(patch) %i sin %i sclip  %i sout %i bin %i bclip %i bout\n",
                      tr.pc.c_sphere_cull_patch_in, tr.pc.c_sphere_cull_patch_clip, tr.pc.c_sphere_cull_patch_out,
                      tr.pc.c_box_cull_patch_in, tr.pc.c_box_cull_patch_clip, tr.pc.c_box_cull_patch_out );
        CL_RefPrintf( PRINT_ALL, "(md3) %i sin %i sclip  %i sout %i bin %i bclip %i bout\n",
                      tr.pc.c_sphere_cull_md3_in, tr.pc.c_sphere_cull_md3_clip, tr.pc.c_sphere_cull_md3_out,
                      tr.pc.c_box_cull_md3_in, tr.pc.c_box_cull_md3_clip, tr.pc.c_box_cull_md3_out );
    }
    else if( r_speeds->integer == 3 )
    {
        CL_RefPrintf( PRINT_ALL, "viewcluster: %i\n", tr.viewCluster );
    }
    else if( r_speeds->integer == 4 )
    {
        if( backEnd.pc.c_dlightVertexes )
        {
            CL_RefPrintf( PRINT_ALL, "dlight srf:%i  culled:%i  verts:%i  tris:%i\n",
                          tr.pc.c_dlightSurfaces, tr.pc.c_dlightSurfacesCulled,
                          backEnd.pc.c_dlightVertexes, backEnd.pc.c_dlightIndexes / 3 );
        }
    }
    else if( r_speeds->integer == 5 )
    {
        CL_RefPrintf( PRINT_ALL, "zFar: %.0f\n", tr.viewParms.zFar );
    }
    else if( r_speeds->integer == 6 )
    {
        CL_RefPrintf( PRINT_ALL, "flare adds:%i tests:%i renders:%i\n",
                      backEnd.pc.c_flareAdds, backEnd.pc.c_flareTests, backEnd.pc.c_flareRenders );
    }
    else if( r_speeds->integer == 7 )
    {
        CL_RefPrintf( PRINT_ALL, "VAO draws: static %i dynamic %i\n",
                      backEnd.pc.c_staticVaoDraws, backEnd.pc.c_dynamicVaoDraws );
        CL_RefPrintf( PRINT_ALL, "GLSL binds: %i  draws: gen %i light %i fog %i dlight %i\n",
                      backEnd.pc.c_glslShaderBinds, backEnd.pc.c_genericDraws, backEnd.pc.c_lightallDraws, backEnd.pc.c_fogDraws, backEnd.pc.c_dlightDraws );
    }
    
    ::memset( &tr.pc, 0, sizeof( tr.pc ) );
    ::memset( &backEnd.pc, 0, sizeof( backEnd.pc ) );
}


/*
====================
R_IssueRenderCommands
====================
*/
void R_IssueRenderCommands( bool runPerformanceCounters )
{
    renderCommandList_t*	cmdList;
    
    cmdList = &backEndData->commands;
    assert( cmdList );
    // add an end-of-list command
    *( S32* )( cmdList->cmds + cmdList->used ) = RC_END_OF_LIST;
    
    // clear it out, in case this is a sync and not a buffer flip
    cmdList->used = 0;
    
    if( runPerformanceCounters )
    {
        R_PerformanceCounters();
    }
    
    // actually start the commands going
    if( !r_skipBackEnd->integer )
    {
        // let it start on the new batch
        RB_ExecuteRenderCommands( cmdList->cmds );
    }
}


/*
====================
R_IssuePendingRenderCommands

Issue any pending commands and wait for them to complete.
====================
*/
void R_IssuePendingRenderCommands( void )
{
    if( !tr.registered )
    {
        return;
    }
    R_IssueRenderCommands( false );
}

/*
============
R_GetCommandBufferReserved

make sure there is enough command space
============
*/
void* R_GetCommandBufferReserved( S32 bytes, S32 reservedBytes )
{
    renderCommandList_t*	cmdList;
    
    cmdList = &backEndData->commands;
    bytes = PAD( bytes, sizeof( void* ) );
    
    // always leave room for the end of list command
    if( cmdList->used + bytes + sizeof( S32 ) + reservedBytes > MAX_RENDER_COMMANDS )
    {
        if( bytes > MAX_RENDER_COMMANDS - sizeof( S32 ) )
        {
            Com_Error( ERR_FATAL, "R_GetCommandBuffer: bad size %i", bytes );
        }
        // if we run out of room, just start dropping commands
        return NULL;
    }
    
    cmdList->used += bytes;
    
    return cmdList->cmds + cmdList->used - bytes;
}

/*
=============
R_GetCommandBuffer

returns NULL if there is not enough space for important commands
=============
*/
void* R_GetCommandBuffer( S32 bytes )
{
    return R_GetCommandBufferReserved( bytes, PAD( sizeof( swapBuffersCommand_t ), sizeof( void* ) ) );
}


/*
=============
R_AddDrawSurfCmd

=============
*/
void	R_AddDrawSurfCmd( drawSurf_t* drawSurfs, S32 numDrawSurfs )
{
    drawSurfsCommand_t*	cmd;
    
    cmd = ( drawSurfsCommand_t* )R_GetCommandBuffer( sizeof( *cmd ) );
    if( !cmd )
    {
        return;
    }
    cmd->commandId = RC_DRAW_SURFS;
    
    cmd->drawSurfs = drawSurfs;
    cmd->numDrawSurfs = numDrawSurfs;
    
    cmd->refdef = tr.refdef;
    cmd->viewParms = tr.viewParms;
}


/*
=============
R_AddCapShadowmapCmd

=============
*/
void	R_AddCapShadowmapCmd( S32 map, S32 cubeSide )
{
    capShadowmapCommand_t*	cmd;
    
    cmd = ( capShadowmapCommand_t* )R_GetCommandBuffer( sizeof( *cmd ) );
    if( !cmd )
    {
        return;
    }
    cmd->commandId = RC_CAPSHADOWMAP;
    
    cmd->map = map;
    cmd->cubeSide = cubeSide;
}


/*
=============
R_PostProcessingCmd

=============
*/
void	R_AddPostProcessCmd( )
{
    postProcessCommand_t*	cmd;
    
    cmd = ( postProcessCommand_t* )R_GetCommandBuffer( sizeof( *cmd ) );
    if( !cmd )
    {
        return;
    }
    cmd->commandId = RC_POSTPROCESS;
    
    cmd->refdef = tr.refdef;
    cmd->viewParms = tr.viewParms;
}

/*
=============
idRenderSystemLocal::SetColor

Passing NULL will set the color to white
=============
*/
void idRenderSystemLocal::SetColor( const F32* rgba )
{
    setColorCommand_t*	cmd;
    
    if( !tr.registered )
    {
        return;
    }
    cmd = ( setColorCommand_t* )R_GetCommandBuffer( sizeof( *cmd ) );
    if( !cmd )
    {
        return;
    }
    cmd->commandId = RC_SET_COLOR;
    if( !rgba )
    {
        static F32 colorWhite[4] = { 1, 1, 1, 1 };
        
        rgba = colorWhite;
    }
    
    cmd->color[0] = rgba[0];
    cmd->color[1] = rgba[1];
    cmd->color[2] = rgba[2];
    cmd->color[3] = rgba[3];
}

/*
=============
R_ClipRegion
=============
*/
static bool R_ClipRegion( F32* x, F32* y, F32* w, F32* h, F32* s1, F32* t1, F32* s2, F32* t2 )
{
    F32 left, top, right, bottom;
    F32 _s1, _t1, _s2, _t2;
    F32 clipLeft, clipTop, clipRight, clipBottom;
    
    if( tr.clipRegion[2] <= tr.clipRegion[0] ||
            tr.clipRegion[3] <= tr.clipRegion[1] )
    {
        return false;
    }
    
    left = *x;
    top = *y;
    right = *x + *w;
    bottom = *y + *h;
    
    _s1 = *s1;
    _t1 = *t1;
    _s2 = *s2;
    _t2 = *t2;
    
    clipLeft = tr.clipRegion[0];
    clipTop = tr.clipRegion[1];
    clipRight = tr.clipRegion[2];
    clipBottom = tr.clipRegion[3];
    
    // Completely clipped away
    if( right <= clipLeft || left >= clipRight ||
            bottom <= clipTop || top >= clipBottom )
    {
        return true;
    }
    
    // Clip left edge
    if( left < clipLeft )
    {
        F32 f = ( clipLeft - left ) / ( right - left );
        *s1 = ( f * ( _s2 - _s1 ) ) + _s1;
        *x = clipLeft;
        *w -= ( clipLeft - left );
    }
    
    // Clip right edge
    if( right > clipRight )
    {
        F32 f = ( clipRight - right ) / ( left - right );
        *s2 = ( f * ( _s1 - _s2 ) ) + _s2;
        *w = clipRight - *x;
    }
    
    // Clip top edge
    if( top < clipTop )
    {
        F32 f = ( clipTop - top ) / ( bottom - top );
        *t1 = ( f * ( _t2 - _t1 ) ) + _t1;
        *y = clipTop;
        *h -= ( clipTop - top );
    }
    
    // Clip bottom edge
    if( bottom > clipBottom )
    {
        F32 f = ( clipBottom - bottom ) / ( top - bottom );
        *t2 = ( f * ( _t1 - _t2 ) ) + _t2;
        *h = clipBottom - *y;
    }
    
    return false;
}

/*
=============
idRenderSystemLocal::SetClipRegion
=============
*/
void idRenderSystemLocal::SetClipRegion( const F32* region )
{
    if( region == NULL )
    {
        ::memset( tr.clipRegion, 0, sizeof( vec4_t ) );
    }
    else
    {
        Vector4Copy( region, tr.clipRegion );
    }
}

/*
=============
idRenderSystemLocal::DrawStretchPic
=============
*/
void idRenderSystemLocal::DrawStretchPic( F32 x, F32 y, F32 w, F32 h, F32 s1, F32 t1, F32 s2, F32 t2, qhandle_t hShader )
{
    stretchPicCommand_t*	cmd;
    
    if( !tr.registered )
    {
        return;
    }
    if( R_ClipRegion( &x, &y, &w, &h, &s1, &t1, &s2, &t2 ) )
    {
        return;
    }
    cmd = ( stretchPicCommand_t* )R_GetCommandBuffer( sizeof( *cmd ) );
    if( !cmd )
    {
        return;
    }
    cmd->commandId = RC_STRETCH_PIC;
    cmd->shader = R_GetShaderByHandle( hShader );
    cmd->x = x;
    cmd->y = y;
    cmd->w = w;
    cmd->h = h;
    cmd->s1 = s1;
    cmd->t1 = t1;
    cmd->s2 = s2;
    cmd->t2 = t2;
}

#define MODE_RED_CYAN	1
#define MODE_RED_BLUE	2
#define MODE_RED_GREEN	3
#define MODE_GREEN_MAGENTA 4
#define MODE_MAX	MODE_GREEN_MAGENTA

void R_SetColorMode( GLboolean* rgba, stereoFrame_t stereoFrame, S32 colormode )
{
    rgba[0] = rgba[1] = rgba[2] = rgba[3] = GL_TRUE;
    
    if( colormode > MODE_MAX )
    {
        if( stereoFrame == STEREO_LEFT )
            stereoFrame = STEREO_RIGHT;
        else if( stereoFrame == STEREO_RIGHT )
            stereoFrame = STEREO_LEFT;
            
        colormode -= MODE_MAX;
    }
    
    if( colormode == MODE_GREEN_MAGENTA )
    {
        if( stereoFrame == STEREO_LEFT )
            rgba[0] = rgba[2] = GL_FALSE;
        else if( stereoFrame == STEREO_RIGHT )
            rgba[1] = GL_FALSE;
    }
    else
    {
        if( stereoFrame == STEREO_LEFT )
            rgba[1] = rgba[2] = GL_FALSE;
        else if( stereoFrame == STEREO_RIGHT )
        {
            rgba[0] = GL_FALSE;
            
            if( colormode == MODE_RED_BLUE )
                rgba[1] = GL_FALSE;
            else if( colormode == MODE_RED_GREEN )
                rgba[2] = GL_FALSE;
        }
    }
}


/*
====================
idRenderSystemLocal::BeginFrame

If running in stereo, RE_BeginFrame will be called twice
for each RE_EndFrame
====================
*/
void idRenderSystemLocal::BeginFrame( stereoFrame_t stereoFrame )
{
    drawBufferCommand_t* cmd = NULL;
    colorMaskCommand_t* colcmd = NULL;
    
    if( !tr.registered )
    {
        return;
    }
    glState.finishCalled = false;
    
    tr.frameCount++;
    tr.frameSceneNum = 0;
    
    //
    // do overdraw measurement
    //
    if( r_measureOverdraw->integer )
    {
        if( glConfig.stencilBits < 4 )
        {
            CL_RefPrintf( PRINT_ALL, "Warning: not enough stencil bits to measure overdraw: %d\n", glConfig.stencilBits );
            Cvar_Set( "r_measureOverdraw", "0" );
            r_measureOverdraw->modified = false;
        }
        else if( r_shadows->integer == 2 )
        {
            CL_RefPrintf( PRINT_ALL, "Warning: stencil shadows and overdraw measurement are mutually exclusive\n" );
            Cvar_Set( "r_measureOverdraw", "0" );
            r_measureOverdraw->modified = false;
        }
        else
        {
            R_IssuePendingRenderCommands();
            glEnable( GL_STENCIL_TEST );
            glStencilMask( ~0U );
            glClearStencil( 0U );
            glStencilFunc( GL_ALWAYS, 0U, ~0U );
            glStencilOp( GL_KEEP, GL_INCR, GL_INCR );
        }
        r_measureOverdraw->modified = false;
    }
    else
    {
        // this is only reached if it was on and is now off
        if( r_measureOverdraw->modified )
        {
            R_IssuePendingRenderCommands();
            glDisable( GL_STENCIL_TEST );
        }
        r_measureOverdraw->modified = false;
    }
    
    //
    // texturemode stuff
    //
    if( r_textureMode->modified )
    {
        R_IssuePendingRenderCommands();
        GL_TextureMode( r_textureMode->string );
        r_textureMode->modified = false;
    }
    
    //
    // gamma stuff
    //
    if( r_gamma->modified )
    {
        r_gamma->modified = false;
        
        R_IssuePendingRenderCommands();
        R_SetColorMappings();
    }
    
    // check for errors
    if( !r_ignoreGLErrors->integer )
    {
        S32	err;
        
        R_IssuePendingRenderCommands();
        if( ( err = glGetError() ) != GL_NO_ERROR )
            Com_Error( ERR_FATAL, "idRenderSystemLocal::BeginFrame() - glGetError() failed (0x%x)!", err );
    }
    
    if( glConfig.stereoEnabled )
    {
        if( !( cmd = ( drawBufferCommand_t* )R_GetCommandBuffer( sizeof( *cmd ) ) ) )
            return;
            
        cmd->commandId = RC_DRAW_BUFFER;
        
        if( stereoFrame == STEREO_LEFT )
        {
            cmd->buffer = ( S32 )GL_BACK_LEFT;
        }
        else if( stereoFrame == STEREO_RIGHT )
        {
            cmd->buffer = ( S32 )GL_BACK_RIGHT;
        }
        else
        {
            Com_Error( ERR_FATAL, "idRenderSystemLocal::BeginFrame: Stereo is enabled, but stereoFrame was %i", stereoFrame );
        }
    }
    else
    {
        if( r_anaglyphMode->integer )
        {
            if( r_anaglyphMode->modified )
            {
                // clear both, front and backbuffer.
                glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
                backEnd.colorMask[0] = GL_FALSE;
                backEnd.colorMask[1] = GL_FALSE;
                backEnd.colorMask[2] = GL_FALSE;
                backEnd.colorMask[3] = GL_FALSE;
                
                if( glRefConfig.framebufferObject )
                {
                    // clear all framebuffers
                    if( tr.msaaResolveFbo )
                    {
                        FBO_Bind( tr.msaaResolveFbo );
                        glClear( GL_COLOR_BUFFER_BIT );
                    }
                    
                    if( tr.renderFbo )
                    {
                        FBO_Bind( tr.renderFbo );
                        glClear( GL_COLOR_BUFFER_BIT );
                    }
                    
                    FBO_Bind( NULL );
                }
                
                glDrawBuffer( GL_FRONT );
                glClear( GL_COLOR_BUFFER_BIT );
                glDrawBuffer( GL_BACK );
                glClear( GL_COLOR_BUFFER_BIT );
                
                r_anaglyphMode->modified = false;
            }
            
            if( stereoFrame == STEREO_LEFT )
            {
                if( !( cmd = ( drawBufferCommand_t* )R_GetCommandBuffer( sizeof( *cmd ) ) ) )
                    return;
                    
                if( !( colcmd = ( colorMaskCommand_t* )R_GetCommandBuffer( sizeof( *colcmd ) ) ) )
                    return;
            }
            else if( stereoFrame == STEREO_RIGHT )
            {
                clearDepthCommand_t* cldcmd;
                
                if( !( cldcmd = ( clearDepthCommand_t* )R_GetCommandBuffer( sizeof( *cldcmd ) ) ) )
                    return;
                    
                cldcmd->commandId = RC_CLEARDEPTH;
                
                if( !( colcmd = ( colorMaskCommand_t* )R_GetCommandBuffer( sizeof( *colcmd ) ) ) )
                    return;
            }
            else
                Com_Error( ERR_FATAL, "idRenderSystemLocal::BeginFrame: Stereo is enabled, but stereoFrame was %i", stereoFrame );
                
            R_SetColorMode( colcmd->rgba, stereoFrame, r_anaglyphMode->integer );
            colcmd->commandId = RC_COLORMASK;
        }
        else
        {
            if( stereoFrame != STEREO_CENTER )
                Com_Error( ERR_FATAL, "idRenderSystemLocal::BeginFrame: Stereo is disabled, but stereoFrame was %i", stereoFrame );
                
            if( !( cmd = ( drawBufferCommand_t* )R_GetCommandBuffer( sizeof( *cmd ) ) ) )
                return;
        }
        
        if( cmd )
        {
            cmd->commandId = RC_DRAW_BUFFER;
            
            if( r_anaglyphMode->modified )
            {
                glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
                backEnd.colorMask[0] = 0;
                backEnd.colorMask[1] = 0;
                backEnd.colorMask[2] = 0;
                backEnd.colorMask[3] = 0;
                r_anaglyphMode->modified = false;
            }
            
            if( !Q_stricmp( r_drawBuffer->string, "GL_FRONT" ) )
                cmd->buffer = ( S32 )GL_FRONT;
            else
                cmd->buffer = ( S32 )GL_BACK;
        }
    }
    
    tr.refdef.stereoFrame = stereoFrame;
}


/*
=============
idRenderSystemLocal::EndFrame

Returns the number of msec spent in the back end
=============
*/
void idRenderSystemLocal::EndFrame( S32* frontEndMsec, S32* backEndMsec )
{
    swapBuffersCommand_t*	cmd;
    
    if( !tr.registered )
    {
        return;
    }
    cmd = ( swapBuffersCommand_t* )R_GetCommandBufferReserved( sizeof( *cmd ), 0 );
    if( !cmd )
    {
        return;
    }
    cmd->commandId = RC_SWAP_BUFFERS;
    
    R_IssueRenderCommands( true );
    
    R_InitNextFrame();
    
    if( frontEndMsec )
    {
        *frontEndMsec = tr.frontEndMsec;
    }
    tr.frontEndMsec = 0;
    if( backEndMsec )
    {
        *backEndMsec = backEnd.pc.msec;
    }
    backEnd.pc.msec = 0;
}

/*
=============
idRenderSystemLocal::TakeVideoFrame
=============
*/
void idRenderSystemLocal::TakeVideoFrame( S32 width, S32 height, U8* captureBuffer, U8* encodeBuffer, bool motionJpeg )
{
    videoFrameCommand_t*	cmd;
    
    if( !tr.registered )
    {
        return;
    }
    
    cmd = ( videoFrameCommand_t* )R_GetCommandBuffer( sizeof( *cmd ) );
    if( !cmd )
    {
        return;
    }
    
    cmd->commandId = RC_VIDEOFRAME;
    
    cmd->width = width;
    cmd->height = height;
    cmd->captureBuffer = captureBuffer;
    cmd->encodeBuffer = encodeBuffer;
    cmd->motionJpeg = motionJpeg;
}
