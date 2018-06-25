////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2007-2009 Robert Beckebans <trebor_7@users.sourceforge.net>
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
// File name:   r_vbo.cpp
// Version:     v1.00
// Created:
// Compilers:   Visual Studio 2015
// Description:
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#include <OWLIb/precompiled.h>

void R_VaoPackTangent( S16* out, vec4_t v )
{
    out[0] = v[0] * 32767.0f + ( v[0] > 0.0f ? 0.5f : -0.5f );
    out[1] = v[1] * 32767.0f + ( v[1] > 0.0f ? 0.5f : -0.5f );
    out[2] = v[2] * 32767.0f + ( v[2] > 0.0f ? 0.5f : -0.5f );
    out[3] = v[3] * 32767.0f + ( v[3] > 0.0f ? 0.5f : -0.5f );
}

void R_VaoPackNormal( S16* out, vec3_t v )
{
    out[0] = v[0] * 32767.0f + ( v[0] > 0.0f ? 0.5f : -0.5f );
    out[1] = v[1] * 32767.0f + ( v[1] > 0.0f ? 0.5f : -0.5f );
    out[2] = v[2] * 32767.0f + ( v[2] > 0.0f ? 0.5f : -0.5f );
    out[3] = 0;
}

void R_VaoPackColor( U16* out, vec4_t c )
{
    out[0] = c[0] * 65535.0f + 0.5f;
    out[1] = c[1] * 65535.0f + 0.5f;
    out[2] = c[2] * 65535.0f + 0.5f;
    out[3] = c[3] * 65535.0f + 0.5f;
}

void R_VaoUnpackTangent( vec4_t v, S16* pack )
{
    v[0] = pack[0] / 32767.0f;
    v[1] = pack[1] / 32767.0f;
    v[2] = pack[2] / 32767.0f;
    v[3] = pack[3] / 32767.0f;
}

void R_VaoUnpackNormal( vec3_t v, S16* pack )
{
    v[0] = pack[0] / 32767.0f;
    v[1] = pack[1] / 32767.0f;
    v[2] = pack[2] / 32767.0f;
}

void Vao_SetVertexPointers( vao_t* vao )
{
    S32 attribIndex;
    
    // set vertex pointers
    for( attribIndex = 0; attribIndex < ATTR_INDEX_COUNT; attribIndex++ )
    {
        U32 attribBit = 1 << attribIndex;
        vaoAttrib_t* vAtb = &vao->attribs[attribIndex];
        
        if( vAtb->enabled )
        {
            glVertexAttribPointer( attribIndex, vAtb->count, vAtb->type, vAtb->normalized, vAtb->stride, BUFFER_OFFSET( vAtb->offset ) );
            if( glRefConfig.vertexArrayObject || !( glState.vertexAttribsEnabled & attribBit ) )
                glEnableVertexAttribArray( attribIndex );
                
            if( !glRefConfig.vertexArrayObject || vao == tess.vao )
                glState.vertexAttribsEnabled |= attribBit;
        }
        else
        {
            // don't disable vertex attribs when using vertex array objects
            // Vao_SetVertexPointers is only called during init when using VAOs, and vertex attribs start disabled anyway
            if( !glRefConfig.vertexArrayObject && ( glState.vertexAttribsEnabled & attribBit ) )
                glDisableVertexAttribArray( attribIndex );
                
            if( !glRefConfig.vertexArrayObject || vao == tess.vao )
                glState.vertexAttribsEnabled &= ~attribBit;
        }
    }
}

/*
============
R_CreateVao
============
*/
vao_t* R_CreateVao( StringEntry name, U8* vertexes, S32 vertexesSize, U8* indexes, S32 indexesSize, vaoUsage_t usage )
{
    vao_t*          vao;
    S32				glUsage;
    
    switch( usage )
    {
        case VAO_USAGE_STATIC:
            glUsage = GL_STATIC_DRAW;
            break;
            
        case VAO_USAGE_DYNAMIC:
            glUsage = GL_DYNAMIC_DRAW;
            break;
            
        default:
            Com_Error( ERR_FATAL, "bad vaoUsage_t given: %i", usage );
            return NULL;
    }
    
    if( strlen( name ) >= MAX_QPATH )
    {
        Com_Error( ERR_DROP, "R_CreateVao: \"%s\" is too long", name );
    }
    
    if( tr.numVaos == MAX_VAOS )
    {
        Com_Error( ERR_DROP, "R_CreateVao: MAX_VAOS hit" );
    }
    
    R_IssuePendingRenderCommands();
    
    vao = tr.vaos[tr.numVaos] = ( vao_t* )Hunk_Alloc( sizeof( *vao ), h_low );
    tr.numVaos++;
    
    memset( vao, 0, sizeof( *vao ) );
    
    Q_strncpyz( vao->name, name, sizeof( vao->name ) );
    
    
    if( glRefConfig.vertexArrayObject )
    {
        glGenVertexArrays( 1, &vao->vao );
        glBindVertexArray( vao->vao );
    }
    
    
    vao->vertexesSize = vertexesSize;
    
    glGenBuffers( 1, &vao->vertexesVBO );
    
    glBindBuffer( GL_ARRAY_BUFFER, vao->vertexesVBO );
    glBufferData( GL_ARRAY_BUFFER, vertexesSize, vertexes, glUsage );
    
    
    vao->indexesSize = indexesSize;
    
    glGenBuffers( 1, &vao->indexesIBO );
    
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, vao->indexesIBO );
    glBufferData( GL_ELEMENT_ARRAY_BUFFER, indexesSize, indexes, glUsage );
    
    
    glState.currentVao = vao;
    
    GL_CheckErrors();
    
    return vao;
}

