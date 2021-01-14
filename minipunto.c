/******************************************************************************
 *                               minipunto.c                                  *
 ******************************************************************************

 A small and simple molecular dynamics visualisation tool for X, without
 non-standard dependencies. NO WARRANTY.

 (cc by 2.0) Marc Meléndez Schofield.
 https://creativecommons.org/licenses/by/2.0/
*/

# define _GNU_SOURCE
# include <stdio.h>
# include <unistd.h>
# include <stdlib.h>
# include <X11/Xutil.h>
# include <X11/keysymdef.h>
# include <math.h>
# include <string.h>

/*** Program parameters ***/
# define VERSION "0.2" /* Program version */
# define WIDTH 600 /* Width of window in pixels */
# define HEIGHT 600 /* Height of window in pixels */
# define BACKGROUND_COLOUR 0 /* Default background colour */
# define BACKGROUND_HORIZON
# define TEXT_COLOUR 0x00FF00 /* Default text colour */

// # define FAST_MATH /* Sloppy but possibly faster math */
// # define RAW_VIDEO_TO_FILE /* Output raw video to file (instead of sending it to avconv) */
// # define NO_FADING /* Do not dim lights as particles move away from the camera */
// # define BACKGROUND_HORIZON if(j > HEIGHT/2) XPutPixel(I, i, j, 0x007700); else /* Background horizon */

/*** Macros ***/
# define vector(x, y, z) (float[3]){(x), (y), (z)}

/*** Terminal colours ***/
# define NORMAL "\x1B[0m"
# define WHITE  "\x1B[37m"
# define RED    "\x1B[31m"
# define YELLOW "\x1B[33m"
# define GREEN  "\x1B[32m"
# define CYAN   "\x1B[36m"
# define BLUE   "\x1B[34m"

/*** Unicode special characters ***/
# define ULINE  "\xe2\x94\x80"

/*** Key codes ***/
/* Check key codes with xmodmap -pk */
# define KEY_PLUS  XK_plus
# define KEY_MINUS XK_minus
# define KEY_LEFT  XK_Left
# define KEY_RIGHT XK_Right
# define KEY_UP    XK_Up
# define KEY_DOWN  XK_Down
# define KEY_Q     XK_Q
# define KEY_q     XK_q
# define KEY_ESC   XK_Escape
# define KEY_W     XK_W
# define KEY_w     XK_w
# define KEY_A     XK_A
# define KEY_a     XK_a
# define KEY_B     XK_B
# define KEY_b     XK_b
# define KEY_S     XK_S
# define KEY_s     XK_s
# define KEY_D     XK_D
# define KEY_d     XK_d
# define KEY_Z     XK_Z
# define KEY_z     XK_z
# define KEY_X     XK_X
# define KEY_x     XK_x
# define KEY_R     XK_R
# define KEY_r     XK_r
# define KEY_F     XK_F
# define KEY_f     XK_f
# define KEY_1     XK_1
# define KEY_2     XK_2
# define KEY_3     XK_3
# define KEY_4     XK_4
# define KEY_P     XK_P
# define KEY_p     XK_p
# define KEY_SPC   XK_space
# define KEY_C     XK_C
# define KEY_c     XK_c
# define KEY_O     XK_O
# define KEY_o     XK_o
# define KEY_0     XK_0

/*** Mathematical vector functions ***/
/* Dot product */
__inline__ float dot(float u[3], float v[3]) {
  int i; /* Index */
  float prod = 0; /* Scalar product */

  for(i = 0; i < 3; i++)
    prod += u[i]*v[i];

  return prod;
}

/* Cross product */
__inline__ void cross(float u[3], float v[3], float uxv[3]) {
  float x, y, z;

  x = u[1]*v[2] - u[2]*v[1];
  y = u[2]*v[0] - u[0]*v[2];
  z = u[0]*v[1] - u[1]*v[0];

  uxv[0] = x;
  uxv[1] = y;
  uxv[2] = z;

  return;
}

/* Modulus of a vector */
__inline__ float modulus(float v[3]) {
  return sqrt(dot(v, v));
}

