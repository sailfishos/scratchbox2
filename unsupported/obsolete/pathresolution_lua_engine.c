/*
 * Copyright (c) 2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * License: LGPL-2.1
*/

/* pathresolution.c contains both versions (calls to C and Lua
 * mapping logic).
 * Either SB2_PATHRESOLUTION_C_ENGINE or SB2_PATHRESOLUTION_LUA_ENGINE
 * must be defined while compiling it.
*/
#define SB2_PATHRESOLUTION_LUA_ENGINE
#include "pathresolution.c"