/*
============
R_CreateVao2
============
*/
vao_t* R_CreateVao2( StringEntry name, S32 numVertexes, srfVert_t* verts, S32 numIndexes, U32* indexes )
{
    vao_t*          vao;
    S32             i;
    
    U8*           data;
    S32             dataSize;
    S32             dataOfs;
    
    S32				glUsage = GL_STATIC_DRAW;
    
    if( !numVertexes || !numIndexes )
        return NULL;
        
    if( strlen( name ) >= MAX_QPATH )
    {
        Com_Error( ERR_DROP, "R_CreateVao2: \"%s\" is too long", name );
    }
    
    if( tr.numVaos == MAX_VAOS )
    {
        Com_Error( ERR_DROP, "R_CreateVao2: MAX_VAOS hit" );
    }
    
    R_IssuePendingRenderCommands();
    
    vao = tr.vaos[tr.numVaos] = ( vao_t* )Hunk_Alloc( sizeof( *vao ), h_low );
    tr.numVaos++;
    
    memset( vao, 0, sizeof( *vao ) );
    
    Q_strncpyz( vao->name, name, sizeof( vao->name ) );
    
    // since these vertex attributes are never altered, interleave them
    vao->attribs[ATTR_INDEX_POSITION      ].enabled = 1;
    vao->attribs[ATTR_INDEX_NORMAL        ].enabled = 1;
    vao->attribs[ATTR_INDEX_TANGENT       ].enabled = 1;
    vao->attribs[ATTR_INDEX_TEXCOORD      ].enabled = 1;
    vao->attribs[ATTR_INDEX_LIGHTCOORD    ].enabled = 1;
    vao->attribs[ATTR_INDEX_COLOR         ].enabled = 1;
    vao->attribs[ATTR_INDEX_LIGHTDIRECTION].enabled = 1;
    
    vao->attribs[ATTR_INDEX_POSITION      ].count = 3;
    vao->attribs[ATTR_INDEX_NORMAL        ].count = 4;
    vao->attribs[ATTR_INDEX_TANGENT       ].count = 4;
    vao->attribs[ATTR_INDEX_TEXCOORD      ].count = 2;
    vao->attribs[ATTR_INDEX_LIGHTCOORD    ].count = 2;
    vao->attribs[ATTR_INDEX_COLOR         ].count = 4;
    vao->attribs[ATTR_INDEX_LIGHTDIRECTION].count = 4;
    
    vao->attribs[ATTR_INDEX_POSITION      ].type = GL_FLOAT;
    vao->attribs[ATTR_INDEX_NORMAL        ].type = GL_SHORT;
    vao->attribs[ATTR_INDEX_TANGENT       ].type = GL_SHORT;
    vao->attribs[ATTR_INDEX_TEXCOORD      ].type = GL_FLOAT;
    vao->attribs[ATTR_INDEX_LIGHTCOORD    ].type = GL_FLOAT;
    vao->attribs[ATTR_INDEX_COLOR         ].type = GL_UNSIGNED_SHORT;
    vao->attribs[ATTR_INDEX_LIGHTDIRECTION].type = GL_SHORT;
    
    vao->attribs[ATTR_INDEX_POSITION      ].normalized = GL_FALSE;
    vao->attribs[ATTR_INDEX_NORMAL        ].normalized = GL_TRUE;
    vao->attribs[ATTR_INDEX_TANGENT       ].normalized = GL_TRUE;
    vao->attribs[ATTR_INDEX_TEXCOORD      ].normalized = GL_FALSE;
    vao->attribs[ATTR_INDEX_LIGHTCOORD    ].normalized = GL_FALSE;
    vao->attribs[ATTR_INDEX_COLOR         ].normalized = GL_TRUE;
    vao->attribs[ATTR_INDEX_LIGHTDIRECTION].normalized = GL_TRUE;
    
    vao->attribs[ATTR_INDEX_POSITION      ].offset = 0;
    dataSize  = sizeof( verts[0].xyz );
    vao->attribs[ATTR_INDEX_NORMAL        ].offset = dataSize;
    dataSize += sizeof( verts[0].normal );
    vao->attribs[ATTR_INDEX_TANGENT       ].offset = dataSize;
    dataSize += sizeof( verts[0].tangent );
    vao->attribs[ATTR_INDEX_TEXCOORD      ].offset = dataSize;
    dataSize += sizeof( verts[0].st );
    vao->attribs[ATTR_INDEX_LIGHTCOORD    ].offset = dataSize;
    dataSize += sizeof( verts[0].lightmap );
    vao->attribs[ATTR_INDEX_COLOR         ].offset = dataSize;
    dataSize += sizeof( verts[0].color );
    vao->attribs[ATTR_INDEX_LIGHTDIRECTION].offset = dataSize;
    dataSize += sizeof( verts[0].lightdir );
    
    vao->attribs[ATTR_INDEX_POSITION      ].stride = dataSize;
    vao->attribs[ATTR_INDEX_NORMAL        ].stride = dataSize;
    vao->attribs[ATTR_INDEX_TANGENT       ].stride = dataSize;
    vao->attribs[ATTR_INDEX_TEXCOORD      ].stride = dataSize;
    vao->attribs[ATTR_INDEX_LIGHTCOORD    ].stride = dataSize;
    vao->attribs[ATTR_INDEX_COLOR         ].stride = dataSize;
    vao->attribs[ATTR_INDEX_LIGHTDIRECTION].stride = dataSize;
    
    
    if( glRefConfig.vertexArrayObject )
    {
        glGenVertexArrays( 1, &vao->vao );
        glBindVertexArray( vao->vao );
    }
    
    
    // create VBO
    dataSize *= numVertexes;
    data = ( U8* )Hunk_AllocateTempMemory( dataSize );
    dataOfs = 0;
    
    for( i = 0; i < numVertexes; i++ )
    {
        // xyz
        memcpy( data + dataOfs, &verts[i].xyz, sizeof( verts[i].xyz ) );
        dataOfs += sizeof( verts[i].xyz );
        
        // normal
        memcpy( data + dataOfs, &verts[i].normal, sizeof( verts[i].normal ) );
        dataOfs += sizeof( verts[i].normal );
        
        // tangent
        memcpy( data + dataOfs, &verts[i].tangent, sizeof( verts[i].tangent ) );
        dataOfs += sizeof( verts[i].tangent );
        
        // texcoords
        memcpy( data + dataOfs, &verts[i].st, sizeof( verts[i].st ) );
        dataOfs += sizeof( verts[i].st );
        
        // lightmap texcoords
        memcpy( data + dataOfs, &verts[i].lightmap, sizeof( verts[i].lightmap ) );
        dataOfs += sizeof( verts[i].lightmap );
        
        // colors
        memcpy( data + dataOfs, &verts[i].color, sizeof( verts[i].color ) );
        dataOfs += sizeof( verts[i].color );
        
        // light directions
        memcpy( data + dataOfs, &verts[i].lightdir, sizeof( verts[i].lightdir ) );
        dataOfs += sizeof( verts[i].lightdir );
    }
    
    vao->vertexesSize = dataSize;
    
    glGenBuffers( 1, &vao->vertexesVBO );
    
    glBindBuffer( GL_ARRAY_BUFFER, vao->vertexesVBO );
    glBufferData( GL_ARRAY_BUFFER, vao->vertexesSize, data, glUsage );
    
    
    // create IBO
    vao->indexesSize = numIndexes * sizeof( U32 );
    
    glGenBuffers( 1, &vao->indexesIBO );
    
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, vao->indexesIBO );
    glBufferData( GL_ELEMENT_ARRAY_BUFFER, vao->indexesSize, indexes, glUsage );
    
    
    Vao_SetVertexPointers( vao );
    
    
    glState.currentVao = vao;
    
    GL_CheckErrors();
    
    Hunk_FreeTempMemory( data );
    
    return vao;
}


