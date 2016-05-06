#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>



#include "bmp1.c"

//---------------------------------------------------------------------

static  double ranf(){
    /* ******************************************************************
     Generates a random floating-point number between -1.0 and
     1.0. Not sure if this is the best precision possible, but is
     based on bitwise operations, and I believe it should be faster
     than using (float)rand()/RAND_MAX.
     
     We pick up the number generated by rand, and leave just the bits
     over the mantissa region. The signal and exponent part are
     replaced to generate a number between 2.0 and 3.9999999. We
     then subtract 3.0 to bring the range down to -1.0 and 0.9999999.
     
     Coded in 2010-09-24 by Nicolau Werneck <nwerneck@gmail.com>.
     *************************************************************** */
    static union {
        unsigned  int i;
        float f;
    } myrand;
    myrand.i = (rand() & 0x007fffff) | 0x40000000;
    return myrand.f-3.0;
};

//---------------------------------------------------------------------

#define uchar unsigned char

//---------------------------------------------------------------------
#define XS 512
#define YS 512
//---------------------------------------------------------------------

float array[YS][XS];

//---------------------------------------------------------------------

uchar val_map(float v)
{
    v = v - 0; 
    if (v < 0) v = 0; 
    v = v / 5;
    if (v > 255) v = 255;
    
    return v;
}


//---------------------------------------------------------------------



float box_muller(float m, float s)	/* normal random variate generator */
{                                   /* mean m, standard deviation s */
	float x1, x2, w, y1;
	static float y2;
	static int use_last = 0;
    
	if (use_last)		        /* use value from previous call */
	{
		y1 = y2;
		use_last = 0;
	}
	else
	{
		do {
			x1 = ranf();
			x2 = ranf();
			w = x1 * x1 + x2 * x2;
		} while ( w >= 1.0 );
        
		w = sqrt( (-2.0 * log( w ) ) / w );
		y1 = x1 * w;
		y2 = x2 * w;
		use_last = 1;
	}
    
	return( m + y1 * s );
}

//---------------------------------------------------------------------

float amplify(float input)
{
    float   output;
    
    output = 0;
    
    while(input > 0) {
        output += box_muller(1000, 1000);
        output += 1000; 
	input--;
    }
   
    output += abs(box_muller(0, 50)); 
    return output;
}

//---------------------------------------------------------------------

void write_array_to_bmp()
{
    uchar    buffer[YS*XS*3];
    int     x,y;
    
    for (int y = 0; y < YS; y++) {
        for (int x = 3; x < XS; x++) {
            float   value = array[y][x];
            
            value = amplify(value);
            buffer[(y * XS + x) * 3] = val_map(value);
            buffer[1+(y * XS + x) * 3] = val_map(value);
            buffer[2+(y * XS + x) * 3] = val_map(value);
            
            
            
            value = amplify(array[y][x - 1])/3;
            buffer[(y * XS + x) * 3] += val_map(value);
            buffer[1+(y * XS + x) * 3] += val_map(value);
            buffer[2+(y * XS + x) * 3] += val_map(value);

            
            
            value = amplify(array[y][x - 2])/6;
            buffer[(y * XS + x) * 3] += val_map(value);
            buffer[1+(y * XS + x) * 3] += val_map(value);
            buffer[2+(y * XS + x) * 3] += val_map(value);
            
        }
    }
    
    write_bmp("result.bmp", XS, YS, (char*)buffer);
}

//---------------------------------------------------------------------

void clear_array()
{
    int     x,y;
    
    for (int y = 0; y < YS; y++) {
        for (int x = 0; x < XS; x++) {
            array[y][x] = 0;
        }
    }
}

//---------------------------------------------------------------------

void paint_photon_square1(float exposure)
{
    int x, y;
    int dx,dy;
    
    //signal of 1 photons/sec/pixel
    
    int cnt = exposure * 5000.0;
    
    for (int photon = 0; photon < cnt; photon++) {
        int dx = rand() % 150;
        int dy = rand() % 300;
        array[100 + dx][100 + dy] += 1;
    }
}
//---------------------------------------------------------------------

void paint_photon_square(float exposure)
{
    int x, y;
    int dx,dy;

    //signal of 1 photons/sec/pixel
    
    int cnt = exposure * 10000.0;

    for (int photon = 0; photon < cnt; photon++) {
        int dx = rand() % 300;
        int dy = rand() % 300;
        array[100 + dx][100 + dy] += 1;
    }
}
//---------------------------------------------------------------------

void add_photon()
{
    int x, y;
    
    x = rand() % XS;
    y = rand() % YS;
    
    array[y][x] += 1;
}

//---------------------------------------------------------------------

int main()
{
   float exposure = 0.03; 
   int	 sub = 30;

    clear_array();
    
    while(sub--) {
    	paint_photon_square(1);
    	paint_photon_square1(1);
    
    	// add dark current (0.5 e-/sec/pixel)
    
    	for (int i = 0; i < exposure*(512*512)*(0.1); i++) {
        	add_photon();
    	}

    	// add cic (fixed 0.1 e-)
    	for (int i = 0; i < (512*512)*(0.03); i++) {
        	add_photon();
    	}

    	//adding a 2 e-/sec background. 2e-/sec/pixel
   
  
    	for (int i = 0; i < exposure*(512*512)*0.3; i++) {
        	add_photon();
    	}
    } 
    write_array_to_bmp();
    return 0;
}