/* Rotate a vector by a given angle around an axis */
__inline__ void rotate(float v[3], float axis[3], float angle) {
  int i; /* Index */
  float rot[3]; /* Rotation */
  float length1, length2; /* Length of a vector */

  length1 = angle/modulus(axis);
  for(i = 0; i < 3; i++) rot[i] = length1*axis[i];
  length1 = modulus(v);
  cross(v, rot, rot);
  for(i = 0; i < 3; i++) v[i] += rot[i];
  length2 = modulus(v);
  for(i = 0; i < 3; i++) v[i] *= length1/length2;
}

/*** Data types ***/

/* Boolean type */
typedef enum {false = 0, true} bool;

/* Camera data type */
struct camera {
  float location[3];  /* Location */
  float aim[3];       /* Point to look at */
  float distance;     /* Distance from aim */
  float direction[3]; /* Direction unit vector */
  float screenx[3], screeny[3]; /* Direction of the screen x and y axes */
};

/* RGB colour */
struct colour {
  int r;
  int g;
  int b;
};

/*** Auxiliary functions ***/

/* Set up camera position and orientation */
void setcamera(struct camera * cam, float location[3], float aim[3], float zenith[3])
{
  int i; /* Coordinate index */
  float r; /* Vector modulus */

  for(i = 0; i < 3; i++) cam->location[i] = location[i];
  for(i = 0; i < 3; i++) cam->aim[i] = aim[i];
  for(i = 0; i < 3; i++) cam->direction[i] = aim[i] - location[i];

  /* Normalise direction vector */
  cam->distance = modulus(cam->direction);
  if(cam->distance == 0)
    cam->direction[0] = -1;
  else
    for(i = 0; i < 3; i++) cam->direction[i] /= cam->distance;

  /* Screen axes */
  cross(cam->direction, zenith, cam->screenx);
  if(modulus(cam->screenx) == 0) cam->screenx[1] = 1;
  cross(cam->screenx, cam->direction, cam->screeny);

  /* Resize screen axes */
  r = modulus(cam->screenx);
  for(i = 0; i < 3; i++) cam->screenx[i] /= r;
  r = modulus(cam->screeny);
  for(i = 0; i < 3; i++) cam->screeny[i] /= r;
}