/*
============
R_BindVao
============
*/
void R_BindVao( vao_t* vao )
{
    if( !vao )
    {
        //R_BindNullVao();
        Com_Error( ERR_DROP, "R_BindVao: NULL vao" );
        return;
    }
    
    if( r_logFile->integer )
    {
        // don't just call LogComment, or we will get a call to va() every frame!
        GLimp_LogComment( ( UTF8* )va( "--- R_BindVao( %s ) ---\n", vao->name ) );
    }
    
    if( glState.currentVao != vao )
    {
        glState.currentVao = vao;
        
        glState.vertexAttribsInterpolation = 0;
        glState.vertexAnimation = false;
        backEnd.pc.c_vaoBinds++;
        
        if( glRefConfig.vertexArrayObject )
        {
            glBindVertexArray( vao->vao );
            
            // Intel Graphics doesn't save GL_ELEMENT_ARRAY_BUFFER binding with VAO binding.
            if( glRefConfig.intelGraphics || vao == tess.vao )
                glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, vao->indexesIBO );
                
            // tess VAO always has buffers bound
            if( vao == tess.vao )
                glBindBuffer( GL_ARRAY_BUFFER, vao->vertexesVBO );
        }
        else
        {
            glBindBuffer( GL_ARRAY_BUFFER, vao->vertexesVBO );
            glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, vao->indexesIBO );
            
            // tess VAO doesn't have vertex pointers set until data is uploaded
            if( vao != tess.vao )
                Vao_SetVertexPointers( vao );
        }
    }
}

/*
============
R_BindNullVao
============
*/
void R_BindNullVao( void )
{
    GLimp_LogComment( "--- R_BindNullVao ---\n" );
    
    if( glState.currentVao )
    {
        if( glRefConfig.vertexArrayObject )
        {
            glBindVertexArray( 0 );
            
            // why you no save GL_ELEMENT_ARRAY_BUFFER binding, Intel?
            if( 1 ) glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
        }
        else
        {
            glBindBuffer( GL_ARRAY_BUFFER, 0 );
            glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
        }
        glState.currentVao = NULL;
    }
    
    GL_CheckErrors();
}


