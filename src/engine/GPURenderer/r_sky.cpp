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
// File name:   r_sky.cpp
// Version:     v1.01
// Created:
// Compilers:   Visual Studio 2017, gcc 7.3.0
// Description:
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#include <OWLib/precompiled.h>

#define SKY_SUBDIVISIONS		8
#define HALF_SKY_SUBDIVISIONS	(SKY_SUBDIVISIONS/2)

static F32 s_cloudTexCoords[6][SKY_SUBDIVISIONS + 1][SKY_SUBDIVISIONS + 1][2];
static F32 s_cloudTexP[6][SKY_SUBDIVISIONS + 1][SKY_SUBDIVISIONS + 1];

/*
===================================================================================

POLYGON TO BOX SIDE PROJECTION

===================================================================================
*/

static vec3_t sky_clip[6] =
{
    {1, 1, 0},
    {1, -1, 0},
    {0, -1, 1},
    {0, 1, 1},
    {1, 0, 1},
    { -1, 0, 1}
};

static F32	sky_mins[2][6], sky_maxs[2][6];
static F32	sky_min, sky_max;

/*
================
AddSkyPolygon
================
*/
static void AddSkyPolygon( S32 nump, vec3_t vecs )
{
    S32		i, j;
    vec3_t	v, av;
    F32	s, t, dv;
    S32		axis;
    F32*	vp;
    // s = [0]/[2], t = [1]/[2]
    static S32	vec_to_st[6][3] =
    {
        { -2, 3, 1},
        {2, 3, -1},
        
        {1, 3, 2},
        { -1, 3, -2},
        
        { -2, -1, 3},
        { -2, 1, -3}
        
        //	{-1,2,3},
        //	{1,2,-3}
    };
    
    // decide which face it maps to
    VectorCopy( vec3_origin, v );
    for( i = 0, vp = vecs ; i < nump ; i++, vp += 3 )
    {
        VectorAdd( vp, v, v );
    }
    av[0] = fabs( v[0] );
    av[1] = fabs( v[1] );
    av[2] = fabs( v[2] );
    if( av[0] > av[1] && av[0] > av[2] )
    {
        if( v[0] < 0 )
            axis = 1;
        else
            axis = 0;
    }
    else if( av[1] > av[2] && av[1] > av[0] )
    {
        if( v[1] < 0 )
            axis = 3;
        else
            axis = 2;
    }
    else
    {
        if( v[2] < 0 )
            axis = 5;
        else
            axis = 4;
    }
    
    // project new texture coords
    for( i = 0 ; i < nump ; i++, vecs += 3 )
    {
        j = vec_to_st[axis][2];
        if( j > 0 )
            dv = vecs[j - 1];
        else
            dv = -vecs[-j - 1];
        if( dv < 0.001 )
            continue;	// don't divide by zero
        j = vec_to_st[axis][0];
        if( j < 0 )
            s = -vecs[-j - 1] / dv;
        else
            s = vecs[j - 1] / dv;
        j = vec_to_st[axis][1];
        if( j < 0 )
            t = -vecs[-j - 1] / dv;
        else
            t = vecs[j - 1] / dv;
            
        if( s < sky_mins[0][axis] )
            sky_mins[0][axis] = s;
        if( t < sky_mins[1][axis] )
            sky_mins[1][axis] = t;
        if( s > sky_maxs[0][axis] )
            sky_maxs[0][axis] = s;
        if( t > sky_maxs[1][axis] )
            sky_maxs[1][axis] = t;
    }
}

#define	ON_EPSILON		0.1f			// point on plane side epsilon
#define	MAX_CLIP_VERTS	64
/*
================
ClipSkyPolygon
================
*/
static void ClipSkyPolygon( S32 nump, vec3_t vecs, S32 stage )
{
    F32*	norm;
    F32*	v;
    bool	front, back;
    F32	d, e;
    F32	dists[MAX_CLIP_VERTS];
    S32		sides[MAX_CLIP_VERTS];
    vec3_t	newv[2][MAX_CLIP_VERTS];
    S32		newc[2];
    S32		i, j;
    
    if( nump > MAX_CLIP_VERTS - 2 )
        Com_Error( ERR_DROP, "ClipSkyPolygon: MAX_CLIP_VERTS" );
    if( stage == 6 )
    {
        // fully clipped, so draw it
        AddSkyPolygon( nump, vecs );
        return;
    }
    
    front = back = false;
    norm = sky_clip[stage];
    for( i = 0, v = vecs ; i < nump ; i++, v += 3 )
    {
        d = DotProduct( v, norm );
        if( d > ON_EPSILON )
        {
            front = true;
            sides[i] = SIDE_FRONT;
        }
        else if( d < -ON_EPSILON )
        {
            back = true;
            sides[i] = SIDE_BACK;
        }
        else
            sides[i] = SIDE_ON;
        dists[i] = d;
    }
    
    if( !front || !back )
    {
        // not clipped
        ClipSkyPolygon( nump, vecs, stage + 1 );
        return;
    }
    
    // clip it
    sides[i] = sides[0];
    dists[i] = dists[0];
    VectorCopy( vecs, ( vecs + ( i * 3 ) ) );
    newc[0] = newc[1] = 0;
    
    for( i = 0, v = vecs ; i < nump ; i++, v += 3 )
    {
        switch( sides[i] )
        {
            case SIDE_FRONT:
                VectorCopy( v, newv[0][newc[0]] );
                newc[0]++;
                break;
            case SIDE_BACK:
                VectorCopy( v, newv[1][newc[1]] );
                newc[1]++;
                break;
            case SIDE_ON:
                VectorCopy( v, newv[0][newc[0]] );
                newc[0]++;
                VectorCopy( v, newv[1][newc[1]] );
                newc[1]++;
                break;
        }
        
        if( sides[i] == SIDE_ON || sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i] )
            continue;
            
        d = dists[i] / ( dists[i] - dists[i + 1] );
        for( j = 0 ; j < 3 ; j++ )
        {
            e = v[j] + d * ( v[j + 3] - v[j] );
            newv[0][newc[0]][j] = e;
            newv[1][newc[1]][j] = e;
        }
        newc[0]++;
        newc[1]++;
    }
    
    // continue
    ClipSkyPolygon( newc[0], newv[0][0], stage + 1 );
    ClipSkyPolygon( newc[1], newv[1][0], stage + 1 );
}

