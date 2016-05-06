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
#define MAX_INPUT 5000
//---------------------------------------------------------------------

#define uchar  unsigned char
#define ushort unsigned short

//---------------------------------------------------------------------

double sums[YS*2][XS*2];
float *inputs[MAX_INPUT];
float counts[YS*2][XS*2];

float dxs[MAX_INPUT];
float dys[MAX_INPUT];

float    dark[512*512];


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

void vector_sub(float *in, double *summer, float *counter, int cnt)
{
  while (cnt > 4) {
    *summer++ -= (double)*in++;
    *summer++ -= (double)*in++;
    *summer++ -= (double)*in++;
    *summer++ -= (double)*in++;
    *counter++ -= 1;
    *counter++ -= 1;
    *counter++ -= 1;
    *counter++ -= 1;

    cnt -= 4;
  }
  while (cnt > 0) {
    *summer++ -= (double)*in++;
    *counter++ -= 1;
    cnt--;
  }
  
}

//---------------------------------------------------------------------

void vector_add(float *in, double *summer, float *counter, int cnt)
{
  while (cnt > 4) {
    *summer++ += (double)*in++;
    *summer++ += (double)*in++;
    *summer++ += (double)*in++;
    *summer++ += (double)*in++;
    
    *counter++ += 1;
    *counter++ += 1;
    *counter++ += 1;
    *counter++ += 1;

    cnt -= 4;
  }
  while (cnt > 0) {
    *summer++ += (double)*in++;
    *counter++ += 1;
    cnt--;
  }
  
}

//---------------------------------------------------------------------

double sharpness()
{
  int     yp, x;
  int     halfwidth = 180;
  
  double   sum = 0;
  
  for (yp = XS - halfwidth; yp < XS + halfwidth; yp++) {
    double   *p1;
    
    p1 = &sums[yp][XS-halfwidth];
    for (x = 0; x < halfwidth * 2; x++) {
      
      double   v;
      
      v = *p1++;
      v -= (double)40802;
      
      //printf("%f\n", v);
      if (v < 0) v = 0;
      
      
      sum += v * v;
    }
  }
  return sqrt(sum);
}

//---------------------------------------------------------------------

void write_sum()
{
  float    buffer[2*2*512*512];
  int     x,y;
  FILE    *file;
  

  file = fopen ("./sum.raw", "wb");
  
  for (int y = 0; y < YS*2; y++) {
    for (int x = 0; x < XS*2; x++) {
      double   value = sums[y][x];
      float   cnt = counts[y][x];
      if (cnt == 0)
	cnt = 1;
          
      buffer[y * XS * 2 + x] = value/cnt;
    }
  }
  
  fwrite(buffer, sizeof(float), XS*YS*2*2, file);
  fclose(file);

}



//---------------------------------------------------------------------

void add_image(int idx, int dx, int dy)
{
  int     x,y;
  
  float  *input;
  
  input = inputs[idx];
  
  for (y = 0; y < YS; y++) {
    double   *sum_ptr;
    float    *c;
    
    sum_ptr = &sums[y + dy + 256][256 + dx];
    c = &counts[y + dy + 256][256 + dx];
   
    vector_add (&input[y*XS], sum_ptr, c, XS);
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
    float    *c;

    
    sum_ptr = &sums[y + dy + 256][256 + dx];
    c = &counts[y + dy + 256][256 + dx];

    vector_sub (&input[y*XS], sum_ptr, c, XS);
  }
}

//---------------------------------------------------------------------

double  bias = -15.0;

uchar val_map(double v)
{
  v = v - (bias);
  if (v < 0) v = 0; 
  v = v / 3.32;
  //printf("%f\n", v);
  if (v > 255) v = 255;
  
  return v;
}


//---------------------------------------------------------------------


void write_array_to_bmp(double divi, int idx)
{
  uchar    buffer[2 * 2 * YS*XS*3];
  int     x,y;
  char    fn[128];
  

  for (int y = 0; y < YS * 2; y++) {
    for (int x = 3; x < XS * 2; x++) {
      float   value = sums[y][x];
      float   cnt = counts[y][x];
      
      if (cnt < 1.0)
	cnt = 1;
      
      value /= cnt;
      
      buffer[(y * XS * 2 + x) * 3] = val_map(value);
      buffer[1+(y * XS * 2 + x) * 3] = val_map(value);
      buffer[2+(y * XS * 2 + x) * 3] = val_map(value);
    }
  }
  
 
  sprintf(fn, "result%3d.bmp", idx);
  write_bmp(fn, XS * 2, YS * 2, (char*)buffer);
}


//---------------------------------------------------------------------

float MAXD = 65;

