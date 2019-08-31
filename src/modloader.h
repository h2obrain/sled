// The functions regarding the initialization and shutdown of SLED's modules.

#ifndef __INCLUDED_MODLOADER__
#define __INCLUDED_MODLOADER__

#include "asl.h"

// Initializes. Then, runs through over and over again to initialize all mod modules that show up.
int modloader_initmod();
// Sets up the output chain here. This is an exception to the pattern, because it has to return the top of the output chain.
// Thus, -1 is error.
// The first filter name and filter argument is the actual output module.
// flt_names and flt_args may not be completely cleared by this function; clear them manually.
int modloader_initout(asl_av_t* flt_names, asl_av_t* flt_args);

// -- Matrix/Timers should be inited here --

// Loads all gfx/bgm modules that show up
int modloader_loadgfx(void);

// Deinitialize the GFX/BGM modules
void modloader_unloadgfx(void);

// Initializes a single gfx/bgm module that show up
int modloader_initgfx(int mid);

// Deinitializes a single GFX/BGM module
int modloader_deinitgfx(int mid);

// -- Matrix/Timers should go down here --

// Deinitialize filters, output module, and modloaders, returning the module system to the initial state.
// DO NOT CALL IF GFX/BGM IS ACTIVE.
void modloader_deinitend(void);

// This memory is managed in main.c, but controls the setdir of modloaders.
extern char* modloader_modpath;
// While this memory is actually managed in modloader.c
extern asl_iv_t modloader_gfx_rotation;

#endif
