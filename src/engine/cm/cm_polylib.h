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
// File name:   cm_polylib.h
// Version:     v1.01
// Created:
// Compilers:   Visual Studio 2017, gcc 7.3.0
// Description:
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#ifndef __CM_POLYLIB_H__
#define __CM_POLYLIB_H__

#ifndef __Q_SHARED_H__
#include <qcommon/q_shared.h>
#endif

// this is only used for visualization tools in cm_ debug functions

typedef struct
{
    S32     numpoints;
    vec3_t  p[4];		// variable sized
} winding_t;

#define MAX_POINTS_ON_WINDING   64

// plane sides
#define SIDE_FRONT  0
#define SIDE_BACK   1
#define SIDE_ON     2
#define SIDE_CROSS  3

#define CLIP_EPSILON    0.1f

#define MAX_MAP_BOUNDS  65535

// you can define on_epsilon in the makefile as tighter
#ifndef ON_EPSILON
#define ON_EPSILON  0.1f
#endif

winding_t*      AllocWinding( S32 points );
vec_t           WindingArea( winding_t* w );
void            WindingCenter( winding_t* w, vec3_t center );
void            ClipWindingEpsilon( winding_t* in, vec3_t normal, vec_t dist, vec_t epsilon, winding_t** front, winding_t** back );
winding_t*      ChopWinding( winding_t* in, vec3_t normal, vec_t dist );
winding_t*      CopyWinding( winding_t* w );
winding_t*      ReverseWinding( winding_t* w );
winding_t*      BaseWindingForPlane( vec3_t normal, vec_t dist );
void            CheckWinding( winding_t* w );
void            WindingPlane( winding_t* w, vec3_t normal, vec_t* dist );
void            RemoveColinearPoints( winding_t* w );
S32             WindingOnPlaneSide( winding_t* w, vec3_t normal, vec_t dist );
void            FreeWinding( winding_t* w );
void            WindingBounds( winding_t* w, vec3_t mins, vec3_t maxs );
void            AddWindingToConvexHull( winding_t* w, winding_t** hull, vec3_t normal );
void            ChopWindingInPlace( winding_t** w, vec3_t normal, vec_t dist, vec_t epsilon );

// frees the original if clipped
void            pw( winding_t* w );

#endif //!__CM_POLYLIB_H__
