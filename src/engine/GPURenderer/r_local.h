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
// File name:   r_local.h
// Version:     v1.00
// Created:
// Compilers:   Visual Studio 2015
// Description:
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#ifndef __R_LOCAL_H__
#define __R_LOCAL_H__

#if !defined ( DEDICATED ) && !defined ( UPDATE_SERVER )

#ifndef __CM_LOCAL_H__
#include <cm/cm_local.h>
#endif
#ifndef __Q_SHARED_H__
#include <qcommon/q_shared.h>
#endif
#ifndef __QFILES_H__
#include <qcommon/qfiles.h>
#endif
#ifndef __QCOMMON_H__
#include <qcommon/qcommon.h>
#endif
#ifndef __R_PUBLIC_H__
#include <GPURenderer/r_public.h>
#endif
#ifndef __R_COMMON_H__
#include <GPURenderer/r_common.h>
#endif
#ifndef __R_EXTRATYPES_H__
#include <GPURenderer/r_extratypes.h>
#endif
#ifndef __R_EXTRAMATH_H__
#include <GPURenderer/r_extramath.h>
#endif
#ifndef __R_FBO_H__
#include <GPURenderer/r_fbo.h>
#endif
#ifndef __R_POSTPROCESS_H__
#include <GPURenderer/r_postprocess.h>
#endif
#ifndef __IQM_H__
#include <GPURenderer/iqm.h>
#endif

typedef void ( *xcommand_t )( void );

extern F32    displayAspect;	// FIXME

//#define __DYNAMIC_SHADOWS__
#ifdef __DYNAMIC_SHADOWS__
#define MAX_DYNAMIC_SHADOWS 4
#endif //__DYNAMIC_SHADOWS__

// these are just here temp while I port everything to c++.
void CL_PlayCinematic_f( void );
void SCR_DrawCinematic( void );
void SCR_RunCinematic( void );
void SCR_StopCinematic( void );
S32 CIN_PlayCinematic( StringEntry arg0, S32 xpos, S32 ypos, S32 width, S32 height, S32 bits );
e_status CIN_StopCinematic( S32 handle );
e_status CIN_RunCinematic( S32 handle );
void CIN_DrawCinematic( S32 handle );
void CIN_SetExtents( S32 handle, S32 x, S32 y, S32 w, S32 h );
void CIN_SetLooping( S32 handle, bool loop );
void CIN_UploadCinematic( S32 handle );
void CIN_CloseAllVideos( void );
void CL_RefPrintf( S32 print_level, StringEntry fmt, ... );
void CL_WriteAVIVideoFrame( const U8* imageBuffer, S32 size );
bool CL_VideoRecording( void );
S32 CL_ScaledMilliseconds( void );
S32 FS_ReadFile( StringEntry qpath, void** buffer );
void FS_FreeFile( void* buffer );
void Cbuf_ExecuteText( S32 exec_when, StringEntry text );
UTF8** FS_ListFiles( StringEntry path, StringEntry extension, S32* numfiles );
void FS_FreeFileList( UTF8** list );
S32	Cmd_Argc( void );
void FS_WriteFile( StringEntry qpath, const void* buffer, S32 size );
bool FS_FileExists( StringEntry file );
void FS_WriteFile( StringEntry qpath, const void* buffer, S32 size );
UTF8* Cmd_Argv( S32 arg );
void Cmd_AddCommand( StringEntry cmd_name, xcommand_t function );
void Cmd_RemoveCommand( StringEntry cmd_name );
void* CL_RefMalloc( S32 size );

#define GL_INDEX_TYPE		GL_UNSIGNED_INT

#define BUFFER_OFFSET(i) ((UTF8 *)NULL + (i))

// 14 bits
// can't be increased without changing bit packing for drawsurfs
// see QSORT_SHADERNUM_SHIFT
#define SHADERNUM_BITS	14
#define MAX_SHADERS		(1<<SHADERNUM_BITS)

#ifndef __DYNAMIC_SHADOWS__
#define	MAX_FBOS      64
#else //__DYNAMIC_SHADOWS__
#define	MAX_FBOS      ((MAX_DYNAMIC_SHADOWS*4) + 64)
#endif //__DYNAMIC_SHADOWS__

#define MAX_VISCOUNTS 5
#define MAX_VAOS      4096

#define MAX_CALC_PSHADOWS    64
#define MAX_DRAWN_PSHADOWS    16 // do not increase past 32, because bit flags are used on surfaces
#define PSHADOW_MAP_SIZE      512
#define CUBE_MAP_MIPS      8
#define CUBE_MAP_SIZE      (1 << CUBE_MAP_MIPS)

typedef struct cubemap_s
{
    UTF8 name[MAX_QPATH];
    vec3_t origin;
    F32 parallaxRadius;
    image_t* image;
} cubemap_t;

typedef struct dlight_s
{
    vec3_t	origin;
    vec3_t	color;				// range from 0.0 to 1.0, should be color normalized
    F32	radius;
    
    vec3_t	transformed;		// origin in local coordinate system
    S32		additive;			// texture detail is lost tho when the lightmap is dark
#ifdef __DYNAMIC_SHADOWS__
    bool activeShadows;
#endif //__DYNAMIC_SHADOWS__
} dlight_t;


// a trRefEntity_t has all the information passed in by
// the client game, as well as some locally derived info
typedef struct
{
    refEntity_t	e;
    
    F32		axisLength;		// compensate for non-normalized axis
    
    bool	needDlights;	// true for bmodels that touch a dlight
    bool	lightingCalculated;
    bool	mirrored;		// mirrored matrix, needs reversed culling
    vec3_t		lightDir;		// normalized direction towards light, in world space
    vec3_t      modelLightDir;  // normalized direction towards light, in model space
    vec3_t		ambientLight;	// color normalized to 0-255
    S32			ambientLightInt;	// 32 bit rgba packed
    vec3_t		directedLight;
} trRefEntity_t;


typedef struct
{
    vec3_t		origin;			// in world coordinates
    vec3_t		axis[3];		// orientation in world
    vec3_t		viewOrigin;		// viewParms->or.origin in local coordinates
    F32		modelMatrix[16];
    F32		transformMatrix[16];
} orientationr_t;

// Ensure this is >= the ATTR_INDEX_COUNT enum below
#define VAO_MAX_ATTRIBS 16

typedef enum
{
    VAO_USAGE_STATIC,
    VAO_USAGE_DYNAMIC
} vaoUsage_t;

typedef struct vaoAttrib_s
{
    U32 enabled;
    U32 count;
    U32 type;
    U32 normalized;
    U32 stride;
    U32 offset;
}
vaoAttrib_t;

typedef struct vao_s
{
    UTF8            name[MAX_QPATH];
    
    U32        vao;
    
    U32        vertexesVBO;
    S32             vertexesSize;	// amount of memory data allocated for all vertices in bytes
    vaoAttrib_t     attribs[VAO_MAX_ATTRIBS];
    
    U32        frameSize;      // bytes to skip per frame when doing vertex animation
    
    U32        indexesIBO;
    S32             indexesSize;	// amount of memory data allocated for all triangles in bytes
} vao_t;

//===============================================================================

typedef enum
{
    SS_BAD,
    SS_PORTAL,			// mirrors, portals, viewscreens
    SS_ENVIRONMENT,		// sky box
    SS_OPAQUE,			// opaque
    
    SS_DECAL,			// scorch marks, etc.
    SS_SEE_THROUGH,		// ladders, grates, grills that may have small blended edges
    // in addition to alpha test
    SS_BANNER,
    
    SS_FOG,
    
    SS_UNDERWATER,		// for items that should be drawn in front of the water plane
    
    SS_BLEND0,			// regular transparency and filters
    SS_BLEND1,			// generally only used for additive type effects
    SS_BLEND2,
    SS_BLEND3,
    
    SS_BLEND6,
    SS_STENCIL_SHADOW,
    SS_ALMOST_NEAREST,	// gun smoke puffs
    
    SS_NEAREST			// blood blobs
} shaderSort_t;


#define MAX_SHADER_STAGES 8

typedef enum
{
    GF_NONE,
    
    GF_SIN,
    GF_SQUARE,
    GF_TRIANGLE,
    GF_SAWTOOTH,
    GF_INVERSE_SAWTOOTH,
    
    GF_NOISE
    
} genFunc_t;


typedef enum
{
    DEFORM_NONE,
    DEFORM_WAVE,
    DEFORM_NORMALS,
    DEFORM_BULGE,
    DEFORM_MOVE,
    DEFORM_PROJECTION_SHADOW,
    DEFORM_AUTOSPRITE,
    DEFORM_AUTOSPRITE2,
    DEFORM_TEXT0,
    DEFORM_TEXT1,
    DEFORM_TEXT2,
    DEFORM_TEXT3,
    DEFORM_TEXT4,
    DEFORM_TEXT5,
    DEFORM_TEXT6,
    DEFORM_TEXT7
} deform_t;

// deformVertexes types that can be handled by the GPU
typedef enum
{
    // do not edit: same as genFunc_t
    
    DGEN_NONE,
    DGEN_WAVE_SIN,
    DGEN_WAVE_SQUARE,
    DGEN_WAVE_TRIANGLE,
    DGEN_WAVE_SAWTOOTH,
    DGEN_WAVE_INVERSE_SAWTOOTH,
    DGEN_WAVE_NOISE,
    
    // do not edit until this line
    
    DGEN_BULGE,
    DGEN_MOVE
} deformGen_t;

typedef enum
{
    AGEN_IDENTITY,
    AGEN_SKIP,
    AGEN_ENTITY,
    AGEN_ONE_MINUS_ENTITY,
    AGEN_VERTEX,
    AGEN_ONE_MINUS_VERTEX,
    AGEN_LIGHTING_SPECULAR,
    AGEN_WAVEFORM,
    AGEN_PORTAL,
    AGEN_CONST,
} alphaGen_t;

typedef enum
{
    CGEN_BAD,
    CGEN_IDENTITY_LIGHTING,	// tr.identityLight
    CGEN_IDENTITY,			// always (1,1,1,1)
    CGEN_ENTITY,			// grabbed from entity's modulate field
    CGEN_ONE_MINUS_ENTITY,	// grabbed from 1 - entity.modulate
    CGEN_EXACT_VERTEX,		// tess.vertexColors
    CGEN_VERTEX,			// tess.vertexColors * tr.identityLight
    CGEN_EXACT_VERTEX_LIT,	// like CGEN_EXACT_VERTEX but takes a light direction from the lightgrid
    CGEN_VERTEX_LIT,		// like CGEN_VERTEX but takes a light direction from the lightgrid
    CGEN_ONE_MINUS_VERTEX,
    CGEN_WAVEFORM,			// programmatically generated
    CGEN_LIGHTING_DIFFUSE,
    CGEN_FOG,				// standard fog
    CGEN_CONST				// fixed color
} colorGen_t;

typedef enum
{
    TCGEN_BAD,
    TCGEN_IDENTITY,			// clear to 0,0
    TCGEN_LIGHTMAP,
    TCGEN_TEXTURE,
    TCGEN_ENVIRONMENT_MAPPED,
    TCGEN_FOG,
    TCGEN_VECTOR			// S and T from world coordinates
} texCoordGen_t;

typedef enum
{
    ACFF_NONE,
    ACFF_MODULATE_RGB,
    ACFF_MODULATE_RGBA,
    ACFF_MODULATE_ALPHA
} acff_t;

typedef struct
{
    genFunc_t	func;
    
    F64 base;
    F64 amplitude;
    F64 phase;
    F64 frequency;
} waveForm_t;

#define TR_MAX_TEXMODS 4

typedef enum
{
    TMOD_NONE,
    TMOD_TRANSFORM,
    TMOD_TURBULENT,
    TMOD_SCROLL,
    TMOD_SCALE,
    TMOD_STRETCH,
    TMOD_ROTATE,
    TMOD_ENTITY_TRANSLATE
} texMod_t;

