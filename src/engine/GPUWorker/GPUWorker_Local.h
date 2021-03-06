////////////////////////////////////////////////////////////////////////////////////////
// Copyright(C) 1999 - 2010 id Software LLC, a ZeniMax Media company.
// Copyright(C) 2011 - 2012 Justin Marshall <justinmarshall20@gmail.com>
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
// File name:   GPUWorker_Local.h
// Version:     v1.01
// Created:
// Compilers:   Visual Studio 2017, gcc 7.3.0
// Description:
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#ifndef __GPUWORKER_LOCAL_H__
#define __GPUWORKER_LOCAL_H__

#ifndef __OPENCL_H
#ifdef _WIN32
#include <CL/OpenCL.h>
#else
#include <CL/cl.h>
#endif
#ifndef __UTIL_LIST_H__
#include <OWLib/util_list.h>
#endif
#ifndef __GPUWORKER_H__
#include <GPUWorker/GPUWorker.h>
#endif
#endif

#define ID_GPUWORKER_CALLAPI( x )	owGpuWorkerLocal::clError = x
#define ID_GPUWORKER_HASERROR		owGpuWorkerLocal::clError != CL_SUCCESS

//
// owGpuWorkerLocal
//
class owGpuWorkerLocal : public owGpuWorker
{
public:
    virtual void					Init( void );
    virtual void					Shutdown( void );
    
    virtual owGPUWorkerProgram*		FindProgram( StringEntry path );
    virtual void					AppendProgram( owGPUWorkerProgram* program )
    {
        programPool.Append( program );
    };
    
    cl_context						GetDeviceContext( void )
    {
        return context;
    }
    cl_device_id					GetDeviceId( void )
    {
        return ( cl_device_id )device;
    }
    cl_command_queue				GetCommandQueue( void )
    {
        return queue;
    }
    
    static cl_int					clError;
private:

    cl_platform_id                  platform;
    cl_context                      context;
    cl_command_queue                queue;
    cl_device_id                    device;
    
    idList<owGPUWorkerProgram*>     programPool;
};

extern owGpuWorkerLocal		        gpuWorkerLocal;

#endif // !__GPUWORKER_LOCAL_H__
