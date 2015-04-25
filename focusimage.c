#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pam.h>

#include "bayer.h"

#if !defined(PAM_STRUCT_SIZE)
#define PAM_MEMBER_OFFSET(mbrname) \
  ((size_t)(unsigned long)(char*)&((struct pam *)0)->mbrname)
#define PAM_MEMBER_SIZE(mbrname) \
  sizeof(((struct pam *)0)->mbrname)
#define PAM_STRUCT_SIZE(mbrname) \
  (PAM_MEMBER_OFFSET(mbrname) + PAM_MEMBER_SIZE(mbrname))
#endif

/* BAYER RGGB */
static const int my_bayer[] = {0, 1, 1, 2};

/*
 *  idata is bayer sensor data at 16 bits per pixel
 *  rgb_out is expanded RGB at 24 bits per pixel
 */
void
bayer_expand(unsigned short *idata, int width, int height, unsigned char *rgb_out)
{
  int indx;
  int minv, maxv;
  int x, y;
  int bayer_pos;
  int color_indx;
  unsigned char *outp;

  minv = 4096;
  maxv = 0;
  indx = 0;
  for (y=0; y < height; y++)
  {
    for (x=0; x < width; x++)
    {
      int val;

      val = idata[indx];
      if (val < minv) minv = val;
      if (val > maxv) maxv = val;
      indx++;
    }
  }

  indx = 0;
  outp = rgb_out;
  for (y=0; y < height; y++)
  {
    for (x=0; x < width; x++)
    {
      int val;

      val = idata[indx];
      indx++;
      bayer_pos = (x&1?0:1) + (y&1?0:2);
      color_indx = my_bayer[bayer_pos];

      /*
       * There is lots of color information in the metadataRef0.txt
       * file.  I can't figure out how to make the colors come out like they
       * do with the lytro software.  So I just mucked up some stuff
       * that looks sort of ok to me by trial and error.
       */
      val = (val - minv) * (4095.0 - 168.0) / (maxv - minv);
      val = val * 2.0;
      if (val < 0) val = 0;
/*      val = val + 168; */
      val = (val * 255.0 / (4095.0)) + 0.5;
      if (val > 255) val = 255;

      *outp = 0;
      *(outp + 1) = 0;
      *(outp + 2) = 0;
      *(outp + color_indx) = (unsigned char)val;
      outp = outp + 3;
    }
  }
}


void
writepnm(unsigned char *img_data, int width, int height, int cnt)
{
  FILE *wfile;
  struct pam pam1;
  tuple **new_img;
  unsigned char *iptr;
  int x, y;
  char filename[1000];

  wfile = fopen("base.pnm", "r");
  if (wfile == NULL)
  {
    fprintf(stderr, "Cannot open (base.pnm) to set up a pnm header!\n");
    return;
  }
  pnm_readpaminit(wfile, &pam1, PAM_STRUCT_SIZE(tuple_type));
  fclose(wfile);

  pam1.width = width;
  pam1.height = height;
  new_img = pnm_allocpamarray(&pam1);
  iptr = img_data;
  for (y = 0; y < height; y++)
  {
    for (x = 0; x < width; x++)
    {
      new_img[y][x][0] = *iptr++;
      new_img[y][x][1] = *iptr++;
      new_img[y][x][2] = *iptr++;
    }
  }

  sprintf(filename, "image_%02d.pnm", cnt);
  wfile = fopen(filename, "w");
  pam1.file = wfile;
  pnm_writepam(&pam1, new_img);
  fclose(wfile);

  pnm_freepamarray(new_img, &pam1);
}


unsigned char *
subpixel(unsigned char *img_data, int width, int height, int subsize)
{
  unsigned char *sub_img;
  unsigned char *sptr;
  unsigned char *iptr;
  int x, y, s1, s2;

  sub_img = (unsigned char *)malloc(width * height * subsize * subsize * 3);
  sptr = sub_img;
  iptr = img_data;
  for (y = 0; y < height; y++)
  {
    for (s1 = 0; s1 < subsize; s1++)
    {
      unsigned char *tptr;

      tptr = iptr;
      for (x = 0; x < width; x++)
      {
        for (s2 = 0; s2 < subsize; s2++)
        {
          *sptr = *tptr;
          *(sptr + 1) = *(tptr + 1);
          *(sptr + 2) = *(tptr + 2);
          sptr += 3;
        }
        tptr += 3;
      }
    }
    iptr += (width * 3);
  }
  return(sub_img);
}