/*
============
R_InitVaos
============
*/
void R_InitVaos( void )
{
    S32             vertexesSize, indexesSize;
    S32             offset;
    
    CL_RefPrintf( PRINT_ALL, "------- R_InitVaos -------\n" );
    
    tr.numVaos = 0;
    
    vertexesSize  = sizeof( tess.xyz[0] );
    vertexesSize += sizeof( tess.normal[0] );
    vertexesSize += sizeof( tess.tangent[0] );
    vertexesSize += sizeof( tess.color[0] );
    vertexesSize += sizeof( tess.texCoords[0] );
    vertexesSize += sizeof( tess.lightCoords[0] );
    vertexesSize += sizeof( tess.lightdir[0] );
    vertexesSize *= SHADER_MAX_VERTEXES;
    
    indexesSize = sizeof( tess.indexes[0] ) * SHADER_MAX_INDEXES;
    
    tess.vao = R_CreateVao( "tessVertexArray_VAO", NULL, vertexesSize, NULL, indexesSize, VAO_USAGE_DYNAMIC );
    
    offset = 0;
    
    tess.vao->attribs[ATTR_INDEX_POSITION      ].enabled = 1;
    tess.vao->attribs[ATTR_INDEX_NORMAL        ].enabled = 1;
    tess.vao->attribs[ATTR_INDEX_TANGENT       ].enabled = 1;
    tess.vao->attribs[ATTR_INDEX_TEXCOORD      ].enabled = 1;
    tess.vao->attribs[ATTR_INDEX_LIGHTCOORD    ].enabled = 1;
    tess.vao->attribs[ATTR_INDEX_COLOR         ].enabled = 1;
    tess.vao->attribs[ATTR_INDEX_LIGHTDIRECTION].enabled = 1;
    
    tess.vao->attribs[ATTR_INDEX_POSITION      ].count = 3;
    tess.vao->attribs[ATTR_INDEX_NORMAL        ].count = 4;
    tess.vao->attribs[ATTR_INDEX_TANGENT       ].count = 4;
    tess.vao->attribs[ATTR_INDEX_TEXCOORD      ].count = 2;
    tess.vao->attribs[ATTR_INDEX_LIGHTCOORD    ].count = 2;
    tess.vao->attribs[ATTR_INDEX_COLOR         ].count = 4;
    tess.vao->attribs[ATTR_INDEX_LIGHTDIRECTION].count = 4;
    
    tess.vao->attribs[ATTR_INDEX_POSITION      ].type = GL_FLOAT;
    tess.vao->attribs[ATTR_INDEX_NORMAL        ].type = GL_SHORT;
    tess.vao->attribs[ATTR_INDEX_TANGENT       ].type = GL_SHORT;
    tess.vao->attribs[ATTR_INDEX_TEXCOORD      ].type = GL_FLOAT;
    tess.vao->attribs[ATTR_INDEX_LIGHTCOORD    ].type = GL_FLOAT;
    tess.vao->attribs[ATTR_INDEX_COLOR         ].type = GL_UNSIGNED_SHORT;
    tess.vao->attribs[ATTR_INDEX_LIGHTDIRECTION].type = GL_SHORT;
    
    tess.vao->attribs[ATTR_INDEX_POSITION      ].normalized = GL_FALSE;
    tess.vao->attribs[ATTR_INDEX_NORMAL        ].normalized = GL_TRUE;
    tess.vao->attribs[ATTR_INDEX_TANGENT       ].normalized = GL_TRUE;
    tess.vao->attribs[ATTR_INDEX_TEXCOORD      ].normalized = GL_FALSE;
    tess.vao->attribs[ATTR_INDEX_LIGHTCOORD    ].normalized = GL_FALSE;
    tess.vao->attribs[ATTR_INDEX_COLOR         ].normalized = GL_TRUE;
    tess.vao->attribs[ATTR_INDEX_LIGHTDIRECTION].normalized = GL_TRUE;
    
    tess.vao->attribs[ATTR_INDEX_POSITION      ].offset = offset;
    offset += sizeof( tess.xyz[0] )         * SHADER_MAX_VERTEXES;
    tess.vao->attribs[ATTR_INDEX_NORMAL        ].offset = offset;
    offset += sizeof( tess.normal[0] )      * SHADER_MAX_VERTEXES;
    tess.vao->attribs[ATTR_INDEX_TANGENT       ].offset = offset;
    offset += sizeof( tess.tangent[0] )     * SHADER_MAX_VERTEXES;
    tess.vao->attribs[ATTR_INDEX_TEXCOORD      ].offset = offset;
    offset += sizeof( tess.texCoords[0] )   * SHADER_MAX_VERTEXES;
    tess.vao->attribs[ATTR_INDEX_LIGHTCOORD    ].offset = offset;
    offset += sizeof( tess.lightCoords[0] ) * SHADER_MAX_VERTEXES;
    tess.vao->attribs[ATTR_INDEX_COLOR         ].offset = offset;
    offset += sizeof( tess.color[0] )       * SHADER_MAX_VERTEXES;
    tess.vao->attribs[ATTR_INDEX_LIGHTDIRECTION].offset = offset;
    
    tess.vao->attribs[ATTR_INDEX_POSITION      ].stride = sizeof( tess.xyz[0] );
    tess.vao->attribs[ATTR_INDEX_NORMAL        ].stride = sizeof( tess.normal[0] );
    tess.vao->attribs[ATTR_INDEX_TANGENT       ].stride = sizeof( tess.tangent[0] );
    tess.vao->attribs[ATTR_INDEX_TEXCOORD      ].stride = sizeof( tess.texCoords[0] );
    tess.vao->attribs[ATTR_INDEX_LIGHTCOORD    ].stride = sizeof( tess.lightCoords[0] );
    tess.vao->attribs[ATTR_INDEX_COLOR         ].stride = sizeof( tess.color[0] );
    tess.vao->attribs[ATTR_INDEX_LIGHTDIRECTION].stride = sizeof( tess.lightdir[0] );
    
    tess.attribPointers[ATTR_INDEX_POSITION]       = tess.xyz;
    tess.attribPointers[ATTR_INDEX_NORMAL]         = tess.normal;
    tess.attribPointers[ATTR_INDEX_TANGENT]        = tess.tangent;
    tess.attribPointers[ATTR_INDEX_TEXCOORD]       = tess.texCoords;
    tess.attribPointers[ATTR_INDEX_LIGHTCOORD]     = tess.lightCoords;
    tess.attribPointers[ATTR_INDEX_COLOR]          = tess.color;
    tess.attribPointers[ATTR_INDEX_LIGHTDIRECTION] = tess.lightdir;
    
    Vao_SetVertexPointers( tess.vao );
    
    R_BindNullVao();
    
    VaoCache_Init();
    
    GL_CheckErrors();
}