/*
==============
ClearSkyBox
==============
*/
static void ClearSkyBox( void )
{
    S32		i;
    
    for( i = 0 ; i < 6 ; i++ )
    {
        sky_mins[0][i] = sky_mins[1][i] = 9999;
        sky_maxs[0][i] = sky_maxs[1][i] = -9999;
    }
}

/*
================
RB_ClipSkyPolygons
================
*/
void RB_ClipSkyPolygons( shaderCommands_t* input )
{
    vec3_t		p[5];	// need one extra point for clipping
    S32			i, j;
    
    ClearSkyBox();
    
    for( i = 0; i < input->numIndexes; i += 3 )
    {
        for( j = 0 ; j < 3 ; j++ )
        {
            VectorSubtract( input->xyz[input->indexes[i + j]],
                            backEnd.viewParms.orientation.origin,
                            p[j] );
        }
        ClipSkyPolygon( 3, p[0], 0 );
    }
}

/*
===================================================================================

CLOUD VERTEX GENERATION

===================================================================================
*/

/*
** MakeSkyVec
**
** Parms: s, t range from -1 to 1
*/
static void MakeSkyVec( F32 s, F32 t, S32 axis, F32 outSt[2], vec3_t outXYZ )
{
    // 1 = s, 2 = t, 3 = 2048
    static S32	st_to_vec[6][3] =
    {
        {3, -1, 2},
        { -3, 1, 2},
        
        {1, 3, 2},
        { -1, -3, 2},
        
        { -2, -1, 3},		// 0 degrees yaw, look straight up
        {2, -1, -3}		// look straight down
    };
    
    vec3_t		b;
    S32			j, k;
    F32	boxSize;
    
    boxSize = backEnd.viewParms.zFar / 1.75;		// div sqrt(3)
    b[0] = s * boxSize;
    b[1] = t * boxSize;
    b[2] = boxSize;
    
    for( j = 0 ; j < 3 ; j++ )
    {
        k = st_to_vec[axis][j];
        if( k < 0 )
        {
            outXYZ[j] = -b[-k - 1];
        }
        else
        {
            outXYZ[j] = b[k - 1];
        }
    }
    
    // avoid bilerp seam
    s = ( s + 1 ) * 0.5;
    t = ( t + 1 ) * 0.5;
    if( s < sky_min )
    {
        s = sky_min;
    }
    else if( s > sky_max )
    {
        s = sky_max;
    }
    
    if( t < sky_min )
    {
        t = sky_min;
    }
    else if( t > sky_max )
    {
        t = sky_max;
    }
    
    t = 1.0 - t;
    
    
    if( outSt )
    {
        outSt[0] = s;
        outSt[1] = t;
    }
}

static S32	sky_texorder[6] = {0, 2, 1, 3, 4, 5};
static vec3_t	s_skyPoints[SKY_SUBDIVISIONS + 1][SKY_SUBDIVISIONS + 1];
static F32	s_skyTexCoords[SKY_SUBDIVISIONS + 1][SKY_SUBDIVISIONS + 1][2];