/***** Main function *****/
int main(int argc, char * argv[]) {
  int i, j; /* Indices */
  FILE * mddata = NULL; /* Pointer to data file */
  FILE * videopipe = NULL; /* Pointer to named pipe for video output */

  /* Default options */
  int background = BACKGROUND_COLOUR; /* Colour for background */
  int text = TEXT_COLOUR; /* Colour for background */
  float L = 40; /* Initial camera distance */
  float loc[3] = {L, 0, 0}; /* Camera location */
  float aim[3] = {0, 0, 0}; /* Camera aim */
  float zen[3] = {0, 0, 1}; /* Camera zenith vector */
  int fade = 1; /* Fading flag */

  /* Read command line arguments */
  if(argc < 2 && isatty(0)) { /* Use help message */
    printf("--- minipunto (version " VERSION ") ---\n");
    printf("Display molecular dynamics data in ASCII files. The files should\n"
           "contain at least three columns (x, y and z coordinates) but may\n"
           "include radii and RGB colours in the fourth and fifth columns.\n"
           "Comments, marked with a # at the beginning of a line, are ignored.\n"
           "Blank lines separate frames.\n\n");
    printf("Usage: %s [options] <MD data file>\n", argv[0]);
    printf("Options:\n"
           "  -b <RGB integer> Background colour.\n"
           "  -t <RGB integer> Text colour.\n"
           "  -L <x value>     Initial camera distance.\n"
           "  -l <x> <y> <z>   Initial location of camera.\n"
           "  -a <x> <y> <z>   Camera aim.\n"
           "  -z <x> <y> <z>   Camera zenith vector.\n");
    printf("Interaction keys:\n"
           "  (Arrow keys)     Rotate system.\n"
           "  +, -             Zoom in, out.\n"
           "  w, s             Forward, backwards.\n"
           "  a, d             Turn left, right.\n"
           "  z, x             Move sideways to the left, right.\n"
           "  r, f             Move up, down.\n"
           "  1, 2             Look up, down.\n"
           "  3, 4             Camera roll counter-clockwise, clockwise.\n"
           "  b                Rewind data file.\n"
           "  p, (space bar)   Toggle pause on/off.\n"
           "  .                Toggle fading on/off.\n"
           "  c                Output camera information.\n"
           "  o                Take (ppm) screenshot.\n"
           "  0                Start/stop recording video.\n"
           "  q, (escape)      Quit program.\n");
    return 0;
  }
  else { /* Open file */
    for(i = 1; i < argc; i++) {
      /* Open md data file */
      if(argv[i][0] != '-')
        mddata = fopen(argv[i], "r");
      /* Other options */
      else if(argv[i][1] == 'b') { /* Background colour */
        i++;
        background = atoi(argv[i]);
      }
      else if(argv[i][1] == 't') { /* Text colour */
        i++;
        text = atoi(argv[i]);
      }
      else if(argv[i][1] == 'L') { /* Initial camera distance */
        i++;
        L = atof(argv[i]);
        loc[0] = L; loc[1] = loc[2] = 0;
      }
      else if(argv[i][1] == 'l') { /* Initial camera location */
        loc[0] = atof(argv[i + 1]);
        loc[1] = atof(argv[i + 2]);
        loc[2] = atof(argv[i + 3]);
        i += 3;
      }
      else if(argv[i][1] == 'a') { /* Initial camera aim */
        aim[0] = atof(argv[i + 1]);
        aim[1] = atof(argv[i + 2]);
        aim[2] = atof(argv[i + 3]);
        i += 3;
      }
      else if(argv[i][1] == 'z') { /* Initial camera zenith */
        zen[0] = atof(argv[i + 1]);
        zen[1] = atof(argv[i + 2]);
        zen[2] = atof(argv[i + 3]);
        i += 3;
      }
      else i++; /* Skip unrecognised options */
    }
  }
  if(!isatty(0)) { /* Open stdin */
    mddata = stdin;
  }

  if(mddata == NULL) { /* File error */
    printf("File not found or error opening file.\n");
    return -1;
  }

  /* Text message */
  fprintf(stderr, GREEN "  \xe2\x94\x8c" ULINE ULINE ULINE ULINE "\xe2\x94\x90\n"
                  "  \xe2\x94\x82" BLUE "sº" CYAN "o~" GREEN "\xe2\x94\x82  " WHITE "minipunto.\n"
                  GREEN "  \xe2\x94\x94" ULINE ULINE ULINE ULINE "\xe2\x94\x98\n" NORMAL);


  /* Initialise X */
  Display *d = XOpenDisplay((char*)0);
  Window w = XCreateSimpleWindow(d, DefaultRootWindow(d), 0, 0, WIDTH, HEIGHT, 5, 0, 0);
  XStoreName(d, w, "minipunto (v " VERSION ")"); /* Name in window bar */
  XEvent e;  /* X11 event */
  KeySym key; /* X11 KeySym code */
  XComposeStatus compose; /* Don't ask... */
  XSelectInput(d, w, KeyPressMask); /* List of event types to recognise */
  GC g = XCreateGC(d, w, 0, 0); /* Graphics context */
  XWindowAttributes wa;
  XGetWindowAttributes(d, w, &wa);
  char screenbuffer[4*WIDTH*HEIGHT];
  XImage * I;
  I = XCreateImage(d, DefaultVisual(d, 0), wa.depth, ZPixmap, 0, screenbuffer, 600, 600, 32, 0);
  XMapRaised(d,w);
  XSetForeground(d, g, text);

  /* Data variables */
  float dat[5]; /* Stream read from file (position -x, y and z-, radius and colour) */
  char buffer[250]; /* String from data file */
  char command[50]; /* Command in data file */
  char msg[250]; msg[0] = '\0'; /* On-screen message */
  bool paused = false; /* Paused flag */
  bool recording = false; /* Recording to video flag */
  bool screenshot = false; /* Screenshot flag */
  int nscreenshot = 0; /* Screenshot number */
  long filepos = ftell(mddata); /* Position in a stream */

  /* 3D variables */
  struct camera cam; /* Camera */
  float r[3]; /* Camera-particle displacement vector */
  float R; /* Radius */
  struct colour c; /* RGB colour */
  float depth = 0; /* Depth of point from camera */
  int xs, ys, s; /* Screen coordinates of particle */
  float zbuffer[WIDTH*HEIGHT]; /* Pixel depth z-buffer */

  /* Set the camera position and orientation */
  setcamera(&cam, loc, aim, zen);

  /* Main loop (read data, events and refresh frame) */
  while(1) {
    if(!fgets(buffer, 250, mddata)) { /* Read a line from file */
      rewind(mddata); /* Check the case of stdin */
    }

    /* Default radius and colour */
    R = 1; c = (struct colour) {250, 250, 250};

    /* Read string with sscanf */
    s=sscanf(buffer, "%f %f %f %f %f", &dat[0], &dat[1], &dat[2], &dat[3], &dat[4]);

    if(s > 2) { /* Enough data to draw a particle */
      /* Get particle data */
      if(s > 3) R = dat[3];

      /* Colour RGB components */
      if(s > 4) {
        c.r = ((int) dat[4])/65536;
        c.g = (((int) dat[4])/256)%256;
        c.b = ((int) dat[4])%256;
      }

      /* Camera-particle vector */
      for(i = 0; i < 3; i++) r[i] = dat[i] - cam.location[i];

      /* Depth of particle measured from camera */
      depth = dot(r, cam.direction)/3.732;

      if(depth > 1) {
        /* Screen coordinates of particle */
        xs=(int)(0.5f*WIDTH*(1 + dot(r, cam.screenx)/depth));
        ys=(int)(0.5f*HEIGHT*(1 - dot(r, cam.screeny)/depth));
        s=(int)(0.5f*WIDTH*R/depth);
        // # pragma omp parallel for private(j)
        for(i=-s; i <= s; i++) {
          for(j = -s; j <= s; j++) {
            /* Only paint points on a circle */
            if(i*i + j*j > s*s) continue;

            /* Light angle factor */
            # ifdef FAST_MATH
            float lighting = 1 - (i*i + j*j)/(2.0f*s*s);
            # else
            float lighting = sqrtf(1 - (float) (i*i + j*j)/(s*s));
            # endif
            if(lighting > 1) lighting = 1.0f;

            if(abs(xs + i - WIDTH/2) < WIDTH/2 && abs(ys + j - HEIGHT/2) < HEIGHT/2) {
              if(zbuffer[WIDTH*(xs + i)+(ys + j)] > depth - lighting) { /* Check whether point is visible */
                zbuffer[WIDTH*(xs + i)+(ys + j)] = depth - lighting; /* Set z-buffer value */
                # ifndef NO_FADING
                lighting *= (1.0f - fade*(depth - 1.0f)/(2.0f*L - 1.0f)); /* Modify colour by depth */
                # endif
                if(lighting < 0.0f) lighting = 0.0f;
                XPutPixel(I, xs + i, ys + j, (int) (c.r*lighting)*65536 + (int) (c.g*lighting)*256 + (int) (c.b*lighting)); /* Draw point */
              }
            }
          }
        }
      }
    }
    else if(buffer[0]=='#') { /* Ignore comments */
      if(buffer[1]=='%') { /* Magic commands */
        s = sscanf(buffer, "#%% %s", command);
        if(!strcmp(command,"camera")) {
          s = sscanf(buffer, "#%% camera %f %f %f %f %f %f %f %f %f",
                             &loc[0], &loc[1], &loc[2],
                             &aim[0], &aim[1], &aim[2],
                             &zen[0], &zen[1], &zen[2]);
        }
      }
      else if(buffer[1]=='\'') { /* Print text */
        s = sscanf(buffer, "#' %250[^\n]", msg);
      }
    }
    else // if(buffer[0]=='\n') /* If line is blank, then draw frame */
    {
      XPutImage(d, w, g, I, 0, 0, 0, 0, 600, 600); /* Display frame */
      XDrawString(d, w, g, 2, 12, msg, strlen(msg)); /* Display messages */
      if(recording) XDrawString(d, w, g, WIDTH-45, 15, "[0 REC]", 7);
      XFlush(d); /* Refresh screen */
      usleep(30); /* Sleep for 30 microseconds */

      if(paused) fseek(mddata, filepos, SEEK_SET); /* Stay on this frame */
      else filepos = ftell(mddata); /* Store position of next frame */

      setcamera(&cam, loc, aim, zen); /* Reset the camera position */

      if(screenshot) { /* Take screenshot */
        char * screenshot_filename; /* String to store number */
        s = asprintf(&screenshot_filename, "%d.ppm", nscreenshot);

        /* Open screenshot file */
        FILE * screenshot_file;
        screenshot_file = fopen((char *) screenshot_filename, "w");

        /* Output image */
        fprintf(screenshot_file, "P3\n"); /* Magic number */
        fprintf(screenshot_file, "%d %d\n", WIDTH, HEIGHT);
        fprintf(screenshot_file, "255\n"); /* Colour depth */
        for(j = 0; j < HEIGHT; j++) {
          for(i = 0; i < WIDTH; i++) {
            s = XGetPixel(I, i, j);
            fprintf(screenshot_file, "%d %d %d\n", s/65536, (s/256)%256, s%256);
          }
        }

        fclose(screenshot_file); /* Close file */
        nscreenshot++; /* Advance screenshot number */
        screenshot = false; /* Reset screenshot flag */
      }

      if(recording) { /* Add frame to video */
        /* Output raw pixel data to named pipe */
        for(j = 0; j < HEIGHT; j++) {
          for(i = 0; i < WIDTH; i++) {
            s = XGetPixel(I, i, j);
            fwrite(&s, sizeof(int), 1, videopipe);
          }
        }
      }

      /* Clear the window and z-buffer to start drawing the next frame */
      for(i = 0; i < WIDTH; i++) {
        for(j = 0; j < HEIGHT; j++) {
          BACKGROUND_HORIZON
          XPutPixel(I, i, j, background);
          zbuffer[WIDTH*i + j] = 2.5f*L;
        }
      }
    }
    while(XPending(d)>0) {
      XNextEvent(d, &e);
      if(e.type==KeyPress){
        s = XLookupString(&e.xkey, buffer, 250, &key, &compose);
        switch(key)
        {
          case KEY_PLUS: /* Zoom in */
            for(i = 0; i < 3; i++) {
              loc[i] *= 0.99;
              aim[i] *= 0.99;
            }
            break;
          case KEY_MINUS: /* Zoom out */
            for(i = 0; i < 3; i++) {
              loc[i] /= 0.99;
              aim[i] /= 0.99;
            }
            break;
          /* Rotate system */
          case KEY_LEFT:
            rotate(loc, zen, 0.07);
            if(modulus(aim) != 0)
              rotate(aim, zen, 0.07);
            break;
          case KEY_RIGHT:
            rotate(loc, zen, -0.07);
            if(modulus(aim) != 0)
              rotate(aim, zen, -0.07);
            break;
          case KEY_UP:
            rotate(zen, cam.screenx, 0.07);
            rotate(loc, cam.screenx, 0.07);
            if(modulus(aim) != 0)
              rotate(aim, cam.screenx, 0.07);
            break;
          case KEY_DOWN:
            rotate(zen, cam.screenx, -0.07);
            rotate(loc, cam.screenx, -0.07);
            if(modulus(aim) != 0)
              rotate(aim, cam.screenx, -0.07);
            break;
          /* Move camera */
          case KEY_W: /* Forward */
          case KEY_w:
            for(i = 0; i < 3; i++) loc[i] += 0.1*cam.direction[i];
            for(i = 0; i < 3; i++) aim[i] += 0.1*cam.direction[i];
            break;
          case KEY_S: /* Back */
          case KEY_s:
            for(i = 0; i < 3; i++) loc[i] -= 0.1*cam.direction[i];
            for(i = 0; i < 3; i++) aim[i] -= 0.1*cam.direction[i];
            break;
          case KEY_A: /* Left */
          case KEY_a:
            for(i = 0; i < 3; i++) aim[i] -= loc[i];
            rotate(aim, zen, -0.05);
            for(i = 0; i < 3; i++) aim[i] += loc[i];
            break;
          case KEY_D: /* Right */
          case KEY_d:
            for(i = 0; i < 3; i++) aim[i] -= loc[i];
            rotate(aim, zen, 0.05);
            for(i = 0; i < 3; i++) aim[i] += loc[i];
            break;
          case KEY_Z: /* Sideways (left) */
          case KEY_z:
            for(i = 0; i < 3; i++) loc[i] -= 0.1*cam.screenx[i];
            for(i = 0; i < 3; i++) aim[i] -= 0.1*cam.screenx[i];
            break;
          case KEY_X: /* Sideways (right) */
          case KEY_x:
            for(i = 0; i < 3; i++) loc[i] += 0.1*cam.screenx[i];
            for(i = 0; i < 3; i++) aim[i] += 0.1*cam.screenx[i];
            break;
          case KEY_F: /* Down */
          case KEY_f:
            for(i = 0; i < 3; i++) loc[i] -= 0.1*cam.screeny[i];
            for(i = 0; i < 3; i++) aim[i] -= 0.1*cam.screeny[i];
            break;
          case KEY_R: /* Up */
          case KEY_r:
            for(i = 0; i < 3; i++) loc[i] += 0.1*cam.screeny[i];
            for(i = 0; i < 3; i++) aim[i] += 0.1*cam.screeny[i];
            break;
          /* Rotate camera */
          case KEY_1:
            for(i = 0; i < 3; i++) aim[i] -= loc[i];
            rotate(aim, cam.screenx, -0.05);
            for(i = 0; i < 3; i++) aim[i] += loc[i];
            break;
          case KEY_2:
            for(i = 0; i < 3; i++) aim[i] -= loc[i];
            rotate(aim, cam.screenx, 0.05);
            for(i = 0; i < 3; i++) aim[i] += loc[i];
            break;
          case KEY_3:
            rotate(zen, cam.direction, -0.07);
            break;
          case KEY_4:
            rotate(zen, cam.direction, 0.07);
            break;
          /* Rewind MD data file */
          case KEY_B:
          case KEY_b:
            rewind(mddata); /* Check the case of stdin */
            filepos = ftell(mddata); /* Position in a stream */
            break;
          /* Pause */
          case KEY_P:
          case KEY_p:
          case KEY_SPC:
            paused = 1 - paused;
            break;
          case KEY_C:
          case KEY_c:
            fprintf(stderr, "Camera information:\n");
            fprintf(stderr, " location (%f, %f, %f),\n", loc[0], loc[1], loc[2]);
            fprintf(stderr, " aim (%f, %f, %f),\n", aim[0], aim[1], aim[2]);
            fprintf(stderr, " zenith (%f, %f, %f).\n", zen[0], zen[1], zen[2]);
            break;
          case XK_period:
            fade = 1 - fade;
            break;
          case KEY_O:
          case KEY_o:
            screenshot = true;
            break;
          case KEY_0:
            recording = 1 - recording;
            if(recording) {
              # ifdef RAW_VIDEO_TO_FILE
              /* Open video file */
              videopipe = fopen("video.raw", "w");
              # else
              char pipecommand[120];
              sprintf(pipecommand,
                      "cat | avconv -loglevel panic -y -f rawvideo -s %dx%d -pix_fmt rgb32"
                      " -r 30 -i - -an -b:v 24000k video.mp4", WIDTH, HEIGHT);
              /* Open pipe */
              videopipe = popen(pipecommand, "w");
              # endif
              fprintf(stderr, "Recording video...\n");
              if(videopipe == NULL) {
                fprintf(stderr, "Error: unable to open video pipe.\n");
                exit(-1);
              }
            }
            else
            {
              fprintf(stderr, "Recording stopped.\n");
              /* Close pipe */
              # ifdef RAW_VIDEO_TO_FILE
              fclose(videopipe);
              # else
              pclose(videopipe);
              # endif
              videopipe = NULL;
            }
            break;
          /* Close X and exit */
          case KEY_ESC:
          case KEY_Q:
          case KEY_q:
          XFreeGC(d, g);
          XDestroyWindow(d,w);
          XCloseDisplay(d);

          /* Close files */
          fclose(mddata);
          # ifdef RAW_VIDEO_TO_FILE
          if(videopipe != NULL) fclose(videopipe);
          # else
          if(videopipe != NULL) pclose(videopipe);
          # endif
          return 0;
        }
      }
    }
  }
}