/*
============
R_ShutdownVaos
============
*/
void R_ShutdownVaos( void )
{
    S32             i;
    vao_t*          vao;
    
    CL_RefPrintf( PRINT_ALL, "------- R_ShutdownVaos -------\n" );
    
    R_BindNullVao();
    
    for( i = 0; i < tr.numVaos; i++ )
    {
        vao = tr.vaos[i];
        
        if( vao->vao )
            glDeleteVertexArrays( 1, &vao->vao );
            
        if( vao->vertexesVBO )
        {
            glDeleteBuffers( 1, &vao->vertexesVBO );
        }
        
        if( vao->indexesIBO )
        {
            glDeleteBuffers( 1, &vao->indexesIBO );
        }
    }
    
    tr.numVaos = 0;
}

/*
============
R_VaoList_f
============
*/
void R_VaoList_f( void )
{
    S32             i;
    vao_t*          vao;
    S32             vertexesSize = 0;
    S32             indexesSize = 0;
    
    CL_RefPrintf( PRINT_ALL, " size          name\n" );
    CL_RefPrintf( PRINT_ALL, "----------------------------------------------------------\n" );
    
    for( i = 0; i < tr.numVaos; i++ )
    {
        vao = tr.vaos[i];
        
        CL_RefPrintf( PRINT_ALL, "%d.%02d MB %s\n", vao->vertexesSize / ( 1024 * 1024 ),
                      ( vao->vertexesSize % ( 1024 * 1024 ) ) * 100 / ( 1024 * 1024 ), vao->name );
                      
        vertexesSize += vao->vertexesSize;
    }
    
    for( i = 0; i < tr.numVaos; i++ )
    {
        vao = tr.vaos[i];
        
        CL_RefPrintf( PRINT_ALL, "%d.%02d MB %s\n", vao->indexesSize / ( 1024 * 1024 ),
                      ( vao->indexesSize % ( 1024 * 1024 ) ) * 100 / ( 1024 * 1024 ), vao->name );
                      
        indexesSize += vao->indexesSize;
    }
    
    CL_RefPrintf( PRINT_ALL, " %i total VAOs\n", tr.numVaos );
    CL_RefPrintf( PRINT_ALL, " %d.%02d MB total vertices memory\n", vertexesSize / ( 1024 * 1024 ),
                  ( vertexesSize % ( 1024 * 1024 ) ) * 100 / ( 1024 * 1024 ) );
    CL_RefPrintf( PRINT_ALL, " %d.%02d MB total triangle indices memory\n", indexesSize / ( 1024 * 1024 ),
                  ( indexesSize % ( 1024 * 1024 ) ) * 100 / ( 1024 * 1024 ) );
}


