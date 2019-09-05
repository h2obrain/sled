// Main loader for sled. The entry point.
//
// Copyright (c) 2019, Adrian "vifino" Pistol <vifino@tty.sh>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#include "types.h"
#include "matrix.h"
#include "mod.h"
#include "timers.h"
#include "random.h"
#include "util.h"
#include "asl.h"
#include "oscore.h"
#include "taskpool.h"
#include "modloader.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>

#include <inttypes.h>

static int outmodno = -1;

static oscore_mutex rmod_lock;
// Usually -1.
static int main_rmod_override = -1;
static asl_av_t main_rmod_override_args;

const char default_moduledir[] = DEFAULT_MODULEDIR;

#ifdef CIMODE
static int ci_iteration_count = 0;
#endif

static int deinit(void) {
	slogn(9,"Cleaning up GFX/BGM modules...");
	int ret;
	modloader_unloadgfx();
	slogn(9," Done!\nCleaning up output module interface...");
	if ((ret = matrix_deinit()) != 0)
		return ret;
	if ((ret = timers_deinit()) != 0)
		return ret;
	slogn(9," Done!\nCleaning up output module and filters...");
	modloader_deinitend();
	slogn(9," Done!\nCleaning up bits and pieces...");
	oscore_mutex_free(rmod_lock);
	if (main_rmod_override != -1)
		asl_clearav(&main_rmod_override_args);
	if (modloader_modpath != default_moduledir) free(modloader_modpath);
	modloader_modpath = NULL;

	slogn(9," Done!\nCleaning up Taskpool...");
	taskpool_forloop_free();
	taskpool_destroy(TP_GLOBAL);


	slogn(1," Done.\nGoodbye. :(\n");
	return 0;
}

#ifndef CIMODE
static int pick_next_random(int current_modno, oscore_time in) {
	int next_mod;

	if (modloader_gfx_rotation.argc == 0) {
		in += 5000000;
		next_mod = -2;
		//slogn(10,"No mods in modloader_gfx_rotation\n");
	} else {
		for (int i = 0; i < 2; i++) {
			next_mod = modloader_gfx_rotation.argv[rand() % modloader_gfx_rotation.argc];
			slogn(10,"Randomly selected modno:%d\n", next_mod);
			if (next_mod != current_modno)
				break;
		}
	}
	return timer_add(in, next_mod, 0, NULL);
}
#endif

#ifdef CIMODE
static int pick_next_seq(int current_modno, oscore_time in) {
	int next_mod = 0;

	// No modules, uhoh
	if (modloader_gfx_rotation.argc == 0) {
		in += 5000000;
		next_mod = -2;
	} else {
		next_mod = modloader_gfx_rotation.argv[0];
		// Did we find the current module in the rotation?
		int found_here = 0;
#ifdef CIMODE
		// Notably, this doesn't count if current_modno was invalid, so the CI iteration counter doesn't go off spuriously
		int hit_end_of_loop = current_modno >= 0;
#endif
		for (int i = 0; i < modloader_gfx_rotation.argc; i++) {
			if (found_here) {
				// The previous iteration was the current module, so this iteration is?
				next_mod = modloader_gfx_rotation.argv[i];
#ifdef CIMODE
				hit_end_of_loop = 0;
#endif
				break;
			}
			if (modloader_gfx_rotation.argv[i] == current_modno) {
				// Great!
				found_here = 1;
			}
		}
#ifdef CIMODE
		if (hit_end_of_loop) {
			ci_iteration_count++;
			if (ci_iteration_count > 10) { // maybe make this configurable, but its ok for now
				timers_quitting = 1;
				return 0;
			}
		}
#endif
	}
	return timer_add(in, next_mod, 0, NULL);
}
#endif

// this could also be easily rewritten to be an actual feature
static int pick_next(int current_modno, oscore_time in) {
	oscore_mutex_lock(rmod_lock);
	if (main_rmod_override != -1) {
		int res = timer_add(in, main_rmod_override, main_rmod_override_args.argc, main_rmod_override_args.argv);
		main_rmod_override = -1;
		oscore_mutex_unlock(rmod_lock);
		return res;
	}
	oscore_mutex_unlock(rmod_lock);

#ifdef CIMODE
	return pick_next_seq(current_modno, in);
#else
	return pick_next_random(current_modno, in);
#endif
}

