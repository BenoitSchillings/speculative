#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>



#include "bmp1.c"

//---------------------------------------------------------------------
//#define XS 1280
//#define YS 959 

#define	  XS 512 
#define   YS 512 
#define MAX_INPUT 114500 
#define MAX_ALLOC 5800
#define XCNT 31000
//---------------------------------------------------------------------

#define uchar  unsigned char
#define ushort unsigned short

//---------------------------------------------------------------------

#define IF int32_t 
#define OF long 

OF sums[YS*2][XS*2];
IF *inputs[MAX_INPUT];


float dark[YS][XS];

float dxs[MAX_INPUT];
float dys[MAX_INPUT];
char  selected[MAX_INPUT];
float ldx = 0;
float ldy = 0;

int cnt = 0;
float max_mover = 0;
float std = 0;
int allocated = 0;


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
double temp = 0*0.0076;

// accept a positive delta with a prop related to temp
 
char accept(float delta)
{	
	float rnd = fabs(ranf());

	temp = temp * 0.999999;
	//printf("%f, %f\n", exp(-delta/temp), rnd);
	if (rnd < exp(-delta/temp)) {
		return 1;	
	}
	return 0;
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

void vector_sub(IF *in, OF *summer, OF *cptr, int cnt)
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

void vector_add(IF *in, OF *summer, OF *cptr, int cnt)
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


double sharpness()
{
    int     yp, x;
    int     halfwidth = 140;
    int	    halfheight = 175; 
    int     mx, my;
    double    sum = 0;
    double     bias;

    //bias = calc_bias(); 
   
    //printf("bias %f\n", bias); 
    bias = 200;
 
    for (yp = YS-halfheight; yp < YS+halfheight; yp++) {
	OF   *p1;
	OF   *p2;
	OF   *p0;


        p1 = &sums[yp][XS - halfwidth];
        p0 = &sums[yp-1][XS - halfwidth];
	p2 = &sums[yp+1][XS - halfwidth];	
	
	if (1)
	for (x = 0; x < halfwidth*2; x++) {
	    double v1 = *p1;
	    v1 = v1 / XCNT;
	    v1 -= bias;
	    sum += v1 * v1;
	    p1++; 
	} 
    }
    return sqrt(sum);
}

//---------------------------------------------------------------------

void add_imagex(int idx, int dx, int dy)
{
    int     x,y;
    int	    dh = XS/2 - 140;
    int	    hs = 280; 
    IF  *input;
   
    input = inputs[idx];
   
    for (y = (YS/2) - 150; y < (YS/2) + 150; y++) { 
        OF   *sum_ptr;
 
	sum_ptr = &sums[y + dy + YS/2][XS/2 + dx];
 
        vector_add (&input[y*XS] + dh, sum_ptr + dh, 0, hs);
    }
}

//---------------------------------------------------------------------

void sub_imagex(int idx, int dx, int dy)
{
    int    x,y;
    int	   dh = XS/2 - 140;
    int	   hs = 280; 
    IF  *input;
   
    input = inputs[idx];
  
    for (y = (YS/2) - 150; y < (YS/2) + 150; y++) {
    //for (y = 0; y < YS; y++) {
        OF   *sum_ptr;
 
        sum_ptr = &sums[y + dy + YS/2][XS/2 + dx];

 
        vector_sub (&input[y*XS] + dh, sum_ptr + dh, 0, hs);
    }
}



void add_image(int idx, int dx, int dy)
{
    int     x,y;
    int	    dh = 0;//XS/2 - 150;
    int	    hs = 512;//300 //XS 
    IF  *input;
   
    input = inputs[idx];
   
    //for (y = (YS/2) - 150; y < (YS/2) + 150; y++) { 
    for (y = 0; y < YS; y++) {
        OF   *sum_ptr;
 
	sum_ptr = &sums[y + dy + YS/2][XS/2 + dx];
 
        vector_add (&input[y*XS] + dh, sum_ptr + dh, 0, hs);
    }
}

//---------------------------------------------------------------------

void sub_image(int idx, int dx, int dy)
{
    int    x,y;
    int	   dh = 0;//XS/2 - 150;
    int	   hs = 512;//300; //XS 
    IF  *input;
   
    input = inputs[idx];
  
    //for (y = (YS/2) - 150; y < (YS/2) + 150; y++) {
    for (y = 0; y < YS; y++) {
        OF   *sum_ptr;
 
        sum_ptr = &sums[y + dy + YS/2][XS/2 + dx];

 
        vector_sub (&input[y*XS] + dh, sum_ptr + dh, 0, hs);
    }
}

//---------------------------------------------------------------------

//double bias = 0*1745.0;
double bias = 105;

uchar val_map(double v)
{
    v = v - (bias);
    if (v < 0) v = 0; 
    v = v * 2.1;
    if (v > 255) v = 255;
    
    return v;
}


//---------------------------------------------------------------------
uchar buffer[2*2*2*YS*XS*3];


float FloatSwap( float f )
{
  union
  {
    float f;
    unsigned char b[4];
  } dat1, dat2;

  dat1.f = f;
  dat2.b[0] = dat1.b[3];
  dat2.b[1] = dat1.b[2];
  dat2.b[2] = dat1.b[1];
  dat2.b[3] = dat1.b[0];


  return dat2.f;
}



void save_fit()
{
        FILE    *file = fopen("template.fit", "wb");
        FILE	*temp = fopen("t0.fit", "rb");
	char	buf[0xb40];	
	float	v;
	int	x, y;
   
	fread(buf, 0xb40, 1, temp);
	fwrite(buf, 0xb40, 1, file);
	fclose(temp);

	for (y = 0; y < YS*2; y++) {
		for (x = 0; x < XS*2; x++) {
			
			v = 10.0*sums[y][x]/XCNT;
			v = FloatSwap(v);
       			fwrite(&v, 1, sizeof(v), file);
		}
	}

	fclose(file);
}

void write_array_to_bmp(double divi, int idx)
{
    char    fn[128];
    int	    x, y; 
    printf("write\n"); 
    for (y = 0; y < YS * 2; y++) {
        for (x = 3; x < XS * 2; x++) {
            float   value = sums[y][x];
	    float   cnt = 0;

            cnt = XCNT;
 
	    value /= cnt;
            buffer[(y * XS * 2 + x) * 3] = val_map(value);
            buffer[1+(y * XS * 2 + x) * 3] = val_map(value);
            buffer[2+(y * XS * 2 + x) * 3] = val_map(value);
         }
    }
    
    sprintf(fn, "result%3d.bmp", idx);
    printf("x1\n"); 
    write_bmp(fn, XS * 2, YS * 2, (char*)buffer);
    save_fit();
}


//---------------------------------------------------------------------

float MAXD = 130;

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

    ldx = fdx;ldy = fdy;
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
double current_sharpness = 0;

int selector()
{
    int		v1;
    int		v2;
    
    v1 = rand() % XCNT;
    v2 = rand() % XCNT;
    
    if (inputs[v1] == 0) return 0;
    if (inputs[v2] == 0) return 0;
    if (selected[v1] == 0) return 0;
    if (selected[v2] != 0) return 0;

 
    float s0 = current_sharpness;
    
    sub_victim(v1);
    add_victim(v2);
    
    float s1 = sharpness();
    
    if (s1 < s0) {
      sub_victim(v2);
      add_victim(v1);
      return 1;
    }
    
    selected[v1] ^= 1;
    selected[v2] ^= 1;
    
    current_sharpness = s1;
    
    printf("Select %f\n", current_sharpness);
    
    return 1;
}

//---------------------------------------------------------------------


int mover()
{
    int victim;
  
    victim = rand() % XCNT;
   
    if (inputs[victim] == 0)
		return 0;
    if (selected[victim] == 0)
	        return 0;


    double s0 = current_sharpness;
    sub_victim(victim);
   
    float old_dx = dxs[victim];
    float old_dy = dys[victim];
    
    move(victim);
   
    add_victim(victim);
  
    double s1 = sharpness();
    
    //printf("%f %f\n", s0, s1);
    if (s1 <= s0 && !accept(s0-s1)) {
        sub_victim(victim);
        dxs[victim] = old_dx;
        dys[victim] = old_dy;
        add_victim(victim);
    }
    else {
        printf("better %d %f, %f %f, %f\n", debug, s1, ldx, ldy, temp);
        current_sharpness = s1;
    }
    debug++;
    return 1;
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

	for (i = 0; i < MAX_INPUT; i++) {
		dxs[i] = 0;
		dys[i] = 0;
	}

	if (file == 0) {
		printf("zero\n");
		for (i =0; i < MAX_INPUT; i++) {
			dxs[i] = i%5;
			dys[i] = i%7;
		}
		return;
	}

	printf("read pos\n");
        int v = fread(dxs, 1, sizeof(float)*MAX_INPUT, file);
        v = fread(dys, 1, sizeof(float)*MAX_INPUT, file);

	for (i = 0; i < 400; i++) {
		printf("position %d = %f %f\n", i, dxs[i], dys[i]);
        } 
	fclose(file);

}

//---------------------------------------------------------------------

void save_selected()
{
	FILE	*file = fopen("sele", "wb");

		
	fwrite(selected, 1, sizeof(char)*MAX_INPUT, file);
	
	fclose(file);
 
}

//---------------------------------------------------------------------


void read_selected()
{
        
	int	i;

	FILE    *file = fopen("sele", "rb");

	printf("read select\n");

	for (i = 0; i < MAX_INPUT; i++) {
		selected[i] = 0;
	}

	if (file == 0) {
		printf("zero select\n");
		for (i =0; i < MAX_INPUT; i++) {
			selected[i] = ((i%8) == 0);
		}
		return;
	}

	printf("read sele\n");
        int v = fread(selected, 1, sizeof(char)*MAX_INPUT, file);
	fclose(file);

}

//---------------------------------------------------------------------

void remove_victim()
{
	int	rnd;

	do {
		rnd = rand() % cnt; 

		if (inputs[rnd] != 0) {
			free((char *)inputs[rnd]);
			inputs[rnd] = 0;
			return;
		}
	} while(1);	
}


//---------------------------------------------------------------------

FILE *input = 0;

void load_image(char *fn, long idx)
{
    short  buffer[XS];
    IF	   *cur_buffer;
    float avg = 0;
  

    if (inputs[idx] != 0)
		return;

    if (input == 0) { 
    	input = fopen (fn, "rb");
    }
 
    fseek(input,XS*YS*2L*(idx) , SEEK_SET);
    
    int x,y;
   
    cur_buffer = (IF *)malloc(XS * YS * sizeof(IF));
   	
    allocated++;
    if (allocated > MAX_ALLOC) {
	remove_victim();
	allocated--;
    }
  

    inputs[idx] = cur_buffer;
    
    for (y = 0; y < YS; y++) {
        int v = fread(buffer, 2, XS, input);
        
        for (x = 0; x < XS; x++) {
            ushort swap;
          
	    swap = buffer[x]; 
     	    swap = (swap>>8) | ((swap & 0xff) << 8);

      	    swap = (swap - 32768);
	    float swap1 = swap - 1.0 * dark[y][x];
	    //swap1 += 1640.0;
	    cur_buffer[y * XS + x] = swap1;
            avg += swap;
         }
        
    }
  
    float sub = (avg/(XS*YS));
    //sub -= 1000.0;
 
    for (y = 0; y < YS; y++) {
        for (x = 0; x < XS; x++) {
 		//cur_buffer[y * XS + x] -= sub;
	}
    } 

    //fclose(file);
    
    if (idx > cnt)
        cnt = idx;
   
   printf("%s %f %d %d\n", fn, (avg/(XS*YS)), allocated, idx);
   
   float a = (avg/(XS*YS));

   int i;

   if (a > 4000.0) {
 	for (i = 0; i < XS*YS; i++) {
		cur_buffer[i] = 0;
	}  
  	printf("bad\n"); 
  }
}

//---------------------------------------------------------------------

int main(int argc, char **argv)
{
    int i;

    int x,y;

    srand (time(NULL)); 
  
    FILE *file = fopen ("./dark.raw", "rb");
    int v = fread(dark, sizeof(float), XS*YS, file);
    fclose(file);
 
	
    for (x = 0; x < XS * 2; x++) {
	for (y = 0; y < YS * 2; y++) {
		sums[y][x] = 0;
	}
    } 

    for (i = 0; i < MAX_INPUT; i++) {
	inputs[i] = 0;
	dxs[i] = 0;
	dys[i] = 0;	
    }
	
    read_pos();
    read_selected();
 
    for (i = 1; i < XCNT; i++) {
        load_image(argv[1], i - 1);
  	selected[i-1] = 1;	
	if (selected[i - 1])
	  add_image(i - 1, round(dxs[i - 1]), round(dys[i - 1]));
    }
   
 
    std = 8.0;
    cnt--; 
    long iter = 0;
   
    current_sharpness = sharpness();
    
    while(1) {
        std = std - (1.0/10000.0);
       
	if ((iter % 5000) == 0) {
		int x;
		for (x = 0; x < 80; x++) 
			load_image(argv[1], rand() % cnt);
		iter++;
	}
	else {
		std = 2.5;	
		iter += mover();
		//iter += selector();
	} 
	//iter += selector();
        if (iter % 50000 == 1) {
             write_array_to_bmp(4.7 * XCNT, (iter/10000));
       	     save_pos(); 
	     save_selected();
	     printf("std = %f\n", std); 
	     iter++;
	}
    }
    
    
    printf("%f \n", sharpness());
    return 0;
}