/*
 * These magic rotation amounts come from the metadataRef0.txt
 * file.  I was too lazy to write code to parse the file, so I just
 * hardcoded the values from devices:mla:rotation:
 */
void rotate1(int inx, int iny, int *outx, int *outy)
{
  double cosx = 0.999987041;
  double sinx = 0.005091014;
  *outx = (cosx * inx) - (sinx * iny);
  *outy = (sinx * inx) + (cosx * iny);
}


void rotate2(int inx, int iny, int *outx, int *outy)
{
  double cosx = 0.999987041;
  double sinx = -0.005091014;
  *outx = (cosx * inx) - (sinx * iny);
  *outy = (sinx * inx) + (cosx * iny);
}


unsigned char *
rot_image_add_border(unsigned char *img_data, int width, int height, int border, int rot)
{
  int x, y;
  int new_x, new_y;
  int new_w, new_h;
  unsigned char *new_data;
  unsigned char *nptr;
  unsigned char *iptr;

  new_w = width + (2 * border);
  new_h = height + (2 * border);

  new_data = (unsigned char *)malloc(new_w * new_h * 3);
  nptr = new_data;
  for (y=0; y < new_h; y++)
  {
    for (x=0; x < new_w; x++)
    {
      *nptr++ = 0;
      *nptr++ = 0;
      *nptr++ = 0;
    }
  }

  for (y=0; y < width; y++)
  {
    for (x=0; x < width; x++)
    {
      iptr = img_data + (y * width * 3) + (x * 3);
      if (rot == 1)
      {
        rotate1(x, y, &new_x, &new_y);
      }
      else
      {
        rotate2(x, y, &new_x, &new_y);
      }
      new_x = new_x + border;
      new_y = new_y + border;
      nptr = new_data + (new_y * new_w * 3) + (new_x * 3);
      *nptr++ = *iptr++;
      *nptr++ = *iptr++;
      *nptr++ = *iptr++;
    }
  }
  return(new_data);
}


unsigned char *
remove_border(unsigned char *img_data, int width, int height, int border)
{
  int x, y;
  int b_x, b_y, b_w;
  unsigned char *new_data;
  unsigned char *nptr;
  unsigned char *iptr;

  b_w = width + (2 * border);
  new_data = (unsigned char *)malloc(width * height * 3);
  nptr = new_data;
  for (y=0; y < height; y++)
  {
    for (x=0; x < width; x++)
    {
      b_x = x + border;
      b_y = y + border;

      iptr = img_data + (b_y * b_w * 3) + (b_x * 3);
      *nptr++ = *iptr++;
      *nptr++ = *iptr++;
      *nptr++ = *iptr++;
    }
  }
  return(new_data);
}


unsigned char *
undo_subpixel(unsigned char *img_data, int width, int height, int subsize)
{
  unsigned char *new_img;
  unsigned char *nptr;
  unsigned char *iptr;
  int x, y, s1, s2;
  int new_w, new_h;

  new_w = width / subsize;
  new_h = height / subsize;
  new_img = (unsigned char *)malloc(new_w * new_h * 3);
  nptr = new_img;
  for (y = 0; y < new_h; y++)
  {
    for (x = 0; x < new_w; x++)
    {
      unsigned char *tptr;
      int r, g, b;

      r = g = b = 0;
      iptr = img_data + ((y * subsize) * width * 3) + ((x * subsize) * 3);
      for (s1 = 0; s1 < subsize; s1++)
      {
        tptr = iptr;
        for (s2 = 0; s2 < subsize; s2++)
        {
          r = r + *tptr++;
          g = g + *tptr++;
          b = b + *tptr++;
        }
        iptr = iptr + (width * 3);
      }
      *nptr++ = (unsigned char)(r / (subsize * subsize));
      *nptr++ = (unsigned char)(g / (subsize * subsize));
      *nptr++ = (unsigned char)(b / (subsize * subsize));
    }
  }
  return(new_img);
}


