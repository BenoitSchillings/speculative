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
#define XS 512
#define YS 512
#define MAX_INPUT 4096
//---------------------------------------------------------------------

#define uchar  unsigned char
#define ushort unsigned short

//---------------------------------------------------------------------

double sums[YS*4][XS*4];
float *inputs[MAX_INPUT];

float dxs[MAX_INPUT];
float dys[MAX_INPUT];

float    dark[XS*YS];


int cnt = 0;
float std = 0;


//---------------------------------------------------------------------


static  double ranf(){
    static union {
        unsigned  int i;
        float f;
    } myrand;
    myrand.i = (rand() & 0x007fffff) | 0x40000000;
    return myrand.f-3.0;
};


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

void vector_sub(float *in, double *summer, int cnt)
{

    while (cnt > 0) {
        *summer++ -= *in;
        *summer++ -= *in++;
        cnt--;
    }
    
}

//---------------------------------------------------------------------

void vector_add(float *in, double *summer, int cnt)
{

    while (cnt > 0) {
        *summer++ += *in;
        *summer++ += *in++;
        cnt--;
    }
   
}

//---------------------------------------------------------------------

float sharpness()
{
    int     yp, x;
    int     halfwidth = 128 * 2;
    
    float   sum = 0;
    
    for (yp = (XS * 2) - halfwidth; yp < (XS * 2) + halfwidth; yp++) {
        double   *p1;
        
        p1 = &sums[yp][(XS * 2) - halfwidth];
        for (x = 0; x < halfwidth * 2; x++) {

            float   v;
            
            v = *p1++;
            
            sum += v * v;
        }
    }
    return sqrt(sum);
}


//---------------------------------------------------------------------

void add_image(int idx, int dx, int dy)
{
    int     x,y;
    
    float  *input;
    
    input = inputs[idx];
    
    for (y = 0; y < YS * 2; y++) {
        double   *sum_ptr;
        
        sum_ptr = &sums[y + dy + YS][XS + dx];
        
        vector_add (&input[(y/2)*XS], sum_ptr, XS);
    }
}

//---------------------------------------------------------------------

void sub_image(int idx, int dx, int dy)
{
    int     x,y;
    
    float  *input;
    
    input = inputs[idx];
    
    for (y = 0; y < YS * 2; y++) {
        double   *sum_ptr;
        
        sum_ptr = &sums[y + dy + YS][XS + dx];
        
        vector_sub (&input[(y/2)*XS], sum_ptr, XS);
    }
}

//---------------------------------------------------------------------

double  bias = 150.0;

uchar val_map(double v)
{
    v = v - (bias);
    if (v < 0) v = 0; 
    v = v / 2.4;
    //printf("%f\n", v);
    if (v > 255) v = 255;
    
    return v;
}


//---------------------------------------------------------------------
uchar    buffer[4 * 4 * YS * XS * 3];


void write_array_to_bmp(double divi)
{
    int     x,y;
    
    for (int y = 0; y < YS * 4; y++) {
        for (int x = 0; x < XS * 4; x++) {
            float   value = sums[y][x];
            value /= divi;
            
            buffer[(y * XS * 4 + x) * 3] = val_map(value);
            buffer[1+(y * XS * 4 + x) * 3] = val_map(value);
            buffer[2+(y * XS * 4 + x) * 3] = val_map(value);
         }
    }
    
    write_bmp("result.bmp", XS * 4, YS * 4, (char*)buffer);
}


//---------------------------------------------------------------------

float MAXD = 10;

void move(int victim)
{
    float   fdx, fdy;
    
    fdx = box_muller(0, std);
    fdy = box_muller(0, std);
    
    dxs[victim] += fdx;
    dys[victim] += fdy;
    
    if (dxs[victim]>MAXD)
        dxs[victim] = MAXD;
    if (dxs[victim] < -MAXD)
        dxs[victim] = -MAXD;
    
    if (dys[victim]>MAXD)
        dys[victim] = MAXD;
    if (dys[victim] < -MAXD)
        dys[victim] = -MAXD;
}

//---------------------------------------------------------------------

void add_victim(int victim)
{
    add_image(victim, round(2.0 * dxs[victim]), round(2.0 * dys[victim]));
}

//---------------------------------------------------------------------

void sub_victim(int victim)
{
    sub_image(victim, round(2.0 * dxs[victim]), round(2.0 * dys[victim]));
}

//---------------------------------------------------------------------
int debug = 0;
float current_sharpness = 0;


void mover()
{
    int victim;
    
    victim = rand() % cnt;
    
    float s0 = current_sharpness;
    sub_victim(victim);
    
    float old_dx = dxs[victim];
    float old_dy = dys[victim];
    
    move(victim);
    
    add_victim(victim);
    
    float s1 = sharpness();
   
    if (s1 < s0) {
        sub_victim(victim);
        dxs[victim] = old_dx;
        dys[victim] = old_dy;
        add_victim(victim);
    }
    else {
        printf("%d %f\n", debug, s1);
        current_sharpness = s1;
    }
    debug++;
}

//---------------------------------------------------------------------


void load_image(char *fn, int idx)
{
    short  buffer[XS];
    float *cur_buffer;
    float avg = 0;
    
    FILE *file = fopen (fn, "rb");
    
    fseek(file,0xb40 , SEEK_SET);
    
    int x,y;
    
    cur_buffer = (float *)malloc(XS * YS * 4);
    inputs[idx] = cur_buffer;
    
    for (y = 0; y < YS; y++) {
        fread(buffer, 2, XS, file);
        
        for (x = 0; x < XS; x++) {
            ushort swap;
            
            swap = buffer[x];

            swap = (swap>>8) | ((swap & 0xff) << 8);
            
            swap = (swap - 32768);
            //printf("%f\n", dark[y * XS + x]);
            //cur_buffer[y * XS + x] = dark[y * XS + x];
            cur_buffer[y * XS + x] = swap -  1.0 * dark[y * XS + x];
            avg += swap;
         }
        
    }
    fclose(file);
    dxs[idx] = 0;
    dys[idx] = 0;
    
    if (idx > cnt)
        cnt = idx;
    
    printf("%s %f\n", fn, (avg/(XS*YS)));
    
}

//---------------------------------------------------------------------

int main(int argc, char **argv)
{
    FILE *file = fopen ("./dark.raw", "rb");
    fread(dark, sizeof(float), XS*YS, file);
    fclose(file);

    
    for (int i = 1; i < argc - 2; i++) {
        load_image(argv[i], i - 1);
        add_image(i - 1, dxs[i - 1], dys[i - 1]);
    }
    
    std = 3.0;
    
    long cnt = 0;
    
    current_sharpness = sharpness();
    while(1) {
        std = std + (1.0/115000.0);
        MAXD += (0.008);
        if (MAXD > 240)
            MAXD = 240;
        
        if (std > 12)
            std = 3;
        mover();
        cnt++;
        if (cnt % 10000 == 1) {
             write_array_to_bmp(argc - 3);
        }
    }
    
    write_array_to_bmp(argc - 3);
    
    printf("%f \n", sharpness());
    return 0;
}
