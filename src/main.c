// MIT License - Copyright (c) 2024 Anaël Seghezzi
// raymarching rendering with precomputed tiles interleaved in both planes at 60fps
// full screen in mode 256x224 with 4 pixels wide column (effective resolution 64x224)
// it's possible to use 60+1 colors, but only 32+1 are used here
// reference for raymarching: https://lodev.org/cgtutor/raycasting.html

// the code is optimised further using GCC's automatic unrolling
#pragma GCC push_options
#pragma GCC optimize ("unroll-loops")

#include "genesis.h"
#include "clear_buffer.h"

//#define SHOW_TEXCOORD // show texture coords

#define FS 8
#define FP (1<<FS) // fixed precision
#define STEP_COUNT 15 // (STEP_COUNT+1 should be a power of two)
#define AP 128 // angle precision (optimal for a rotation step of 8 : 1024/8 = 128)

#include "tab_wall_div.h"
#include "tab_deltas.h"

#define MAP_SIZE 16
static const u8 map[MAP_SIZE][MAP_SIZE] = {
	{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1},
	{1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 1},
	{1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1},
	{1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
	{1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1},
	{1, 0, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1},
	{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1},
	{1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 1},
	{1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1},
	{1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 1, 1},
	{1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1},
	{1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1},
	{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
	{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
};

inline static void write_vline_full(u16 *dest, u16 x, u16 color)
{
	u16 *tile = dest + x;
	for (u16 y = 0; y < 896; y+=32) {
		tile[y] = color;
	}
}

inline static void write_vline(u16 *dest, u16 x, u16 h, u16 color)
{
	u16 *tile = dest + x;
	u16 a = 112 - h;
	u16 ta = (a / 8);
	u16 y;

	// 28 vertical tiles, 14-1 possibilities unrolled
	switch (ta) {
		case 0:
			for (y = 1; y < 27; y++) tile[y*32] = color;
			break;
		case 1:
			for (y = 2; y < 26; y++) tile[y*32] = color;
			break;
		case 2:
			for (y = 3; y < 25; y++) tile[y*32] = color;
			break;
		case 3:
			for (y = 4; y < 24; y++) tile[y*32] = color;
			break;
		case 4:
			for (y = 5; y < 23; y++) tile[y*32] = color;
			break;
		case 5:
			for (y = 6; y < 22; y++) tile[y*32] = color;
			break;
		case 6:
			for (y = 7; y < 21; y++) tile[y*32] = color;
			break;
		case 7:
			for (y = 8; y < 20; y++) tile[y*32] = color;
			break;
		case 8:
			for (y = 9; y < 19; y++) tile[y*32] = color;
			break;
		case 9:
			for (y = 10; y < 18; y++) tile[y*32] = color;
			break;
		case 10:
			for (y = 11; y < 17; y++) tile[y*32] = color;
			break;
		case 11:
			for (y = 12; y < 16; y++) tile[y*32] = color;
			break;
		case 12:
			for (y = 13; y < 15; y++) tile[y*32] = color;
			break;
	}

	tile[ta*32] = color + (a & 7); // top
	tile[(27-ta)*32] = (color + (a & 7)) + (1 << TILE_ATTR_VFLIP_SFT); // bottom (flipped)
}

int main(bool hardReset)
{
	static const u16 unit = FIX16(FP);

	u16 frame_buffer[896*2];
	u16 frame_buffer_xid[64];
	for (u16 i = 0; i < 64; i++) {
		frame_buffer_xid[i] = i&1 ? 896 + i/2 : i/2; 
	}
	u16 posX = 2 * FP, posY = 2 * FP;
	s16 dirX = 0, dirY = FP;
	u16 angle = 0;
	
	// render tiles
	SYS_disableInts();
	{
		for (u16 i = 0; i < 9; i++) { // all 9 possible tile heights (0 to 8)
			u8 tile[8][4] = {0};
			if (i > 0) {
				for (u16 c = 0; c < 8; c++) { // 8 values to create the depth gradient
					for (u16 j = i-1; j < 8; j++) { // tile rows
						for (u16 b = 0; b < 2; b++) { // tile width (4 pixels = 2 bytes)
							tile[j][b] = (c+0) + ((c+1) << 4);
						}
					}
					VDP_loadTileData((u32 *)tile, i + c*8, 1, CPU);
				}
			}
			else { // tile zero
				VDP_loadTileData((u32 *)tile, i, 1, CPU);
			}
		}
	}
	SYS_enableInts();

	// init VDP
	VDP_setScreenWidth256();
	VDP_setPlaneSize(32, 32, TRUE);
	VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
	VDP_setHorizontalScroll(BG_A, 0);
	VDP_setHorizontalScroll(BG_B, 4); // offset second plane by 4 pixels
	VDP_setBackgroundColor(1);
	u16 PAA = VDP_getPlaneAddress(BG_A, 0, 0);
	u16 PBA = VDP_getPlaneAddress(BG_B, 0, 0);

	while(TRUE)
	{
		clear_buffer(frame_buffer);

		// handle inputs
		u16 joy = JOY_readJoypad(0);
		if (joy) {

			// collisions
			if ((joy & BUTTON_UP) || (joy & BUTTON_DOWN)) {

				s16 dx = dirX / 24;
				s16 dy = dirY / 24;
				if (joy & BUTTON_DOWN) {
					dx = -dx;
					dy = -dy;
				}

				u16 x = posX / FP;
				u16 y = posY / FP;
				u16 yt = (posY-63) / FP;
				u16 yb = (posY+63) / FP;
				posX += dx;
				posY += dy;

				if (dx > 0) {
					if (map[y][x+1] || map[yt][x+1] || map[yb][x+1]) {
						posX = min(posX, (x+1)*FP-64);
						x = posX / FP;
					}
				}
				else {
					if (x == 0 || map[y][x-1] || map[yt][x-1] || map[yb][x-1]) {
						posX = max(posX, x*FP+64);
						x = posX / FP;
					}
				}

				u16 xl = (posX-63) / FP;
				u16 xr = (posX+63) / FP;

				if (dy > 0) {
					if (map[y+1][x] || map[y+1][xl] || map[y+1][xr])
						posY = min(posY, (y+1)*FP-64);
				}
				else {
					if (y == 0 || map[y-1][x] || map[y-1][xl] || map[y-1][xr])
						posY = max(posY, y*FP+64);
				}
			}

			// rotation
			if (joy & BUTTON_LEFT)
				angle = (angle + 8) & 1023;
			if (joy & BUTTON_RIGHT)
				angle = (angle - 8) & 1023;

			if ((joy & BUTTON_LEFT) || (joy & BUTTON_RIGHT)) {
				f16 sina = sinFix16(angle);
				f16 cosa = cosFix16(angle);
				f16 tmp;
				tmp = fix16Mul(unit, sina);
				dirX = fix16ToInt(tmp);
				tmp = fix16Mul(unit, cosa);
				dirY = fix16ToInt(tmp);
			}
		}

		// DDA
		{
			u16 a = angle / (1024 / AP);

			u16 mapX_start = posX / FP;
			u16 mapY_start = posY / FP;
			u16 sideDistX_l0 = (posX - mapX_start*FP);
			u16 sideDistX_l1 = ((mapX_start + 1)*FP - posX);
			u16 sideDistY_l0 = (posY - mapY_start*FP);
			u16 sideDistY_l1 = ((mapY_start + 1)*FP - posY);

			const u16 *delta_a_ptr = tab_deltas + (a * 256);

			for (u16 x = 0; x < 64; x++) {

				const u16 *delta_ptr = delta_a_ptr + (x * 4);
				u16 deltaDistX = delta_ptr[0];
				u16 deltaDistY = delta_ptr[1];
				s16 rayDirX = *(s16 *)&delta_ptr[2];
				s16 rayDirY = *(s16 *)&delta_ptr[3];

				u16 mapX = mapX_start;
				u16 mapY = mapY_start;
				const u8 *map_ptr = &map[mapY][mapX];

				u32 sideDistX, sideDistY;
				s16 stepX, stepY;
		
				if (rayDirX < 0) {
					stepX = -1;
					sideDistX = mulu(sideDistX_l0, deltaDistX) >> FS;
				}
				else {
					stepX = 1;
					sideDistX = mulu(sideDistX_l1, deltaDistX) >> FS;
				}
				
				if (rayDirY < 0) {
					stepY = -1;
					sideDistY = mulu(sideDistY_l0, deltaDistY) >> FS;
				}
				else {
					stepY = 1;
					sideDistY = mulu(sideDistY_l1, deltaDistY) >> FS;
				}
		
				for (u16 n = 0; n < STEP_COUNT; n++) {
				
					// side X
					if (sideDistX < sideDistY) {
						
						mapX += stepX;
						map_ptr += stepX;
				
						u16 hit = *map_ptr; // map[mapY][mapX];
						if (hit) {

							u16 t = tab_wall_div[sideDistX];
							u16 *idata = frame_buffer + frame_buffer_xid[x];
							u16 d = 7 - min(7, sideDistX / FP);

						#ifdef SHOW_TEXCOORD
							u16 wallY = posY + (muls(sideDistX, rayDirY) >> FS);
							//wallY = ((wallY * 8) >> FS) & 7; // faster
							wallY = max((wallY - mapY*FP) * 8 / FP, 0); // cleaner
							u16 color = ((0 + 2*(mapY&1)) << TILE_ATTR_PALETTE_SFT) + 1 + min(d, wallY)*8;
						#else
							u16 color;
							if (mapY&1)
								color = (2 << TILE_ATTR_PALETTE_SFT) + 1 + d*8;
							else
								color = 1 + d*8;
						#endif

							if (t >= 112)
								write_vline_full(idata, 0, color);
							else
								write_vline(idata, 0, t, color);
						
							break;
						}

						sideDistX += deltaDistX;
					}
					// side Y
					else {
						
						mapY += stepY;
						map_ptr += stepY > 0 ? MAP_SIZE : -MAP_SIZE;
						
						u16 hit = *map_ptr; // map[mapY][mapX];
						if (hit) {

							u16 t = tab_wall_div[sideDistY];
							u16 *idata = frame_buffer + frame_buffer_xid[x];
							u16 d = 7 - min(7, sideDistY / FP);

						#ifdef SHOW_TEXCOORD
							u16 wallX = posX + (muls(sideDistY, rayDirX) >> FS);
							//wallX = ((wallX * 8) >> FS) & 7; // faster
							wallX = max((wallX - mapX*FP) * 8 / FP, 0); // cleaner
							u16 color = ((1 + 2*(mapX&1)) << TILE_ATTR_PALETTE_SFT) + 1 + min(d, wallX)*8;
						#else
							u16 color;
							if (mapX&1)
								color = (3 << TILE_ATTR_PALETTE_SFT) + 1 + d*8;
							else
								color = (1 << TILE_ATTR_PALETTE_SFT) + 1 + d*8;
						#endif

							if (t >= 112)
								write_vline_full(idata, 0, color);
							else
								write_vline(idata, 0, t, color);
							
							break;
						}

						sideDistY += deltaDistY;
					}
				}
			}
		}

		// end of the frame and dma copy
		SYS_doVBlankProcess();
		DMA_doDmaFast(DMA_VRAM, frame_buffer, PAA, 896, 2);
		DMA_doDmaFast(DMA_VRAM, frame_buffer+896, PBA, 896, 2);

		//VDP_showFPS(1);
		VDP_showCPULoad();
	}

	return 0;
}