#define	MAX_SHADER_DEFORMS	3
typedef struct
{
    deform_t	deformation;			// vertex coordinate modification type
    
    vec3_t		moveVector;
    waveForm_t	deformationWave;
    F32		deformationSpread;
    
    F32		bulgeWidth;
    F32		bulgeHeight;
    F32		bulgeSpeed;
} deformStage_t;


typedef struct
{
    texMod_t		type;
    
    // used for TMOD_TURBULENT and TMOD_STRETCH
    waveForm_t		wave;
    
    // used for TMOD_TRANSFORM
    F32			matrix[2][2];		// s' = s * m[0][0] + t * m[1][0] + trans[0]
    F32			translate[2];		// t' = s * m[0][1] + t * m[0][1] + trans[1]
    
    // used for TMOD_SCALE
    F32			scale[2];			// s *= scale[0]
    // t *= scale[1]
    
    // used for TMOD_SCROLL
    F32			scroll[2];			// s' = s + scroll[0] * time
    // t' = t + scroll[1] * time
    
    // + = clockwise
    // - = counterclockwise
    F32			rotateSpeed;
    
} texModInfo_t;


#define	MAX_IMAGE_ANIMATIONS 128

typedef struct
{
    image_t*			image[MAX_IMAGE_ANIMATIONS];
    S32				numImageAnimations;
    F64			imageAnimationSpeed;
    
    texCoordGen_t	tcGen;
    vec3_t			tcGenVectors[2];
    
    S32				numTexMods;
    texModInfo_t*	texMods;
    
    S32				videoMapHandle;
    bool		isLightmap;
    bool		isVideoMap;
    bool		specularLoaded;
} textureBundle_t;

enum
{
    TB_COLORMAP    = 0,
    TB_DIFFUSEMAP  = 0,
    TB_LIGHTMAP    = 1,
    TB_LEVELSMAP   = 1,
    TB_SHADOWMAP3  = 1,
    TB_NORMALMAP   = 2,
    TB_DELUXEMAP   = 3,
    TB_SHADOWMAP2  = 3,
    TB_SPECULARMAP = 4,
    TB_SHADOWMAP   = 5,
    TB_CUBEMAP     = 6,
    TB_SHADOWMAP4  = 6,
    NUM_TEXTURE_BUNDLES = 7
};

typedef enum
{
    // material shader stage types
    ST_COLORMAP = 0,			// vanilla Q3A style shader treatening
    ST_DIFFUSEMAP = 0,          // treat color and diffusemap the same
    ST_NORMALMAP,
    ST_NORMALPARALLAXMAP,
    ST_SPECULARMAP,
    ST_GLSL
} stageType_t;

typedef struct
{
    bool		active;
    bool		isDetail;
    bool        isWater;
    bool		hasSpecular;
    
    textureBundle_t	bundle[NUM_TEXTURE_BUNDLES];
    
    waveForm_t		rgbWave;
    colorGen_t		rgbGen;
    
    waveForm_t		alphaWave;
    alphaGen_t		alphaGen;
    
    U8			    constantColor[4];			// for CGEN_CONST and AGEN_CONST
    
    U32		        stateBits;					// GLS_xxxx mask
    
    acff_t			adjustColorsForFog;
    
    stageType_t     type;
    struct shaderProgram_s* glslShaderGroup;
    S32 glslShaderIndex;
    
    vec4_t normalScale;
    vec4_t specularScale;
    
} shaderStage_t;

struct shaderCommands_s;

typedef enum
{
    FP_NONE,		// surface is translucent and will just be adjusted properly
    FP_EQUAL,		// surface is opaque but possibly alpha tested
    FP_LE			// surface is trnaslucent, but still needs a fog pass (fog surface)
} fogPass_t;

typedef struct
{
    F32		cloudHeight;
    image_t*		outerbox[6], *innerbox[6];
} skyParms_t;

typedef struct
{
    vec3_t	color;
    F32	depthForOpaque;
} fogParms_t;


typedef struct shader_s
{
    UTF8		name[MAX_QPATH];		// game path, including extension
    S32			lightmapIndex;			// for a shader to match, both name and lightmapIndex must match
    
    S32			index;					// this shader == tr.shaders[index]
    S64			sortedIndex;			// this shader == tr.sortedShaders[sortedIndex]
    
    F32		sort;					// lower numbered shaders draw before higher numbered
    
    bool	defaultShader;			// we want to return index 0 if the shader failed to
    // load for some reason, but R_FindShader should
    // still keep a name allocated for it, so if
    // something calls idRenderSystemLocal::RegisterShader again with
    // the same name, we don't try looking for it again
    
    bool	explicitlyDefined;		// found in a .shader file
    
    S32			surfaceFlags;			// if explicitlyDefined, this will have SURF_* flags
    S32			contentFlags;
    
    bool	entityMergable;			// merge across entites optimizable (smoke, blood)
    
    bool	isSky;
    skyParms_t	sky;
    fogParms_t	fogParms;
    
    F32		portalRange;			// distance to fog out at
    bool	isPortal;
    
    cullType_t	cullType;				// CT_FRONT_SIDED, CT_BACK_SIDED, or CT_TWO_SIDED
    bool	polygonOffset;			// set for decals and other items that must be offset
    bool	noMipMaps;				// for console fonts, 2D elements, etc.
    bool	noPicMip;				// for images that must always be full resolution
    
    fogPass_t	fogPass;				// draw a blended pass, possibly with depth test equals
    
    S32         vertexAttribs;          // not all shaders will need all data to be gathered
    
    S32			numDeforms;
    deformStage_t	deforms[MAX_SHADER_DEFORMS];
    
    S32			numUnfoggedPasses;
    shaderStage_t*	stages[MAX_SHADER_STAGES];
    
    S32     lightingStage;
    
    void	( *optimalStageIteratorFunc )( void );
    
    F64 clampTime;                                  // time this shader is clamped to
    F64 timeOffset;                                 // current time offset for this shader
    
    struct shader_s* remappedShader;                  // current shader this one is remapped too
    
    struct	shader_s*	next;
} shader_t;

bool ShaderRequiresCPUDeforms( const shader_t* );

enum
{
    ATTR_INDEX_POSITION       = 0,
    ATTR_INDEX_TEXCOORD       = 1,
    ATTR_INDEX_LIGHTCOORD     = 2,
    ATTR_INDEX_TANGENT        = 3,
    ATTR_INDEX_NORMAL         = 4,
    ATTR_INDEX_COLOR          = 5,
    ATTR_INDEX_PAINTCOLOR     = 6,
    ATTR_INDEX_LIGHTDIRECTION = 7,
    ATTR_INDEX_BONE_INDEXES   = 8,
    ATTR_INDEX_BONE_WEIGHTS   = 9,
    
    // GPU vertex animations
    ATTR_INDEX_POSITION2      = 10,
    ATTR_INDEX_TANGENT2       = 11,
    ATTR_INDEX_NORMAL2        = 12,
    
    ATTR_INDEX_COUNT          = 13
};

enum
{
    ATTR_POSITION =       1 << ATTR_INDEX_POSITION,
    ATTR_TEXCOORD =       1 << ATTR_INDEX_TEXCOORD,
    ATTR_LIGHTCOORD =     1 << ATTR_INDEX_LIGHTCOORD,
    ATTR_TANGENT =        1 << ATTR_INDEX_TANGENT,
    ATTR_NORMAL =         1 << ATTR_INDEX_NORMAL,
    ATTR_COLOR =          1 << ATTR_INDEX_COLOR,
    ATTR_PAINTCOLOR =     1 << ATTR_INDEX_PAINTCOLOR,
    ATTR_LIGHTDIRECTION = 1 << ATTR_INDEX_LIGHTDIRECTION,
    ATTR_BONE_INDEXES =   1 << ATTR_INDEX_BONE_INDEXES,
    ATTR_BONE_WEIGHTS =   1 << ATTR_INDEX_BONE_WEIGHTS,
    
    // for .md3 interpolation
    ATTR_POSITION2 =      1 << ATTR_INDEX_POSITION2,
    ATTR_TANGENT2 =       1 << ATTR_INDEX_TANGENT2,
    ATTR_NORMAL2 =        1 << ATTR_INDEX_NORMAL2,
    
    ATTR_DEFAULT = ATTR_POSITION,
    ATTR_BITS =	ATTR_POSITION |
                ATTR_TEXCOORD |
                ATTR_LIGHTCOORD |
                ATTR_TANGENT |
                ATTR_NORMAL |
                ATTR_COLOR |
                ATTR_PAINTCOLOR |
                ATTR_LIGHTDIRECTION |
                ATTR_BONE_INDEXES |
                ATTR_BONE_WEIGHTS |
                ATTR_POSITION2 |
                ATTR_TANGENT2 |
                ATTR_NORMAL2
};

enum
{
    GENERICDEF_USE_DEFORM_VERTEXES  = 0x0001,
    GENERICDEF_USE_TCGEN_AND_TCMOD  = 0x0002,
    GENERICDEF_USE_VERTEX_ANIMATION = 0x0004,
    GENERICDEF_USE_FOG              = 0x0008,
    GENERICDEF_USE_RGBAGEN          = 0x0010,
    GENERICDEF_ALL                  = 0x001F,
    GENERICDEF_COUNT                = 0x0020,
};

enum
{
    FOGDEF_USE_DEFORM_VERTEXES  = 0x0001,
    FOGDEF_USE_VERTEX_ANIMATION = 0x0002,
    FOGDEF_ALL                  = 0x0003,
    FOGDEF_COUNT                = 0x0004,
};

enum
{
    DLIGHTDEF_USE_DEFORM_VERTEXES  = 0x0001,
    DLIGHTDEF_ALL                  = 0x0001,
    DLIGHTDEF_COUNT                = 0x0002,
};

enum
{
    LIGHTDEF_USE_LIGHTMAP        = 0x0001,
    LIGHTDEF_USE_LIGHT_VECTOR    = 0x0002,
    LIGHTDEF_USE_LIGHT_VERTEX    = 0x0003,
    LIGHTDEF_LIGHTTYPE_MASK      = 0x0003,
    LIGHTDEF_ENTITY              = 0x0004,
    LIGHTDEF_USE_TCGEN_AND_TCMOD = 0x0008,
    LIGHTDEF_USE_PARALLAXMAP     = 0x0010,
    LIGHTDEF_USE_SHADOWMAP       = 0x0020,
    LIGHTDEF_ALL                 = 0x003F,
    LIGHTDEF_COUNT               = 0x0040
};

enum
{
    GLSL_INT,
    GLSL_FLOAT,
    GLSL_FLOAT5,
    GLSL_VEC2,
    GLSL_VEC3,
    GLSL_VEC4,
    GLSL_MAT16
};

#define MAX_BLOCKS (32)
#define MAX_BLOCK_NAME_LEN (32)
struct Block
{
    StringEntry blockText;
    U64 blockTextLength;
    S32 blockTextFirstLine;
    
    StringEntry blockHeaderTitle;
    U64 blockHeaderTitleLength;
    
    StringEntry blockHeaderText;
    U64 blockHeaderTextLength;
};

enum GPUShaderType
{
    GPUSHADER_VERTEX,
    GPUSHADER_FRAGMENT,
    GPUSHADER_GEOMETRY,
    GPUSHADER_TYPE_COUNT
};

struct GPUShaderDesc
{
    GPUShaderType type;
    StringEntry source;
    S32 firstLineNumber;
};

struct GPUProgramDesc
{
    U64 numShaders;
    GPUShaderDesc* shaders;
};


