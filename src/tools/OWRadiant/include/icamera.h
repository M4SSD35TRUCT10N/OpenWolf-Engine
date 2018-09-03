/*
   Copyright (C) 1999-2007 id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

//-----------------------------------------------------------------------------
//
// DESCRIPTION:
// camera interface
//

#ifndef __ICAMERA_H_
#define __ICAMERA_H_

#define CAMERA_MAJOR "camera"

typedef void ( *PFN_GETCAMERA )( vec3_t origin, vec3_t angles );
typedef void ( *PFN_SETCAMERA )( vec3_t origin, vec3_t angles );
typedef void ( *PFN_GETCAMWINDOWEXTENTS )( int* x, int* y, int* width, int* height );

struct _QERCameraTable
{
    int m_nSize;
    PFN_GETCAMERA m_pfnGetCamera;
    PFN_SETCAMERA m_pfnSetCamera;
    PFN_GETCAMWINDOWEXTENTS m_pfnGetCamWindowExtents;
};

#endif
