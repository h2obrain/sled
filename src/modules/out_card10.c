// Matrix order and size
#define LCD_X 160
#define LCD_Y 80

#define PIXEL_SIZE 5

#define LCD_PIXELS (LCD_X * LCD_Y)
#define TOGGLE_ROCKETS 10   // Let the rocket leds shine
#define BLINKING_ROCKETS 0 // Let the rocket leds blink

#include <epicardium.h>

#include "FreeRTOS.h"
#include <types.h>
#include <timers.h>
#include <matrix.h>
#include <timers.h>
#include <colors.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <colors.h>
#include <assert.h>

union disp_framebuffer fb;

static int rockets[3] = {0,62,0};

int init(void) {
	for (int i = 0; i<3; i++) {
		epic_leds_set_rocket(i, TOGGLE_ROCKETS);
	}
	return epic_disp_open();
}

int deinit(void) {
	return epic_disp_close();
}

int getx(void) {
	return LCD_X/PIXEL_SIZE;
}
int gety(void) {
	return LCD_Y/PIXEL_SIZE;
}

static
int apply_range(int *x, int *y) {
	int ret = 0;
	if (*x < 0) {
		ret = 1;
		*x = 0;
	} else
	if (*x >= getx()) {
		ret = 2;
		*x  = getx()-1;
	}
	if (*y < 0) {
		ret |= 4;
		*y = 0;
	} else
	if (*y >= gety()) {
		ret |= 8;
		*y  = gety()-1;
	}
	return ret;
}

RGB get(int _modno, int x, int y) {
	apply_range(&x,&y);
	x = x * PIXEL_SIZE + PIXEL_SIZE/2;
	y = y * PIXEL_SIZE + PIXEL_SIZE/2;
	uint16_t converted = 0;
	converted  = fb.fb[LCD_Y-1-y][x][0] << 8;
	converted += fb.fb[LCD_Y-1-y][x][1];
	return RGB5652RGB(converted);
}

int set(int modno, int x, int y, RGB color) {
	//printf("\nx: %3d\ty: %3d", x,y); fflush(stdout);
	apply_range(&x,&y);
	x *= PIXEL_SIZE;
	y *= PIXEL_SIZE;
	// No OOB check, because performance.
	uint16_t converted = RGB2RGB565(color);
	
	int x0 = LCD_X-1-x;
	y = LCD_Y-1-y;
	for (int yi = 0; yi < PIXEL_SIZE; yi++) {
		x = x0;
		for (int xi = 0; xi < PIXEL_SIZE; xi++) {
			assert(x>=0);
			assert(y>=0);
			assert(x<LCD_X);
			assert(y<LCD_Y);
			fb.fb[y][x][0] = converted >> 8;
			fb.fb[y][x][1] = converted & 0xFF;
			x--;
		}
		y--;
	}
	return 0;
}

int clear(void) {
	memset(fb.fb, 0, sizeof(uint16_t) * LCD_X * LCD_Y);
	return 0;
}

static void update_rocket(int n);

int render(void) {
	if (BLINKING_ROCKETS) {
		for (int i = 0; i<3; i++) {
			update_rocket(i);
		}
	}
	return epic_disp_framebuffer(&fb);
}

ulong wait_until(ulong desired_usec) {
	if (desired_usec > 1) {
		return timers_wait_until_core(desired_usec);
	}
	return -1;
}

void wait_until_break(void) {
	timers_wait_until_break_core();
}

void update_rocket(int n) {
	epic_leds_set_rocket(n, abs(31 - rockets[n] % 62));
	rockets[n] = (rockets[n] > 61) ? 0 : rockets[n];
	rockets[n]++;
}