typedef enum
{
    UNIFORM_DIFFUSEMAP = 0,
    UNIFORM_LIGHTMAP,
    UNIFORM_NORMALMAP,
    UNIFORM_DELUXEMAP,
    UNIFORM_SPECULARMAP,
    
    UNIFORM_TEXTUREMAP,
    UNIFORM_LEVELSMAP,
    UNIFORM_CUBEMAP,
    
    UNIFORM_SCREENIMAGEMAP,
    UNIFORM_SCREENDEPTHMAP,
    
    UNIFORM_SHADOWMAP,
    UNIFORM_SHADOWMAP2,
    UNIFORM_SHADOWMAP3,
    UNIFORM_SHADOWMAP4,
    
    UNIFORM_SHADOWMVP,
    UNIFORM_SHADOWMVP2,
    UNIFORM_SHADOWMVP3,
    UNIFORM_SHADOWMVP4,
    
    UNIFORM_ENABLETEXTURES,
    
    UNIFORM_DIFFUSETEXMATRIX,
    UNIFORM_DIFFUSETEXOFFTURB,
    
    UNIFORM_TCGEN0,
    UNIFORM_TCGEN0VECTOR0,
    UNIFORM_TCGEN0VECTOR1,
    
    UNIFORM_DEFORMGEN,
    UNIFORM_DEFORMPARAMS,
    
    UNIFORM_COLORGEN,
    UNIFORM_ALPHAGEN,
    UNIFORM_COLOR,
    UNIFORM_BASECOLOR,
    UNIFORM_VERTCOLOR,
    
    UNIFORM_DLIGHTINFO,
    UNIFORM_LIGHTFORWARD,
    UNIFORM_LIGHTUP,
    UNIFORM_LIGHTRIGHT,
    UNIFORM_LIGHTORIGIN,
    UNIFORM_LIGHTORIGIN1,
    UNIFORM_LIGHTORIGIN2,
    UNIFORM_LIGHTORIGIN3,
    UNIFORM_LIGHTORIGIN4,
    UNIFORM_LIGHTORIGIN5,
    UNIFORM_LIGHTORIGIN6,
    UNIFORM_LIGHTORIGIN7,
    UNIFORM_LIGHTORIGIN8,
    UNIFORM_LIGHTORIGIN9,
    UNIFORM_LIGHTORIGIN10,
    UNIFORM_LIGHTORIGIN11,
    UNIFORM_LIGHTORIGIN12,
    UNIFORM_LIGHTORIGIN13,
    UNIFORM_LIGHTORIGIN14,
    UNIFORM_LIGHTORIGIN15,
    UNIFORM_LIGHTORIGIN16,
    UNIFORM_LIGHTORIGIN17,
    UNIFORM_LIGHTORIGIN18,
    UNIFORM_LIGHTORIGIN19,
    UNIFORM_LIGHTORIGIN20,
    UNIFORM_LIGHTORIGIN21,
    UNIFORM_LIGHTORIGIN22,
    UNIFORM_LIGHTORIGIN23,
    UNIFORM_LIGHTORIGIN24,
    UNIFORM_LIGHTORIGIN25,
    UNIFORM_LIGHTORIGIN26,
    UNIFORM_LIGHTORIGIN27,
    UNIFORM_LIGHTORIGIN28,
    UNIFORM_LIGHTORIGIN29,
    UNIFORM_LIGHTORIGIN30,
    UNIFORM_LIGHTORIGIN31,
    UNIFORM_LIGHTCOLOR,
    UNIFORM_LIGHTCOLOR1,
    UNIFORM_LIGHTCOLOR2,
    UNIFORM_LIGHTCOLOR3,
    UNIFORM_LIGHTCOLOR4,
    UNIFORM_LIGHTCOLOR5,
    UNIFORM_LIGHTCOLOR6,
    UNIFORM_LIGHTCOLOR7,
    UNIFORM_LIGHTCOLOR8,
    UNIFORM_LIGHTCOLOR9,
    UNIFORM_LIGHTCOLOR10,
    UNIFORM_LIGHTCOLOR11,
    UNIFORM_LIGHTCOLOR12,
    UNIFORM_LIGHTCOLOR13,
    UNIFORM_LIGHTCOLOR14,
    UNIFORM_LIGHTCOLOR15,
    UNIFORM_LIGHTCOLOR16,
    UNIFORM_LIGHTCOLOR17,
    UNIFORM_LIGHTCOLOR18,
    UNIFORM_LIGHTCOLOR19,
    UNIFORM_LIGHTCOLOR20,
    UNIFORM_LIGHTCOLOR21,
    UNIFORM_LIGHTCOLOR22,
    UNIFORM_LIGHTCOLOR23,
    UNIFORM_LIGHTCOLOR24,
    UNIFORM_LIGHTCOLOR25,
    UNIFORM_LIGHTCOLOR26,
    UNIFORM_LIGHTCOLOR27,
    UNIFORM_LIGHTCOLOR28,
    UNIFORM_LIGHTCOLOR29,
    UNIFORM_LIGHTCOLOR30,
    UNIFORM_LIGHTCOLOR31,
    UNIFORM_MODELLIGHTDIR,
    UNIFORM_LIGHTRADIUS,
    UNIFORM_LIGHTRADIUS1,
    UNIFORM_LIGHTRADIUS2,
    UNIFORM_LIGHTRADIUS3,
    UNIFORM_LIGHTRADIUS4,
    UNIFORM_LIGHTRADIUS5,
    UNIFORM_LIGHTRADIUS6,
    UNIFORM_LIGHTRADIUS7,
    UNIFORM_LIGHTRADIUS8,
    UNIFORM_LIGHTRADIUS9,
    UNIFORM_LIGHTRADIUS10,
    UNIFORM_LIGHTRADIUS11,
    UNIFORM_LIGHTRADIUS12,
    UNIFORM_LIGHTRADIUS13,
    UNIFORM_LIGHTRADIUS14,
    UNIFORM_LIGHTRADIUS15,
    UNIFORM_LIGHTRADIUS16,
    UNIFORM_LIGHTRADIUS17,
    UNIFORM_LIGHTRADIUS18,
    UNIFORM_LIGHTRADIUS19,
    UNIFORM_LIGHTRADIUS20,
    UNIFORM_LIGHTRADIUS21,
    UNIFORM_LIGHTRADIUS22,
    UNIFORM_LIGHTRADIUS23,
    UNIFORM_LIGHTRADIUS24,
    UNIFORM_LIGHTRADIUS25,
    UNIFORM_LIGHTRADIUS26,
    UNIFORM_LIGHTRADIUS27,
    UNIFORM_LIGHTRADIUS28,
    UNIFORM_LIGHTRADIUS29,
    UNIFORM_LIGHTRADIUS30,
    UNIFORM_LIGHTRADIUS31,
    UNIFORM_AMBIENTLIGHT,
    UNIFORM_DIRECTEDLIGHT,
    
    UNIFORM_PORTALRANGE,
    
    UNIFORM_FOGDISTANCE,
    UNIFORM_FOGDEPTH,
    UNIFORM_FOGEYET,
    UNIFORM_FOGCOLORMASK,
    
    UNIFORM_MODELMATRIX,
    UNIFORM_MODELVIEWPROJECTIONMATRIX,
    UNIFORM_INVPROJECTIONMATRIX,
    UNIFORM_INVEYEPROJECTIONMATRIX,
    
    UNIFORM_TIME,
    UNIFORM_VERTEXLERP,
    UNIFORM_NORMALSCALE,
    UNIFORM_SPECULARSCALE,
    
    UNIFORM_VIEWINFO, // znear, zfar, width/2, height/2
    UNIFORM_VIEWORIGIN,
    UNIFORM_LOCALVIEWORIGIN,
    UNIFORM_VIEWFORWARD,
    UNIFORM_VIEWLEFT,
    UNIFORM_VIEWUP,
    
    UNIFORM_INVTEXRES,
    UNIFORM_AUTOEXPOSUREMINMAX,
    UNIFORM_TONEMINAVGMAXLINEAR,
    
    UNIFORM_PRIMARYLIGHTORIGIN,
    UNIFORM_PRIMARYLIGHTCOLOR,
    UNIFORM_PRIMARYLIGHTAMBIENT,
    UNIFORM_PRIMARYLIGHTRADIUS,
    
    UNIFORM_CUBEMAPINFO,
    
    UNIFORM_ALPHATEST,
    
    UNIFORM_BRIGHTNESS,
    UNIFORM_CONTRAST,
    UNIFORM_GAMMA,
    
    UNIFORM_DIMENSIONS,
    UNIFORM_HEIGHTMAP,
    UNIFORM_LOCAL0,
    UNIFORM_LOCAL1,
    UNIFORM_LOCAL2,
    UNIFORM_LOCAL3,
    UNIFORM_TEXTURE0,
    UNIFORM_TEXTURE1,
    UNIFORM_TEXTURE2,
    UNIFORM_TEXTURE3,
    
    UNIFORM_COUNT
} uniform_t;

// shaderProgram_t represents a pair of one
// GLSL vertex and one GLSL fragment shader
typedef struct shaderProgram_s
{
    UTF8       name[MAX_QPATH];
    
    U32        program;
    U32        vertexShader;
    U32        fragmentShader;
    U32        attribs;	// vertex array attributes
    
    // uniform parameters
    S32        uniforms[UNIFORM_COUNT];
    S16        uniformBufferOffsets[UNIFORM_COUNT]; // max 32767/64=511 uniforms
    UTF8*      uniformBuffer;
} shaderProgram_t;

// trRefdef_t holds everything that comes in refdef_t,
// as well as the locally generated scene information
typedef struct
{
    S32			x, y, width, height;
    F32		    fov_x, fov_y;
    vec3_t		vieworg;
    vec3_t		viewaxis[3];		// transformation matrix
    
    stereoFrame_t	stereoFrame;
    
    S32			time;				// time in milliseconds for shader effects and other time dependent rendering issues
    S32			rdflags;			// RDF_NOWORLDMODEL, etc
    
    // 1 bits will prevent the associated area from rendering at all
    U8		    areamask[MAX_MAP_AREA_BYTES];
    bool	    areamaskModified;	// true if areamask changed since last scene
    
    F64		    floatTime;			// tr.refdef.time / 1000.0
    
    F32		    blurFactor;
    
    // text messages for deform text shaders
    UTF8		text[MAX_RENDER_STRINGS][MAX_RENDER_STRING_LENGTH];
    
    S64			num_entities;
    trRefEntity_t*	entities;
    
    S32			num_dlights;
    struct dlight_s*	dlights;
    
    S32			numPolys;
    struct srfPoly_s*	polys;
    
    S32			numDrawSurfs;
    struct drawSurf_s*	drawSurfs;
    
    U32         dlightMask;
    S32         num_pshadows;
    struct pshadow_s* pshadows;
    
#ifdef __DYNAMIC_SHADOWS__
    F32         dlightShadowMvp[MAX_DYNAMIC_SHADOWS][3][16];
#endif //__DYNAMIC_SHADOWS__
    F32         sunShadowMvp[4][16];
    F32         sunDir[4];
    F32         sunCol[4];
    F32         sunAmbCol[4];
    F32         colorScale;
    
    F32         autoExposureMinMax[2];
    F32         toneMinAvgMaxLinear[3];
} trRefdef_t;


//=================================================================================

// max surfaces per-skin
// This is an arbitry limit. Vanilla Q3 only supported 32 surfaces in skins but failed to
// enforce the maximum limit when reading skin files. It was possile to use more than 32
// surfaces which accessed out of bounds memory past end of skin->surfaces hunk block.
#define MAX_SKIN_SURFACES	256

// skins allow models to be retextured without modifying the model file
typedef struct
{
    UTF8		name[MAX_QPATH];
    shader_t*	shader;
} skinSurface_t;

typedef struct skin_s
{
    UTF8		name[MAX_QPATH];		// game path, including extension
    S32			numSurfaces;
    skinSurface_t*	surfaces;			// dynamically allocated array of surfaces
} skin_t;


typedef struct
{
    S32			originalBrushNumber;
    vec3_t		bounds[2];
    
    U32      	colorInt;				// in packed U8 format
    F32		    tcScale;				// texture coordinate vector scales
    fogParms_t	parms;
    
    // for clipping distance in fog when outside
    bool	    hasSurface;
    F32		    surface[4];
} fog_t;

