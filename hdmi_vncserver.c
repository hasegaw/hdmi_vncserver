/**
 * @example example.c
 * This is an example of how to use libvncserver.
 * 
 * libvncserver example
 * Copyright (C) 2001 Johannes E. Schindelin <Johannes.Schindelin@gmx.de>
 * 
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

#ifdef WIN32
#define sleep Sleep
#else
#include <unistd.h>
#endif

#ifdef __IRIX__
#include <netdb.h>
#endif

#include <rfb/rfb.h>
#include <rfb/keysym.h>

#include <stdio.h>
#include <stdlib.h>

#include <opencv/cv.h>
extern "C" {
    #include </usr/include/libxlnk_cma.h>
}
#define CMA_BUFF_SIZE 1920*1080*3

static const int bpp=4;
static int maxx=1920, maxy=1080;
/* TODO: odd maxx doesn't work (vncviewer bug) */

char *vdma = NULL;
void *buf_cma;
IplImage *img3, *img4;
void *img3_buf, *img4_buf;
rfbScreenInfoPtr _screen;

static void init_cma_buffer()
{
    buf_cma = cma_alloc(CMA_BUFF_SIZE, 0);
    if (buf_cma == NULL) {
        fputs("cannot alloc CMA buffer - are you root?\n", stderr);
    }
}

static void free_cma_buffer()
{
    if (buf_cma != NULL) {
        cma_free(buf_cma);
        buf_cma = NULL;
    }
}

void map_vdma() {
    uint32_t addr = 0x43000000;
    vdma = (char*) cma_mmap(addr, 0xff);
    if (vdma == NULL) {
        printf("cannot get ptr of VDMA\n");
        exit(1);
    }
}

void unmap_vdma() {
    if (vdma != NULL) {
        cma_munmap(vdma, 0xff);
        vdma = NULL;
    }
}

void update_buffer(void* buffer) {
   *((uint32_t*) (vdma + 0x30)) = 0x04; 
   *((uint32_t*) (vdma + 0x30)) = 0x8b; 
   *((uint32_t*) (vdma + 0xac)) = cma_get_phy_addr(buf_cma);

   *((uint32_t*) (vdma + 0xa8)) = 1920 * 3; // stride
   *((uint32_t*) (vdma + 0xa4)) = 1920 * 3; // bytes per line

   *((uint32_t*) (vdma + 0xa0)) = 1080; // lines (start DMA)
   int dma_sr = 1;
   while (dma_sr & 1) {
       dma_sr = *((uint8_t*) (vdma + 0x34));
   }
}

static void initBuffer(unsigned char* buffer)
{
    update_buffer((void *) img3->imageData);
    cvCvtColor(img3, img4, CV_BGR2RGBA);
    memcpy(buffer, (void *) img4->imageData, 1920*1080*4);
}

/* Here we create a structure so that every client has its own pointer */

typedef struct ClientData {
  rfbBool oldButton;
  int oldx,oldy;
} ClientData;

static void clientgone(rfbClientPtr cl)
{
  free(cl->clientData);
  cl->clientData = NULL;
}

static enum rfbNewClientAction newclient(rfbClientPtr cl)
{
  cl->clientData = (void*)calloc(sizeof(ClientData),1);
  cl->clientGoneHook = clientgone;
  return RFB_CLIENT_ACCEPT;
}

/* switch to new framebuffer contents */

static void newframebuffer(rfbScreenInfoPtr screen, int width, int height)
{
  unsigned char *oldfb, *newfb;

  maxx = width;
  maxy = height;
  oldfb = (unsigned char*)screen->frameBuffer;
  newfb = (unsigned char*)malloc(maxx * maxy * bpp);
  initBuffer(newfb);
  rfbNewFramebuffer(screen, (char*)newfb, maxx, maxy, 8, 3, bpp);
  free(oldfb);

  /*** FIXME: Re-install cursor. ***/
}

/* aux function to draw a line */

