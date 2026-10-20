

#include "epd.h"


/******************************************************************************/

// 硬件参数
int scr_w;
int scr_h;
int scr_mode;
int line_bytes;
int scr_padding;
int update_mode;
int lut_size = 153;

int detect_w = 104;
int detect_h = 212;
int detect_mode = EPD_BW;


// 窗口参数
int win_w;
int win_h;


/******************************************************************************/


// https://www.mdpi.com/2072-666X/12/5/578
#define VSS  (0b00)
#define VH1  (0b01)
#define VSL  (0b10)
#define VH2  (0b11)
#define V(a,b,c,d)  (a<<6|b<<4|c<<2|d)

#define W___ V(VSL, VSS, VSS, VSS)
#define R___ V(VH2, VSS, VSS, VSS)
#define B___ V(VH1, VSS, VSS, VSS)
#define _B__ V(VSS, VH1, VSS, VSS)
#define _W__ V(VSS, VSL, VSS, VSS)
#define _R__ V(VSS, VH2, VSS, VSS)
#define __R_ V(VSS, VSS, VH2, VSS)
#define BW__ V(VH1, VSL, VSS, VSS)
#define RW__ V(VH2, VSL, VSS, VSS)

// fast refresh for clock.
static const u8 lut_fly_153[] = {
//  VS [0-11] phase [ABCD]
//     0     1     2     3     4     5     6     7     8     9    10    11
    0x61, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     // LUT0 B
    0x98, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     // LUT1 W
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     // LUT2 R
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     // LUT3 NC?
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     // LUT4 VCOM
// 60: 
//  TP A, TP B, SRAB, TP C, TP D, SRCD,   RP
    0x01, 0x01, 0x03, 0x08, 0x05, 0x07, 0x00,   // 0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 1
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 2
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 3
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 4
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 5
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 6
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 7
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 8
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 9
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 10
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 11
// 144: FR  25-200Hz
    0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
// 150: XON
    0x00, 0x00, 0x00,
// 153:
//  EOPT    VGH     VSH1    VSH2    VSL     VCOM
//  3F      03      04                      2C
//  22      -20v    15v     3v      -15v
    0x22,   0x17,   0x41,   0x94,   0x32,   0x36    
};

// fast full refresh.
static const u8 lut_fast_153[] = {
//  VS [0-11] phase [ABCD]
//     0     1     2     3     4     5     6     7     8     9    10    11
    W___, BW__, B___, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     // LUT0 B
    _B__, BW__, _W__, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     // LUT1 W
    _R__, RW__, _W__, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     // LUT2 R
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     // LUT3 NC?
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     // LUT4 VCOM
// 60: 
//  TP A, TP B, SRAB, TP C, TP D, SRCD,   RP
    0x03, 0x03, 0x01, 0x00, 0x00, 0x00, 0x02,   // 0
    0x09, 0x09, 0x01, 0x00, 0x00, 0x00, 0x02,   // 1
    0x03, 0x03, 0x01, 0x00, 0x00, 0x00, 0x02,   // 2
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 3
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 4
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 5
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 6
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 7
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 8
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 9
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 10
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 11
// 144: FR  25-200Hz
    0x22, 0x20, 0x00, 0x00, 0x00, 0x00,
// 150: XON
    0x00, 0x00, 0x00,
// 153:
//  EOPT    VGH     VSH1    VSH2    VSL     VCOM
//  3F      03      04                      2C
//  22      -20v    15v     3v      -15v
    0x22,   0x17,   0x41,   0x94,   0x32,   0x36    
};

const u8 *lut_fast = lut_fast_153;
const u8 *lut_fly  = lut_fly_153;


/******************************************************************************/

static int sram_dx = 1;

// x必须是8的倍数，且x2>x1, Y2>y1
void epd_window(int x1, int y1, int x2, int y2)
{
	int tmp;

	fb_w = (x2-x1)+1;
	fb_h = (y2-y1)+1;
	x1 = x1/8;
	x2 = x2/8;
	win_w = (x2-x1+1)*8;
	win_h = y2-y1+1;

	int rmode = scr_mode&3;
	if(rmode==1 || rmode==3){
		tmp = fb_w;
		fb_w = fb_h;
		fb_h = tmp;
	}


	if(scr_mode&MIRROR_H){
		tmp = x1;
		x1 = x2;
		x2 = tmp;
	}
	if(scr_mode&MIRROR_V){
		tmp = y1;
		y1 = y2;
		y2 = tmp;
	}
	
//	printk("epd_window: win: %d x %d  fb: %d x %d\n", win_w, win_h, fb_w, fb_h);
//	printk("  [%d-%d] [%d-%d]\n", x1*8, x2*8, y1, y2);

	int xinc = (x2>x1)? 1: 0;
	int yinc = (y2>y1)? 1: 0;

	epd_cmd2(0x0f, y1, y1>>8);
	epd_cmd3(0x01, y2, y2>>8, 0x00);  // Gate Driver output control

	epd_cmd(0x11);  // data entry mode
	epd_data((yinc<<1)|xinc);

	epd_cmd(0x44);  // set Ram-X address start/end position
	epd_data(x1+sram_dx);
	epd_data(x2+sram_dx);

	epd_cmd(0x45);  // set Ram-Y address start/end position
	epd_data(y1&0xff);
	epd_data(y1>>8);
	epd_data(y2&0xff);
	epd_data(y2>>8);

	epd_cmd(0x4e);  // set RAM-x address
	epd_data(x1+sram_dx);
	epd_cmd(0x4f);  // set RAM-y address
	epd_data(y1&0xff);
	epd_data(y1>>8);

}