static void DrawSkySide( struct image_s* image, const S32 mins[2], const S32 maxs[2] )
{
    S32 s, t;
    S32 firstVertex = tess.numVertexes;
    //S32 firstIndex = tess.numIndexes;
    vec4_t color;
    
    //tess.numVertexes = 0;
    //tess.numIndexes = 0;
    tess.firstIndex = tess.numIndexes;
    
    GL_BindToTMU( image, TB_COLORMAP );
    GL_Cull( CT_TWO_SIDED );
    
    for( t = mins[1] + HALF_SKY_SUBDIVISIONS; t <= maxs[1] + HALF_SKY_SUBDIVISIONS; t++ )
    {
        for( s = mins[0] + HALF_SKY_SUBDIVISIONS; s <= maxs[0] + HALF_SKY_SUBDIVISIONS; s++ )
        {
            tess.xyz[tess.numVertexes][0] = s_skyPoints[t][s][0];
            tess.xyz[tess.numVertexes][1] = s_skyPoints[t][s][1];
            tess.xyz[tess.numVertexes][2] = s_skyPoints[t][s][2];
            tess.xyz[tess.numVertexes][3] = 1.0;
            
            tess.texCoords[tess.numVertexes][0] = s_skyTexCoords[t][s][0];
            tess.texCoords[tess.numVertexes][1] = s_skyTexCoords[t][s][1];
            
            tess.numVertexes++;
            
            if( tess.numVertexes >= SHADER_MAX_VERTEXES )
            {
                Com_Error( ERR_DROP, "SHADER_MAX_VERTEXES hit in DrawSkySideVBO()" );
            }
        }
    }
    
    for( t = 0; t < maxs[1] - mins[1]; t++ )
    {
        for( s = 0; s < maxs[0] - mins[0]; s++ )
        {
            if( tess.numIndexes + 6 >= SHADER_MAX_INDEXES )
            {
                Com_Error( ERR_DROP, "SHADER_MAX_INDEXES hit in DrawSkySideVBO()" );
            }
            
            tess.indexes[tess.numIndexes++] =  s +       t      * ( maxs[0] - mins[0] + 1 ) + firstVertex;
            tess.indexes[tess.numIndexes++] =  s + ( t + 1 ) * ( maxs[0] - mins[0] + 1 ) + firstVertex;
            tess.indexes[tess.numIndexes++] = ( s + 1 ) +  t      * ( maxs[0] - mins[0] + 1 ) + firstVertex;
            
            tess.indexes[tess.numIndexes++] = ( s + 1 ) +  t      * ( maxs[0] - mins[0] + 1 ) + firstVertex;
            tess.indexes[tess.numIndexes++] =  s + ( t + 1 ) * ( maxs[0] - mins[0] + 1 ) + firstVertex;
            tess.indexes[tess.numIndexes++] = ( s + 1 ) + ( t + 1 ) * ( maxs[0] - mins[0] + 1 ) + firstVertex;
        }
    }
    
    // FIXME: A lot of this can probably be removed for speed, and refactored into a more convenient function
    RB_UpdateTessVao( ATTR_POSITION | ATTR_TEXCOORD );
    /*
    	{
    		shaderProgram_t *sp = &tr.textureColorShader;
    
    		GLSL_VertexAttribsState(ATTR_POSITION | ATTR_TEXCOORD);
    		GLSL_BindProgram(sp);
    
    		GLSL_SetUniformMat4(sp, UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection);
    
    		color[0] =
    		color[1] =
    		color[2] = tr.identityLight;
    		color[3] = 1.0f;
    		GLSL_SetUniformVec4(sp, UNIFORM_COLOR, color);
    	}
    */
    {
        shaderProgram_t* sp = &tr.shadowPassShader;// &tr.lightallShader[0];
        vec4_t vector;
        
        //GLSL_VertexAttribsState(ATTR_POSITION | ATTR_TEXCOORD0 | ATTR_NORMAL | ATTR_TANGENT);
        GLSL_BindProgram( sp );
        
        VectorSet4( vector, 0.0, 0.0, 0.0, 1024.0 );
        GLSL_SetUniformVec4( sp, UNIFORM_LOCAL1, vector ); // parallaxScale, hasSpecular, specularScale, materialType
        VectorSet4( vector, 0.0, 0.0, 0.0, 0.0 );
        GLSL_SetUniformVec4( sp, UNIFORM_LOCAL4, vector );
        GLSL_SetUniformVec4( sp, UNIFORM_LOCAL5, vector );
        
        {
            // Set up basic shader settings... This way we can avoid the bind bloat of dumb vert shader #ifdefs...
            GLSL_SetUniformVec4( sp, UNIFORM_SETTINGS0, vector );
            GLSL_SetUniformVec4( sp, UNIFORM_SETTINGS1, vector );
            GLSL_SetUniformVec4( sp, UNIFORM_SETTINGS2, vector );
            GLSL_SetUniformVec4( sp, UNIFORM_SETTINGS3, vector );
        }
        
        GLSL_SetUniformVec4( sp, UNIFORM_ENABLETEXTURES, vector );
        
        VectorSet4( vector, r_baseNormalX->value, r_baseNormalY->value, 1.0f, r_baseParallax->value );
        GLSL_SetUniformVec4( sp, UNIFORM_NORMALSCALE, vector );
        
        GLSL_SetUniformMat4( sp, UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection );
        
        color[0] =
            color[1] =
                color[2] = backEnd.refdef.colorScale;
        color[3] = 1.0f;
        GLSL_SetUniformVec4( sp, UNIFORM_BASECOLOR, color );
        
        color[0] =
            color[1] =
                color[2] =
                    color[3] = 0.0f;
        GLSL_SetUniformVec4( sp, UNIFORM_VERTCOLOR, color );
        
        VectorSet4( vector, 1.0, 0.0, 0.0, 1.0 );
        GLSL_SetUniformVec4( sp, UNIFORM_DIFFUSETEXMATRIX, vector );
        
        VectorSet4( vector, 0.0, 0.0, 0.0, 0.0 );
        GLSL_SetUniformVec4( sp, UNIFORM_DIFFUSETEXOFFTURB, vector );
        
        GLSL_SetUniformInt( sp, UNIFORM_ALPHATEST, 0 );
    }
    
    R_DrawElements( tess.numIndexes - tess.firstIndex, tess.firstIndex, false );
    
    tess.numIndexes = tess.firstIndex;
    tess.numVertexes = firstVertex;
    tess.firstIndex = 0;
}