typedef enum
{
    VPF_NONE            = 0x00,
    VPF_NOVIEWMODEL     = 0x01,
    VPF_SHADOWMAP       = 0x02,
    VPF_DEPTHSHADOW     = 0x04,
    VPF_DEPTHCLAMP      = 0x08,
    VPF_ORTHOGRAPHIC    = 0x10,
    VPF_USESUNLIGHT     = 0x20,
    VPF_FARPLANEFRUSTUM = 0x40,
    VPF_NOCUBEMAPS      = 0x80,
    VPF_NOPOSTPROCESS   = 0x100
} viewParmFlags_t;

typedef struct
{
    orientationr_t	orientation;
    orientationr_t	world;
    vec3_t	     	pvsOrigin;			// may be different than or.origin for portals
    bool	        isPortal;			// true if this view is through a portal
    bool	        isMirror;			// the portal is a mirror, invert the face culling
    S32/*viewParmFlags_t*/ flags;
    S32				frameSceneNum;		// copied from tr.frameSceneNum
    S32				frameCount;			// copied from tr.frameCount
    cplane_t		portalPlane;		// clip anything behind this if mirroring
    S32				viewportX, viewportY, viewportWidth, viewportHeight;
    FBO_t*			targetFbo;
    S32				targetFboLayer;
    S32				targetFboCubemapIndex;
    F32				fovX, fovY;
    F32				projectionMatrix[16];
    cplane_t		frustum[5];
    vec3_t			visBounds[2];
    F32				zFar;
    F32				zNear;
    stereoFrame_t	stereoFrame;
} viewParms_t;


/*
==============================================================================

SURFACES

==============================================================================
*/
typedef U8 color4ub_t[4];

// any changes in surfaceType must be mirrored in rb_surfaceTable[]
typedef enum
{
    SF_BAD,
    SF_SKIP,				// ignore
    SF_FACE,
    SF_GRID,
    SF_TRIANGLES,
    SF_POLY,
    SF_MDV,
    SF_MDR,
    SF_IQM,
    SF_FLARE,
    SF_ENTITY,				// beams, rails, lightning, etc that can be determined by entity
    SF_VAO_MDVMESH,
    
    SF_NUM_SURFACE_TYPES,
    SF_MAX = 0x7fffffff			// ensures that sizeof( surfaceType_t ) == sizeof( S32 )
} surfaceType_t;

typedef struct drawSurf_s
{
    U64                 sort;			// bit combination for fast compares
    S64                 cubemapIndex;
    surfaceType_t*		surface;		// any of surface*_t
} drawSurf_t;

#define	MAX_FACE_POINTS		1024

#define	MAX_PATCH_SIZE		32			// max dimensions of a patch mesh in map file
#define	MAX_GRID_SIZE		65			// max dimensions of a grid mesh in memory

// when cgame directly specifies a polygon, it becomes a srfPoly_t
// as soon as it is called
typedef struct srfPoly_s
{
    surfaceType_t	surfaceType;
    qhandle_t		hShader;
    S64				fogIndex;
    S32				numVerts;
    polyVert_t*		verts;
} srfPoly_t;


typedef struct srfFlare_s
{
    surfaceType_t	surfaceType;
    vec3_t			origin;
    vec3_t			normal;
    vec3_t			color;
} srfFlare_t;

typedef struct
{
    vec3_t          xyz;
    vec2_t          st;
    vec2_t          lightmap;
    S16         normal[4];
    S16         tangent[4];
    S16         lightdir[4];
    U16        color[4];
    
#if DEBUG_OPTIMIZEVERTICES
    U32    id;
#endif
} srfVert_t;

#define srfVert_t_cleared(x) srfVert_t (x) = {{0, 0, 0}, {0, 0}, {0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}

// srfBspSurface_t covers SF_GRID, SF_TRIANGLES and SF_POLY
typedef struct srfBspSurface_s
{
    surfaceType_t   surfaceType;
    
    // dynamic lighting information
    S32				dlightBits;
    S32             pshadowBits;
    
    // culling information
    vec3_t			cullBounds[2];
    vec3_t			cullOrigin;
    F32				cullRadius;
    cplane_t        cullPlane;
    
    // indexes
    S32             numIndexes;
    U32*			indexes;
    
    // vertexes
    S32             numVerts;
    srfVert_t*      verts;
    
    // SF_GRID specific variables after here
    
    // lod information, which may be different
    // than the culling information to allow for
    // groups of curves that LOD as a unit
    vec3_t			lodOrigin;
    F32				lodRadius;
    S32				lodFixed;
    S32				lodStitched;
    
    // vertexes
    S32				width, height;
    F32*			widthLodError;
    F32*			heightLodError;
} srfBspSurface_t;

// inter-quake-model
typedef struct
{
    S32		num_vertexes;
    S32		num_triangles;
    S32		num_frames;
    S32		num_surfaces;
    S32		num_joints;
    S32		num_poses;
    struct srfIQModel_s*	surfaces;
    
    F32*		positions;
    F32*		texcoords;
    F32*		normals;
    F32*		tangents;
    U8*		blendIndexes;
    union
    {
        F32*	f;
        U8*	b;
    } blendWeights;
    U8*		colors;
    S32*		triangles;
    
    // depending upon the exporter, blend indices and weights might be S32/F32
    // as opposed to the recommended U8/U8, for example Noesis exports
    // S32/F32 whereas the official IQM tool exports U8/U8
    U8 blendWeightsType; // IQM_UBYTE or IQM_FLOAT
    
    S32*		jointParents;
    F32*		jointMats;
    F32*		poseMats;
    F32*		bounds;
    UTF8*		names;
} iqmData_t;

// inter-quake-model surface
typedef struct srfIQModel_s
{
    surfaceType_t	surfaceType;
    UTF8		name[MAX_QPATH];
    shader_t*	shader;
    iqmData_t*	data;
    S32			first_vertex, num_vertexes;
    S32			first_triangle, num_triangles;
} srfIQModel_t;

typedef struct srfVaoMdvMesh_s
{
    surfaceType_t   surfaceType;
    
    struct mdvModel_s* mdvModel;
    struct mdvSurface_s* mdvSurface;
    
    // backEnd stats
    S32       numIndexes;
    S32       numVerts;
    U32       minIndex;
    U32       maxIndex;
    
    // static render data
    vao_t*          vao;
} srfVaoMdvMesh_t;

extern	void ( *rb_surfaceTable[SF_NUM_SURFACE_TYPES] )( void* );

/*
==============================================================================

SHADOWS

==============================================================================
*/

typedef struct pshadow_s
{
    F32 sort;
    
    S32    numEntities;
    S32    entityNums[8];
    vec3_t entityOrigins[8];
    F32  entityRadiuses[8];
    
    F32 viewRadius;
    vec3_t viewOrigin;
    
    vec3_t lightViewAxis[3];
    vec3_t lightOrigin;
    F32  lightRadius;
    cplane_t cullPlane;
} pshadow_t;


/*
==============================================================================

BRUSH MODELS

==============================================================================
*/


//
// in memory representation
//

#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2

#define CULLINFO_NONE   0
#define CULLINFO_BOX    1
#define CULLINFO_SPHERE 2
#define CULLINFO_PLANE  4

typedef struct cullinfo_s
{
    S32             type;
    vec3_t          bounds[2];
    vec3_t			localOrigin;
    F32				radius;
    cplane_t        plane;
} cullinfo_t;

typedef struct msurface_s
{
    //S32				viewCount;		// if == tr.viewCount, already added
    struct shader_s*	shader;
    S64					fogIndex;
    S32                 cubemapIndex;
    cullinfo_t          cullinfo;
    
    surfaceType_t*		data;			// any of srf*_t
} msurface_t;


#define	CONTENTS_NODE		-1
typedef struct mnode_s
{
    // common with leaf and node
    S32				contents;		// -1 for nodes, to differentiate from leafs
    S32             visCounts[MAX_VISCOUNTS];	// node needs to be traversed if current
    vec3_t			mins, maxs;		// for bounding box culling
    struct mnode_s*	parent;
    
    // node specific
    cplane_t*		plane;
    struct mnode_s*	children[2];
    
    // leaf specific
    S32				cluster;
    S32				area;
    
    S32				firstmarksurface;
    S32				nummarksurfaces;
} mnode_t;

typedef struct
{
    vec3_t			bounds[2];		// for culling
    S32				firstSurface;
    S32				numSurfaces;
} bmodel_t;

typedef struct
{
    UTF8		name[MAX_QPATH];		// ie: maps/tim_dm2.bsp
    UTF8		baseName[MAX_QPATH];	// ie: tim_dm2
    
    S32			dataSize;
    
    S32			numShaders;
    dshader_t*	shaders;
    
    S32			numBModels;
    bmodel_t*	bmodels;
    
    S32			numplanes;
    cplane_t*	planes;
    
    S32			numnodes;		// includes leafs
    S32			numDecisionNodes;
    mnode_t*	nodes;
    
    S32         numWorldSurfaces;
    
    S32			numsurfaces;
    msurface_t*	surfaces;
    S32*        surfacesViewCount;
    S32*        surfacesDlightBits;
    S32*		surfacesPshadowBits;
    
    S32			nummarksurfaces;
    S32*        marksurfaces;
    
    S32			numfogs;
    fog_t*		fogs;
    
    vec3_t		lightGridOrigin;
    vec3_t		lightGridSize;
    vec3_t		lightGridInverseSize;
    S32			lightGridBounds[3];
    U8*			lightGridData;
    U16*		lightGrid16;
    
    
    S32			numClusters;
    S32			clusterBytes;
    const U8*	vis;			// may be passed in by CM_LoadMap to save space
    
    UTF8*		entityString;
    UTF8*		entityParsePoint;
} world_t;


/*
==============================================================================
MDV MODELS - meta format for vertex animation models like .md2, .md3, .mdc
==============================================================================
*/
typedef struct
{
    F32           bounds[2][3];
    F32           localOrigin[3];
    F32           radius;
} mdvFrame_t;

typedef struct
{
    F32           origin[3];
    F32           axis[3][3];
} mdvTag_t;

typedef struct
{
    UTF8            name[MAX_QPATH];	// tag name
} mdvTagName_t;

typedef struct
{
    vec3_t      xyz;
    S16         normal[4];
    S16         tangent[4];
} mdvVertex_t;

typedef struct
{
    F32           st[2];
} mdvSt_t;

typedef struct mdvSurface_s
{
    surfaceType_t   surfaceType;
    
    UTF8            name[MAX_QPATH];	// polyset name
    
    S32             numShaderIndexes;
    S32*			shaderIndexes;
    
    S32             numVerts;
    mdvVertex_t*    verts;
    mdvSt_t*        st;
    
    S32             numIndexes;
    U32*			indexes;
    
    struct mdvModel_s* model;
} mdvSurface_t;

typedef struct mdvModel_s
{
    S32             numFrames;
    mdvFrame_t*     frames;
    
    S32             numTags;
    mdvTag_t*       tags;
    mdvTagName_t*   tagNames;
    
    S32             numSurfaces;
    mdvSurface_t*   surfaces;
    
    S32             numVaoSurfaces;
    srfVaoMdvMesh_t*  vaoSurfaces;
    
    S32             numSkins;
} mdvModel_t;


//======================================================================

typedef enum
{
    MOD_BAD,
    MOD_BRUSH,
    MOD_MESH,
    MOD_MDR,
    MOD_IQM
} modtype_t;

typedef struct model_s
{
    UTF8		name[MAX_QPATH];
    modtype_t	type;
    S32			index;		// model = tr.models[model->index]
    S32			dataSize;	// just for listing purposes
    bmodel_t*	bmodel;		// only if type == MOD_BRUSH
    mdvModel_t*	mdv[MD3_MAX_LODS];	// only if type == MOD_MESH
    void*	    modelData;			// only if type == (MOD_MDR | MOD_IQM)
    S32			numLods;
} model_t;


#define	MAX_MOD_KNOWN	1024

