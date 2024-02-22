// MIT License - Copyright (c) 2024 Anaël Seghezzi
// raymarching rendering with precomputed tiles interleaved in both planes at 60fps
// full screen in mode 256x224 with 4 pixels wide column (effective resolution 64x224)
// it's possible to use 60+1 colors, but only 32+1 are used here
// reference for raymarching: https://lodev.org/cgtutor/raycasting.html

#include "genesis.h"

// the code is optimised further using GCC's automatic unrolling
#pragma GCC push_options
#pragma GCC optimize ("unroll-loops")

#define FP 1024 // fixed precision
#define STEP_COUNT 6
#define F16_UNIT 256
#define AP 64 // angle precision

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
	u16 data[896*2];
	u16 deltas[AP][64];
	u8 wall_div[FP];
	{ // precompute wall projections to avoid division later
		for (u16 i = 0; i < FP; i++) {
			u32 v = (u32)85*FP / (i * 16 + 1);
			wall_div[i] = min(v, MAX_U8);
		}
	}
	u16 posX = 2 * FP, posY = 2 * FP;
	s16 dirX = 0, dirY = FP;
	u16 angle = 0;
	u16 unit = FIX16(F16_UNIT);

	// precompute ray deltas for all angles, one axis is enough because of symmetry
	{
		for (u16 i = 0; i < AP; i++) { // we do half a turn
			
			u16 a = i * (512 / AP);
			f16 sina = sinFix16(a);
			f16 cosa = cosFix16(a);
			f16 tmp, dx, dy;
			u16 x;

			tmp = fix16Mul(unit, sina);
			dx = fix16ToInt(tmp)*(FP/F16_UNIT);
			tmp = fix16Mul(unit, cosa);
			dy = fix16ToInt(tmp)*(FP/F16_UNIT);

			s16 rx1 = dx + dy;
			s16 rx2 = dx - dy;
			s16 rdx = (rx2 - rx1) / 64;
			s16 rayDirX = rx1;

			// interpolate rayDir and fill the delta table
    		for (x = 0; x < 64; x++) {
    			u16 divisor = abs(rayDirX);
    			u32 d = (divisor == 0) ? MAX_U16 : ((u32)FP*FP) / divisor;
				deltas[i][x] = min(d, MAX_U16) / STEP_COUNT;
      			rayDirX += rdx;
      		}
		}
	}

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

    VDP_setScreenWidth256();
    VDP_setPlaneSize(32, 32, TRUE);
	VDP_setBackgroundColor(1);
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setHorizontalScroll(BG_B, 4); // offset second plane by 4 pixels

    while(TRUE)
    {
    	// clear the frame
    	memset(data, 0, 896*2*sizeof(u16));

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
				posX += dx;
				posY += dy;

    			if (dx > 0) {
    				if (map[y][x+1])
    					posX = min(posX, (x+1)*FP-FP/4);
    			}
    			else {
    				if (x == 0 || map[y][x-1])
    					posX = max(posX, x*FP+FP/4);
    			}
            	x = posX / FP;

    			if (dy > 0) {
    				if (map[y+1][x])
    					posY = min(posY, (y+1)*FP-FP/4);
    			}
    			else {
    				if (y == 0 || map[y-1][x])
    					posY = max(posY, y*FP+FP/4);
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
				dirX = fix16ToInt(tmp)*(FP/F16_UNIT);
				tmp = fix16Mul(unit, cosa);
				dirY = fix16ToInt(tmp)*(FP/F16_UNIT);
			}
    	}

    	// DDA
    	{
	    	u16 a = angle / (512 / AP);
			u16 a1 = a & (AP-1);
			u16 a2 = ((AP / 2) + a) & (AP-1);

			s16 rx1 = dirX + dirY;
			s16 ry1 = dirY - dirX;
			s16 rx2 = dirX - dirY;
			s16 ry2 = dirY + dirX;
			s16 rdx = (rx2 - rx1) / 64;
			s16 rdy = (ry2 - ry1) / 64;

			u16 mapX_start = posX / FP;
	      	u16 mapY_start = posY / FP;
	      	u16 sideDistX_l0 = (posX - mapX_start*FP);
	      	u16 sideDistX_l1 = ((mapX_start + 1)*FP - posX);
	      	u16 sideDistY_l0 = (posY - mapY_start*FP);
	      	u16 sideDistY_l1 = ((mapY_start + 1)*FP - posY);

			s16 rayDirX = rx1;
      		s16 rayDirY = ry1;

    		for (u16 x = 0; x < 64; x++) {

				u16 deltaDistX = deltas[a1][x];
      			u16 deltaDistY = deltas[a2][x];
      			u16 mapX = mapX_start;
      			u16 mapY = mapY_start;
      			u16 distX, distY;

				u16 sideDistX, sideDistY;
				s16 stepX, stepY;
		
				if (rayDirX < 0) {
					stepX = -1;
					sideDistX = sideDistX_l0 * deltaDistX / FP;
				}
				else {
					stepX = 1;
					sideDistX = sideDistX_l1 * deltaDistX / FP;
				}
				
				if (rayDirY < 0) {
					stepY = -1;
					sideDistY = sideDistY_l0 * deltaDistY / FP;
				}
				else {
					stepY = 1;
					sideDistY = sideDistY_l1 * deltaDistY / FP;
				}
				
				for (u16 n = 0; n < STEP_COUNT; n++) {
				
					// side X
					if (sideDistX < sideDistY) {
						
						sideDistX += deltaDistX;
						mapX += stepX;
						
						u16 hit = map[mapY][mapX];
						if (hit) {
							distX = sideDistX - deltaDistX;
							u16 distX8 = distX * 16 / FP;
							if (distX8 < 16) {

								u16 h = wall_div[distX];
								u16 d = 7 - distX8/2;
								u16 color = ((0 + 2*(mapY&1)) << TILE_ATTR_PALETTE_SFT) + 1 + d * 8;
								
								u16 *idata = data + (x&1)*896;
								if (h >= 112)
									write_vline_full(idata, x/2, color);
								else
									write_vline(idata, x/2, h, color);
							}
							
							break;
						}
					}
					// side Y
					else {
						
						sideDistY += deltaDistY;
						mapY += stepY;
						
						u16 hit = map[mapY][mapX];
						if (hit) {
							distY = sideDistY - deltaDistY;
							u16 distY8 = distY * 16 / FP;
							if (distY8 < 16) {
								
								u16 h = wall_div[distY];
								u16 d = 7 - distY8/2;
								u16 color = ((1 + 2*(mapX&1)) << TILE_ATTR_PALETTE_SFT) + 1 + d * 8;
								
								u16 *idata = data + (x&1)*896;
								if (h >= 112)
									write_vline_full(idata, x/2, color);
								else
									write_vline(idata, x/2, h, color);
							}
							
							break;
						}
					}
				}

				// interpolated rayDir
      			rayDirX += rdx;
      			rayDirY += rdy;
    		}
    	}

        // end of the frame and dma copy
        SYS_doVBlankProcess();
		SYS_disableInts();
		DMA_doDma(DMA_VRAM, data, VDP_getPlaneAddress(BG_A, 0, 0), 896, 2);
		DMA_doDma(DMA_VRAM, data+896, VDP_getPlaneAddress(BG_B, 0, 0), 896, 2);
		SYS_enableInts();

		VDP_showFPS(1);
		//VDP_showCPULoad();
    }

    return 0;
}