static void drawline(unsigned char* buffer,int rowstride,int bpp,int x1,int y1,int x2,int y2)
{
  int i,j;
  i=x1-x2; j=y1-y2;
  if(i==0 && j==0) {
     for(i=0;i<bpp;i++)
       buffer[y1*rowstride+x1*bpp+i]=0xff;
     return;
  }
  if(i<0) i=-i;
  if(j<0) j=-j;
  if(i<j) {
    if(y1>y2) { i=y2; y2=y1; y1=i; i=x2; x2=x1; x1=i; }
    for(j=y1;j<=y2;j++)
      for(i=0;i<bpp;i++)
	buffer[j*rowstride+(x1+(j-y1)*(x2-x1)/(y2-y1))*bpp+i]=0xff;
  } else {
    if(x1>x2) { i=y2; y2=y1; y1=i; i=x2; x2=x1; x1=i; }
    for(i=x1;i<=x2;i++)
      for(j=0;j<bpp;j++)
	buffer[(y1+(i-x1)*(y2-y1)/(x2-x1))*rowstride+i*bpp+j]=0xff;
  }
}
    
/* Here the pointer events are handled */

static void doptr(int buttonMask,int x,int y,rfbClientPtr cl)
{
   ClientData* cd=cl->clientData;
   if (0) {
      int x1, y1, x2, y2;
      x1 = x - 32;
      y1 = y - 32;
      x2 = x + 32;
      y2 = y + 32;
      if (x1 < 0) x1 = 0;
      if (y1 < 0) y1 = 0;
      if (x2 > 1900) x2 = 1900;
      if (y2 > 1000) y2 = 1000;
      rfbMarkRectAsModified(cl->screen, x1, y1, x2, y2);
   }


   if(x>=0 && y>=0 && x<maxx && y<maxy) {
      if(buttonMask) {
	 int i,j,x1,x2,y1,y2;

	 if(cd->oldButton==buttonMask) { /* draw a line */
	    drawline((unsigned char*)cl->screen->frameBuffer,cl->screen->paddedWidthInBytes,bpp,
		     x,y,cd->oldx,cd->oldy);
	    x1=x; y1=y;
	    if(x1>cd->oldx) x1++; else cd->oldx++;
	    if(y1>cd->oldy) y1++; else cd->oldy++;
	    rfbMarkRectAsModified(cl->screen,x,y,cd->oldx,cd->oldy);
	 } else { /* draw a point (diameter depends on button) */
	    int w=cl->screen->paddedWidthInBytes;
	    x1=x-buttonMask; if(x1<0) x1=0;
	    x2=x+buttonMask; if(x2>maxx) x2=maxx;
	    y1=y-buttonMask; if(y1<0) y1=0;
	    y2=y+buttonMask; if(y2>maxy) y2=maxy;

	    for(i=x1*bpp;i<x2*bpp;i++)
	      for(j=y1;j<y2;j++)
		cl->screen->frameBuffer[j*w+i]=(char)0xff;
	    rfbMarkRectAsModified(cl->screen,x1,y1,x2,y2);
	 }

	 /* we could get a selection like that:
	  rfbGotXCutText(cl->screen,"Hallo",5);
	  */
      } else
	cd->oldButton=0;

      cd->oldx=x; cd->oldy=y; cd->oldButton=buttonMask;
   }
   rfbDefaultPtrAddEvent(buttonMask,x,y,cl);
}

/* Here the key events are handled */