void epd_pos(int x, int y)
{
	epd_cmd(0x4e);  // set RAM-x address to 0
	epd_data(x+sram_dx);
	epd_cmd(0x4f);  // set RAM-y address to 249
	epd_data(y&0xff);
	epd_data(y>>8);
}



void epd_load_lut(const u8 *lut)
{
	epd_cmd(0x32);
	epd_data_array((u8*)lut, lut_size);

	epd_cmd1(0x3f, lut[lut_size+0]);
	epd_cmd1(0x03, lut[lut_size+1]);
	epd_cmd3(0x04, lut[lut_size+2], lut[lut_size+3], lut[lut_size+4]);
	epd_cmd1(0x2c, lut[lut_size+5]); // VCOM
}

int epd_lut_size(void)
{
	u8 lut[256];
	int i;
	
	memset(lut, 0xa5, 256);
	epd_cmd(0x32);
	epd_data_array(lut, 256);

	memset(lut, 0x00, 256);
	epd_cmd_read(0x33, lut, 256);
	
	for(i=0; i<256; i++){
		if(lut[i]!=0xa5)
			break;
	}
	lut_size = i;
	printk("LUT size: %d\n", lut_size);
	//hex_dump("LUT", lut, 256);

	return lut_size;
}


int epd_temp(void)
{
	int temp;
	u8 buf[2];

	epd_cmd_read(0x1b, buf, 2);
	temp = (buf[0]<<8)|buf[1];
	temp += 0x0080;
	temp >>= 8;
	return temp;
}


void epd_init(void)
{
//	printk("epd_init: %dx%d\n", scr_w, scr_h);

#if 0
	epd_power(1);
	epd_reset(0);
	delay_ms(20);
#endif
	epd_reset(1);
	delay_ms(20);
	epd_wait();

#if 0
	epd_cmd(0x12);  // SWRESET
	delay_ms(10);
	epd_wait();
#endif
	//epd_lut_size();

	epd_cmd3(0x01, scr_h-1, (scr_h-1)>>8, 0x00);  // Gate Driver output control
	epd_cmd1(0x11, 0x03);  // data entry mode
	epd_cmd1(0x3c, 0x05);  // border wavefrom

	epd_window(0, 0, scr_w-1, scr_h-1);

	epd_cmd1(0x18, 0x80);  // Read Built-in temperature sensor
}


void epd_update_mode(int mode)
{
	update_mode = mode;
}


void epd_update(void)
{
	int seq;

	if(update_mode==UPDATE_FULL){
		seq = 0xf7;
	}else{
		if(update_mode==UPDATE_FAST){
			epd_load_lut(lut_fast);
		}else{
			epd_load_lut(lut_fly);
		}
		seq = 0xe7;
	}

	epd_cmd1(0x22, seq);
	epd_cmd(0x20);
}


void epd_sleep(void)
{
	epd_cmd1(0x10, 0x01);
}


/******************************************************************************/

void epd_screen_update(void)
{
	int i;

	epd_cmd(0x24);  // write RAM for black(0)/white(1)
	for(i=0; i<win_h*line_bytes; i++){
		epd_data(fb_bw[i]);
	}

	if(scr_mode&EPD_BWR){
		epd_cmd(0x26);
		for(i=0; i<win_h*line_bytes; i++){
			//epd_data(fb_rr[i]);
			epd_data(0);
		}
	}
}


void epd_screen_clean(int mode)
{
	int x, y;

	epd_cmd(0x24);  // write RAM for black(0)/white(1)
	for(y=0; y<win_h; y++){
		for(x=0; x<win_w; x+=8){
			int d;
			if(mode==2){
				if(y<(win_h/2)){
					d = (x<win_w/2)? 0x77 : 0x00;
				}else{
					d = (x<win_w/2)? 0x00 : 0xff;
				}
			}else if(mode==1){
				d = 0xff;
			}else{
				d = 0x00;
			}
			epd_data(d);
		}
	}

	if(scr_mode&EPD_BWR){
		epd_cmd(0x26);  // write RAM for red(1)/other(0)
		for(y=0; y<win_h; y++){
			for(x=0; x<win_w; x+=8){
				int d = 0x00;
				if(mode==2){
					if(y<(win_h/2)){
						d = 0x00;
					}else{
						d = (x<win_w/2)? 0xff : 0x00;
					}
				}
				epd_data(d);
			}
		}
	}
}


/******************************************************************************/


#if 0

void epd_test(void)
{
	epd_hw_init(122, 250, EPD_BWR|ROTATE_3);  // for 2.13 board BWR

	epd_init();
	epd_update_mode(UPDATE_FULL);

#if 0
	epd_screen_clean(2);
#else
	fb_test();
#endif
	epd_update();
	epd_wait();


#if 1
	epd_update_mode(UPDATE_FLY);
//	epd_cmd1(0x2c, 0x55);
//	epd_update(0xc0);

	for(int i=0; i<10; i++)
	{
		printk("%d\n", i);
		delay_ms(3000);
		fb_test();
		epd_update();
		epd_wait();
	}
#endif

	delay_ms(5000);
	printk("\nepd_test done.\n");
	epd_cmd1(0x10, 0x01);
	epd_power(0);
	epd_hw_close();
	//epd_update(0x03);
}

#endif