void R_ModelInit( void );
model_t* R_GetModelByHandle( qhandle_t hModel );
void R_Modellist_f( void );

//====================================================

#define	MAX_DRAWIMAGES			2048
#define	MAX_SKINS				1024


#define	MAX_DRAWSURFS			0x10000
#define	DRAWSURF_MASK			(MAX_DRAWSURFS-1)

/*

the drawsurf sort data is packed into a single 32 bit value so it can be
compared quickly during the qsorting process

the bits are allocated as follows:

0 - 1	: dlightmap index
//2		: used to be clipped flag REMOVED - 03.21.00 rad
2 - 6	: fog index
11 - 20	: entity index
21 - 31	: sorted shader index

	TTimo - 1.32
0-1   : dlightmap index
2-6   : fog index
7-16  : entity index
17-30 : sorted shader index

    SmileTheory - for pshadows
17-31 : sorted shader index
7-16  : entity index
2-6   : fog index
1     : pshadow flag
0     : dlight flag
*/
#define	QSORT_FOGNUM_SHIFT	2
#define	QSORT_REFENTITYNUM_SHIFT	7
#define	QSORT_SHADERNUM_SHIFT	(QSORT_REFENTITYNUM_SHIFT+REFENTITYNUM_BITS)
#if (QSORT_SHADERNUM_SHIFT+SHADERNUM_BITS) > 64
#error "Need to update sorting, too many bits."
#endif
#define QSORT_PSHADOW_SHIFT     1

extern	S32	gl_filter_min, gl_filter_max;

/*
** performanceCounters_t
*/
typedef struct
{
    S32		c_sphere_cull_patch_in, c_sphere_cull_patch_clip, c_sphere_cull_patch_out;
    S32		c_box_cull_patch_in, c_box_cull_patch_clip, c_box_cull_patch_out;
    S32		c_sphere_cull_md3_in, c_sphere_cull_md3_clip, c_sphere_cull_md3_out;
    S32		c_box_cull_md3_in, c_box_cull_md3_clip, c_box_cull_md3_out;
    
    S32		c_leafs;
    S32		c_dlightSurfaces;
    S32		c_dlightSurfacesCulled;
} frontEndCounters_t;

#define	FOG_TABLE_SIZE		256
#define FUNCTABLE_SIZE		1024
#define FUNCTABLE_SIZE2		10
#define FUNCTABLE_MASK		(FUNCTABLE_SIZE-1)


// the renderer front end should never modify glstate_t
typedef struct
{
    bool		finishCalled;
    S32			texEnv[2];
    S32			faceCulling;
    S32         faceCullFront;
    U32			glStateBits;
    U32			storedGlState;
    F32         vertexAttribsInterpolation;
    bool        vertexAnimation;
    U32			vertexAttribsEnabled;  // global if no VAOs, tess only otherwise
    FBO_t*      currentFBO;
    vao_t*      currentVao;
    mat4_t      modelview;
    mat4_t      projection;
    mat4_t		modelviewProjection;
    mat4_t		invProjection;
    mat4_t		invEyeProjection;
} glstate_t;

typedef enum
{
    MI_NONE,
    MI_NVX,
    MI_ATI
} memInfo_t;

typedef enum
{
    TCR_NONE = 0x0000,
    TCR_RGTC = 0x0001,
    TCR_BPTC = 0x0002,
} textureCompressionRef_t;

// We can't change glConfig_t without breaking DLL/vms compatibility, so
// store extensions we have here.
typedef struct
{
    S32 openglMajorVersion;
    S32 openglMinorVersion;
    
    bool intelGraphics;
    
    bool occlusionQuery;
    
    S32 glslMajorVersion;
    S32 glslMinorVersion;
    
    memInfo_t memInfo;
    
    bool framebufferObject;
    S32 maxRenderbufferSize;
    S32 maxColorAttachments;
    
    bool textureFloat;
    S32 /*textureCompressionRef_t*/ textureCompression;
    bool swizzleNormalmap;
    
    bool framebufferMultisample;
    bool framebufferBlit;
    
    bool depthClamp;
    bool seamlessCubeMap;
    
    bool vertexArrayObject;
    bool directStateAccess;
    bool immutableTextures;
} glRefConfig_t;


typedef struct
{
    S32 c_surfaces, c_shaders, c_vertexes, c_indexes, c_totalIndexes;
    S32 c_surfBatches;
    F32	c_overDraw;
    
    S32	c_vaoBinds;
    S32	c_vaoVertexes;
    S32	c_vaoIndexes;
    
    S32 c_staticVaoDraws;
    S32 c_dynamicVaoDraws;
    
    S32	c_dlightVertexes;
    S32	c_dlightIndexes;
    
    S32	c_flareAdds;
    S32	c_flareTests;
    S32	c_flareRenders;
    
    S32 c_glslShaderBinds;
    S32 c_genericDraws;
    S32 c_lightallDraws;
    S32 c_fogDraws;
    S32 c_dlightDraws;
    
    S32		msec;			// total msec for backend run
} backEndCounters_t;

// all state modified by the back end is seperated
// from the front end state
typedef struct
{
    trRefdef_t	refdef;
    viewParms_t	viewParms;
    orientationr_t	orientation;
    backEndCounters_t	pc;
    bool	isHyperspace;
    trRefEntity_t*	currentEntity;
    bool	skyRenderedThisView;	// flag for drawing sun
    
    bool	projection2D;	// if true, drawstretchpic doesn't need to change modes
    U8		color2D[4];
    bool	vertexes2D;		// shader needs to be finished
    trRefEntity_t	entity2D;	// currentEntity will point at this when doing 2D rendering
    
    bool floatfix;
    FBO_t* last2DFBO;
    bool    colorMask[4];
    bool    framePostProcessed;
    bool    depthFill;
} backEndState_t;

/*
** trGlobals_t
**
** Most renderer globals are defined here.
** backend functions should never modify any of these fields,
** but may read fields that aren't dynamically modified
** by the frontend.
*/
typedef struct
{
    bool registered; // cleared at shutdown, set at beginRegistration
    
    S32 visIndex;
    S32 visClusters[MAX_VISCOUNTS];
    S32 visCounts[MAX_VISCOUNTS]; // incremented every time a new vis cluster is entered
    S32 frameCount; // incremented every frame
    S32 sceneCount; // incremented every scene
    S32 viewCount; // incremented every view (twice a scene if portaled)
    // and every R_MarkFragments call
    
    S32 frameSceneNum; // zeroed at RE_BeginFrame
    
    bool worldMapLoaded;
    bool worldDeluxeMapping;
    vec2_t autoExposureMinMax;
    vec3_t toneMinAvgMaxLevel;
    world_t* world;
    
    const U8* externalVisData;	// from RE_SetWorldVisData, shared with CM_Load
    
    image_t* defaultImage;
    image_t* scratchImage[32];
    image_t* fogImage;
    image_t* dlightImage; // inverse-quare highlight for projective adding
    image_t* flareImage;
    image_t* whiteImage; // full of 0xff
    image_t* identityLightImage; // full of tr.identityLightByte
    image_t* shadowCubemaps[MAX_DLIGHTS];
    image_t* renderImage;
    image_t* normalDetailedImage;
    image_t* sunRaysImage;
    image_t* renderDepthImage;
    image_t* pshadowMaps[MAX_DRAWN_PSHADOWS];
    image_t* screenScratchImage;
    image_t* textureScratchImage[2];
    image_t* quarterImage[2];
    image_t* calcLevelsImage;
    image_t* targetLevelsImage;
    image_t* fixedLevelsImage;
    image_t* sunShadowDepthImage[4];
    image_t* screenShadowImage;
#ifdef __DYNAMIC_SHADOWS__
    image_t* dlightShadowDepthImage[MAX_DYNAMIC_SHADOWS][3];
    //image_t* screenDlightShadowImage;
#endif //__DYNAMIC_SHADOWS__
    image_t* screenSsaoImage;
    image_t* hdrDepthImage;
    image_t* renderCubeImage;
    image_t* textureDepthImage;
    
    FBO_t* renderFbo;
    FBO_t* sunRaysFbo;
    FBO_t* depthFbo;
    FBO_t* pshadowFbos[MAX_DRAWN_PSHADOWS];
    FBO_t* screenScratchFbo;
    FBO_t* textureScratchFbo[2];
    FBO_t* quarterFbo[2];
    FBO_t* calcLevelsFbo;
    FBO_t* targetLevelsFbo;
    FBO_t* sunShadowFbo[4];
    FBO_t* screenShadowFbo;
    FBO_t* screenSsaoFbo;
    FBO_t* hdrDepthFbo;
    FBO_t* renderCubeFbo;
#ifdef __DYNAMIC_SHADOWS__
    FBO_t* dlightShadowFbo[MAX_DYNAMIC_SHADOWS][3];
    //FBO_t* screenDlightShadowFbo;
#endif //__DYNAMIC_SHADOWS__
    
    shader_t* defaultShader;
    shader_t* shadowShader;
    shader_t* projectionShadowShader;
    
    shader_t* flareShader;
    shader_t* sunShader;
    shader_t* sunFlareShader;
    
    S32	numLightmaps;
    S32	lightmapSize;
    image_t** lightmaps;
    image_t** deluxemaps;
    
    S32	fatLightmapCols;
    S32	fatLightmapRows;
    
    S32 numCubemaps;
    cubemap_t* cubemaps;
    
    trRefEntity_t* currentEntity;
    trRefEntity_t worldEntity; // point currentEntity at this when rendering world
    S64 currentEntityNum;
    S64	shiftedEntityNum; // currentEntityNum << QSORT_REFENTITYNUM_SHIFT
    model_t* currentModel;
    
    //
    // GPU shader programs
    //
    shaderProgram_t genericShader[GENERICDEF_COUNT];
    shaderProgram_t textureColorShader;
    shaderProgram_t fogShader[FOGDEF_COUNT];
    shaderProgram_t dlightShader[DLIGHTDEF_COUNT];
    shaderProgram_t lightallShader[LIGHTDEF_COUNT];
    shaderProgram_t shadowmapShader;
    shaderProgram_t pshadowShader;
    shaderProgram_t down4xShader;
    shaderProgram_t bokehShader;
    shaderProgram_t tonemapShader;
    shaderProgram_t calclevels4xShader[2];
    shaderProgram_t shadowmaskShader;
    shaderProgram_t ssaoShader;
    shaderProgram_t depthBlurShader[4];
    shaderProgram_t testcubeShader;
    shaderProgram_t gaussianBlurShader[2];
    
    shaderProgram_t darkexpandShader;
    shaderProgram_t hdrShader;
    shaderProgram_t dofShader;
    shaderProgram_t anaglyphShader;
    //shaderProgram_t uniqueskyShader;
    shaderProgram_t waterShader;
    shaderProgram_t esharpeningShader;
    shaderProgram_t esharpening2Shader;
    shaderProgram_t texturecleanShader;
    shaderProgram_t lensflareShader;
    shaderProgram_t multipostShader;
    shaderProgram_t anamorphicDarkenShader;
    shaderProgram_t anamorphicBlurShader;
    shaderProgram_t anamorphicCombineShader;
    shaderProgram_t volumelightShader;
    shaderProgram_t vibrancyShader;
    shaderProgram_t fxaaShader;
    shaderProgram_t bloomDarkenShader;
    shaderProgram_t bloomBlurShader;
    shaderProgram_t bloomCombineShader;
    shaderProgram_t ssgiShader;
    shaderProgram_t ssgiBlurShader;
    shaderProgram_t texturedetailShader;
    shaderProgram_t rbmShader;
    shaderProgram_t contrastShader;
    
    image_t*        bloomRenderFBOImage[3];
    image_t*        anamorphicRenderFBOImage[3];
    image_t*        genericFBOImage;
    
    FBO_t*          bloomRenderFBO[3];
    FBO_t*          anamorphicRenderFBO[3];
    FBO_t*	   	    genericFbo;
    
    // -----------------------------------------
    
    viewParms_t viewParms;
    
    F32 identityLight; // 1.0 / ( 1 << overbrightBits )
    S32 identityLightByte; // identityLight * 255
    S32 overbrightBits; // r_overbrightBits->integer, but set to 0 if no hw gamma
    
    orientationr_t orientation;	// for current entity
    
    trRefdef_t refdef;
    
    S32 viewCluster;
    
    F32 sunShadowScale;
    
    bool sunShadows;
    vec3_t sunLight; // from the sky shader for this level
    vec3_t sunDirection;
    vec3_t lastCascadeSunDirection;
    F32 lastCascadeSunMvp[16];
    
    frontEndCounters_t pc;
    S32 frontEndMsec; // not in pc due to clearing issue
    
    vec4_t clipRegion; // 2D clipping region
    
    //
    // put large tables at the end, so most elements will be
    // within the +/32K indexed range on risc processors
    //
    model_t* models[MAX_MOD_KNOWN];
    S32 numModels;
    
    S32 numImages;
    image_t* images[MAX_DRAWIMAGES];
    
    S32 numFBOs;
    FBO_t* fbos[MAX_FBOS];
    
    S32 numVaos;
    vao_t* vaos[MAX_VAOS];
    
    // shader indexes from other modules will be looked up in tr.shaders[]
    // shader indexes from drawsurfs will be looked up in sortedShaders[]
    // lower indexed sortedShaders must be rendered first (opaque surfaces before translucent)
    S32 numShaders;
    shader_t* shaders[MAX_SHADERS];
    shader_t* sortedShaders[MAX_SHADERS];
    
    S32 numSkins;
    skin_t* skins[MAX_SKINS];
    
    U32 sunFlareQuery[2];
    S32 sunFlareQueryIndex;
    bool sunFlareQueryActive[2];
    
    F32 sinTable[FUNCTABLE_SIZE];
    F32 squareTable[FUNCTABLE_SIZE];
    F32 triangleTable[FUNCTABLE_SIZE];
    F32 sawToothTable[FUNCTABLE_SIZE];
    F32 inverseSawToothTable[FUNCTABLE_SIZE];
    F32 fogTable[FOG_TABLE_SIZE];
    F32 distanceCull, distanceCullSquared;
} trGlobals_t;