unsigned char *
hex_subapimages(unsigned char *img_data, int width, int height,
            int start_y, float dy, int start_x, float dx)
{
  unsigned char *f_img;
  unsigned char *fptr;
  unsigned char *fptr2;
  unsigned char *new_img;
  unsigned char *nptr;
  unsigned char *nptr2;
  unsigned char *cnts;
  unsigned char *cptr;
  unsigned char *cptr2;
  unsigned char *iptr;
  int x, y, i;
  int x1, y1;
  int sx, sy;
  int ul_x, ul_y;
  float fx, fy;
  int rows, cols;

  f_img = (unsigned char *)malloc(width * height * 3);
  fptr = f_img;
  new_img = (unsigned char *)malloc(width * height * 3);
  nptr = new_img;
  cnts = (unsigned char *)malloc(width * height * 3);
  cptr = cnts;
  for (i=0; i < (width * height * 3); i++)
  {
    *nptr++ = 0;
    *cptr++ = 0;
    *fptr++ = 0;
  }

  for (sy = 0; sy < 10; sy++)
  {
    for (sx = 0; sx < 10; sx++)
    {
      nptr = new_img;
      cptr = cnts;
      for (i=0; i < (width * height * 3); i++)
      {
        *nptr++ = 0;
        *cptr++ = 0;
      }

      rows = cols = 0;
      fy = start_y - dy;
      y = fy;
      while (y < height)
      {
	fx = start_x - dx;
	x = fx;
	while (x < width)
	{
	  ul_x = x;
	  ul_y = y;
          if ((ul_x > 0)&&(ul_x < (width - 11))&&(ul_y > 0)&&(ul_y < (height - 11)))
          {
	    nptr = new_img + (ul_y * width * 3) + (ul_x * 3);
	    cptr = cnts + (ul_y * width * 3) + (ul_x * 3);
	    iptr = img_data + ((ul_y + sy) * width * 3) + ((ul_x + sx) * 3);
	    *nptr++ = *iptr++;
	    *cptr++ = 1;
	    *nptr++ = *iptr++;
	    *cptr++ = 1;
	    *nptr++ = *iptr++;
	    *cptr++ = 1;
          }

	  ul_x = x + ((dx - 1.0) / 2.0);
	  ul_y = y + ((dy + 1.0) / 2.0);
          if ((ul_x > 0)&&(ul_x < (width - 11))&&(ul_y > 0)&&(ul_y < (height - 11)))
          {
	    nptr = new_img + (ul_y * width * 3) + (ul_x * 3);
	    cptr = cnts + (ul_y * width * 3) + (ul_x * 3);
	    iptr = img_data + ((ul_y + sy) * width * 3) + ((ul_x + sx) * 3);
	    *nptr++ = *iptr++;
	    *cptr++ = 1;
	    *nptr++ = *iptr++;
	    *cptr++ = 1;
	    *nptr++ = *iptr++;
	    *cptr++ = 1;
          }

	  fx += dx;
	  x = (int)(fx + 0.5);
	}
	fy += dy;
	y = (int)(fy + 0.5);
      }

      fptr = f_img + ((sy * height / 10) * width * 3) + ((sx * width / 10) * 3);
      for (y = 0; y < (height / 10); y++)
      {
        fptr2 = fptr + (y * width * 3);
        for (x = 0; x < (width / 10); x++)
        {
          int r, g, b, c1, c2, c3;

	  nptr = new_img + ((y * 10) * width * 3) + ((x * 10) * 3);
	  cptr = cnts + ((y * 10) * width * 3) + ((x * 10) * 3);
          r = g = b = c1 = c2 = c3 = 0;
          for (y1 = 0; y1 < 10; y1++)
          {
            nptr2 = nptr + (y1 * width * 3);
            cptr2 = cptr + (y1 * width * 3);
            for (x1 = 0; x1 < 10; x1++)
            {
              r = r + *nptr2;
              c1 = c1 + *cptr2;
              nptr2++;
              cptr2++;
              g = g + *nptr2;
              c2 = c2 + *cptr2;
              nptr2++;
              cptr2++;
              b = b + *nptr2;
              c3 = c3 + *cptr2;
              nptr2++;
              cptr2++;
            }
          }
          if (c1 > 0) r = r / c1;
          if (c2 > 0) g = g / c2;
          if (c3 > 0) b = b / c3;
          *fptr2 = r;
          *(fptr2 + 1) = g;
          *(fptr2 + 2) = b;
          fptr2 = fptr2 + 3;
        }
      }

    }
  }
  free(new_img);
  free(cnts);
  return(f_img);
}


