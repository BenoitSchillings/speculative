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
#define XS 1004 
#define YS 1002 
#define MAX_INPUT 4500 
//---------------------------------------------------------------------

#define uchar  unsigned char
#define ushort unsigned short

//---------------------------------------------------------------------

double sums[YS*2][XS*2];
float *inputs[MAX_INPUT];

float dxs[MAX_INPUT];
float dys[MAX_INPUT];



int cnt = 0;
float max_mover = 0;
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
    while (cnt > 4) {
        *summer++ -= *in++;
        *summer++ -= *in++;
        *summer++ -= *in++;
        *summer++ -= *in++;
        cnt -= 4;
    }
    while (cnt > 0) {
        *summer++ -= *in++;
        cnt--;
    }
    
}

//---------------------------------------------------------------------

void vector_add(float *in, double *summer, int cnt)
{
    while (cnt > 4) {
        *summer++ += *in++;
        *summer++ += *in++;
        *summer++ += *in++;
        *summer++ += *in++;
        cnt -= 4;
    }
    
    while (cnt > 0) {
        *summer++ += *in++;
        cnt--;
    }
   
}

//---------------------------------------------------------------------

float sharpness()
{
    int     yp, x;
    int     halfwidth = 100;
    
    float   sum = 0;
    
    for (yp = 256 + YS/2 - halfwidth; yp < 256 + YS/2 + halfwidth; yp++) {
        double   *p1;
      
        p1 = &sums[yp][XS/2 + 256 - halfwidth];
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
    
    for (y = 0; y < YS; y++) {
        double   *sum_ptr;
       
	sum_ptr = &sums[y + dy + 256][256 + dx];
        
        vector_add (&input[y*XS], sum_ptr, XS);
    }
}

//---------------------------------------------------------------------

void sub_image(int idx, int dx, int dy)
{
    int     x,y;
    
    float  *input;
   
    input = inputs[idx];
   
    for (y = 0; y < YS; y++) {
        double   *sum_ptr;
       
        sum_ptr = &sums[y + dy + 256][256 + dx];
        
        vector_sub (&input[y*XS], sum_ptr, XS);
    }
}

//---------------------------------------------------------------------

double  bias = 15.0;

uchar val_map(double v)
{
    v = v - (bias);
    if (v < 0) v = 0; 
    v = v / 3.8;
    if (v > 255) v = 255;
    
    return v;
}


//---------------------------------------------------------------------
uchar buffer[2*2*2*YS*XS*3];

void write_array_to_bmp(double divi, int idx)
{
    char    fn[128];
    int	    x, y; 
    printf("write\n"); 
    for (y = 0; y < YS * 2; y++) {
        for (x = 3; x < XS * 2; x++) {
            float   value = sums[y][x];
            value /= divi;
            
            buffer[(y * XS * 2 + x) * 3] = val_map(value);
            buffer[1+(y * XS * 2 + x) * 3] = val_map(value);
            buffer[2+(y * XS * 2 + x) * 3] = val_map(value);
         }
    }
    
    sprintf(fn, "result%3d.bmp", idx);
    printf("x1\n"); 
    write_bmp(fn, XS * 2, YS * 2, (char*)buffer);
}


//---------------------------------------------------------------------

float MAXD = 170;

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
    add_image(victim, round(dxs[victim]), round(dys[victim]));
}

//---------------------------------------------------------------------

void sub_victim(int victim)
{
    sub_image(victim, round(dxs[victim]), round(dys[victim]));
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

void save_pos()
{
	FILE	*file = fopen("pos", "wb");

		
	fwrite(dxs, 1, sizeof(float)*MAX_INPUT, file);
	fwrite(dys, 1, sizeof(float)*MAX_INPUT, file);
	
	fclose(file);
 
}

//---------------------------------------------------------------------


void read_pos()
{
        
	int	i;

	FILE    *file = fopen("pos", "rb");

	printf("read pos\n");

	if (file == 0) {
		printf("zero\n");
		for (i =0; i < MAX_INPUT; i++) {
			dxs[i] = 0;
			dys[i] = 0;
		}
		return;
	}

	printf("read pos\n");
        fread(dxs, 1, sizeof(float)*MAX_INPUT, file);
        fread(dys, 1, sizeof(float)*MAX_INPUT, file);

	printf("position = %f\n", dxs[50]);
        fclose(file);

}

//---------------------------------------------------------------------


void load_image(char *fn, long idx)
{
    short  buffer[XS];
    float *cur_buffer;
    float avg = 0;
    
    FILE *file = fopen (fn, "rb");
    
    fseek(file,XS*YS*2L*idx , SEEK_SET);
    
    int x,y;
    
    cur_buffer = (float *)malloc(XS * YS * 4);
    inputs[idx] = cur_buffer;
    
    for (y = 0; y < YS; y++) {
        fread(buffer, 2, XS, file);
        
        for (x = 0; x < XS; x++) {
            ushort swap;
            swap = buffer[x];
            swap = (swap>>8) | ((swap & 0xff) << 8);
	    swap -= 32768; 
	    cur_buffer[y * XS + x] = swap;
            avg += swap;
         }
        
    }
    fclose(file);
    
    if (idx > cnt)
        cnt = idx;
    
    printf("%s %f\n", fn, (avg/(XS*YS)));
    
}

//---------------------------------------------------------------------

int main(int argc, char **argv)
{
    int i;
 
   	
    read_pos();
 
    for (i = 1; i < 2400; i++) {
        load_image(argv[1], i - 1);
        add_image(i - 1, dxs[i - 1], dys[i - 1]);
    }
   
 
    std = 3.0;
    cnt--; 
    long cnt = 0;
   
    printf("x1\n"); 
    current_sharpness = sharpness();
    while(1) {
        std = std + (1.0/115000.0);
        
        if (std > 15)
            std = 3;
        mover();
        cnt++;
        if (cnt % 10000 == 1) {
             write_array_to_bmp(8*2400, (cnt/10000));
       	     save_pos(); 
	}
    }
    
    
    printf("%f \n", sharpness());
    return 0;
}