/*
==============
RB_UpdateTessVao

Adapted from Tess_UpdateVBOs from xreal

Update the default VAO to replace the client side vertex arrays
==============
*/
void RB_UpdateTessVao( U32 attribBits )
{
    GLimp_LogComment( "--- RB_UpdateTessVao ---\n" );
    
    backEnd.pc.c_dynamicVaoDraws++;
    
    // update the default VAO
    if( tess.numVertexes > 0 && tess.numVertexes <= SHADER_MAX_VERTEXES && tess.numIndexes > 0 && tess.numIndexes <= SHADER_MAX_INDEXES )
    {
        S32 attribIndex;
        S32 attribUpload;
        
        R_BindVao( tess.vao );
        
        // orphan old vertex buffer so we don't stall on it
        glBufferData( GL_ARRAY_BUFFER, tess.vao->vertexesSize, NULL, GL_DYNAMIC_DRAW );
        
        // if nothing to set, set everything
        if( !( attribBits & ATTR_BITS ) )
            attribBits = ATTR_BITS;
            
        attribUpload = attribBits;
        
        for( attribIndex = 0; attribIndex < ATTR_INDEX_COUNT; attribIndex++ )
        {
            U32 attribBit = 1 << attribIndex;
            vaoAttrib_t* vAtb = &tess.vao->attribs[attribIndex];
            
            if( attribUpload & attribBit )
            {
                // note: tess has a VBO where stride == size
                glBufferSubData( GL_ARRAY_BUFFER, vAtb->offset, tess.numVertexes * vAtb->stride, tess.attribPointers[attribIndex] );
            }
            
            if( attribBits & attribBit )
            {
                if( !glRefConfig.vertexArrayObject )
                    glVertexAttribPointer( attribIndex, vAtb->count, vAtb->type, vAtb->normalized, vAtb->stride, BUFFER_OFFSET( vAtb->offset ) );
                    
                if( !( glState.vertexAttribsEnabled & attribBit ) )
                {
                    glEnableVertexAttribArray( attribIndex );
                    glState.vertexAttribsEnabled |= attribBit;
                }
            }
            else
            {
                if( ( glState.vertexAttribsEnabled & attribBit ) )
                {
                    glDisableVertexAttribArray( attribIndex );
                    glState.vertexAttribsEnabled &= ~attribBit;
                }
            }
        }
        
        // orphan old index buffer so we don't stall on it
        glBufferData( GL_ELEMENT_ARRAY_BUFFER, tess.vao->indexesSize, NULL, GL_DYNAMIC_DRAW );
        
        glBufferSubData( GL_ELEMENT_ARRAY_BUFFER, 0, tess.numIndexes * sizeof( tess.indexes[0] ), tess.indexes );
    }
}

typedef struct bufferCacheEntry_s
{
    void* data;
    S32 size;
    S32 bufferOffset;
}
bufferCacheEntry_t;

typedef struct queuedSurface_s
{
    srfVert_t* vertexes;
    S32 numVerts;
    U32* indexes;
    S32 numIndexes;
}
queuedSurface_t;

#define VAOCACHE_MAX_BUFFERED_SURFACES (1 << 16)
#define VAOCACHE_VERTEX_BUFFER_SIZE 12 * 1024 * 1024
#define VAOCACHE_INDEX_BUFFER_SIZE 12 * 1024 * 1024

#define VAOCACHE_MAX_QUEUED_VERTEXES (1 << 16)
#define VAOCACHE_MAX_QUEUED_INDEXES (VAOCACHE_MAX_QUEUED_VERTEXES * 6 / 4)

#define VAOCACHE_MAX_QUEUED_SURFACES 4096

static struct
{
    vao_t* vao;
    bufferCacheEntry_t indexEntries[VAOCACHE_MAX_BUFFERED_SURFACES];
    S32 indexChainLengths[VAOCACHE_MAX_BUFFERED_SURFACES];
    S32 numIndexEntries;
    S32 vertexOffset;
    S32 indexOffset;
    
    srfVert_t vertexes[VAOCACHE_MAX_QUEUED_VERTEXES];
    S32 vertexCommitSize;
    
    U32 indexes[VAOCACHE_MAX_QUEUED_INDEXES];
    S32 indexCommitSize;
    
    queuedSurface_t surfaceQueue[VAOCACHE_MAX_QUEUED_SURFACES];
    S32 numSurfacesQueued;
}
vc = { 0 };

void VaoCache_Commit( void )
{
    bufferCacheEntry_t* entry1;
    queuedSurface_t* surf, *end = vc.surfaceQueue + vc.numSurfacesQueued;
    
    R_BindVao( vc.vao );
    
    // search entire cache for exact chain of indexes
    // FIXME: use faster search
    for( entry1 = vc.indexEntries; entry1 < vc.indexEntries + vc.numIndexEntries; )
    {
        S32 chainLength = vc.indexChainLengths[entry1 - vc.indexEntries];
        
        if( chainLength == vc.numSurfacesQueued )
        {
            bufferCacheEntry_t* entry2 = entry1;
            for( surf = vc.surfaceQueue; surf < end; surf++, entry2++ )
            {
                if( surf->indexes != entry2->data || ( surf->numIndexes * sizeof( U32 ) ) != entry2->size )
                    break;
            }
            
            if( surf == end )
                break;
        }
        
        entry1 += chainLength;
    }
    
    // if found, use that
    if( entry1 < vc.indexEntries + vc.numIndexEntries )
    {
        tess.firstIndex = entry1->bufferOffset / sizeof( U32 );
        //CL_RefPrintf(PRINT_ALL, "firstIndex %d numIndexes %d\n", tess.firstIndex, tess.numIndexes);
        //CL_RefPrintf(PRINT_ALL, "vc.numIndexEntries %d\n", vc.numIndexEntries);
    }
    // if not, rebuffer all the indexes but reuse any existing vertexes
    else
    {
        srfVert_t* dstVertex = vc.vertexes;
        U32* dstIndex = vc.indexes;
        S32* indexChainLength = vc.indexChainLengths + vc.numIndexEntries;
        
        tess.firstIndex = vc.indexOffset / sizeof( U32 );
        vc.vertexCommitSize = 0;
        vc.indexCommitSize = 0;
        for( surf = vc.surfaceQueue; surf < end; surf++ )
        {
            srfVert_t* srcVertex = surf->vertexes;
            U32* srcIndex = surf->indexes;
            S32 vertexesSize = surf->numVerts * sizeof( srfVert_t );
            S32 indexesSize = surf->numIndexes * sizeof( U32 );
            S32 i, indexOffset = ( vc.vertexOffset + vc.vertexCommitSize ) / sizeof( srfVert_t );
            
            for( i = 0; i < surf->numVerts; i++ )
                *dstVertex++ = *srcVertex++;
                
            vc.vertexCommitSize += vertexesSize;
            
            entry1 = vc.indexEntries + vc.numIndexEntries;
            entry1->data = surf->indexes;
            entry1->size = indexesSize;
            entry1->bufferOffset = vc.indexOffset + vc.indexCommitSize;
            vc.numIndexEntries++;
            
            *indexChainLength++ = ( ( surf == vc.surfaceQueue ) ? vc.numSurfacesQueued : 0 );
            
            for( i = 0; i < surf->numIndexes; i++ )
                *dstIndex++ = *srcIndex++ + indexOffset;
                
            vc.indexCommitSize += indexesSize;
        }
        
        //CL_RefPrintf(PRINT_ALL, "committing %d to %d, %d to %d\n", vc.vertexCommitSize, vc.vertexOffset, vc.indexCommitSize, vc.indexOffset);
        
        if( vc.vertexCommitSize )
        {
            glBindBuffer( GL_ARRAY_BUFFER, vc.vao->vertexesVBO );
            glBufferSubData( GL_ARRAY_BUFFER, vc.vertexOffset, vc.vertexCommitSize, vc.vertexes );
            vc.vertexOffset += vc.vertexCommitSize;
        }
        
        if( vc.indexCommitSize )
        {
            glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, vc.vao->indexesIBO );
            glBufferSubData( GL_ELEMENT_ARRAY_BUFFER, vc.indexOffset, vc.indexCommitSize, vc.indexes );
            vc.indexOffset += vc.indexCommitSize;
        }
    }
}