extern backEndState_t	backEnd;
extern trGlobals_t	tr;
extern glstate_t	glState;		// outside of TR since it shouldn't be cleared during ref re-init
extern glRefConfig_t glRefConfig;

//
// cvars
//
extern cvar_t*  r_mode;
extern cvar_t*	r_flareSize;
extern cvar_t*	r_flareFade;
// coefficient for the flare intensity falloff function.
#define FLARE_STDCOEFF "150"
extern cvar_t*	r_flareCoeff;

extern cvar_t*	r_railWidth;
extern cvar_t*	r_railCoreWidth;
extern cvar_t*	r_railSegmentLength;

extern cvar_t*	r_ignore;				// used for debugging anything
extern cvar_t*	r_verbose;				// used for verbose debug spew

extern cvar_t*	r_znear;				// near Z clip plane
extern cvar_t*	r_zproj;				// z distance of projection plane
extern cvar_t*	r_stereoSeparation;			// separation of cameras for stereo rendering

extern cvar_t*	r_measureOverdraw;		// enables stencil buffer overdraw measurement

extern cvar_t*	r_lodbias;				// push/pull LOD transitions
extern cvar_t*	r_lodscale;

extern cvar_t*	r_inGameVideo;				// controls whether in game video should be draw
extern cvar_t*	r_fastsky;				// controls whether sky should be cleared or drawn
extern cvar_t*	r_drawSun;				// controls drawing of sun quad
extern cvar_t*	r_dynamiclight;		// dynamic lights enabled/disabled
extern cvar_t*	r_dlightBacks;			// dlight non-facing surfaces for continuity

extern cvar_t*	r_norefresh;			// bypasses the ref rendering
extern cvar_t*	r_drawentities;		// disable/enable entity rendering
extern cvar_t*	r_drawworld;			// disable/enable world rendering
extern cvar_t*	r_speeds;				// various levels of information display
extern cvar_t*	r_detailTextures;		// enables/disables detail texturing stages
extern cvar_t*	r_novis;				// disable/enable usage of PVS
extern cvar_t*	r_nocull;
extern cvar_t*	r_facePlaneCull;		// enables culling of planar surfaces with back side test
extern cvar_t*	r_nocurves;
extern cvar_t*	r_showcluster;

extern cvar_t*	r_gamma;

extern cvar_t*  r_ext_framebuffer_object;
extern cvar_t*  r_ext_framebuffer_blit;
extern cvar_t*  r_ext_texture_float;
extern cvar_t*  r_ext_framebuffer_multisample;
extern cvar_t*  r_arb_seamless_cube_map;
extern cvar_t*  r_arb_vertex_array_object;
extern cvar_t*  r_ext_direct_state_access;

extern	cvar_t*	r_nobind;						// turns off binding to appropriate textures
extern	cvar_t*	r_singleShader;				// make most world faces use default shader
extern	cvar_t*	r_roundImagesDown;
extern	cvar_t*	r_colorMipLevels;				// development aid to see texture mip usage
extern	cvar_t*	r_picmip;						// controls picmip values
extern	cvar_t*	r_finish;
extern	cvar_t*	r_textureMode;
extern	cvar_t*	r_offsetFactor;
extern	cvar_t*	r_offsetUnits;

extern	cvar_t*	r_fullbright;					// avoid lightmap pass
extern	cvar_t*	r_lightmap;					// render lightmaps only
extern	cvar_t*	r_vertexLight;					// vertex lighting mode for better performance
extern	cvar_t*	r_uiFullScreen;				// ui is running fullscreen

extern	cvar_t*	r_logFile;						// number of frames to emit GL logs
extern	cvar_t*	r_showtris;					// enables wireframe rendering of the world
extern	cvar_t*	r_showsky;						// forces sky in front of all surfaces
extern	cvar_t*	r_shownormals;					// draws wireframe normals
extern	cvar_t*	r_clear;						// force screen clear every frame

extern	cvar_t*	r_shadows;						// controls shadows: 0 = none, 1 = blur, 2 = stencil, 3 = black planar projection
extern	cvar_t*	r_flares;						// light flares

extern	cvar_t*	r_intensity;

extern	cvar_t*	r_lockpvs;
extern	cvar_t*	r_noportals;
extern	cvar_t*	r_portalOnly;

extern	cvar_t*	r_subdivisions;
extern	cvar_t*	r_lodCurveError;
extern	cvar_t*	r_skipBackEnd;

extern	cvar_t*	r_anaglyphMode;
extern  cvar_t* r_floatLightmap;
extern  cvar_t* r_postProcess;

extern  cvar_t* r_toneMap;
extern  cvar_t* r_forceToneMap;
extern  cvar_t* r_forceToneMapMin;
extern  cvar_t* r_forceToneMapAvg;
extern  cvar_t* r_forceToneMapMax;

extern  cvar_t* r_autoExposure;
extern  cvar_t* r_forceAutoExposure;
extern  cvar_t* r_forceAutoExposureMin;
extern  cvar_t* r_forceAutoExposureMax;

extern  cvar_t* r_cameraExposure;

extern  cvar_t* r_depthPrepass;
extern  cvar_t* r_ssao;

extern  cvar_t* r_normalMapping;
extern  cvar_t* r_specularMapping;
extern  cvar_t* r_deluxeMapping;
extern  cvar_t* r_parallaxMapping;
extern  cvar_t* r_cubeMapping;
extern  cvar_t* r_horizonFade;
extern  cvar_t* r_deluxeSpecular;
extern  cvar_t* r_pbr;
extern  cvar_t* r_baseNormalX;
extern  cvar_t* r_baseNormalY;
extern  cvar_t* r_baseParallax;
extern  cvar_t* r_baseSpecular;
extern  cvar_t* r_baseGloss;
extern  cvar_t* r_glossType;
extern  cvar_t* r_dlightMode;
extern  cvar_t* r_pshadowDist;
extern  cvar_t* r_mergeLightmaps;
extern  cvar_t* r_imageUpsample;
extern  cvar_t* r_imageUpsampleMaxSize;
extern  cvar_t* r_imageUpsampleType;
extern  cvar_t* r_genNormalMaps;
extern  cvar_t* r_forceSun;
extern  cvar_t* r_forceSunLightScale;
extern  cvar_t* r_forceSunAmbientScale;
extern  cvar_t* r_sunlightMode;
extern  cvar_t* r_drawSunRays;
extern  cvar_t* r_sunShadows;
extern  cvar_t* r_shadowFilter;
extern  cvar_t* r_shadowBlur;
extern  cvar_t* r_shadowMapSize;
extern  cvar_t* r_shadowCascadeZNear;
extern  cvar_t* r_shadowCascadeZFar;
extern  cvar_t* r_shadowCascadeZBias;
extern  cvar_t* r_ignoreDstAlpha;

extern	cvar_t*	r_greyscale;

extern	cvar_t*	r_ignoreGLErrors;

extern	cvar_t*	r_overBrightBits;
extern	cvar_t*	r_mapOverBrightBits;

extern	cvar_t*	r_debugSurface;
extern	cvar_t*	r_simpleMipMaps;

extern	cvar_t*	r_showImages;
extern	cvar_t*	r_debugSort;

extern cvar_t* r_printShaders;

extern cvar_t* r_marksOnTriangleMeshes;
extern cvar_t* r_floatfix;
extern cvar_t* r_lensflare;
extern cvar_t* r_volumelight;
extern cvar_t* r_anamorphic;
extern cvar_t* r_anamorphicDarkenPower;
extern cvar_t* r_ssgi;
extern cvar_t* r_ssgiWidth;
extern cvar_t* r_ssgiSamples;
extern cvar_t* r_darkexpand;
extern cvar_t* r_truehdr;
extern cvar_t* r_dof;
extern cvar_t* r_esharpening;
extern cvar_t* r_esharpening2;
extern cvar_t* r_fxaa;
extern cvar_t* r_bloom;
extern cvar_t* r_bloomPasses;
extern cvar_t* r_bloomDarkenPower;
extern cvar_t* r_bloomScale;
extern cvar_t* r_multipost;
extern cvar_t* r_textureClean;
extern cvar_t* r_textureCleanSigma;
extern cvar_t* r_textureCleanBSigma;
extern cvar_t* r_textureCleanMSize;
extern cvar_t* r_trueAnaglyph;
extern cvar_t* r_trueAnaglyphSeparation;
extern cvar_t* r_trueAnaglyphRed;
extern cvar_t* r_trueAnaglyphGreen;
extern cvar_t* r_trueAnaglyphBlue;
extern cvar_t* r_vibrancy;
extern cvar_t* r_multithread;
extern cvar_t* r_texturedetail;
extern cvar_t* r_texturedetailStrength;
extern cvar_t* r_rbm;
extern cvar_t* r_rbmStrength;
extern cvar_t* r_screenblur;
extern cvar_t* r_brightness;
extern cvar_t* r_contrast;
extern cvar_t* r_gamma;

//====================================================================

void R_SwapBuffers( S32 );

void R_RenderView( viewParms_t* parms );
void R_RenderDlightCubemaps( const refdef_t* fd );
void R_RenderPshadowMaps( const refdef_t* fd );
void R_RenderSunShadowMaps( const refdef_t* fd, S32 level );
void R_RenderCubemapSide( S32 cubemapIndex, S32 cubemapSide, bool subscene );
#ifdef __DYNAMIC_SHADOWS__
void R_RenderDlightShadowMaps( const refdef_t* fd, S32 level );
#endif //__DYNAMIC_SHADOWS__

void R_AddMD3Surfaces( trRefEntity_t* e );
void R_AddNullModelSurfaces( trRefEntity_t* e );
void R_AddBeamSurfaces( trRefEntity_t* e );
void R_AddRailSurfaces( trRefEntity_t* e, bool isUnderwater );
void R_AddLightningBoltSurfaces( trRefEntity_t* e );