#define UP_SAMP 12

unsigned char *
refocus(unsigned char *img_data, int width, int height, float focus)
{
  int x, y, i;
  int sx, sy;
  int rows, cols;
  int new_w, new_h;
  int indx;
  unsigned char *new_img;
  unsigned char *nptr;
  unsigned char *iptr;
  unsigned char *iptr2;
  int *idata;
  short *cnts;
  float dx, dy;
  int idx, idy;
  int nx, ny;

  cols = width / 10;
  rows = height / 10;
  new_w = cols * UP_SAMP;
  new_h = rows * UP_SAMP;
  new_img = (unsigned char *)malloc(new_w * new_h * 3);
  idata = (int *)malloc(new_w * new_h * 3 * sizeof(int));
  cnts = (short *)malloc(new_w * new_h * 3 * sizeof(short));
  for (indx = 0; indx < (new_w * new_h * 3); indx++)
  {
    idata[indx] = 0;
    cnts[indx] = 0;
  }
  for (y=1; y<=8; y++)
  {
    for (x=1; x<=8; x++)
    {
      iptr = img_data + (y * rows * width * 3) + (x * cols * 3);
      nptr = new_img;
      for (sy=0; sy<rows; sy++)
      {
        iptr2 = iptr + (sy * width * 3);
        for (sx=0; sx<cols; sx++)
        {
          for (i=0; i<UP_SAMP; i++)
          {
            *nptr++ = *iptr2;
            *nptr++ = *(iptr2 + 1);
            *nptr++ = *(iptr2 + 2);
          }
          iptr2 = iptr2 + 3;
        }
        for (i=0; i < (UP_SAMP - 1); i++)
        {
          for (sx=0; sx < (cols * UP_SAMP * 3); sx++)
          {
            *nptr = *(nptr - (new_w * 3));
            nptr++;
          }
        }
      }
      dx = (x - 4.5) - ((x - 4.5) / focus);
      dy = (y - 4.5) - ((y - 4.5) / focus);
      if (dx > 0.0) idx = dx + 0.5;
      else idx = dx - 0.5;
      if (dy > 0.0) idy = dy + 0.5;
      else idy = dy - 0.5;
      nptr = new_img;
      for (sy=0; sy<new_h; sy++)
      {
        for (sx=0; sx<new_w; sx++)
        {
          nx = sx + idx;
          ny = sy + idy;
          if ((nx >= 0)&&(nx < new_w)&&(ny >= 0)&&(ny < new_h))
          {
            indx = (ny * new_w * 3) + (nx * 3);
            idata[indx] = idata[indx] + *nptr++;
            cnts[indx] = cnts[indx] + 1;
            idata[indx + 1] = idata[indx + 1] + *nptr++;
            cnts[indx + 1] = cnts[indx + 1] + 1;
            idata[indx + 2] = idata[indx + 2] + *nptr++;
            cnts[indx + 2] = cnts[indx + 2] + 1;
          }
          else
          {
            nptr = nptr + 3;
          }
        }
      }

    }
  }
  free(new_img);
  new_img = (unsigned char *)malloc(rows * cols * 3);
  nptr = new_img;
  for (sy=0; sy<rows; sy++)
  {
    for (sx=0; sx<cols; sx++)
    {
      int r, g, b;

      r = g = b = 0;
      indx = ((sy * UP_SAMP) * new_w * 3) + (sx * UP_SAMP * 3);
      for (y=0; y < UP_SAMP; y++)
      {
        for (x=0; x < UP_SAMP; x++)
        {
          int val;
  
          if (cnts[indx] == 0) val = 0;
          else val = idata[indx] / cnts[indx];
          r = r + val;
          if (cnts[indx + 1] == 0) val = 0;
          else val = idata[indx + 1] / cnts[indx + 1];
          g = g + val;
          if (cnts[indx + 2] == 0) val = 0;
          else val = idata[indx + 2] / cnts[indx + 2];
          b = b + val;
          indx = indx + 3;
        }
        indx = indx - (3 * UP_SAMP) + (new_w * 3);
      }
      *nptr++ = (unsigned char)(r / (UP_SAMP * UP_SAMP));
      *nptr++ = (unsigned char)(g / (UP_SAMP * UP_SAMP));
      *nptr++ = (unsigned char)(b / (UP_SAMP * UP_SAMP));
    }
  }
  free(idata);
  free(cnts);
  return(new_img);
}



