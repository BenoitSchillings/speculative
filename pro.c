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

#define uchar  unsigned char
#define ushort unsigned short
//---------------------------------------------------------------------
#define XS 512
#define YS 512
//---------------------------------------------------------------------

float bias = 2070.0;

float array[YS][XS];
float sum[YS][XS];
float tmp[YS][XS];

uchar val_map(float v)
{
    v = v - (bias);
    if (v < 0) v = 0; 
    v = v / 6.4;
    printf("%f\n", v);
    if (v > 255) v = 255;
    
    return v;
}


//---------------------------------------------------------------------


void write_array_to_bmp(float divi)
{
    uchar    buffer[YS*XS*3];
    int     x,y;
    
    for (int y = 0; y < YS; y++) {
        for (int x = 3; x < XS; x++) {
            float   value = sum[y][x];
            value /= divi;
            
            buffer[(y * XS + x) * 3] = val_map(value);
            buffer[1+(y * XS + x) * 3] = val_map(value);
            buffer[2+(y * XS + x) * 3] = val_map(value);
         }
    }
    
    write_bmp("result.bmp", XS, YS, (char*)buffer);
}



//---------------------------------------------------------------------

void clean()
{
    int x,y;
    
    
    for (y = 0; y < 512; y++) {
        for (x = 0; x < 512; x++) {
            /*
            if (array[y][x] < bias)
                goto skip;
            if (array[y][x+1] < bias)
                goto skip;
            if (array[y][x+2] < bias)
                goto skip;
            
            if (array[y][x] < array[y][x+1])
                goto skip;
            if (array[y][x + 1] < array[y][x+2])
                goto skip;
            
            array[y][x] += (array[y][x+1] - bias);
            array[y][x+1] = bias;
            
            array[y][x] += (array[y][x+2] - bias);
            array[y][x+2] = bias;
             */
            
        skip:;
            
            if (array[y][x]<= bias) {
                tmp[y][x] = bias;
            }
            
            if (array[y][x] > bias)
                tmp[y][x] = bias + 50;
            
        }
    }
    for (y = 0; y < 512; y++) {
        for (x = 0; x < 512; x++) {
            array[y][x] = tmp[y][x];
        }
    }
}

//---------------------------------------------------------------------

void add()
{
    int x,y;
    
    for (y = 0; y < 512; y++) {
        for (x = 0; x < 512; x++) {
            sum[y][x] += array[y][x];
        }
    }
}

//---------------------------------------------------------------------

void clearb()
{
    int x,y;
    
    for (y = 0; y < 512; y++) {
        for (x = 0; x < 512; x++) {
            sum[y][x] = 0;
        }
    }
}

//---------------------------------------------------------------------

void clear()
{
    int x,y;
    
    for (y = 0; y < 512; y++) {
        for (x = 0; x < 512; x++) {
            sum[y][x] = 0;
        }
    }
}

//---------------------------------------------------------------------

void load_image(char *fn)
{
    short  buffer[512];
    
    FILE *file = fopen (fn, "rb");
    
    fseek(file,0xb40 , SEEK_SET);
    
    int x,y;
    
    for (y = 0; y < 512; y++) {
        fread(buffer, 2, 512, file);
        
        for (x = 0; x < 512; x++) {
            ushort swap;
            
            swap = buffer[x];
            
            swap = (swap>>8) | ((swap & 0xff) << 8);
            
            array[y][x] = (swap - 32768);
         }
        
    }
    fclose(file);
}

//---------------------------------------------------------------------

int main(int argc, char **argv)
{
    clearb();
    for (int i = 1; i < argc - 2; i++) {
        printf("%d %s\n", i, argv[i]);
        load_image(argv[i]);
        //clean();
        add();
    }
    
    write_array_to_bmp(argc - 3);
    return 0;
}
