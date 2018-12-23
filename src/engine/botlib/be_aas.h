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
// File name:   be_aas.h
// Version:     v1.01
// Created:
// Compilers:   Visual Studio 2017, gcc 7.3.0
// Description:
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#ifndef __BE_AAS_H__
#define __BE_AAS_H__

#ifndef MAX_STRINGFIELD
#define MAX_STRINGFIELD				80
#endif

//travel flags
#define TFL_INVALID				0x00000001	//traveling temporary not possible
#define TFL_WALK				0x00000002	//walking
#define TFL_CROUCH				0x00000004	//crouching
#define TFL_BARRIERJUMP			0x00000008	//jumping onto a barrier
#define TFL_JUMP				0x00000010	//jumping
#define TFL_LADDER				0x00000020	//climbing a ladder
#define TFL_WALKOFFLEDGE		0x00000080	//walking of a ledge
#define TFL_SWIM				0x00000100	//swimming
#define TFL_WATERJUMP			0x00000200	//jumping out of the water
#define TFL_TELEPORT			0x00000400	//teleporting
#define TFL_ELEVATOR			0x00000800	//elevator
#define TFL_ROCKETJUMP			0x00001000	//rocket jumping
#define TFL_BFGJUMP				0x00002000	//bfg jumping
#define TFL_GRAPPLEHOOK			0x00004000	//grappling hook
#define TFL_DOUBLEJUMP			0x00008000	//double jump
#define TFL_RAMPJUMP			0x00010000	//ramp jump
#define TFL_STRAFEJUMP			0x00020000	//strafe jump
#define TFL_JUMPPAD				0x00040000	//jump pad
#define TFL_AIR					0x00080000	//travel through air
#define TFL_WATER				0x00100000	//travel through water
#define TFL_SLIME				0x00200000	//travel through slime
#define TFL_LAVA				0x00400000	//travel through lava
#define TFL_DONOTENTER			0x00800000	//travel through donotenter area
#define TFL_FUNCBOB				0x01000000	//func bobbing
#define TFL_FLIGHT				0x02000000	//flight
#define TFL_BRIDGE				0x04000000	//move over a bridge
//
#define TFL_NOTTEAM1			0x08000000	//not team 1
#define TFL_NOTTEAM2			0x10000000	//not team 2

//default travel flags
#define TFL_DEFAULT	TFL_WALK|TFL_CROUCH|TFL_BARRIERJUMP|\
	TFL_JUMP|TFL_LADDER|\
	TFL_WALKOFFLEDGE|TFL_SWIM|TFL_WATERJUMP|\
	TFL_TELEPORT|TFL_ELEVATOR|\
	TFL_AIR|TFL_WATER|TFL_JUMPPAD|TFL_FUNCBOB

typedef enum
{
    SOLID_NOT,			// no interaction with other objects
    SOLID_TRIGGER,		// only touch when inside, after moving
    SOLID_BBOX,			// touch on edge
    SOLID_BSP			// bsp clip, touch on edge
} solid_t;

//a trace is returned when a box is swept through the AAS world
typedef struct aas_trace_s
{
    bool	startsolid;	// if true, the initial point was in a solid area
    F32		fraction;	// time completed, 1.0 = didn't hit anything
    vec3_t		endpos;		// final position
    S32			ent;		// entity blocking the trace
    S32			lastarea;	// last area the trace was in (zero if none)
    S32			area;		// area blocking the trace (zero if none)
    S32			planenum;	// number of the plane that was hit
} aas_trace_t;

/* Defined in botlib.h

//bsp_trace_t hit surface
typedef struct bsp_surface_s
{
	UTF8 name[16];
	S32 flags;
	S32 value;
} bsp_surface_t;

//a trace is returned when a box is swept through the BSP world
typedef struct bsp_trace_s
{
	bool		allsolid;	// if true, plane is not valid
	bool		startsolid;	// if true, the initial point was in a solid area
	F32			fraction;	// time completed, 1.0 = didn't hit anything
	vec3_t			endpos;		// final position
	cplane_t		plane;		// surface normal at impact
	F32			exp_dist;	// expanded plane distance
	S32				sidenum;	// number of the brush side hit
	bsp_surface_t	surface;	// hit surface
	S32				contents;	// contents on other side of surface hit
	S32				ent;		// number of entity hit
} bsp_trace_t;
//
*/