void main_force_random(int mnum, int argc, char ** argv) {
	asl_av_t bundled = {argc, argv};
	while (!timers_quitting) {
		oscore_mutex_lock(rmod_lock);
		if (main_rmod_override == -1) {
			main_rmod_override = mnum;
			main_rmod_override_args = bundled;
			oscore_mutex_unlock(rmod_lock);
			return;
		}
		oscore_mutex_unlock(rmod_lock);
		usleep(5000);
	}
	// Quits out without doing anything to prevent deadlock.
	asl_clearav(&bundled);
}

int usage(char* name) {
	printf("Usage: %s [-of]\n", name);
	printf("\t-m --modpath: Set directory that contains the modules to load.\n");
	printf("\t-o --output:  Set output module. Defaults to dummy.\n");
	printf("\t-f --filter:  Add a filter, can be used multiple times.\n");
	return 1;
}

static struct option longopts[] = {
	{ "modpath", required_argument, NULL, 'm' },
	{ "output",  required_argument, NULL, 'o' },
	{ "filter",  optional_argument, NULL, 'f' },
	{ NULL,      0,                 NULL, 0},
};

static int interrupt_count = 0;
static void interrupt_handler(int sig) {
	//
	if (interrupt_count == 0) {
		slogn(0,"sled: Quitting due to interrupt...\n");
		timers_doquit();
	} else if (interrupt_count == 1) {
		eprintf("sled: Warning: One more interrupt until ungraceful exit!\n");
	} else {
		eprintf("sled: Instantly panic-exiting. Bye.\n");
		exit(1);
	}

	interrupt_count++;
}

