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

float sums[YS][XS];
ushort *inputs[MAX_INPUT];

float dxs[MAX_INPUT];
float dys[MAX_INPUT];

int cnt = 0;
float std = 0;


//---------------------------------------------------------------------

void vector_add(ushort *in, float *summer, int cnt)
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

void add_image(int idx, int dx, int dy)
{
    int     x,y;
    
    ushort  *input;
    
    input = inputs[idx];
    
    for (y = 0; y < YS; y++) {
        float   *sum_ptr;
        
        sum_ptr = &sums[y][0];
        
        vector_add (&input[y*XS], sum_ptr, XS);
    }
}

//---------------------------------------------------------------------


void write_dark(float divi)
{
    float    buffer[512*512];
    int     x,y;
    FILE    *file;
    
    
    file = fopen ("./dark.raw", "wb");
    
    for (int y = 0; y < YS; y++) {
        for (int x = 0; x < XS; x++) {
            float   value = sums[y][x];
            value /= divi;
            
            buffer[y * XS + x] = value;
         }
    }
    
    fwrite(buffer, sizeof(float), XS*YS, file);
    fclose(file);
}




//---------------------------------------------------------------------


void load_image(char *fn, int idx)
{
    short  buffer[512];
    ushort *cur_buffer;
    
    FILE *file = fopen (fn, "rb");
    
    fseek(file,0xb40 , SEEK_SET);
    
    int x,y;
    
    cur_buffer = (ushort *)malloc(XS * YS * 2);
    inputs[idx] = cur_buffer;
    
    for (y = 0; y < 512; y++) {
        fread(buffer, 2, 512, file);
        
        for (x = 0; x < 512; x++) {
            ushort swap;
            
            swap = buffer[x];

            swap = (swap>>8) | ((swap & 0xff) << 8);
            
            swap = (swap - 32768);
            cur_buffer[y * XS + x] = swap;
         }
        
    }
    fclose(file);
    dxs[idx] = 0;
    dys[idx] = 0;
    
    if (idx > cnt)
        cnt = idx;
    
}

//---------------------------------------------------------------------

int main(int argc, char **argv)
{
    for (int i = 1; i < argc - 2; i++) {
        load_image(argv[i], i - 1);
        add_image(i - 1, 0, 0);
    }
    
    write_dark(argc - 3);
    
    return 0;
}