#define SUBPIX	3
#define BORDER	200

void
focus_image(unsigned short *idata, int width, int height, float focus)
{
  unsigned char *rgb_out;
  unsigned char *sub_img;
  unsigned char *rot_sub_img;
  unsigned char *straight_img;
  unsigned char *final_img;
  unsigned char *smash_img;
  unsigned char *ref_img;

  rgb_out = (unsigned char *)malloc(width * height * 3);
  bayer_expand(idata, width, height, rgb_out);
  /* 
  * I stole this function from libgphoto2.
  * There may be better bayer filter inerpolaters to use, but this was easy.
  */
  gp_ahd_interpolate(rgb_out, width, height, BAYER_TILE_RGGB);

  /*
   * Multiply each pixel into subpixels so we can more easily
   * rotate the image smoothly.
   */
  sub_img = subpixel(rgb_out, width, height, SUBPIX);
  width = width * SUBPIX;
  height = height * SUBPIX;
  free(rgb_out);

  /*
   * We need to add a border to the image before rotating it
   * to reduce clipping.
   */
  rot_sub_img = rot_image_add_border(sub_img, width, height, BORDER, 1);
  free(sub_img);
  /* Now remove the border from the rotated image */
  straight_img = remove_border(rot_sub_img, width, height, BORDER);
  free(rot_sub_img);
  /*
   * undo subpixelization to return to our original resolution.
   * We should probably stay in the higher resolution to reduce
   * errors, but it is marginally faster this way.
   */
  final_img = undo_subpixel(straight_img, width, height, SUBPIX);
  free(straight_img);
  width = width / SUBPIX;
  height = height / SUBPIX;

  /*
   * This is a tricky part.
   * Use the 10x10 bounding box around the hex produced by each micro-lens.
   * To produce a 10x10 mosaic of sub-aperture images.
   * Math and roundoff errors here probably result in my images
   * being slightly lower quality than those produced by the lytro software.
   */
  /*
   * Yes, you see random hard-coded numbers there.  The hex grid doesn't
   * start on an even hex boundry, and each hex isn't an even numbers of
   * pixels in size.  These numbers were chosen after extensive trial and
   * error and they work for my camera.
   * I would love to find something in the metadataRef0.txt file that
   * let me derive these numbers. but I could not
   */
  smash_img = hex_subapimages(final_img, width, height, 15, 17.21, 3, 9.93);
  free(final_img);

  /*
   * The refocused image is made by overlaying the sub-aperature images
   * with a sub-pixel offset based on focus.
   * In renng's paper this offset is u(1 - 1/a), v(1 - 1/a) a=focus.
   * Useful values for focus: 1=camera's real focus plane.  as focus approaches
   * zero the focus plane moves away from the camera.  Moving the focus plane
   * towards the camera is focus = -1 moving again towards zero, but
   * from the negative side.
   */
    ref_img = refocus(smash_img, width, height, focus);
    writepnm(ref_img, width / 10, height / 10, 0);
    free(ref_img);
#ifdef MULTI
{
  float f_array[] = {0.09, 0.11, 0.15, 0.19, 0.25, 0.35, 0.5, 0.7, 0.9, 1.0,
        -1.0, -0.8, -0.6, -0.48, -0.38, -0.28, -0.19, -0.11, -0.9, -0.07};
  int cnt;
  float f1;
  int i;

  cnt = 10;
  for (i=0; i<20; i++)
  {
    f1 = f_array[i];
fprintf(stderr, "At %d focus = %f\n", cnt, f1);
    ref_img = refocus(smash_img, width, height, f1);
    writepnm(ref_img, width / 10, height / 10, cnt);
    free(ref_img);
    cnt++;
  }
}
#endif /* MULTI */
  free(smash_img);
}