static void dokey(rfbBool down,rfbKeySym key,rfbClientPtr cl)
{
  if(down) {
    if(key==XK_Escape)
      rfbCloseClient(cl);
    else if(key==XK_F12)
      /* close down server, disconnecting clients */
      rfbShutdownServer(cl->screen,TRUE);
    else if(key==XK_F11)
      /* close down server, but wait for all clients to disconnect */
      rfbShutdownServer(cl->screen,FALSE);
    else if(key==XK_Page_Up) {
      initBuffer((unsigned char*)cl->screen->frameBuffer);
      rfbMarkRectAsModified(cl->screen,0,0,maxx,maxy);
    } else if (key == XK_Up) {
      if (maxx < 1024) {
        if (maxx < 1920) {
          newframebuffer(cl->screen, 1920, 1080);
        } else {
          newframebuffer(cl->screen, 1024, 768);
        }
      }
    } else if(key==XK_Down) {
      if (maxx > 640) {
        if (maxx > 1920) {
          newframebuffer(cl->screen, 1920, 1080);
        } else {
          newframebuffer(cl->screen, 640, 480);
        }
      }
    } else if(key>=' ' && key<0x100) {
      ClientData* cd=cl->clientData;
      int x1=cd->oldx,y1=cd->oldy,x2,y2;
      //cd->oldx+=rfbDrawCharWithClip(cl->screen,&radonFont,cd->oldx,cd->oldy,(char)key,0,0,cl->screen->width,cl->screen->height,0x00ffffff,0x00ffffff);
      //rfbFontBBox(&radonFont,(char)key,&x1,&y1,&x2,&y2);
      rfbMarkRectAsModified(cl->screen,x1,y1,x2-1,y2-1);
    }
  }
}

/* Example for an XCursor (foreground/background only) */

#ifdef JUST_AN_EXAMPLE

static int exampleXCursorWidth=9,exampleXCursorHeight=7;
static char exampleXCursor[]=
  "         "
  " xx   xx "
  "  xx xx  "
  "   xxx   "
  "  xx xx  "
  " xx   xx "
  "         ";

#endif

/* Example for a rich cursor (full-colour) */

static void MakeRichCursor(rfbScreenInfoPtr rfbScreen)
{
  int i,j,w=32,h=32;
  rfbCursorPtr c = rfbScreen->cursor;
  char bitmap[]=
    "                                "
    "              xxxxxx            "
    "       xxxxxxxxxxxxxxxxx        "
    "      xxxxxxxxxxxxxxxxxxxxxx    "
    "    xxxxx  xxxxxxxx  xxxxxxxx   "
    "   xxxxxxxxxxxxxxxxxxxxxxxxxxx  "
    "  xxxxxxxxxxxxxxxxxxxxxxxxxxxxx "
    "  xxxxx   xxxxxxxxxxx   xxxxxxx "
    "  xxxx     xxxxxxxxx     xxxxxx "
    "  xxxxx   xxxxxxxxxxx   xxxxxxx "
    " xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx "
    " xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx "
    " xxxxxxxxxxxx  xxxxxxxxxxxxxxx  "
    " xxxxxxxxxxxxxxxxxxxxxxxxxxxx   "
    " xxxxxxxxxxxxxxxxxxxxxxxxxxxx   "
    " xxxxxxxxxxx   xxxxxxxxxxxxxx   "
    " xxxxxxxxxx     xxxxxxxxxxxx    "
    "  xxxxxxxxx      xxxxxxxxx      "
    "   xxxxxxxxxx   xxxxxxxxx       "
    "      xxxxxxxxxxxxxxxxxxx       "
    "       xxxxxxxxxxxxxxxxxxx      "
    "         xxxxxxxxxxxxxxxxxxx    "
    "             xxxxxxxxxxxxxxxxx  "
    "                xxxxxxxxxxxxxxx "
    "   xxxx           xxxxxxxxxxxxx "
    "  xx   x            xxxxxxxxxxx "
    "  xxx               xxxxxxxxxxx "
    "  xxxx             xxxxxxxxxxx  "
    "   xxxxxx       xxxxxxxxxxxx    "
    "    xxxxxxxxxxxxxxxxxxxxxx      "
    "      xxxxxxxxxxxxxxxx          "
    "                                ";
  c=rfbScreen->cursor = rfbMakeXCursor(w,h,bitmap,bitmap);
  c->xhot = 16; c->yhot = 24;

  c->richSource = (unsigned char*)malloc(w*h*bpp);
  c->cleanupRichSource = TRUE;
  for(j=0;j<h;j++) {
    for(i=0;i<w;i++) {
      c->richSource[j*w*bpp+i*bpp+0]=i*0xff/w;
      c->richSource[j*w*bpp+i*bpp+1]=(i+j)*0xff/(w+h);
      c->richSource[j*w*bpp+i*bpp+2]=j*0xff/h;
      c->richSource[j*w*bpp+i*bpp+3]=0;
    }
  }
}