//entity info
typedef struct aas_entityinfo_s
{
    S32		valid;			// true if updated this frame
    S32		type;			// entity type
    S32		flags;			// entity flags
    F32	ltime;			// local time
    F32	update_time;	// time between last and current update
    S32		number;			// number of the entity
    vec3_t	origin;			// origin of the entity
    vec3_t	angles;			// angles of the model
    vec3_t	old_origin;		// for lerping
    vec3_t	lastvisorigin;	// last visible origin
    vec3_t	mins;			// bounding box minimums
    vec3_t	maxs;			// bounding box maximums
    S32		groundent;		// ground entity
    S32		solid;			// solid type
    S32		modelindex;		// model used
    S32		modelindex2;	// weapons, CTF flags, etc
    S32		frame;			// model frame number
    S32		event;			// impulse events -- muzzle flashes, footsteps, etc
    S32		eventParm;		// even parameter
    S32		powerups;		// bit flags
    S32		weapon;			// determines weapon and flash model, etc
    S32		legsAnim;		// mask off ANIM_TOGGLEBIT
    S32		torsoAnim;		// mask off ANIM_TOGGLEBIT
} aas_entityinfo_t;

// area info
typedef struct aas_areainfo_s
{
    S32 contents;
    S32 flags;
    S32 presencetype;
    S32 cluster;
    vec3_t mins;
    vec3_t maxs;
    vec3_t center;
} aas_areainfo_t;

// client movement prediction stop events, stop as soon as:
#define SE_NONE					0
#define SE_HITGROUND			1		// the ground is hit
#define SE_LEAVEGROUND			2		// there's no ground
#define SE_ENTERWATER			4		// water is entered
#define SE_ENTERSLIME			8		// slime is entered
#define SE_ENTERLAVA			16		// lava is entered
#define SE_HITGROUNDDAMAGE		32		// the ground is hit with damage
#define SE_GAP					64		// there's a gap
#define SE_TOUCHJUMPPAD			128		// touching a jump pad area
#define SE_TOUCHTELEPORTER		256		// touching teleporter
#define SE_ENTERAREA			512		// the given stoparea is entered
#define SE_HITGROUNDAREA		1024	// a ground face in the area is hit
#define SE_HITBOUNDINGBOX		2048	// hit the specified bounding box
#define SE_TOUCHCLUSTERPORTAL	4096	// touching a cluster portal

typedef struct aas_clientmove_s
{
    vec3_t endpos;			//position at the end of movement prediction
    S32 endarea;			//area at end of movement prediction
    vec3_t velocity;		//velocity at the end of movement prediction
    aas_trace_t trace;		//last trace
    S32 presencetype;		//presence type at end of movement prediction
    S32 stopevent;			//event that made the prediction stop
    S32 endcontents;		//contents at the end of movement prediction
    F32 time;				//time predicted ahead
    S32 frames;				//number of frames predicted ahead
} aas_clientmove_t;

// alternate route goals
#define ALTROUTEGOAL_ALL				1
#define ALTROUTEGOAL_CLUSTERPORTALS		2
#define ALTROUTEGOAL_VIEWPORTALS		4

typedef struct aas_altroutegoal_s
{
    vec3_t origin;
    S32 areanum;
    U16 starttraveltime;
    U16 goaltraveltime;
    U16 extratraveltime;
} aas_altroutegoal_t;

// route prediction stop events
#define RSE_NONE				0
#define RSE_NOROUTE				1	//no route to goal
#define RSE_USETRAVELTYPE		2	//stop as soon as on of the given travel types is used
#define RSE_ENTERCONTENTS		4	//stop when entering the given contents
#define RSE_ENTERAREA			8	//stop when entering the given area

typedef struct aas_predictroute_s
{
    vec3_t endpos;			//position at the end of movement prediction
    S32 endarea;			//area at end of movement prediction
    S32 stopevent;			//event that made the prediction stop
    S32 endcontents;		//contents at the end of movement prediction
    S32 endtravelflags;		//end travel flags
    S32 numareas;			//number of areas predicted ahead
    S32 time;				//time predicted ahead (in hundreth of a sec)
} aas_predictroute_t;


#endif //!__BE_AAS_H__