void move(int victim)
{
  float   fdx, fdy;
  
  do {
    fdx = box_muller(0, std);
    fdy = box_muller(0, std);
  } while ((fabs(fdx) + fabs(fdy)) < 1);
  
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
double current_sharpness = 0;


void mover()
{
  int victim;
  
  victim = rand() % cnt;
  
  double s0 = current_sharpness;
  sub_victim(victim);
  
  float old_dx = dxs[victim];
  float old_dy = dys[victim];
  
  move(victim);
  
  add_victim(victim);
  
  double s1 = sharpness();
  
  if (s1 <= s0) {
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


void load_image(char *fn, long idx)
{
  short  buffer[512];
  float  tmp[512*512];
  float *cur_buffer;
  float avg = 0;
  
  //printf("%s %d\n", fn, idx);
  FILE *file = fopen (fn, "rb");
  
  fseek(file,(512L*512L*2L*(idx+25000)) , SEEK_SET);
  
  int x,y;
  
  if (inputs[idx] == 0) {
    cur_buffer = (float *)malloc(XS * YS * 4);
    inputs[idx] = cur_buffer;
  }
  else
    cur_buffer = inputs[idx];
  
  
  for (y = 0; y < 512; y++) {
    fread(buffer, 2, 512, file);
    
    for (x = 0; x < 512; x++) {
      ushort swap;
      
      swap = buffer[x];
      
      swap = (swap>>8) | ((swap & 0xff) << 8);
      
      swap = (swap - 32768);
      //printf("%f\n", dark[y * XS + x]);
      cur_buffer[y * XS + x] = dark[y * XS + x];
      tmp[y * XS + x] = swap -  1.0 * dark[y * XS + x];
      avg += swap;
    }
    
  }
  for (y = 0; y < 512; y++) {
    for (x = 0; x < 512; x++) {
      cur_buffer[y * XS + x] = 0;
    }
  }
  for (y = 0; y < 512; y++) {
    for (x = 0; x < 512; x++) {
      cur_buffer[y * XS + x] = tmp[y * XS + x];
    }
    
  }
  
  fclose(file);
  
  if (idx > cnt)

    cnt = idx;

  
  //printf("%s %f\n", fn, (avg/(512*512.0)));
  
  avg = avg / (512*512.0);
  
  if (avg > 2000 || avg < 1000) {
    printf("zero\n");
    for (y = 0; y < 512; y++) {
      for (x = 0; x < 512; x++) {
	cur_buffer[y * XS + x] = 0;
      }
    } 
  }
}

//---------------------------------------------------------------------
#define M MAX_INPUT

void zero_positions()
{
  for (int i = 0;i < MAX_INPUT; i++) {
    dxs[i] = 0.0;
    dys[i] = 0.0;
  }
}

void write_positions()
{
  FILE *file = fopen ("positions", "wb");
  
  fseek(file, 0 , SEEK_SET);
  fwrite(dxs, 4, MAX_INPUT, file);
  fwrite(dys, 4, MAX_INPUT, file);
  
  fclose(file);
  
}

void read_positions()
{
  FILE *file = fopen ("positions", "rb");
  
  if (file == 0)
    return;
  
  fseek(file, 0 , SEEK_SET);
  fread(dxs, 4, MAX_INPUT, file);
  fread(dys, 4, MAX_INPUT, file);
  
  fclose(file);
  
}

//---------------------------------------------------------------------

void init_sum(char **argv)
{
  int x,y;
  
  for (y = 0; y < 2*512; y++) {
    for (x = 0; x < 2*512; x++) {
      sums[x][y] = 0;
      counts[x][y] = 0;
    }
  }
  for (int i = 1; i < M; i++) {
    if (i % 200 == 0) printf("%d\n", i);
    load_image(argv[1], i - 1);
    add_image(i - 1, round(dxs[i - 1]), round(dys[i - 1]));
  } 
}

int main(int argc, char **argv)
{
  
  for (int i = 0;i < M; i++) {
    inputs[i] = 0;
  }
  
  FILE *file = fopen ("./dark.raw", "rb");
  fread(dark, sizeof(float), XS*YS, file);
  fclose(file);
  
  zero_positions();
  
  read_positions();
  
  init_sum(argv);
  std = 12.0;
  
  long cnt = 0;
  
  current_sharpness = sharpness();
  while(1) {
    std = std + (1.0/11500.0);
    if (std > 6)
      std = 2;
    mover();
    cnt++;
    if (cnt % 10000 == 1) {
      printf("temp %f\n", std);
      write_array_to_bmp(M, (cnt/10000));
      write_positions();
      write_sum();
    }
    
    if (cnt % 70000 == 0) {
      init_sum(argv);
      current_sharpness = sharpness();

    }
  }
  
  
  printf("%f \n", sharpness());
  return 0;
}