/* Initialization */

int main(int argc,char** argv)
{
  map_vdma();
  init_cma_buffer();
  CvSize img_size;
  img_size.width = 1920;
  img_size.height = 1080;
  img3 = cvCreateImage(img_size, IPL_DEPTH_8U, 3);
  img3_buf = img3->imageData;
  img3->imageData = buf_cma;

  img4 = cvCreateImage(img_size, IPL_DEPTH_8U, 4);

  rfbScreenInfoPtr rfbScreen = rfbGetScreen(&argc,argv,maxx,maxy,8,3,bpp);
  _screen = rfbScreen; //FIXME
  if(!rfbScreen)
    return 0;
  rfbScreen->desktopName = "LibVNCServer Example";
#if 0
  rfbScreen->frameBuffer = img4->imageData; //(char*)malloc(maxx*maxy*bpp);
#else
  rfbScreen->frameBuffer = (char*)malloc(maxx*maxy*bpp);
#endif
  rfbScreen->alwaysShared = TRUE;
  rfbScreen->ptrAddEvent = doptr;
  rfbScreen->kbdAddEvent = dokey;
  rfbScreen->newClientHook = newclient;
  rfbScreen->httpDir = "../webclients";
  rfbScreen->httpEnableProxyConnect = TRUE;

  initBuffer((unsigned char*)rfbScreen->frameBuffer);
  //rfbDrawString(rfbScreen,&radonFont,20,100,"Hello, World!",0xffffff);

  /* This call creates a mask and then a cursor: */
  /* rfbScreen->defaultCursor =
       rfbMakeXCursor(exampleCursorWidth,exampleCursorHeight,exampleCursor,0);
  */

  MakeRichCursor(rfbScreen);

  /* initialize the server */
  rfbInitServer(rfbScreen);

#define BACKGROUND_LOOP_TEST
#ifndef BACKGROUND_LOOP_TEST
#ifdef USE_OWN_LOOP
  {
    int i;
    for(i=0;rfbIsActive(rfbScreen);i++) {
      fprintf(stderr,"%d\r",i);
      rfbProcessEvents(rfbScreen,100000);
      initBuffer(rfbScreen->frameBuffer);
      rfbMarkRectAsModified(rfbScreen, 0, 0, 1920, 1080);
    }
  }
aaa
#else
  /* this is the blocking event loop, i.e. it never returns */
  /* 40000 are the microseconds to wait on select(), i.e. 0.04 seconds */
  prinf("plz wait\n");
  rfbRunEventLoop(rfbScreen,40000,FALSE);
      initBuffer(rfbScreen->frameBuffer);
      rfbMarkRectAsModified(rfbScreen, 0, 0, 1920, 1080);
#endif /* OWN LOOP */
#else
#if !defined(LIBVNCSERVER_HAVE_LIBPTHREAD)
#error "I need pthreads for that."
#endif

  /* this is the non-blocking event loop; a background thread is started */
  rfbRunEventLoop(rfbScreen,-1,TRUE);
  fprintf(stderr, "Running background loop...\n");
  /* now we could do some cool things like rendering in idle time */
  while(1) {
      initBuffer(rfbScreen->frameBuffer);
      rfbMarkRectAsModified(rfbScreen, 0, 0, 1920, 1080);

  } //sleep(5); /* render(); */
#endif /* BACKGROUND_LOOP */

  free(rfbScreen->frameBuffer);
  rfbScreenCleanup(rfbScreen);


  free_cma_buffer();
  unmap_vdma();
  return(0);
}
