#pragma once
// Portable build only (included from fakerw.h under NOTSA_PORTABLE): the few D3D9 enum values that kept game code still names as
// render-state arguments (Radar.cpp: RWRSTATE(D3DSHADE_FLAT), RWRSTATE(D3DTADDRESS_CLAMP)). Values from d3d9types.h; they are the RW
// shade-mode / texture-address numbers (rwSHADEMODEFLAT = 1, rwTEXTUREADDRESSCLAMP = 3). No other D3D vocabulary enters the portable build
// (STREAM_R_PLAN.md 1.3): the FF texture-stage constants only appear in files that are excluded or guarded.
enum D3DSHADEMODE : unsigned { D3DSHADE_FLAT = 1, D3DSHADE_GOURAUD = 2, D3DSHADE_PHONG = 3 };
enum D3DTEXTUREADDRESS : unsigned { D3DTADDRESS_WRAP = 1, D3DTADDRESS_MIRROR = 2, D3DTADDRESS_CLAMP = 3, D3DTADDRESS_BORDER = 4, D3DTADDRESS_MIRRORONCE = 5 };
