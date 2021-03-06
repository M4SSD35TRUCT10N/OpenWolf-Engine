////////////////////////////////////////////////////////////////////////////////////////
// Copyright(C) 2018 Dusan Jocic <dusanjocic@msn.com>
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
// File name:   precompiled.h
// Version:     v1.01
// Created:
// Compilers:   Visual Studio 2017, gcc 7.3.0
// Description:
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#ifndef __PRECOMPILED_H__
#define __PRECOMPILED_H__

//Dushan
//FIX ALL THIS

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>
#include <iostream>

#ifndef _WIN32
#include <sys/ioctl.h>
#endif

#include <fcntl.h>
#include <algorithm>
#ifdef _WIN32
#include <SDL_syswm.h>
#endif

#include <signal.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>

#ifdef _WIN32
#include <io.h>
#include <shellapi.h>
#include <timeapi.h>
#include <windows.h>
#include <direct.h>
#include <lmerr.h>
#include <lmcons.h>
#include <lmwksta.h>
#include <conio.h>
#include <wincrypt.h>
#include <shlobj.h>
#include <psapi.h>
#include <float.h>
#pragma fenv_access (on)
#else
#include <fenv.h>
#endif

#ifndef DEDICATED
#include <GPURenderer/qgl.h>
#endif

#include <database/db_local.h>

#ifdef _WIN32
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>
#include <platform/Windows/resource.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#endif
#include <curl/curl.h>
#ifndef DEDICATED
#include <CL/cl.h>
#endif

#include <botlib/botlib.h>
#include <botlib/l_script.h>
#include <botlib/l_precomp.h>
#include <botlib/l_struct.h>
#include <botlib/l_utils.h>
#include <botlib/be_interface.h>
#include <botlib/l_memory.h>
#include <botlib/l_log.h>
#include <botlib/l_libvar.h>
#include <botlib/aasfile.h>
#include <botlib/botlib.h>
#include <botlib/be_aas.h>
#include <botlib/be_aas_funcs.h>
#include <botlib/be_aas_def.h>
#include <botlib/be_interface.h>
#include <botlib/be_ea.h>
#include <botlib/be_ai_weight.h>
#include <botlib/be_ai_goal.h>
#include <botlib/be_ai_move.h>
#include <botlib/be_ai_weap.h>
#include <botlib/be_ai_chat.h>
#include <botlib/be_ai_char.h>
#include <botlib/be_ai_gen.h>
#include <botlib/l_crc.h>
#include <botlib/be_aas_route.h>
#include <botlib/be_aas_routealt.h>

#include <API/sgame_api.h>
#include <audio/s_local.h>
#include <audio/s_codec.h>

#include <API/bgame_api.h>
#include <bgame/bgame_local.h>

#include <qcommon/q_shared.h>
#include <qcommon/qcommon.h>
#include <API/download_api.h>
#include <qcommon/md4.h>
#ifndef DEDICATED
#include <GPURenderer/r_local.h>
#include <GPURenderer/r_fbo.h>
#include <GPURenderer/r_dsa.h>
#include <GPURenderer/r_common.h>
#include <GPURenderer/r_allocator.h>
#endif
#include <client/client.h>
#include <client/keys.h>

#include <qcommon/unzip.h>
#include <qcommon/htable.h>
#include <qcommon/json.h>
#include <qcommon/md4.h>
#include <qcommon/puff.h>
#include <qcommon/surfaceflags.h>

#include <platform/sys_icon.h>
#include <platform/sys_loadlib.h>
#include <platform/sys_local.h>

#include <physicslib/physics_local.h>
#include <API/physics_api.h>

#include <GUI/gui_local.h>
#ifndef DEDICATED
#include <GPUWorker/GPUWorker.h>
#include <GPUWorker/GPUWorker_Local.h>
#include <GPUWorker/GPUWorker_CLCache.h>
#include <GPUWorker/GPUWorker_Backend.h>
#include <GPUWorker/GPUWorker_OpenCL.h>
#endif
#include <cm/cm_local.h>
#include <cm/cm_patch.h>

#include <API/cgame_api.h>
#ifndef DEDICATED
#include <client/cl_HydraManager.h>
#ifdef _WIN32
#include <OVR.h>
#endif
#endif

#ifndef DEDICATED
#ifdef _WIN32
#include <freetype/ft2build.h>
#else
#include <freetype2/ft2build.h>
#endif
#undef getch
#endif

// Dushan
#if defined(_WIN32) || defined(_WIN64)
#include <winsock.h>
#include <mysql.h>
#else
#include <mysql/mysql.h>
#endif

#include <OWLib/types.h>
#include <OWLib/math_angles.h>
#include <OWLib/math_matrix.h>
#include <OWLib/math_quaternion.h>
#include <OWLib/math_vector.h>
#include <OWLib/util_list.h>
#include <OWLib/util_str.h>
#include <OWLib/q_splineshared.h>
#include <OWLib/splines.h>


#ifdef __LINUX__
#include <CL/cl_gl.h>
#endif

#ifdef _MSC_VER
// MSVC users must install the OpenAL SDK which doesn't use the AL/*.h scheme.
#include <al.h>
#include <alc.h>
#elif defined (MACOS_X)
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

#include <API/FileSystem_api.h>
#include <framework/FileSystem.h>
#include <API/CVarSystem_api.h>
#include <framework/CVarSystem.h>
#include <API/serverBot_api.h>
#include <server/serverBot.h>
#include <server/serverCcmds.h>
#include <API/serverClient_api.h>
#include <server/serverClient.h>
#include <API/serverGame_api.h>
#include <server/serverGame.h>
#include <API/serverWorld_api.h>
#include <server/serverWorld.h>
#include <API/serverSnapshot_api.h>
#include <server/serverSnapshot.h>
#include <API/serverNetChan_api.h>
#include <server/serverNetChan.h>
#include <API/serverInit_api.h>
#include <server/serverInit.h>
#include <server/server.h>
#include <API/serverMain_api.h>
#include <server/serverMain.h>

// includes for the OGG codec
#include <errno.h>
#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/vorbisfile.h>

// curses.h defines COLOR_*, which are already defined in q_shared.h
#undef COLOR_BLACK
#undef COLOR_RED
#undef COLOR_GREEN
#undef COLOR_YELLOW
#undef COLOR_BLUE
#undef COLOR_MAGENTA
#undef COLOR_CYAN
#undef COLOR_WHITE

#include <curses.h>

#endif //!__PRECOMPILED_H__