int sled_main(int argc, char** argv) {
	int ch;

	char outmod_c[256] = DEFAULT_OUTMOD;
	char* outarg = NULL;

	asl_av_t filternames = {0, NULL};
	asl_av_t filterargs = {0, NULL};

	while ((ch = getopt_long(argc, argv, "m:o:f:", longopts, NULL)) != -1) {
		switch(ch) {
		case 'm': {
			char* str = strdup(optarg);
			assert(str);
			free(modloader_modpath); // Could be NULL, doesn't matter
			modloader_modpath = str;
			break;
		}
		case 'o': {
			char* modname = strdup(optarg);
			assert(modname);

			char* arg = modname;
			strsep(&arg, ":"); // Cuts modname
			if (arg) {
				free(outarg);
				// Still using modname's memory, copy it
				outarg = strdup(arg);
				assert(outarg);
			}
			util_strlcpy(outmod_c, modname, 256);
			free(modname);
			break;
		}
		case 'f': {
			// Prepend flt_ here. Be careful with this...
			char* modname = malloc((strlen(optarg) + 5) * sizeof(char));
			assert(modname);
			strcpy(modname, "flt_");
			strcpy(modname + 4, optarg);

			char* fltarg = modname;
			strsep(&fltarg, ":"); // Cuts modname
			if (fltarg != NULL) {
				// Still using modname's memory, copy it
				fltarg = strdup(fltarg);
				assert(fltarg);
			}
			asl_growav(&filternames, modname);
			asl_growav(&filterargs, fltarg);
			break;
		}
		case '?':
		default:
			return usage(argv[0]);
		}
	}
	argc -= optind;
	argv += optind;

	int ret;

	// Initialize pseudo RNG.
	random_seed();

	// Prepare for module loading
	if (modloader_modpath == NULL) {
		modloader_modpath = strdup(default_moduledir);
		assert(modloader_modpath);
	}

	ret = modloader_initmod();
	if (ret) {
		eprintf("Failed to load the modloader tree. How'd this happen?\n");
		free(modloader_modpath);
		return ret;
	}

	// Load outmod
	char outmodname[4 + ARRAY_SIZE(outmod_c)];
	snprintf(outmodname, 4 + ARRAY_SIZE(outmod_c), "out_%s", outmod_c);
	char * outmodbuf2 = strdup(outmodname);
	assert(outmodbuf2);
	asl_pgrowav(&filternames, outmodbuf2);
	asl_pgrowav(&filterargs, outarg);
	outmodno = modloader_initout(&filternames, &filterargs);
	// No need for these anymore. This also cleans up outmodbuf2 and outarg if needed.
	asl_clearav(&filternames);
	asl_clearav(&filterargs);
	if (outmodno == -1) {
		eprintf("Failed to load the output/filter stack. This isn't good.\n");
		modloader_deinitend();
		free(modloader_modpath);
		return ret;
	}

	// Initialize Timers.
	ret = timers_init(outmodno);
	if (ret) {
		eprintf("Timers failed to initialize.\n");
		modloader_deinitend();
		free(modloader_modpath);
		return ret;
	}

	// Initialize Matrix.
	ret = matrix_init(outmodno);
	if (ret) {
		// Fail.
		eprintf("Matrix failed to initialize, which means someone's been making matrix_init more complicated. Uhoh.\n");
		timers_deinit();
		modloader_deinitend();
		free(modloader_modpath);
		return ret;
	}

	rmod_lock = oscore_mutex_new();

	ret = modloader_loadgfx();
	if (ret)
		eprintf("Failed to load graphics modules (%i), continuing, what could possibly go wrong?\n", ret);

	// Initialize global task pool.
	int ncpus = oscore_ncpus();
	TP_GLOBAL = taskpool_create("taskpool", ncpus, ncpus*8);

	signal(SIGINT, interrupt_handler);

	// Startup.
	pick_next(-1, oscore_udate());

	slog("udate:%"PRIu32" sleep_until:%"PRIu32"\n",
			(uint32_t)(oscore_udate()/1000),
			(uint32_t)((oscore_udate()+100000)/1000)
		);
	timers_wait_until(oscore_udate()+100000);

	int lastmod = -1;
	while (!timers_quitting) {
		slog("e");
		timer tnext = timer_get();
		slog("f");
		if (tnext.moduleno == -1) {
			// Queue random.
			slog("g");
			pick_next(lastmod, oscore_udate() + TIME_SHORT * T_SECOND);
		} else
		if (tnext.time > timers_wait_until(tnext.time)) {
			slog("h");
		//if (timers_wait_until(tnext.time) == 1) {
			slogn(9,">> Early break\n");
			// Early break (wait was interrupted). Set this timer up for elimination by any 0-time timers that have come along
			if (tnext.time == 0) tnext.time = 1;
			timer_add(tnext.time, tnext.moduleno, tnext.args.argc, tnext.args.argv);
		} else
		if (tnext.moduleno >= 0) {
			assert(tnext.moduleno < mod_count());
			module* mod = mod_get(tnext.moduleno);
			if ((tnext.moduleno != lastmod)||(!mod->is_valid_drawable)) {
				slogn(10,">> Lastmod %d\n", lastmod);
				if (lastmod >= 0) {
					modloader_deinitgfx(lastmod);
					lastmod = -1;
				}
				slogn(5,">> Now drawing '%s' (modno:%d)\n", mod->name,tnext.moduleno);
				modloader_initgfx(tnext.moduleno);
				if (!mod->is_valid_drawable) {
					eprintf(">> Undrawable module in view: %s ; skipping...\n", mod->name);
					asl_clearav(&tnext.args);
					continue;
				}
				// remove this? it's called from most init functions, I think
				if (mod->reset) {
					slogn(9,">> Resetting..\n");
					mod->reset(tnext.moduleno);
				} else {
					eprintf(">> GFX module without reset shouldn't happen: %s\n", mod->name);
				}
			} else {
				slogn(10,".");
			};
			ret = mod->draw(tnext.moduleno, tnext.args.argc, tnext.args.argv);
			asl_clearav(&tnext.args);
			lastmod = tnext.moduleno;
			switch (ret) {
				case 0 :
					break;
				case 1 :
					if (lastmod != tnext.moduleno) // not an animation.
						slogn(9,"Module chose to pass its turn to draw.\n");
					pick_next(lastmod, oscore_udate() + T_MILLISECOND);
					break;
				default :
					eprintf("Module %s failed to draw: Returned %i", mod->name, ret);
					timers_quitting = 1;
					deinit();
					return 7;
			}
		} else {
			// Virtual null module
			//eprintf(">> using virtual null module\n");
			eprintf("!");
			asl_clearav(&tnext.args);
		}
	}
	return deinit();
}