static void DrawSkyBox( shader_t* shader )
{
    S32		i;
    
    sky_min = 0;
    sky_max = 1;
    
    ::memset( s_skyTexCoords, 0, sizeof( s_skyTexCoords ) );
    
    for( i = 0 ; i < 6 ; i++ )
    {
        S32 sky_mins_subd[2], sky_maxs_subd[2];
        S32 s, t;
        
        sky_mins[0][i] = floor( sky_mins[0][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
        sky_mins[1][i] = floor( sky_mins[1][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
        sky_maxs[0][i] = ceil( sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
        sky_maxs[1][i] = ceil( sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
        
        if( ( sky_mins[0][i] >= sky_maxs[0][i] ) ||
                ( sky_mins[1][i] >= sky_maxs[1][i] ) )
        {
            continue;
        }
        
        sky_mins_subd[0] = sky_mins[0][i] * HALF_SKY_SUBDIVISIONS;
        sky_mins_subd[1] = sky_mins[1][i] * HALF_SKY_SUBDIVISIONS;
        sky_maxs_subd[0] = sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS;
        sky_maxs_subd[1] = sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS;
        
        if( sky_mins_subd[0] < -HALF_SKY_SUBDIVISIONS )
            sky_mins_subd[0] = -HALF_SKY_SUBDIVISIONS;
        else if( sky_mins_subd[0] > HALF_SKY_SUBDIVISIONS )
            sky_mins_subd[0] = HALF_SKY_SUBDIVISIONS;
        if( sky_mins_subd[1] < -HALF_SKY_SUBDIVISIONS )
            sky_mins_subd[1] = -HALF_SKY_SUBDIVISIONS;
        else if( sky_mins_subd[1] > HALF_SKY_SUBDIVISIONS )
            sky_mins_subd[1] = HALF_SKY_SUBDIVISIONS;
            
        if( sky_maxs_subd[0] < -HALF_SKY_SUBDIVISIONS )
            sky_maxs_subd[0] = -HALF_SKY_SUBDIVISIONS;
        else if( sky_maxs_subd[0] > HALF_SKY_SUBDIVISIONS )
            sky_maxs_subd[0] = HALF_SKY_SUBDIVISIONS;
        if( sky_maxs_subd[1] < -HALF_SKY_SUBDIVISIONS )
            sky_maxs_subd[1] = -HALF_SKY_SUBDIVISIONS;
        else if( sky_maxs_subd[1] > HALF_SKY_SUBDIVISIONS )
            sky_maxs_subd[1] = HALF_SKY_SUBDIVISIONS;
            
        //
        // iterate through the subdivisions
        //
        for( t = sky_mins_subd[1] + HALF_SKY_SUBDIVISIONS; t <= sky_maxs_subd[1] + HALF_SKY_SUBDIVISIONS; t++ )
        {
            for( s = sky_mins_subd[0] + HALF_SKY_SUBDIVISIONS; s <= sky_maxs_subd[0] + HALF_SKY_SUBDIVISIONS; s++ )
            {
                MakeSkyVec( ( s - HALF_SKY_SUBDIVISIONS ) / ( F32 ) HALF_SKY_SUBDIVISIONS,
                            ( t - HALF_SKY_SUBDIVISIONS ) / ( F32 ) HALF_SKY_SUBDIVISIONS,
                            i,
                            s_skyTexCoords[t][s],
                            s_skyPoints[t][s] );
            }
        }
        
        DrawSkySide( shader->sky.outerbox[sky_texorder[i]],
                     sky_mins_subd,
                     sky_maxs_subd );
    }
    
}

static void FillCloudySkySide( const S32 mins[2], const S32 maxs[2], bool addIndexes )
{
    S32 s, t;
    S32 vertexStart = tess.numVertexes;
    S32 tHeight, sWidth;
    
    tHeight = maxs[1] - mins[1] + 1;
    sWidth = maxs[0] - mins[0] + 1;
    
    for( t = mins[1] + HALF_SKY_SUBDIVISIONS; t <= maxs[1] + HALF_SKY_SUBDIVISIONS; t++ )
    {
        for( s = mins[0] + HALF_SKY_SUBDIVISIONS; s <= maxs[0] + HALF_SKY_SUBDIVISIONS; s++ )
        {
            VectorAdd( s_skyPoints[t][s], backEnd.viewParms.orientation.origin, tess.xyz[tess.numVertexes] );
            tess.texCoords[tess.numVertexes][0] = s_skyTexCoords[t][s][0];
            tess.texCoords[tess.numVertexes][1] = s_skyTexCoords[t][s][1];
            
            tess.numVertexes++;
            
            if( tess.numVertexes >= SHADER_MAX_VERTEXES )
            {
                Com_Error( ERR_DROP, "SHADER_MAX_VERTEXES hit in FillCloudySkySide()" );
            }
        }
    }
    
    // only add indexes for one pass, otherwise it would draw multiple times for each pass
    if( addIndexes )
    {
        for( t = 0; t < tHeight - 1; t++ )
        {
            for( s = 0; s < sWidth - 1; s++ )
            {
                tess.indexes[tess.numIndexes] = vertexStart + s + t * ( sWidth );
                tess.numIndexes++;
                tess.indexes[tess.numIndexes] = vertexStart + s + ( t + 1 ) * ( sWidth );
                tess.numIndexes++;
                tess.indexes[tess.numIndexes] = vertexStart + s + 1 + t * ( sWidth );
                tess.numIndexes++;
                
                tess.indexes[tess.numIndexes] = vertexStart + s + ( t + 1 ) * ( sWidth );
                tess.numIndexes++;
                tess.indexes[tess.numIndexes] = vertexStart + s + 1 + ( t + 1 ) * ( sWidth );
                tess.numIndexes++;
                tess.indexes[tess.numIndexes] = vertexStart + s + 1 + t * ( sWidth );
                tess.numIndexes++;
            }
        }
    }
}

static void FillCloudBox( const shader_t* shader, S32 stage )
{
    S32 i;
    
    for( i = 0; i < 6; i++ )
    {
        S32 sky_mins_subd[2], sky_maxs_subd[2];
        S32 s, t;
        F32 MIN_T;
        
        if( 1 )  // FIXME? shader->sky.fullClouds )
        {
            MIN_T = -HALF_SKY_SUBDIVISIONS;
            
            // still don't want to draw the bottom, even if fullClouds
            if( i == 5 )
                continue;
        }
        else
        {
            switch( i )
            {
                case 0:
                case 1:
                case 2:
                case 3:
                    MIN_T = -1;
                    break;
                case 5:
                    // don't draw clouds beneath you
                    continue;
                case 4:		// top
                default:
                    MIN_T = -HALF_SKY_SUBDIVISIONS;
                    break;
            }
        }
        
        sky_mins[0][i] = floor( sky_mins[0][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
        sky_mins[1][i] = floor( sky_mins[1][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
        sky_maxs[0][i] = ceil( sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
        sky_maxs[1][i] = ceil( sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
        
        if( ( sky_mins[0][i] >= sky_maxs[0][i] ) ||
                ( sky_mins[1][i] >= sky_maxs[1][i] ) )
        {
            continue;
        }
        
        sky_mins_subd[0] = static_cast<S32>( sky_mins[0][i] * HALF_SKY_SUBDIVISIONS );
        sky_mins_subd[1] = static_cast<S32>( sky_mins[1][i] * HALF_SKY_SUBDIVISIONS );
        sky_maxs_subd[0] = static_cast<S32>( sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS );
        sky_maxs_subd[1] = static_cast<S32>( sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS );
        
        if( sky_mins_subd[0] < -HALF_SKY_SUBDIVISIONS )
            sky_mins_subd[0] = -HALF_SKY_SUBDIVISIONS;
        else if( sky_mins_subd[0] > HALF_SKY_SUBDIVISIONS )
            sky_mins_subd[0] = HALF_SKY_SUBDIVISIONS;
        if( sky_mins_subd[1] < MIN_T )
            sky_mins_subd[1] = MIN_T;
        else if( sky_mins_subd[1] > HALF_SKY_SUBDIVISIONS )
            sky_mins_subd[1] = HALF_SKY_SUBDIVISIONS;
            
        if( sky_maxs_subd[0] < -HALF_SKY_SUBDIVISIONS )
            sky_maxs_subd[0] = -HALF_SKY_SUBDIVISIONS;
        else if( sky_maxs_subd[0] > HALF_SKY_SUBDIVISIONS )
            sky_maxs_subd[0] = HALF_SKY_SUBDIVISIONS;
        if( sky_maxs_subd[1] < MIN_T )
            sky_maxs_subd[1] = MIN_T;
        else if( sky_maxs_subd[1] > HALF_SKY_SUBDIVISIONS )
            sky_maxs_subd[1] = HALF_SKY_SUBDIVISIONS;
            
        //
        // iterate through the subdivisions
        //
        for( t = sky_mins_subd[1] + HALF_SKY_SUBDIVISIONS; t <= sky_maxs_subd[1] + HALF_SKY_SUBDIVISIONS; t++ )
        {
            for( s = sky_mins_subd[0] + HALF_SKY_SUBDIVISIONS; s <= sky_maxs_subd[0] + HALF_SKY_SUBDIVISIONS; s++ )
            {
                MakeSkyVec( ( s - HALF_SKY_SUBDIVISIONS ) / ( F32 ) HALF_SKY_SUBDIVISIONS,
                            ( t - HALF_SKY_SUBDIVISIONS ) / ( F32 ) HALF_SKY_SUBDIVISIONS,
                            i,
                            NULL,
                            s_skyPoints[t][s] );
                            
                s_skyTexCoords[t][s][0] = s_cloudTexCoords[i][t][s][0];
                s_skyTexCoords[t][s][1] = s_cloudTexCoords[i][t][s][1];
            }
        }
        
        // only add indexes for first stage
        FillCloudySkySide( sky_mins_subd, sky_maxs_subd, ( stage == 0 ) );
    }
}

/*
** R_BuildCloudData
*/
void R_BuildCloudData( shaderCommands_t* input )
{
    S32			i;
    shader_t*	shader;
    
    shader = input->shader;
    
    assert( shader->isSky );
    
    sky_min = 1.0 / 256.0f;		// FIXME: not correct?
    sky_max = 255.0 / 256.0f;
    
    // set up for drawing
    tess.numIndexes = 0;
    tess.numVertexes = 0;
    tess.firstIndex = 0;
    
    if( shader->sky.cloudHeight )
    {
        for( i = 0; i < MAX_SHADER_STAGES; i++ )
        {
            if( !tess.xstages[i] )
            {
                break;
            }
            FillCloudBox( shader, i );
        }
    }
}

/*
** R_InitSkyTexCoords
** Called when a sky shader is parsed
*/
//#define SQR( a ) ((a)*(a))
void R_InitSkyTexCoords( F32 heightCloud )
{
    S32 i, s, t;
    F32 radiusWorld = 4096;
    F32 p;
    F32 sRad, tRad;
    vec3_t skyVec;
    vec3_t v;
    
    // init zfar so MakeSkyVec works even though
    // a world hasn't been bounded
    backEnd.viewParms.zFar = 1024;
    
    for( i = 0; i < 6; i++ )
    {
        for( t = 0; t <= SKY_SUBDIVISIONS; t++ )
        {
            for( s = 0; s <= SKY_SUBDIVISIONS; s++ )
            {
                // compute vector from view origin to sky side integral point
                MakeSkyVec( ( s - HALF_SKY_SUBDIVISIONS ) / ( F32 ) HALF_SKY_SUBDIVISIONS,
                            ( t - HALF_SKY_SUBDIVISIONS ) / ( F32 ) HALF_SKY_SUBDIVISIONS,
                            i,
                            NULL,
                            skyVec );
                            
                // compute parametric value 'p' that intersects with cloud layer
                p = ( 1.0f / ( 2 * DotProduct( skyVec, skyVec ) ) ) *
                    ( -2 * skyVec[2] * radiusWorld +
                      2 * sqrt( SQR( skyVec[2] ) * SQR( radiusWorld ) +
                                2 * SQR( skyVec[0] ) * radiusWorld * heightCloud +
                                SQR( skyVec[0] ) * SQR( heightCloud ) +
                                2 * SQR( skyVec[1] ) * radiusWorld * heightCloud +
                                SQR( skyVec[1] ) * SQR( heightCloud ) +
                                2 * SQR( skyVec[2] ) * radiusWorld * heightCloud +
                                SQR( skyVec[2] ) * SQR( heightCloud ) ) );
                                
                s_cloudTexP[i][t][s] = p;
                
                // compute intersection point based on p
                VectorScale( skyVec, p, v );
                v[2] += radiusWorld;
                
                // compute vector from world origin to intersection point 'v'
                VectorNormalize( v );
                
                sRad = Q_acos( v[0] );
                tRad = Q_acos( v[1] );
                
                s_cloudTexCoords[i][t][s][0] = sRad;
                s_cloudTexCoords[i][t][s][1] = tRad;
            }
        }
    }
}

//======================================================================================

vec3_t SUN_POSITION;
vec2_t SUN_SCREEN_POSITION;
bool SUN_VISIBLE = false;

extern void R_WorldToLocal( const vec3_t world, vec3_t local );
extern void TR_AxisToAngles( const vec3_t axis[3], vec3_t angles );

bool SUN_InFOV( vec3_t spot )
{
    vec3_t from;
    vec3_t deltaVector, angles, deltaAngles;
    vec3_t fromAnglesCopy;
    vec3_t fromAngles;
    S32 hFOV = backEnd.refdef.fov_x * 1.1;
    S32 vFOV = backEnd.refdef.fov_y * 1.1;
    
    VectorCopy( backEnd.refdef.vieworg, from );
    
    TR_AxisToAngles( tr.refdef.viewaxis, fromAngles );
    
    VectorSubtract( spot, from, deltaVector );
    vectoangles( deltaVector, angles );
    VectorCopy( fromAngles, fromAnglesCopy );
    
    deltaAngles[PITCH] = AngleDelta( fromAnglesCopy[PITCH], angles[PITCH] );
    deltaAngles[YAW] = AngleDelta( fromAnglesCopy[YAW], angles[YAW] );
    
    if( fabs( deltaAngles[PITCH] ) <= vFOV && fabs( deltaAngles[YAW] ) <= hFOV )
    {
        return true;
    }
    
    return false;
}

extern vec3_t VOLUMETRIC_ROOF;

extern bool RB_UpdateSunFlareVis( void );
extern bool Volumetric_Visible( vec3_t from, vec3_t to, bool isSun );
extern void Volumetric_RoofHeight( vec3_t from );

/*
** RB_DrawSun
*/
void RB_DrawSun( F32 scale, shader_t* shader )
{
    F32		size;
    F32		dist;
    vec3_t		origin, vec1, vec2;
    
    if( !backEnd.skyRenderedThisView )
    {
        return;
    }
    
    //qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );
    //qglTranslatef (backEnd.viewParms.orientation.origin[0], backEnd.viewParms.orientation.origin[1], backEnd.viewParms.ori.origin[2]);
    {
        // FIXME: this could be a lot cleaner
        matrix_t translation, modelview;
        
        Mat4Translation( backEnd.viewParms.orientation.origin, translation );
        Mat4Multiply( backEnd.viewParms.world.modelMatrix, translation, modelview );
        GL_SetModelviewMatrix( modelview );
    }
    
    dist = backEnd.viewParms.zFar / 1.75;		// div sqrt(3)
    size = dist * scale;
    
    if( r_proceduralSun->integer )
    {
        size *= r_proceduralSunScale->value;
    }
    
    //VectorSet(tr.sunDirection, r_testshaderValue1->value, r_testshaderValue2->value, r_testshaderValue3->value);
    VectorScale( tr.sunDirection, dist, origin );
    
    PerpendicularVector( vec1, tr.sunDirection );
    CrossProduct( tr.sunDirection, vec1, vec2 );
    
    VectorScale( vec1, size, vec1 );
    VectorScale( vec2, size, vec2 );
    
    // farthest depth range
    qglDepthRange( 1.0, 1.0 );
    
    RB_BeginSurface( shader, 0, 0 );
    
    RB_AddQuadStamp( origin, vec1, vec2, tr.refdef.sunAmbCol/*colorWhite*/ );
    
    RB_EndSurface();
    
    // back to normal depth range
    qglDepthRange( 0.0, 1.0 );
    
    if( r_volumelight->integer )
    {
        // Lets have some volumetrics with that!
        const F32 cutoff = 0.25f;
        F32 dot = DotProduct( tr.sunDirection, backEnd.viewParms.orientation.axis[0] );
        
        F32 dist;
        vec4_t pos, hpos;
        matrix_t trans, model, mvp;
        
        Mat4Translation( backEnd.viewParms.orientation.origin, trans );
        Mat4Multiply( backEnd.viewParms.world.modelMatrix, trans, model );
        Mat4Multiply( backEnd.viewParms.projectionMatrix, model, mvp );
        
        //dist = backEnd.viewParms.zFar / 1.75;		// div sqrt(3)
        dist = 4096.0;
        
        VectorScale( tr.sunDirection, dist, pos );
        
        VectorCopy( pos, SUN_POSITION );
        
        // project sun point
        Mat4Transform( mvp, pos, hpos );
        
        // transform to UV coords
        hpos[3] = 0.5f / hpos[3];
        
        pos[0] = 0.5f + hpos[0] * hpos[3];
        pos[1] = 0.5f + hpos[1] * hpos[3];
        
        VectorCopy( pos, SUN_SCREEN_POSITION );
        
        if( dot < cutoff )
        {
            SUN_VISIBLE = false;
            return;
        }
        
        if( !RB_UpdateSunFlareVis() )
        {
            SUN_VISIBLE = false;
            return;
        }
        
        if( !Volumetric_Visible( backEnd.refdef.vieworg, SUN_POSITION, true ) )
        {
            // Trace to actual position failed... Try above...
            vec3_t tmpOrg;
            vec3_t eyeOrg;
            vec3_t tmpRoof;
            vec3_t eyeRoof;
            
            // Calculate ceiling heights at both positions...
            //Volumetric_RoofHeight(SUN_POSITION);
            //VectorCopy(VOLUMETRIC_ROOF, tmpRoof);
            //Volumetric_RoofHeight(backEnd.refdef.vieworg);
            //VectorCopy(VOLUMETRIC_ROOF, eyeRoof);
            
            VectorSet( tmpRoof, SUN_POSITION[0], SUN_POSITION[1], SUN_POSITION[2] + 512.0 );
            VectorSet( eyeRoof, backEnd.refdef.vieworg[0], backEnd.refdef.vieworg[1], backEnd.refdef.vieworg[2] + 128.0 );
            
            VectorSet( tmpOrg, tmpRoof[0], SUN_POSITION[1], SUN_POSITION[2] );
            VectorSet( eyeOrg, backEnd.refdef.vieworg[0], backEnd.refdef.vieworg[1], backEnd.refdef.vieworg[2] );
            if( !Volumetric_Visible( eyeOrg, tmpOrg, true ) )
            {
                // Trace to above position failed... Try trace from above viewer...
                VectorSet( tmpOrg, SUN_POSITION[0], SUN_POSITION[1], SUN_POSITION[2] );
                VectorSet( eyeOrg, eyeRoof[0], backEnd.refdef.vieworg[1], backEnd.refdef.vieworg[2] );
                if( !Volumetric_Visible( eyeOrg, tmpOrg, true ) )
                {
                    // Trace from above viewer failed... Try trace from above, to above...
                    VectorSet( tmpOrg, tmpRoof[0], SUN_POSITION[1], SUN_POSITION[2] );
                    VectorSet( eyeOrg, eyeRoof[0], backEnd.refdef.vieworg[1], backEnd.refdef.vieworg[2] );
                    if( !Volumetric_Visible( eyeOrg, tmpOrg, true ) )
                    {
                        // Trace from/to above viewer failed...
                        SUN_VISIBLE = false;
                        return; // Can't see this...
                    }
                }
            }
        }
        
        SUN_VISIBLE = true;
    }
}

void DrawSkyDome( shader_t* skyShader )
{
    vec4_t color;
    
    // bloom
    color[0] =
        color[1] =
            color[2] = pow( 2, r_cameraExposure->value );
    color[3] = 1.0f;
    
    GLSL_BindProgram( &tr.skyShader );
    GL_BindToTMU( skyShader->sky.outerbox[0], TB_LEVELSMAP );
    
    GLSL_SetUniformMat4( &tr.skyShader, UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection );
    GLSL_SetUniformMat4( &tr.skyShader, UNIFORM_INVPROJECTIONMATRIX, glState.invProjection );
    GLSL_SetUniformFloat( &tr.skyShader, UNIFORM_TIME, backEnd.refdef.floatTime );
    GLSL_SetUniformVec3( &tr.skyShader, UNIFORM_VIEWORIGIN, backEnd.refdef.vieworg );
    
    vec4_t vec;
    VectorCopy( backEnd.currentEntity->lightDir, vec );
    vec[3] = 0.0f;
    GLSL_SetUniformVec4( &tr.skyShader, UNIFORM_LIGHTORIGIN, vec );
    
    if( skyShader->sky.outerbox[0] )
    {
        vec2_t screensize;
        screensize[0] = skyShader->sky.outerbox[0]->width;
        screensize[1] = skyShader->sky.outerbox[0]->height;
        
        GLSL_SetUniformVec2( &tr.skyShader, UNIFORM_DIMENSIONS, screensize );
        
        vec4_t		imageBox;
        imageBox[0] = 0;
        imageBox[1] = 0;
        imageBox[2] = skyShader->sky.outerbox[0]->width;
        imageBox[3] = skyShader->sky.outerbox[0]->height;
        
        ivec4_t		screenBox;
        screenBox[0] = 0;
        screenBox[1] = 0;
        screenBox[2] = glConfig.vidWidth;
        screenBox[3] = glConfig.vidHeight;
        
        FBO_BlitFromTexture( skyShader->sky.outerbox[0], imageBox, NULL, glState.currentFBO, screenBox, &tr.skyShader, NULL, 0 );
    }
    else
    {
        vec4_t		imageBox;
        imageBox[0] = 0;
        imageBox[1] = 0;
        imageBox[2] = tr.whiteImage->width;
        imageBox[3] = tr.whiteImage->height;
        
        ivec4_t		screenBox;
        screenBox[0] = 0;
        screenBox[1] = 0;
        screenBox[2] = glConfig.vidWidth;
        screenBox[3] = glConfig.vidHeight;
        
        FBO_BlitFromTexture( tr.whiteImage, imageBox, NULL, glState.currentFBO, screenBox, &tr.skyShader, NULL, 0 );
    }
}

/*
================
RB_StageIteratorSky

All of the visible sky triangles are in tess

Other things could be stuck in here, like birds in the sky, etc
================
*/
image_t* skyImage = NULL;

void RB_StageIteratorSky( void )
{
    if( r_fastsky->integer )
    {
        return;
    }
    
    // go through all the polygons and project them onto
    // the sky box to see which blocks on each side need
    // to be drawn
    RB_ClipSkyPolygons( &tess );
    
    // r_showsky will let all the sky blocks be drawn in
    // front of everything to allow developers to see how
    // much sky is getting sucked in
    if( r_showsky->integer )
    {
        qglDepthRange( 0.0, 0.0 );
    }
    else
    {
        qglDepthRange( 1.0, 1.0 );
    }
    
    if( !tess.shader->sky.outerbox[0] || tess.shader->sky.outerbox[0] == tr.defaultImage )
    {
        // Set a default image...
        GL_State( 0 );
        DrawSkyDome( tess.shader );
    }
    else
        // draw the outer skybox
        //if( tess.shader->sky.outerbox[0] && tess.shader->sky.outerbox[0] != tr.defaultImage )
    {
        mat4_t oldmodelview;
        
        GL_State( 0 );
        GL_Cull( CT_FRONT_SIDED );
        //glTranslatef (backEnd.viewParms.orientation.origin[0], backEnd.viewParms.orientation.origin[1], backEnd.viewParms.orientation.origin[2]);
        
        {
            // FIXME: this could be a lot cleaner
            mat4_t trans, product;
            
            Mat4Copy( glState.modelview, oldmodelview );
            Mat4Translation( backEnd.viewParms.orientation.origin, trans );
            Mat4Multiply( glState.modelview, trans, product );
            GL_SetModelviewMatrix( product );
            
        }
        
        DrawSkyBox( tess.shader );
        
        GL_SetModelviewMatrix( oldmodelview );
    }
    
    // generate the vertexes for all the clouds, which will be drawn
    // by the generic shader routine
    R_BuildCloudData( &tess );
    
    RB_StageIteratorGeneric();
    
    // draw the inner skybox
    
    
    // back to normal depth range
    glDepthRange( 0.0, 1.0 );
    
    // note that sky was drawn so we will draw a sun later
    backEnd.skyRenderedThisView = true;
}