void VaoCache_Init( void )
{
    srfVert_t vert;
    S32 dataSize;
    
    vc.vao = R_CreateVao( "VaoCache", NULL, VAOCACHE_VERTEX_BUFFER_SIZE, NULL, VAOCACHE_INDEX_BUFFER_SIZE, VAO_USAGE_DYNAMIC );
    
    vc.vao->attribs[ATTR_INDEX_POSITION].enabled       = 1;
    vc.vao->attribs[ATTR_INDEX_TEXCOORD].enabled       = 1;
    vc.vao->attribs[ATTR_INDEX_LIGHTCOORD].enabled     = 1;
    vc.vao->attribs[ATTR_INDEX_NORMAL].enabled         = 1;
    vc.vao->attribs[ATTR_INDEX_TANGENT].enabled        = 1;
    vc.vao->attribs[ATTR_INDEX_LIGHTDIRECTION].enabled = 1;
    vc.vao->attribs[ATTR_INDEX_COLOR].enabled          = 1;
    
    vc.vao->attribs[ATTR_INDEX_POSITION].count       = 3;
    vc.vao->attribs[ATTR_INDEX_TEXCOORD].count       = 2;
    vc.vao->attribs[ATTR_INDEX_LIGHTCOORD].count     = 2;
    vc.vao->attribs[ATTR_INDEX_NORMAL].count         = 4;
    vc.vao->attribs[ATTR_INDEX_TANGENT].count        = 4;
    vc.vao->attribs[ATTR_INDEX_LIGHTDIRECTION].count = 4;
    vc.vao->attribs[ATTR_INDEX_COLOR].count          = 4;
    
    vc.vao->attribs[ATTR_INDEX_POSITION].type             = GL_FLOAT;
    vc.vao->attribs[ATTR_INDEX_TEXCOORD].type             = GL_FLOAT;
    vc.vao->attribs[ATTR_INDEX_LIGHTCOORD].type           = GL_FLOAT;
    vc.vao->attribs[ATTR_INDEX_NORMAL].type               = GL_SHORT;
    vc.vao->attribs[ATTR_INDEX_TANGENT].type              = GL_SHORT;
    vc.vao->attribs[ATTR_INDEX_LIGHTDIRECTION].type       = GL_SHORT;
    vc.vao->attribs[ATTR_INDEX_COLOR].type                = GL_UNSIGNED_SHORT;
    
    vc.vao->attribs[ATTR_INDEX_POSITION].normalized       = GL_FALSE;
    vc.vao->attribs[ATTR_INDEX_TEXCOORD].normalized       = GL_FALSE;
    vc.vao->attribs[ATTR_INDEX_LIGHTCOORD].normalized     = GL_FALSE;
    vc.vao->attribs[ATTR_INDEX_NORMAL].normalized         = GL_TRUE;
    vc.vao->attribs[ATTR_INDEX_TANGENT].normalized        = GL_TRUE;
    vc.vao->attribs[ATTR_INDEX_LIGHTDIRECTION].normalized = GL_TRUE;
    vc.vao->attribs[ATTR_INDEX_COLOR].normalized          = GL_TRUE;
    
    vc.vao->attribs[ATTR_INDEX_POSITION].offset       = 0;
    dataSize  = sizeof( vert.xyz );
    vc.vao->attribs[ATTR_INDEX_TEXCOORD].offset       = dataSize;
    dataSize += sizeof( vert.st );
    vc.vao->attribs[ATTR_INDEX_LIGHTCOORD].offset     = dataSize;
    dataSize += sizeof( vert.lightmap );
    vc.vao->attribs[ATTR_INDEX_NORMAL].offset         = dataSize;
    dataSize += sizeof( vert.normal );
    vc.vao->attribs[ATTR_INDEX_TANGENT].offset        = dataSize;
    dataSize += sizeof( vert.tangent );
    vc.vao->attribs[ATTR_INDEX_LIGHTDIRECTION].offset = dataSize;
    dataSize += sizeof( vert.lightdir );
    vc.vao->attribs[ATTR_INDEX_COLOR].offset          = dataSize;
    dataSize += sizeof( vert.color );
    
    vc.vao->attribs[ATTR_INDEX_POSITION].stride       = dataSize;
    vc.vao->attribs[ATTR_INDEX_TEXCOORD].stride       = dataSize;
    vc.vao->attribs[ATTR_INDEX_LIGHTCOORD].stride     = dataSize;
    vc.vao->attribs[ATTR_INDEX_NORMAL].stride         = dataSize;
    vc.vao->attribs[ATTR_INDEX_TANGENT].stride        = dataSize;
    vc.vao->attribs[ATTR_INDEX_LIGHTDIRECTION].stride = dataSize;
    vc.vao->attribs[ATTR_INDEX_COLOR].stride          = dataSize;
    
    Vao_SetVertexPointers( vc.vao );
    
    vc.numIndexEntries = 0;
    vc.vertexOffset = 0;
    vc.indexOffset = 0;
    vc.vertexCommitSize = 0;
    vc.indexCommitSize = 0;
    vc.numSurfacesQueued = 0;
}