void R_AddPolygonSurfaces( void );
void R_DecomposeSort( const U64 sort, S64* entityNum, shader_t** shader, S64* fogNum, S64* dlightMap, S64* pshadowMap );
void R_AddDrawSurf( surfaceType_t* surface, shader_t* shader, S64 fogIndex, S64 dlightMap, S64 pshadowMap, S64 cubemap );
void R_CalcTexDirs( vec3_t sdir, vec3_t tdir, const vec3_t v1, const vec3_t v2, const vec3_t v3, const vec2_t w1, const vec2_t w2, const vec2_t w3 );
vec_t R_CalcTangentSpace( vec3_t tangent, vec3_t bitangent, const vec3_t normal, const vec3_t sdir, const vec3_t tdir );
bool R_CalcTangentVectors( srfVert_t* dv[3] );

#define	CULL_IN		0		// completely unclipped
#define	CULL_CLIP	1		// clipped by one or more planes
#define	CULL_OUT	2		// completely outside the clipping planes

void R_LocalNormalToWorld( const vec3_t local, vec3_t world );
void R_LocalPointToWorld( const vec3_t local, vec3_t world );
S32 R_CullBox( vec3_t bounds[2] );
S32 R_CullLocalBox( vec3_t bounds[2] );
S32 R_CullPointAndRadiusEx( const vec3_t origin, F32 radius, const cplane_t* frustum, S32 numPlanes );
S32 R_CullPointAndRadius( const vec3_t origin, F32 radius );
S32 R_CullLocalPointAndRadius( const vec3_t origin, F32 radius );
void R_SetupProjection( viewParms_t* dest, F32 zProj, F32 zFar, bool computeFrustum );
void R_RotateForEntity( const trRefEntity_t* ent, const viewParms_t* viewParms, orientationr_t* orientation );

/*
** GL wrapper/helper functions
*/
void	GL_BindToTMU( image_t* image, S32 tmu );
void	GL_SetDefaultState( void );
void	GL_TextureMode( StringEntry string );
void	GL_CheckErrs( StringEntry file, S32 line );
#define GL_CheckErrors(...) GL_CheckErrs(__FILE__, __LINE__)
void	GL_State( U64 stateVector );
void    GL_SetProjectionMatrix( mat4_t matrix );
void    GL_SetModelviewMatrix( mat4_t matrix );
void	GL_Cull( S32 cullType );

#define GLS_SRCBLEND_ZERO						0x00000001
#define GLS_SRCBLEND_ONE						0x00000002
#define GLS_SRCBLEND_DST_COLOR					0x00000003
#define GLS_SRCBLEND_ONE_MINUS_DST_COLOR		0x00000004
#define GLS_SRCBLEND_SRC_ALPHA					0x00000005
#define GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA		0x00000006
#define GLS_SRCBLEND_DST_ALPHA					0x00000007
#define GLS_SRCBLEND_ONE_MINUS_DST_ALPHA		0x00000008
#define GLS_SRCBLEND_ALPHA_SATURATE				0x00000009
#define	GLS_SRCBLEND_BITS						0x0000000f

#define GLS_DSTBLEND_ZERO						0x00000010
#define GLS_DSTBLEND_ONE						0x00000020
#define GLS_DSTBLEND_SRC_COLOR					0x00000030
#define GLS_DSTBLEND_ONE_MINUS_SRC_COLOR		0x00000040
#define GLS_DSTBLEND_SRC_ALPHA					0x00000050
#define GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA		0x00000060
#define GLS_DSTBLEND_DST_ALPHA					0x00000070
#define GLS_DSTBLEND_ONE_MINUS_DST_ALPHA		0x00000080
#define	GLS_DSTBLEND_BITS						0x000000f0

#define GLS_DEPTHMASK_TRUE						0x00000100

#define GLS_POLYMODE_LINE						0x00001000

#define GLS_DEPTHTEST_DISABLE					0x00010000
#define GLS_DEPTHFUNC_EQUAL						0x00020000
#define GLS_DEPTHFUNC_GREATER                   0x00040000
#define GLS_DEPTHFUNC_BITS                      0x00060000

#define GLS_ATEST_GT_0							0x10000000
#define GLS_ATEST_LT_80							0x20000000
#define GLS_ATEST_GE_80							0x40000000
#define	GLS_ATEST_BITS							0x70000000

#define GLS_DEFAULT			GLS_DEPTHMASK_TRUE

model_t*		R_AllocModel( void );

void R_Init( void );
void R_UpdateSubImage( image_t* image, U8* pic, S32 x, S32 y, S32 width, S32 height, U32 picFormat );

void R_SetColorMappings( void );
void R_GammaCorrect( U8* buffer, S32 bufSize );

void R_ImageList_f( void );
void R_SkinList_f( void );
// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=516
const void* RB_TakeScreenshotCmd( const void* data );
void R_ScreenShot_f( void );

void R_InitFogTable( void );
F32 R_FogFactor( F32 s, F32 t );
void R_InitImages( void );
void R_DeleteTextures( void );
S32	R_SumOfUsedImages( void );
void R_InitSkins( void );
skin_t*	R_GetSkinByHandle( qhandle_t hSkin );

S32 R_ComputeLOD( trRefEntity_t* ent );

const void* RB_TakeVideoFrameCmd( const void* data );

//
// tr_shader.c
//
shader_t*	R_FindShader( StringEntry name, S32 lightmapIndex, bool mipRawImage );
shader_t*	R_GetShaderByHandle( qhandle_t hShader );
shader_t*	R_GetShaderByState( S32 index, S64* cycleTime );
shader_t* R_FindShaderByName( StringEntry name );
void		R_InitShaders( void );
void		R_ShaderList_f( void );

/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/

void		GLimp_InitExtraExtensions( void );

/*
====================================================================

TESSELATOR/SHADER DECLARATIONS

====================================================================
*/

typedef struct stageVars
{
    color4ub_t	colors[SHADER_MAX_VERTEXES];
    vec2_t		texcoords[NUM_TEXTURE_BUNDLES][SHADER_MAX_VERTEXES];
} stageVars_t;

typedef struct shaderCommands_s
{
    U32	indexes[SHADER_MAX_INDEXES];
    vec4_t		xyz[SHADER_MAX_VERTEXES];
    S16		normal[SHADER_MAX_VERTEXES][4];
    S16		tangent[SHADER_MAX_VERTEXES][4];
    vec2_t		texCoords[SHADER_MAX_VERTEXES];
    vec2_t		lightCoords[SHADER_MAX_VERTEXES];
    U16	color[SHADER_MAX_VERTEXES][4];
    S16		lightdir[SHADER_MAX_VERTEXES][4];
    //S32			vertexDlightBits[SHADER_MAX_VERTEXES];
    
    void* attribPointers[ATTR_INDEX_COUNT];
    vao_t*       vao;
    bool    useInternalVao;
    bool    useCacheVao;
    
    stageVars_t	svars;
    
    //color4ub_t	constantColor255[SHADER_MAX_VERTEXES];
    
    shader_t*	shader;
    F64		shaderTime;
    S32			fogNum;
    S32         cubemapIndex;
    
    S32			dlightBits;	// or together of all vertexDlightBits
    S32         pshadowBits;
    
    S32			firstIndex;
    S32			numIndexes;
    S32			numVertexes;
    
    // info extracted from current shader
    S32			numPasses;
    void	( *currentStageIteratorFunc )( void );
    shaderStage_t**	xstages;
} shaderCommands_t;

extern	shaderCommands_t	tess;

void RB_BeginSurface( shader_t* shader, S32 fogNum, S32 cubemapIndex );
void RB_EndSurface( void );
void RB_CheckOverflow( S32 verts, S32 indexes );
#define RB_CHECKOVERFLOW(v,i) if (tess.numVertexes + (v) >= SHADER_MAX_VERTEXES || tess.numIndexes + (i) >= SHADER_MAX_INDEXES ) {RB_CheckOverflow(v,i);}

void R_DrawElements( S32 numIndexes, U32 firstIndex );
void RB_StageIteratorGeneric( void );
void RB_StageIteratorSky( void );
void RB_StageIteratorVertexLitTexture( void );
void RB_StageIteratorLightmappedMultitexture( void );

void RB_AddQuadStamp( vec3_t origin, vec3_t left, vec3_t up, F32 color[4] );
void RB_AddQuadStampExt( vec3_t origin, vec3_t left, vec3_t up, F32 color[4], F32 s1, F32 t1, F32 s2, F32 t2 );
void RB_InstantQuad( vec4_t quadVerts[4] );
//void RB_InstantQuad2(vec4_t quadVerts[4], vec2_t texCoords[4], vec4_t color, shaderProgram_t *sp, vec2_t invTexRes);
void RB_InstantQuad2( vec4_t quadVerts[4], vec2_t texCoords[4] );

void RB_ShowImages( void );


/*
============================================================

WORLD MAP

============================================================
*/

void R_AddBrushModelSurfaces( trRefEntity_t* e );
void R_AddWorldSurfaces( void );

/*
============================================================

FLARES

============================================================
*/

void R_ClearFlares( void );

void RB_AddFlare( void* surface, S32 fogNum, vec3_t point, vec3_t color, vec3_t normal );
void RB_AddDlightFlares( void );
void RB_RenderFlares( void );

/*
============================================================

LIGHTS

============================================================
*/

void R_DlightBmodel( bmodel_t* bmodel );
void R_SetupEntityLighting( const trRefdef_t* refdef, trRefEntity_t* ent );
void R_TransformDlights( S32 count, dlight_t* dl, orientationr_t* orientation );
bool R_LightDirForPoint( vec3_t point, vec3_t lightDir, vec3_t normal, world_t* world );
S32 R_CubemapForPoint( vec3_t point );


/*
============================================================

SHADOWS

============================================================
*/

void RB_ShadowTessEnd( void );
void RB_ShadowFinish( void );
void RB_ProjectionShadowDeform( void );

/*
============================================================

SKIES

============================================================
*/

void R_BuildCloudData( shaderCommands_t* shader );
void R_InitSkyTexCoords( F32 cloudLayerHeight );
void R_DrawSkyBox( shaderCommands_t* shader );
void RB_DrawSun( F32 scale, shader_t* shader );
void RB_ClipSkyPolygons( shaderCommands_t* shader );

/*
============================================================

CURVE TESSELATION

============================================================
*/

#define PATCH_STITCHING

void R_SubdividePatchToGrid( srfBspSurface_t* grid, S32 width, S32 height,
                             srfVert_t points[MAX_PATCH_SIZE* MAX_PATCH_SIZE] );
void R_GridInsertColumn( srfBspSurface_t* grid, S32 column, S32 row, vec3_t point, F32 loderror );
void R_GridInsertRow( srfBspSurface_t* grid, S32 row, S32 column, vec3_t point, F32 loderror );

/*
============================================================

VERTEX BUFFER OBJECTS

============================================================
*/

void R_VaoPackTangent( S16* out, vec4_t v );
void R_VaoPackNormal( S16* out, vec3_t v );
void R_VaoPackColor( U16* out, vec4_t c );
void R_VaoUnpackTangent( vec4_t v, S16* pack );
void R_VaoUnpackNormal( vec3_t v, S16* pack );

vao_t* R_CreateVao( StringEntry name, U8* vertexes, S32 vertexesSize, U8* indexes, S32 indexesSize, vaoUsage_t usage );
vao_t* R_CreateVao2( StringEntry name, S32 numVertexes, srfVert_t* verts, S32 numIndexes, U32* inIndexes );

void R_BindVao( vao_t* vao );
void R_BindNullVao( void );

void Vao_SetVertexPointers( vao_t* vao );

void R_InitVaos( void );
void R_ShutdownVaos( void );
void R_VaoList_f( void );
void RB_UpdateTessVao( U32 attribBits );

