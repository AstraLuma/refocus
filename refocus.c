#include <stdio.h>
#include <stdlib.h>
#include <string.h>


extern void focus_image(unsigned short *, int, int, float);


unsigned short *
read_raw(char *filename)
{
  FILE *fp;
  unsigned short *sdata;
  int len;

  fp = fopen(filename, "r");
  if (fp == NULL)
  {
    fprintf(stderr, "Error: cannot open file (%s)\n", filename);
    return(NULL);
  }
  len = 3280 * 3280 * sizeof(unsigned short);
  sdata = (unsigned short *)malloc(len);
  fread(sdata, 1, len, fp);
  fclose(fp);
  return(sdata);
}



int main(int argc, char *argv[])
{
  unsigned short *sdata;
  float focus;

  focus = 1.0;
  if (argc < 2)
  {
      fprintf(stderr, "Usage: refocus image.raw\n");
      return 1;
  }
  else if (argc >= 3)
  {
    focus = atof(argv[2]);
  }
  
  sdata = read_raw(argv[1]);
  if (sdata == NULL)
  {
      return 1;
  }

  focus_image(sdata, 3280, 3280, focus);
    
  return 0;
}