void VaoCache_BindVao( void )
{
    R_BindVao( vc.vao );
}

void VaoCache_CheckAdd( bool* endSurface, bool* recycleVertexBuffer, bool* recycleIndexBuffer, S32 numVerts, S32 numIndexes )
{
    S32 vertexesSize = sizeof( srfVert_t ) * numVerts;
    S32 indexesSize = sizeof( U32 ) * numIndexes;
    
    if( vc.vao->vertexesSize < vc.vertexOffset + vc.vertexCommitSize + vertexesSize )
    {
        //CL_RefPrintf(PRINT_ALL, "out of space in vertex cache: %d < %d + %d + %d\n", vc.vao->vertexesSize, vc.vertexOffset, vc.vertexCommitSize, vertexesSize);
        *recycleVertexBuffer = true;
        *recycleIndexBuffer = true;
        *endSurface = true;
    }
    
    if( vc.vao->indexesSize < vc.indexOffset + vc.indexCommitSize + indexesSize )
    {
        //CL_RefPrintf(PRINT_ALL, "out of space in index cache\n");
        *recycleIndexBuffer = true;
        *endSurface = true;
    }
    
    if( vc.numIndexEntries + vc.numSurfacesQueued >= VAOCACHE_MAX_BUFFERED_SURFACES )
    {
        //CL_RefPrintf(PRINT_ALL, "out of surfaces in index cache\n");
        *recycleIndexBuffer = true;
        *endSurface = true;
    }
    
    if( vc.numSurfacesQueued == VAOCACHE_MAX_QUEUED_SURFACES )
    {
        //CL_RefPrintf(PRINT_ALL, "out of queued surfaces\n");
        *endSurface = true;
    }
    
    if( VAOCACHE_MAX_QUEUED_VERTEXES * sizeof( srfVert_t ) < vc.vertexCommitSize + vertexesSize )
    {
        //CL_RefPrintf(PRINT_ALL, "out of queued vertexes\n");
        *endSurface = true;
    }
    
    if( VAOCACHE_MAX_QUEUED_INDEXES * sizeof( U32 ) < vc.indexCommitSize + indexesSize )
    {
        //CL_RefPrintf(PRINT_ALL, "out of queued indexes\n");
        *endSurface = true;
    }
}

void VaoCache_RecycleVertexBuffer( void )
{
    glBindBuffer( GL_ARRAY_BUFFER, vc.vao->vertexesVBO );
    glBufferData( GL_ARRAY_BUFFER, vc.vao->vertexesSize, NULL, GL_DYNAMIC_DRAW );
    vc.vertexOffset = 0;
}

void VaoCache_RecycleIndexBuffer( void )
{
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, vc.vao->indexesIBO );
    glBufferData( GL_ELEMENT_ARRAY_BUFFER, vc.vao->indexesSize, NULL, GL_DYNAMIC_DRAW );
    vc.indexOffset = 0;
    vc.numIndexEntries = 0;
}

void VaoCache_InitNewSurfaceSet( void )
{
    vc.vertexCommitSize = 0;
    vc.indexCommitSize = 0;
    vc.numSurfacesQueued = 0;
}

void VaoCache_AddSurface( srfVert_t* verts, S32 numVerts, U32* indexes, S32 numIndexes )
{
    S32 vertexesSize = sizeof( srfVert_t ) * numVerts;
    S32 indexesSize = sizeof( U32 ) * numIndexes;
    queuedSurface_t* queueEntry = vc.surfaceQueue + vc.numSurfacesQueued;
    queueEntry->vertexes = verts;
    queueEntry->numVerts = numVerts;
    queueEntry->indexes = indexes;
    queueEntry->numIndexes = numIndexes;
    vc.numSurfacesQueued++;
    
    vc.vertexCommitSize += vertexesSize;
    vc.indexCommitSize += indexesSize;
}