void VaoCache_Commit( void );
void VaoCache_Init( void );
void VaoCache_BindVao( void );
void VaoCache_CheckAdd( bool* endSurface, bool* recycleVertexBuffer, bool* recycleIndexBuffer, S32 numVerts, S32 numIndexes );
void VaoCache_RecycleVertexBuffer( void );
void VaoCache_RecycleIndexBuffer( void );
void VaoCache_InitNewSurfaceSet( void );
void VaoCache_AddSurface( srfVert_t* verts, S32 numVerts, U32* indexes, S32 numIndexes );

/*
============================================================

GLSL

============================================================
*/

void GLSL_VertexAttribPointers( U32 attribBits );
void GLSL_BindProgram( shaderProgram_t* program );

void GLSL_SetUniformInt( shaderProgram_t* program, S32 uniformNum, S32 value );
void GLSL_SetUniformFloat( shaderProgram_t* program, S32 uniformNum, F32 value );
void GLSL_SetUniformFloat5( shaderProgram_t* program, S32 uniformNum, const vec5_t v );
void GLSL_SetUniformVec2( shaderProgram_t* program, S32 uniformNum, const vec2_t v );
void GLSL_SetUniformVec3( shaderProgram_t* program, S32 uniformNum, const vec3_t v );
void GLSL_SetUniformVec4( shaderProgram_t* program, S32 uniformNum, const vec4_t v );
void GLSL_SetUniformMat4( shaderProgram_t* program, S32 uniformNum, const mat4_t matrix );

shaderProgram_t* GLSL_GetGenericShaderProgram( S32 stage );

/*
============================================================

SCENE GENERATION

============================================================
*/

void R_InitNextFrame( void );
void RE_BeginScene( const refdef_t* fd );
void RE_EndScene( void );

/*
=============================================================

UNCOMPRESSING BONES

=============================================================
*/

#define MC_BITS_X (16)
#define MC_BITS_Y (16)
#define MC_BITS_Z (16)
#define MC_BITS_VECT (16)

#define MC_SCALE_X (1.0f/64)
#define MC_SCALE_Y (1.0f/64)
#define MC_SCALE_Z (1.0f/64)

void MC_UnCompress( F32 mat[3][4], const U8* comp );

/*
=============================================================

ANIMATED MODELS

=============================================================
*/

void R_MDRAddAnimSurfaces( trRefEntity_t* ent );
void RB_MDRSurfaceAnim( mdrSurface_t* surface );
bool R_LoadIQM( model_t* mod, void* buffer, S32 filesize, StringEntry name );
void R_AddIQMSurfaces( trRefEntity_t* ent );
void RB_IQMSurfaceAnim( surfaceType_t* surface );
S32 R_IQMLerpTag( orientation_t* tag, iqmData_t* data, S32 startFrame, S32 endFrame, F32 frac, StringEntry tagName );

/*
=============================================================
=============================================================
*/
void	R_TransformModelToClip( const vec3_t src, const F32* modelMatrix, const F32* projectionMatrix,
                                vec4_t eye, vec4_t dst );
void	R_TransformClipToWindow( const vec4_t clip, const viewParms_t* view, vec4_t normalized, vec4_t window );

void	RB_DeformTessGeometry( void );

void	RB_CalcFogTexCoords( F32* dstTexCoords );

void	RB_CalcScaleTexMatrix( const F32 scale[2], F32* matrix );
void	RB_CalcScrollTexMatrix( const F32 scrollSpeed[2], F32* matrix );
void	RB_CalcRotateTexMatrix( F32 degsPerSecond, F32* matrix );
void RB_CalcTurbulentFactors( const waveForm_t* wf, F32* amplitude, F32* now );
void	RB_CalcTransformTexMatrix( const texModInfo_t* tmi, F32* matrix );
void	RB_CalcStretchTexMatrix( const waveForm_t* wf, F32* matrix );

void	RB_CalcModulateColorsByFog( U8* dstColors );
F32	RB_CalcWaveAlphaSingle( const waveForm_t* wf );
F32	RB_CalcWaveColorSingle( const waveForm_t* wf );

/*
=============================================================

RENDERER BACK END FUNCTIONS

=============================================================
*/

void RB_ExecuteRenderCommands( const void* data );

/*
=============================================================

RENDERER BACK END COMMAND QUEUE

=============================================================
*/

#define	MAX_RENDER_COMMANDS	0x40000

typedef struct
{
    U8	cmds[MAX_RENDER_COMMANDS];
    S32		used;
} renderCommandList_t;

typedef struct
{
    S32		commandId;
    F32	color[4];
} setColorCommand_t;

typedef struct
{
    S32		commandId;
    S32		buffer;
} drawBufferCommand_t;

typedef struct
{
    S32		commandId;
    image_t*	image;
    S32		width;
    S32		height;
    void*	data;
} subImageCommand_t;

typedef struct
{
    S32		commandId;
} swapBuffersCommand_t;

typedef struct
{
    S32		commandId;
    S32		buffer;
} endFrameCommand_t;

typedef struct
{
    S32		commandId;
    shader_t*	shader;
    F32	x, y;
    F32	w, h;
    F32	s1, t1;
    F32	s2, t2;
} stretchPicCommand_t;

typedef struct
{
    S32		commandId;
    trRefdef_t	refdef;
    viewParms_t	viewParms;
    drawSurf_t* drawSurfs;
    S32		numDrawSurfs;
} drawSurfsCommand_t;

typedef struct
{
    S32 commandId;
    S32 x;
    S32 y;
    S32 width;
    S32 height;
    UTF8* fileName;
    bool jpeg;
} screenshotCommand_t;

typedef struct
{
    S32						commandId;
    S32						width;
    S32						height;
    U8*					captureBuffer;
    U8*					encodeBuffer;
    bool			motionJpeg;
} videoFrameCommand_t;

typedef struct
{
    S32 commandId;
    
    GLboolean rgba[4];
} colorMaskCommand_t;

typedef struct
{
    S32 commandId;
} clearDepthCommand_t;

typedef struct
{
    S32 commandId;
    S32 map;
    S32 cubeSide;
} capShadowmapCommand_t;

typedef struct
{
    S32		commandId;
    trRefdef_t	refdef;
    viewParms_t	viewParms;
} postProcessCommand_t;

typedef struct
{
    S32 commandId;
} exportCubemapsCommand_t;

typedef enum
{
    RC_END_OF_LIST,
    RC_SET_COLOR,
    RC_STRETCH_PIC,
    RC_DRAW_SURFS,
    RC_DRAW_BUFFER,
    RC_SWAP_BUFFERS,
    RC_SCREENSHOT,
    RC_VIDEOFRAME,
    RC_COLORMASK,
    RC_CLEARDEPTH,
    RC_CAPSHADOWMAP,
    RC_POSTPROCESS,
    RC_EXPORT_CUBEMAPS
} renderCommand_t;


// these are sort of arbitrary limits.
// the limits apply to the sum of all scenes in a frame --
// the main view, all the 3D icons, etc
#define	MAX_POLYS		600
#define	MAX_POLYVERTS	3000

// all of the information needed by the back end must be
// contained in a backEndData_t
typedef struct
{
    drawSurf_t drawSurfs[MAX_DRAWSURFS];
    dlight_t dlights[MAX_DLIGHTS];
    trRefEntity_t entities[MAX_REFENTITIES];
    srfPoly_t* polys;//[MAX_POLYS];
    polyVert_t*	polyVerts;//[MAX_POLYVERTS];
    pshadow_t pshadows[MAX_CALC_PSHADOWS];
    renderCommandList_t	commands;
} backEndData_t;

extern	S32		max_polys;
extern	S32		max_polyverts;

extern	backEndData_t*	backEndData;	// the second one may not be allocated


void* R_GetCommandBuffer( S32 bytes );
void RB_ExecuteRenderCommands( const void* data );

void R_IssuePendingRenderCommands( void );

void R_AddDrawSurfCmd( drawSurf_t* drawSurfs, S32 numDrawSurfs );
void R_AddCapShadowmapCmd( S32 dlight, S32 cubeSide );
void R_AddPostProcessCmd( void );

void RE_EndFrame( S32* frontEndMsec, S32* backEndMsec );
void RE_SaveJPG( UTF8* filename, S32 quality, S32 image_width, S32 image_height, U8* image_buffer, S32 padding );
U64 RE_SaveJPGToBuffer( U8* buffer, U64 bufSize, S32 quality, S32 image_width, S32 image_height, U8* image_buffer, S32 padding );

class Allocator;
GPUProgramDesc ParseProgramSource( Allocator& allocator, StringEntry text );

//
// idRenderSystemLocal
//
class idRenderSystemLocal : public idRenderSystem
{
public:
    virtual void Shutdown( bool destroyWindow );
    virtual void Init( glconfig_t* config );
    virtual qhandle_t RegisterModel( StringEntry name );
    virtual qhandle_t RegisterSkin( StringEntry name );
    virtual qhandle_t RegisterShader( StringEntry name );
    virtual qhandle_t RegisterShaderNoMip( StringEntry name );
    virtual void LoadWorld( StringEntry name );
    virtual void SetWorldVisData( const U8* vis );
    virtual void EndRegistration( void );
    virtual void ClearScene( void );
    virtual void AddRefEntityToScene( const refEntity_t* re );
    virtual void AddPolyToScene( qhandle_t hShader, S32 numVerts, const polyVert_t* verts, S32 num );
    virtual bool LightForPoint( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );
    virtual void AddLightToScene( const vec3_t org, F32 intensity, F32 r, F32 g, F32 b );
    virtual void AddAdditiveLightToScene( const vec3_t org, F32 intensity, F32 r, F32 g, F32 b );
    virtual void RenderScene( const refdef_t* fd );
    virtual void SetColor( const F32* rgba );
    virtual void SetClipRegion( const F32* region );
    virtual void DrawStretchPic( F32 x, F32 y, F32 w, F32 h, F32 s1, F32 t1, F32 s2, F32 t2, qhandle_t hShader );
    virtual void DrawStretchRaw( S32 x, S32 y, S32 w, S32 h, S32 cols, S32 rows, const U8* data, S32 client, bool dirty );
    virtual void UploadCinematic( S32 w, S32 h, S32 cols, S32 rows, const U8* data, S32 client, bool dirty );
    virtual void BeginFrame( stereoFrame_t stereoFrame );
    virtual void EndFrame( S32* frontEndMsec, S32* backEndMsec );
    virtual S32 MarkFragments( S32 numPoints, const vec3_t* points, const vec3_t projection, S32 maxPoints, vec3_t pointBuffer, S32 maxFragments, markFragment_t* fragmentBuffer );
    virtual S32	LerpTag( orientation_t* tag, qhandle_t model, S32 startFrame, S32 endFrame, F32 frac, StringEntry tagName );
    virtual void ModelBounds( qhandle_t model, vec3_t mins, vec3_t maxs );
    virtual void RegisterFont( StringEntry fontName, S32 pointSize, fontInfo_t* font );
    virtual void RemapShader( StringEntry oldShader, StringEntry newShader, StringEntry offsetTime );
    virtual bool GetEntityToken( UTF8* buffer, S32 size );
    virtual bool inPVS( const vec3_t p1, const vec3_t p2 );
    virtual void TakeVideoFrame( S32 h, S32 w, U8* captureBuffer, U8* encodeBuffer, bool motionJpeg );
public:
    virtual void InitGPUShaders( void );
    virtual void ShutdownGPUShaders( void );
    virtual void FBOInit( void );
    virtual void FBOShutdown( void );
    virtual void PBOInit( void );
    virtual void WriteToPBO( S32 pbo, U8* buffer, S32 DestX, S32 DestY, S32 Width, S32 Height );
    virtual U8* ReadPBO( bool readBack );
    virtual void UnbindPBO( void );
private:
    U8* buffer;
    U32	pboReadbackHandle;
    U32	pboWriteHandle[2];
};

extern idRenderSystemLocal renderSystemLocal;

#endif //!DEDICATED

#endif //!__R_LOCAL_H__
