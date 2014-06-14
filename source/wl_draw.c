#include "include/wl_def.h"
#include <math.h>
/* C AsmRefresh() and related code
   originally from David Haslam -- dch@sirius.demon.co.uk */

/* the door is the last picture before the sprites */
#define DOORWALL	(PMSpriteStart-8)

#define ACTORSIZE	0x4000
#define	MAXVIEWHEIGHT	200

static byte *spritegfx[SPR_TOTAL];

// Begin Erik misc hacky crap
static byte *spriteMinX[SPR_TOTAL];
static byte *spriteMaxX[SPR_TOTAL];
static byte *spriteMinY[SPR_TOTAL];
static byte *spriteMaxY[SPR_TOTAL];
static int offsets[3];
// End Erik misc hacky crap

static int spanstart[MAXVIEWHEIGHT/2];

static fixed basedist[MAXVIEWHEIGHT/2];
static unsigned wallheight[MAXVIEWWIDTH];
static unsigned oldwallheight[MAXVIEWWIDTH];

/* refresh variables */
//fixed viewx, viewy;	  /* the focal point */

static int viewangle;

static unsigned int columnskip;
static unsigned int rowskip;

static unsigned tilehit;

static int xtile, ytile;
static int xtilestep, ytilestep;
static long xintercept, yintercept;

static unsigned short postx;

static fixed focallength;
static fixed scale;
static long heightnumerator;

static void AsmRefresh();

#define NOASM

#ifndef NOASM
#define FixedByFrac(x, y) \
__extension__  \
({ unsigned long z; \
 asm("imull %2; shrdl $16, %%edx, %%eax" : "=a" (z) : "a" (x), "q" (y) : "%edx"); \
 z; \
})
#endif

void ScaleShape(short xcenter, short shapenum, unsigned short height);
void SimpleScaleShape(const short xcenter,const short shapenum,const unsigned short height);
static void DeCompileSprite(short shapenum);
 
/* ======================================================================== */

/*
====================
=
= CalcProjection
=
====================
*/

static const double radtoint = (double)FINEANGLES/2.0/PI;
static unsigned int frach[1024];
static unsigned int deltah[1024];
static 	byte     halfview;
void CalcProjection(long focal)
{
	byte     i;
	long    intang;
	double angle, tang, facedist;

	double ttang;
	short ii,jj;
	focallength = focal;
	facedist = focal+MINDIST;
	halfview = (viewwidth>>1);               /* half view in pixels */
    frach[0]=0;
    deltah[0]=0;
    for (ii = 1; ii < 1024; ii++)
    {
      	frach[ii] = (64 << 16) / ii;
		deltah[ii] = (64 << 16) - frach[ii]*ii;
	}

/*
 calculate scale value for vertical height calculations
 and sprite x calculations
*/
	scale = halfview*facedist/(VIEWGLOBAL/2);

/*
 divide heightnumerator by a posts distance to get the posts height for
 the heightbuffer.  The pixel height is height>>2
*/
	heightnumerator = (TILEGLOBAL*scale)>>6;

/* calculate the angle offset from view angle of each pixel's ray */
	ttang=(double)VIEWGLOBAL/viewwidth/facedist;
	tang=(double)0;
	for (i = 0; i < halfview; i++) {
		angle = atan(tang);
		intang = angle*radtoint;
		pixelangle[halfview-1-i] = intang;
		pixelangle[halfview+i] = -intang;
		tang+=ttang;
	}
}

/*
========================
=
= TransformActor
=
= Takes paramaters:
=   gx,gy		: globalx/globaly of point
=
= globals:
=   viewx,viewy		: point of view
=   viewcos,viewsin	: sin/cos of viewangle
=   scale		: conversion from global value to screen value
=
= sets:
=   screenx,transx,transy,screenheight: projected edge location and size
=
========================
*/
static fixed gx,gy,gxt,gyt,nx,ny;
static void TransformActor(objtype *ob)
{
	

//
// translate point to view centered coordinates
//
	gx = ob->x-viewx;
	gy = ob->y-viewy;

//
// calculate newx
//
	gxt = FixedByFrac(gx, viewcos);
	gyt = FixedByFrac(gy, viewsin);
	nx = gxt-gyt-ACTORSIZE;		// fudge the shape forward a bit, because
					// the midpoint could put parts of the shape
					// into an adjacent wall

//
// calculate newy
//
	gxt = FixedByFrac(gx,viewsin);
	gyt = FixedByFrac(gy,viewcos);
	ny = gyt+gxt;

//
// calculate perspective ratio
//
	ob->transx = nx;
	ob->transy = ny;

	if (nx < MINDIST) /* too close, don't overflow the divide */
	{
		ob->viewheight = 0;
		return;
	}

	ob->viewx = centerx + ny*scale/nx;	

	ob->viewheight = heightnumerator/(nx>>8);
}

/*
========================
=
= TransformTile
=
= Takes paramaters:
=   tx,ty		: tile the object is centered in
=
= globals:
=   viewx,viewy		: point of view
=   viewcos,viewsin	: sin/cos of viewangle
=   scale		: conversion from global value to screen value
=
= sets:
=   screenx,transx,transy,screenheight: projected edge location and size
=
= Returns true if the tile is withing getting distance
=
========================
*/

static boolean TransformTile(int tx, int ty, int *dispx, int *dispheight)
{
//	fixed gx,gy,gxt,gyt,nx,ny;

//
// translate point to view centered coordinates
//
	gx = ((long)tx<<TILESHIFT)+0x8000-viewx;
	gy = ((long)ty<<TILESHIFT)+0x8000-viewy;

//
// calculate newx
//
	gxt = FixedByFrac(gx,viewcos);
	gyt = FixedByFrac(gy,viewsin);
	nx = gxt-gyt-0x2000;		// 0x2000 is size of object

//
// calculate newy
//
	gxt = FixedByFrac(gx,viewsin);
	gyt = FixedByFrac(gy,viewcos);
	ny = gyt+gxt;

//
// calculate perspective ratio
//
	if (nx<MINDIST)			/* too close, don't overflow the divide */
	{
		*dispheight = 0;
		return false;
	}

	*dispx = centerx + ny*scale/nx;	

	*dispheight = heightnumerator/(nx>>8);

//
// see if it should be grabbed
//
	if ( (nx<TILEGLOBAL) && (ny>-TILEGLOBAL/2) && (ny<TILEGLOBAL/2) )
		return true;
	else
		return false;
}

/* ======================================================================== */

/*
=====================
=
= CalcRotate
=
=====================
*/

static int dirangle[9] = {0,ANGLES/8,2*ANGLES/8,3*ANGLES/8,4*ANGLES/8, 5*ANGLES/8,6*ANGLES/8,7*ANGLES/8,ANGLES};

static int CalcRotate(objtype *ob)
{
	int	angle,viewangle;

	/* this isn't exactly correct, as it should vary by a trig value, */
	/* but it is close enough with only eight rotations */

	viewangle = player->angle + ((centerx - ob->viewx)>>3);

	if (ob->obclass == rocketobj || ob->obclass == hrocketobj)
		angle =  (viewangle-180)- ob->angle;
	else
		angle =  (viewangle-180)- dirangle[ob->dir];

	angle+=ANGLES/16;
	while (angle>=ANGLES)
		angle-=ANGLES;
	while (angle<0)
		angle+=ANGLES;

	if (gamestates[ob->state].rotate == 2)  // 2 rotation pain frame
		return (angle/(ANGLES/2))<<2;    // seperated by 3

	return angle/(ANGLES/8);
}


/*
=====================
=
= DrawScaleds
=
= Draws all objects that are visible
=
=====================
*/

#define MAXVISABLE      64

typedef struct {
	int viewx;
	int viewheight;
	int shapenum;
} visobj_t;

static visobj_t vislist[MAXVISABLE], *visptr, *visstep, *farthest;

static void DrawScaleds()
{
	short  	  i,least,numvisable,height;
	byte		*tilespot,*visspot;
	unsigned	spotloc;

	statobj_t	*statptr;
	objtype		*obj;

	visptr = &vislist[0];

/* place static objects */
	for (statptr = &statobjlist[0]; statptr != laststatobj; statptr++)
	{
		if ((visptr->shapenum = statptr->shapenum) == -1)
			continue;			/* object has been deleted */

		if (!*statptr->visspot)
			continue;			/* not visable */

		if (TransformTile(statptr->tilex, statptr->tiley
			,&visptr->viewx,&visptr->viewheight) && statptr->flags & FL_BONUS)
		{
			GetBonus(statptr);
			continue;
		}

		if (!visptr->viewheight)
			continue;			/* too close to the object */

		if (visptr < &vislist[MAXVISABLE-1])	/* don't let it overflow */
			visptr++;
	}

//
// place active objects
//
	for (obj = player->next; obj; obj = obj->next)
	{
		if (!(visptr->shapenum = gamestates[obj->state].shapenum))
			continue;  // no shape

		spotloc = (obj->tilex << 6) + obj->tiley;
		visspot = &spotvis[0][0] + spotloc;
		tilespot = &tilemap[0][0] + spotloc;

		//
		// could be in any of the nine surrounding tiles
		//
		if (*visspot
		|| (*(visspot-1) && !*(tilespot-1))
		|| (*(visspot+1) && !*(tilespot+1))
		|| (*(visspot-65) && !*(tilespot-65))
		|| (*(visspot-64) && !*(tilespot-64))
		|| (*(visspot-63) && !*(tilespot-63))
		|| (*(visspot+65) && !*(tilespot+65))
		|| (*(visspot+64) && !*(tilespot+64))
		|| (*(visspot+63) && !*(tilespot+63))) 
		{
			obj->active = true;
			TransformActor(obj);
			if (!obj->viewheight)
				continue;						// too close or far away

			visptr->viewx = obj->viewx;
			visptr->viewheight = obj->viewheight;
			if (visptr->shapenum == -1)
				visptr->shapenum = obj->temp1;	// special shape

			if (gamestates[obj->state].rotate)
				visptr->shapenum += CalcRotate(obj);

			if (visptr < &vislist[MAXVISABLE-1])	/* don't let it overflow */
				visptr++;
			obj->flags |= FL_VISABLE;
		} else
			obj->flags &= ~FL_VISABLE;
	}

//
// draw from back to front
//
	numvisable = visptr-&vislist[0];

	if (!numvisable)
		return;									// no visable objects

	for (i = 0; i < numvisable; i++)
	{
		least = 32000;
		for (visstep = &vislist[0]; visstep < visptr; visstep++)
		{
			height = visstep->viewheight;
			if (height < least)
			{
				least = height;
				farthest = visstep;
			}
		}
		//
		// draw farthest
		//
		ScaleShape(farthest->viewx, farthest->shapenum, farthest->viewheight);

		farthest->viewheight = 32000;
	}

}

/* ======================================================================== */

/*
==============
=
= DrawPlayerWeapon
=
= Draw the player's hands
=
==============
*/

static int weaponscale[NUMWEAPONS] = {SPR_KNIFEREADY,SPR_PISTOLREADY
	,SPR_MACHINEGUNREADY,SPR_CHAINREADY};

static void DrawPlayerWeapon()
{
	short shapenum;

#ifndef SPEAR
	if (gamestate.victoryflag)
	{
		if ((player->state == s_deathcam) && (get_TimeCount() & 32))
			SimpleScaleShape(halfview,SPR_DEATHCAM,viewheight+1);
		return;
	}
#endif

	if (gamestate.weapon != -1)
	{
		shapenum = weaponscale[gamestate.weapon]+gamestate.weaponframe;
		SimpleScaleShape(halfview,shapenum,(viewheight+1));
	}

	if (demorecord || demoplayback)
		SimpleScaleShape(halfview,SPR_DEMO,viewheight+1);
}

/*
====================
=
= WallRefresh
=
====================
*/

static void WallRefresh()
{
	viewangle = player->angle;
	
	viewsin = sintable[viewangle];
	viewcos = costable[viewangle];
	viewx = player->x - FixedByFrac(focallength, viewcos);
	viewy = player->y + FixedByFrac(focallength, viewsin);

	AsmRefresh();
}

/* ======================================================================== */



static byte planepics[8192];	/* 4k of ceiling, 4k of floor */

static short halfheight = 0;

static byte *planeylookup[MAXVIEWHEIGHT/2];
static unsigned	mirrorofs[MAXVIEWHEIGHT/2];

static int mr_rowofs;
static int mr_count;
static unsigned short int mr_xstep;
static unsigned short int mr_ystep;
static unsigned short int mr_xfrac;
static unsigned short int mr_yfrac;
static byte *mr_dest;

static unsigned int ebx, edx, esi;

static void MapRow()
{
	

	edx = (mr_ystep << 16) | (mr_xstep);
	esi = (mr_yfrac << 16) | (mr_xfrac);
	
	while (mr_count--) {
		ebx = ((esi & 0xFC000000) >> 25) | ((esi & 0xFC00) >> 3);
		esi += edx;
		//ebx = ((mr_yfrac & 0xFC00) >> (25-16)) | ((mr_xfrac & 0xFC00) >> 3);
		//mr_yfrac += mr_ystep;
		//mr_xfrac += mr_xstep;
		
		mr_dest[0] = planepics[ebx+0];
		mr_dest[mr_rowofs] = planepics[ebx+1];
		mr_dest++;
	}
}

extern int yoffset320;
/*
==============
=
= DrawSpans
=
= Height ranges from 0 (infinity) to viewheight/2 (nearest)
==============
*/
//static unsigned long xyoff;

static void DrawSpans(int x1, int x2, int height)
{
	fixed length;
	int prestep;
	fixed startxfrac, startyfrac; 
	byte *toprow;

	toprow = planeylookup[height]+(yoffset320+xoffset);
	mr_rowofs = mirrorofs[height];

	mr_xstep = (viewsin / height) >>1;
	mr_ystep = (viewcos / height) >>1;

	length = basedist[height];
	startxfrac = (viewx + FixedByFrac(length, viewcos));
	startyfrac = (viewy - FixedByFrac(length, viewsin));

// draw two spans simultaniously

	prestep = halfview - x1;

	mr_xfrac = startxfrac - mr_xstep*prestep;
	mr_yfrac = startyfrac - mr_ystep*prestep;

	mr_dest = toprow + x1;
	mr_count = x2 - x1 + 1;

	if (mr_count)
		MapRow();
		
}

/*
===================
=
= SetPlaneViewSize
=
===================
*/

static void SetPlaneViewSize()
{
	short x;
    byte y;
	byte *dest, *src;

	halfheight = viewheight >>1;

	for (y = 0; y < (byte)halfheight; y++) {
		planeylookup[y] = gfxbuf + (halfheight-1-y)*320;
		mirrorofs[y] = 640*y+320;

		if (y > 0)
			basedist[y] = GLOBAL1/2*scale/y;
	}

	src = PM_GetPage(0);
	dest = planepics;
	for (x = 0; x < 4096; x++) {
		*dest = *src++;
		dest += 2;
	}
	
	src = PM_GetPage(1);
	dest = planepics+1;
	for (x = 0; x < 4096; x++) {
		*dest = *src++;
		dest += 2;
	}

}

/*
===================
=
= DrawPlanes
=
===================
*/

static void DrawPlanes()
{
	byte height, lastheight;
	short x;

	if ((viewheight>>1) != halfheight)
		SetPlaneViewSize();	/* screen size has changed */

/* loop over all columns */
	lastheight = halfheight;

	for (x = 0; x < viewwidth; x++)
	{
		height = wallheight[x]>>3;
		if (height < lastheight) {	// more starts
			do {
				spanstart[--lastheight] = x;
			} while (lastheight > height);
		} else if (height > lastheight) {	// draw spans
			if (height > halfheight)
				height = halfheight;
			for (; lastheight < height; lastheight++)
				DrawSpans(spanstart[lastheight], x-1, lastheight);
		}
	}

	height = halfheight;
	for (; lastheight < height; lastheight++)
		DrawSpans(spanstart[lastheight], x-1, lastheight);
}

/* ======================================================================== */

static unsigned int Ceiling[]=
{
#ifndef SPEAR
 0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0xbfbf,
 0x4e4e,0x4e4e,0x4e4e,0x1d1d,0x8d8d,0x4e4e,0x1d1d,0x2d2d,0x1d1d,0x8d8d,
 0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x2d2d,0xdddd,0x1d1d,0x1d1d,0x9898,

 0x1d1d,0x9d9d,0x2d2d,0xdddd,0xdddd,0x9d9d,0x2d2d,0x4d4d,0x1d1d,0xdddd,
 0x7d7d,0x1d1d,0x2d2d,0x2d2d,0xdddd,0xd7d7,0x1d1d,0x1d1d,0x1d1d,0x2d2d,
 0x1d1d,0x1d1d,0x1d1d,0x1d1d,0xdddd,0xdddd,0x7d7d,0xdddd,0xdddd,0xdddd
#else
 0x6f6f,0x4f4f,0x1d1d,0xdede,0xdfdf,0x2e2e,0x7f7f,0x9e9e,0xaeae,0x7f7f,
 0x1d1d,0xdede,0xdfdf,0xdede,0xdfdf,0xdede,0xe1e1,0xdcdc,0x2e2e,0x1d1d,0xdcdc
#endif
};

/*
=====================
=
= ClearScreen
=
=====================
*/
__inline void __asm FastFillScreen(register __a6 byte *gfx,
		       register __d1 long value);
   #define  colf 0x19191919

   static unsigned    long *clrs;
   static unsigned long colc;
   static unsigned int ceiling;
   static unsigned long *clr;
   static byte h, w, ww, hh ,xoff;
   extern unsigned long step;
static void ClearScreen()
{

	ceiling = Ceiling[gamestate.episode*10+mapon] & 0xFF;
	colc=ceiling|(ceiling<<8)|(ceiling<<16)|(ceiling<<24);
if (viewwidth==320) 
{
FastFillScreen((byte*)(gfxbuf+51200), colf);
FastFillScreen((byte*)(gfxbuf+25600), colc);
return;
}

clr=gfxbuf+yoffset320+(xoffset);

colc=ceiling|(ceiling<<8)|(ceiling<<16)|(ceiling<<24);
    ww=viewwidth>>2;
    hh=viewheight>>1;
    xoff=(xoffset>>1);

    h= hh;
    clrs=clr;

    while (h--){
     w=ww;
     while (w--)
     {
	//	if(w%2==columnskip){
		*clr=colc;
		*(clr+step)=colf;
		//}
		clr++;
     }
     clr += xoff;
    }
 



}

/* ======================================================================== */

/*
========================
=
= ThreeDRefresh
=
========================
*/

#ifndef DRAWCEIL
 /* #define DRAWCEIL */
#endif
typedef unsigned char UBYTE;
static long tt1=0,tt2=0;
static long framee=0;
static long elapse=0;
static long xx=0;
static unsigned char dela=0;
static  unsigned short oldv=21;
  extern unsigned char oldface;
  extern unsigned char oldfaceframe;
extern unsigned  char oldammo;
extern unsigned char oldlives;
  extern short oldscore;
extern unsigned char oldhealth;
  extern unsigned char oldweapon;
extern unsigned char oldkeys;
extern unsigned char oldlevel;
  long vw=0;
  extern struct RastPort *rport;
  extern struct RastPort temprp;
  extern struct RastPort temprp2;
  extern struct Screen *screen;
  extern UBYTE toggle;
  extern struct ScreenBuffer *sb[2];
  extern struct BitMap myBitMaps[2];
  extern void tBitMap();
extern  signed char rf;
extern signed char rk;
extern signed char rw;
extern  signed char rh;
extern signed char ra;
extern  signed char rl;
extern  signed char rs;
#define WIDTH 320
#define HEIGHT 200

byte drawScr=0;
#define modulo ((WIDTH+15)& ~15)
static       byte x=0,y=0,col=0;
static  UBYTE *tmp;
static  char Msg[3]={0,0,0};

void VW_UpdateScreen2()
{
	int starty,endy,x,y;

	if (vw!=viewheightwin)
    {
    	oldface--;
        oldammo--;
        oldfaceframe--;
        oldhealth--;
        oldweapon--;
        oldlives--;
        oldkeys--;
        oldscore--;
        oldlevel--;
        WriteChunkyPixels(&temprp2,0,0,WIDTH-1,HEIGHT-1,gfxbuf,WIDTH);
        while (!ChangeScreenBuffer(screen,sb[toggle]))
        	WaitTOF();
		tBitMap();
 		WriteChunkyPixels(&temprp2,0,0,WIDTH-1,HEIGHT-1,gfxbuf,WIDTH);
    }
    else
    	if (drawScr==1)
        {
        	drawScr=0;
            WriteChunkyPixels(&temprp2,0,0,WIDTH-1,HEIGHT-1,gfxbuf,WIDTH);
            while (!ChangeScreenBuffer(screen,sb[toggle]))
        		WaitTOF();
			tBitMap();
 			WriteChunkyPixels(&temprp2,0,0,WIDTH-1,HEIGHT-1,gfxbuf,WIDTH);
        }
	if (StartGame==0 || drawScr==1)
    {
    	oldface--;
        oldammo--;
        oldfaceframe--;
        oldhealth--;
        oldweapon--;
        oldlives--;
        oldkeys--;
        oldscore--;
        oldlevel--;
        WriteChunkyPixels(&temprp2,0,0,WIDTH-1,HEIGHT-1,gfxbuf,WIDTH);
        while (!ChangeScreenBuffer(screen,sb[toggle]))
        	WaitTOF();
		tBitMap();
 		WriteChunkyPixels(&temprp2,0,0,WIDTH-1,HEIGHT-1,gfxbuf,WIDTH);
    }
    else
	if (dela==1){

		tmp=gfxbuf;
		tmp+=320*yoffset;
		tmp+=xoffset;
		WriteChunkyPixels(&temprp2,xoffset,yoffset,xoffset+viewwidth,yoffset+viewheight,tmp,WIDTH);
		tmp=gfxbuf;
		tmp+=320*160;
		WriteChunkyPixels(&temprp2,0,160,WIDTH-1,199,tmp,WIDTH);
	
		//short vww=(viewwidth+7)&~7;  
   		//tmp=gfxbuf;
    	//tmp+=modulo*yoffset+xoffset;
    	//for (y=0;y<viewheight+0;y++)
    	//{
		//WriteChunkyPixels(&temprp2,xoffset,(y*2)+0,xoffset+viewwidth,(y*2)+0,tmp,viewwidth);
			//WriteChunkyPixels(&temprp2,xoffset,(y*2)+1,xoffset+viewwidth,(y*2)+1,tmp,viewwidth);
			//WriteChunkyPixels(&temprp2,xoffset,y*2+1,xoffset+viewwidth,y*2+1,tmp,viewwidth);
	        //WritePixelLine8(&temprp2,xoffset,y,vww,tmp,&temprp);
			//tmp+=modulo;
        //}
		
    /* Changed Face Frame **/
	
	if (rf<0)
    {
     /*  	tmp=gfxbuf;
   		tmp+=modulo*164+17*4;
        for (y=164;y<34+164;y++)
     	{
			WriteChunkyPixels(&temprp2,17*4,y,17*4+(17*4+40+72)& ~7,y,tmp,(17*4+40+72));
        	//WritePixelLine8(&temprp2,17*4,y,(17*4+40+72)& ~7,tmp,&temprp);
      		tmp+=modulo;
       	}*/
    rf++;
	}
    /* Ammo changed */
    if (ra<0)
    {/*
    	tmp=gfxbuf;
        tmp+=modulo*176+27*4;
        for (y=176;y<16+176;y++)
     	{
        	//WritePixelLine8(&temprp2,27*4,y,(27*4+40+72)& ~7,tmp,&temprp);
        	tmp+=modulo;
 		} */
 	ra++;
    }       
    /* Score changed */
    if (rs<0)
    { /*
    	tmp=gfxbuf;
    	tmp+=modulo*176+16*4;
        for (y=176;y<16+176;y++)
     	{
        	//WritePixelLine8(&temprp2,16*4,y,(16*4+40+72)& ~7,tmp,&temprp);
      		tmp+=modulo;
     	} */
    	rs++;
	}
    /* Lifes changed */
     if (rl<0)
     { /*
     	tmp=gfxbuf;
    	tmp+=modulo*176+14*4;
        for (y=176;y<16+176;y++)
     	{
        	//WritePixelLine8(&temprp2,14*4,y,(14*4+40+72)& ~7,tmp,&temprp);
      		tmp+=modulo;
       	} */
    	rl++;
	}
    /* Weapon changed */
    if (rw<0)
    { /*
    	tmp=gfxbuf;
    	tmp+=modulo*164+32*4;
        for (y=164;y<34+164;y++)
     	{
        	//WritePixelLine8(&temprp2,32*4,y,(32*4+48+72)& ~7,tmp,&temprp);
      		tmp+=modulo;
       	} */
    	rw++;
    }
    /*Health changed */
    if (rh<0)
    { /*
    	tmp=gfxbuf;
    	tmp+=modulo*176+21*4;
        for (y=176;y<16+176;y++)
     	{
        	//WritePixelLine8(&temprp2,21*4,y,(21*4+40+72)& ~7,tmp,&temprp);
      		tmp+=modulo;
       	} */
    	rh++;
	}
    /* Keys changed */
    if (rk<0)
    { /*
    	tmp=gfxbuf;
    	tmp+=modulo*176+30*4;
        for (y=176;y<16+176;y++)
     	{
        	//WritePixelLine8(&temprp2,30*4,y,(30*4+16+72)& ~7,tmp,&temprp);
      		tmp+=modulo;
       	} */
    	rk++;
    }
	
}
	else
	{
    	WriteChunkyPixels(&temprp2,0,0,WIDTH-1,HEIGHT-1,gfxbuf,WIDTH);
    }
	if (ingame)
		dela=1;
 	else 
 		dela=0;
 
 /*
	if (fps)
	{
    	framee++;
    	elapse=tt2-tt1;
     	tt2=get_TimeCount();
    	if (tt1>tt2) tt1=0; 
  		if (elapse>=TickBase)
		{
  			xx=framee*TickBase/elapse;
  			tt1=tt2;
  			framee=0;
			Msg[0]=(xx)%100/10+'0';
			Msg[1]=(xx)%10+'0';
			Move(&temprp2,10,10);
			Text(&temprp2,Msg,2);			
  		}

 	}
	*/
  	oldv=viewsize;
  	vw=viewheightwin;
    
    while (!ChangeScreenBuffer(screen,sb[toggle]))
		WaitTOF();
	tBitMap();

/*	  SDL_Flip(surface);   */
}


void ThreeDRefresh()
{
	columnskip=1-columnskip;
	rowskip=1-rowskip;

	ceiling = Ceiling[gamestate.episode*10+mapon] & 0xFF;
	colc=ceiling|(ceiling<<8)|(ceiling<<16)|(ceiling<<24);


/* clear out the traced array */
	memset(spotvis, 0, sizeof(spotvis));

#ifndef DRAWCEIL	
//	ClearScreen();
#endif	

    WallRefresh(); //Cuts from 26FPS down to around 10. Without rendering it's about 15 FPS
#ifdef DRAWCEIL
    DrawPlanes();  /* silly floor/ceiling drawing */
#endif

/* draw all the scaled images */
   DrawScaleds();   /* draw scaled stuff */
   DrawPlayerWeapon();  /* draw player's hands */

/* show screen and time last cycle */	

	VW_UpdateScreen2();
	frameon++;
}

/* ======================================================================== */
/*unsigned short __asm_swap(__reg("d0") unsigned int)=
              //  "\tclr.w\td0\n"
                "\tswap\td0\n"
#define fswap(x) __asm_swap(x)
#inline static void extern ScaledDraw(__reg ("a3")byte *gfx, __reg ("d2") byte count, __reg ("a2") byte *vid, __reg("d3") unsigned int frac, __reg ("d4")unsigned int delta);
*/

static void ScaledDraw(const byte *gfx, byte count, byte *vid, unsigned int frac,const unsigned int delta)
{
	//long *vid2 = vid;

	while (count--) {
		//*vid2 = gfx[frac>>16]<<24 | (gfx[frac>>16]+offsets[0])<<16 | (gfx[frac>>16]+offsets[1])<<8 | (gfx[frac>>16]+offsets[2]);
		*vid=gfx[frac>>16];
		*(vid+1)=gfx[(frac>>16) + offsets[0]];
		*(vid+2)=gfx[(frac>>16) + offsets[1]];
		*(vid+3)=gfx[(frac>>16) + offsets[2]];
		vid += 320;
		//vid2+=80;
		frac += delta;			
	}
} 

/*
__inline void __asm ScaledDraw (register __a3 const byte *gfx,
		       register __d2 byte count,
		       register __a2 byte *vid,
		       register __d3 unsigned int frac,
		       register __d4 const unsigned int delta);
*/	 

__inline void __asm ScaledDrawTrans (register __a3 const byte *gfx,
		       register __d2 byte count,
		       register __a2 byte *vid,
		       register __d3 unsigned int frac,
		       register __d4 const unsigned int delta);

    /* 
	static void ScaledDrawTrans(byte *gfx, byte count, byte *vid, unsigned int frac, unsigned int delta)
{
	while (count--) {
		//if (gfx[frac>>16] != 255)
		//{
			*vid = gfx[frac>>16];
		//}
		vid += 320;
		frac += delta;
	}
}
*/

static void ScaleLine(const unsigned int height,const byte *source,const int x)
{

	unsigned int y,halfheight,delta;
	unsigned long *clr;
	UBYTE *endy;	
		
	if (height) {

		
		if (height < viewheight) {
			
			
			//Draw the floor/ceiling, only if you've gone further away since the last frame (or there's a sprite infront of it)
			if(oldwallheight[x]==0 || oldwallheight[x]>wallheight[x])
			{
				halfheight = (viewheight-height)>>1;
				delta=((halfheight+height)*320)>>2;
				clr=gfxbuf + (yoffset * 320) + x + xoffset;
				for(y=0;y<halfheight;y++,clr+=80){
					*clr=colc;
					*(clr+delta)=colf;
				}	
			}
			
			y = yoffset + ((viewheight - height) >>1);
			
			ScaledDraw(source, height, gfxbuf + (y * 320) + x + xoffset, 
			deltah[(short)height], frach[(short)height],0,0,0);

			oldwallheight[x] = wallheight[x];
			
			return;	
		} 
		
		oldwallheight[x] = wallheight[x];
		
		y = (height - viewheight) >>1;
		y *= frach[(short)height];

		ScaledDraw(source, viewheight, gfxbuf + yoffset320 + x + xoffset, 
		y+deltah[(short)height], frach[(short)height],0,0,0);
	}
}

static void ScaleLineTrans(unsigned int height, byte *source, int x,unsigned int adjustedHeight, unsigned int adjustedOffset)
{
	unsigned int y, frac, delta;
	oldwallheight[(x>>2)<<2]=0;	
	
	if (height) {		
		if (height < viewheight) {

			y = yoffset + ((viewheight - height) >>1);
			ScaledDrawTrans(source, adjustedHeight, gfxbuf + ((y+adjustedOffset) * 320) + x + xoffset, 
			deltah[(short)height], frach[(short)height]);
			return;	
		} 
		
		return; //Todo - fix full screen view
		
		y = (height - viewheight) >>1;
		y *= frach[(short)height];
		
		ScaledDrawTrans(&source[y >> 24], viewheight, gfxbuf + yoffset320 + x + xoffset, 
		y+deltah[(short)height], frach[(short)height]);
	}
}


static void DeCompileSprite(short shapenum)
{
	byte *ptr;
	byte *buf;
	byte *cmdptr;
	byte *pixels;
	byte minX=63,maxX=0,minY=63,maxY=0;
	short yoff;
	short y, y0, y1;
	short x, left, right;
	short cmd;
	byte pixelGap;
	
	MM_GetPtr((void *)&buf, 64 * 64);
	
	memset(buf, 255, 64 * 64);
	
	ptr = PM_GetSpritePage(shapenum);

	/* left = ptr[0] | (ptr[1] << 8); */
	left = ptr[0];
	/* right = ptr[2] | (ptr[3] << 8); */
	right = ptr[2];
	
	cmdptr = &ptr[4];
	
	for (x = left; x <= right; x++) {
	

		cmd = cmdptr[0] | (cmdptr[1] << 8);
		cmdptr += 2;
					
		/* while (ptr[cmd+0] | (ptr[cmd+1] << 8)) { */
		while (ptr[cmd+0]) {
			/* y1 = (ptr[cmd+0] | (ptr[cmd+1] << 8)) / 2; */
			y1 = ptr[cmd+0] >>1;
			yoff = (int16_t)(ptr[cmd+2] | (ptr[cmd+3] << 8));
			/* y0 = (ptr[cmd+4] | (ptr[cmd+5] << 8)) / 2; */
			y0 = ptr[cmd+4] >>1;
			
			pixels = &ptr[y0 + yoff];
			
			pixelGap = 0;
			for (y = y0; y < y1; y++) {
			
				//Is this a non-transparent pixel?
				if(*pixels!=255)
				{
					//The first non transparent pixel
					if(x<minX)
						minX = x;
					if(y<minY)
						minY = y;
					
					//The last non transparent pixel?
					if(x>maxX)
						maxX = x;
					if(y>maxY && pixelGap==0)
						maxY = y;
				}
				else
				{
					pixelGap=1;
				}
			
				*(buf + (x<<6) + y) = *pixels;
				pixels++;
			}
			
			cmd += 6;
		}
	}
	
	spritegfx[shapenum] = buf;
	spriteMinX[shapenum] = minX;
	spriteMaxX[shapenum] = maxX;
	spriteMinY[shapenum] = minY;
	spriteMaxY[shapenum] = 20;//maxY;
	
	//Erik hack, free the page to stop sprites doubling up in memory?
	PM_FreePage(PMSpriteStart + shapenum);
	
	
}

void ScaleShape(short xcenter, short shapenum, unsigned short height)
{
	byte *shape;
	unsigned int scaler =frach[((short)height)>>2];//*2;
	unsigned int x, p, minX, maxX, minY, maxY, y,adjustedHeight,adjustedOffset=0;
	
	unsigned short h = height >> 3;
	
	//if(xcenter%2==1)
		//xcenter-=1;
	

		
	if (spritegfx[shapenum] == NULL)
		DeCompileSprite(shapenum);
		
	shape = spritegfx[shapenum];
	
	//Adjust the center
	minX = ((int)spriteMinX[shapenum]<<16);
	minY = ((int)spriteMinY[shapenum]<<16);
	maxX = ((int)spriteMaxX[shapenum]+1<<16);
	maxY = ((int)spriteMaxY[shapenum]<<16);
	p = xcenter - h;
	for(x=0;x<minX;x+=scaler,p++);
		
	//Adjust the top (TODO - Fix massive gap in lamps etc)
	{
		for(y=0,adjustedHeight=height>>2;y<minY;y+=scaler,adjustedHeight--,adjustedOffset++);
	}
	
	for (x = minX; x < maxX; x += scaler, p++) {
		if ((p < 0) || (p >= viewwidth) || (wallheight[p] >= height))
			continue;
		ScaleLineTrans(height>>2, shape + ((x >> 16) << 6) + (y>>16), p, adjustedHeight, adjustedOffset);
	}	
}

void SimpleScaleShape(const short xcenter,const short shapenum,const unsigned short height)
{
	byte *shape;
	const unsigned int scaler = frach[height];
	unsigned int x, y, frac, p, minX, maxX, minY, maxY, temp;
	int height2, starty, newheight;
	byte *gfx, *startpos, *vid, pixel;

	if(!height) return;
	
	if (spritegfx[shapenum] == NULL)
		DeCompileSprite(shapenum);
	
	shape = spritegfx[shapenum];
	minX = ((int)spriteMinX[shapenum]<<16);
	minY = ((int)spriteMinY[shapenum]<<16);
	maxX = ((int)spriteMaxX[shapenum]+1<<16);
	
	//Flattened draw sprite loop
	p = xcenter-(height>>1);
	//Min X Offset
	for(x=0;x<minX;x+=scaler,p++);
	
	//Min Y offset
	newheight = height;
	y = yoffset + ((viewheight - height) >>1);	
	startpos = gfxbuf + (y*320) + xoffset;
	starty = deltah[height];
	for(y=0;y<minY;y+=scaler,newheight--,startpos+=320,starty+=scaler);

	height2 = newheight;
	frac = starty;
	vid = startpos;	
	vid += p;
	gfx = shape + ((x >> 16) << 6);	
	
	for(;;){
	
		if(height2<2)
		{
			x+= scaler+scaler;
			//Done
			if(!(x<maxX)) return;		
			p+=2;
			oldwallheight[(p>>2)<<2]=0;				
			height2 = newheight;
			vid = (startpos + p);
			gfx = shape + ((x >> 16) << 6);
			frac = starty;
		}
		
		pixel = gfx[frac>>16];
		if (pixel != 255)
		{
			*vid = pixel;
			*(vid+1) = pixel;
			*(vid+320) = pixel;
			*(vid+321) = pixel;			
		}
		vid += 640;
		frac += scaler+scaler;
		height2-=2;
	}
}
			
	
/*
	while (count--) {
		if (gfx[frac>>16] != 255)
		{
			*vid = *(vid+1) = gfx[frac>>16];
		}
		vid += 320;
		frac += delta;
	}
*/
	
	
//	for (p = xcenter - (height >> 1), x = 0; x < (64 << 16); x += scaler, p++) {
		
		//Erik - is this check even required? AFAIK no simple shapes are drawn outside the view window		
		//if ((p < 0) || (p >= viewwidth))// || (x%2==columnskip))
		//	continue;
		
//		if (p%2!=columnskip)
	//		continue;
		
	//	ScaleLineTrans(height, shape + ((x >> 16) << 6), p);
		//shape+=delta;
		
	//}

/* ======================================================================== */

/*
====================
=
= CalcHeight
=
= Calculates the height of xintercept,yintercept from viewx,viewy
=
====================
*/
static fixed gxt,gyt,nx,gx,gy;
static int CalcHeight()
{
	

	gx = xintercept - viewx;
	gxt = FixedByFrac(gx, viewcos);

	gy = yintercept - viewy;
	gyt = FixedByFrac(gy, viewsin);

	nx = gxt-gyt;

  //
  // calculate perspective ratio (heightnumerator/(nx>>8))
  //
	if (nx < MINDIST)
		nx = MINDIST;			/* don't let divide overflow */

	return heightnumerator/(nx>>8);
}

static void ScalePost(const byte *wall,const int texture)
{
	int delta;
	int height, xposition, col1, col2, col3;
	byte *source;
	
	height = (wallheight[postx] & 0xFFF8) >> 2;
	delta = frach[height];
	source = wall+texture;
	
	xposition = texture>>6;
	col1=((delta)>>16);
	col2=((delta+delta)>>16);
	col3=((delta+delta+delta)>>16);
	
		if(xposition+col1>63) offsets[0] = 0; else offsets[0] = col1<<6;
		if(xposition+col2>63) offsets[1] = 0; else offsets[1] = col2<<6;
		if(xposition+col3>63) offsets[2] = 0; else offsets[2] = col3<<6;	
	
	ScaleLine(height, source, postx);
}

static void HitHorizDoor()
{
	unsigned texture, doorpage = 0, doornum;
	byte *wall;

	doornum = tilehit&0x7f;
	texture = ((xintercept-doorposition[doornum]) >> 4) & 0xfc0;
	wallheight[postx] = wallheight[postx+1] = wallheight[postx+2] = wallheight[postx+3] = CalcHeight();

	switch(doorobjlist[doornum].lock) {
		case dr_normal:
			doorpage = DOORWALL;
			break;
		case dr_lock1:
		case dr_lock2:
		case dr_lock3:
		case dr_lock4:
			doorpage = DOORWALL+6;
			break;
		case dr_elevator:
			doorpage = DOORWALL+4;
			break;
	}

	wall = PM_GetPage(doorpage);
	ScalePost(wall, texture);
}

static void HitVertDoor()
{
	unsigned texture, doorpage = 0, doornum;
	byte *wall;

	doornum = tilehit&0x7f;
	texture = ((yintercept-doorposition[doornum]) >> 4) & 0xfc0;

	wallheight[postx] = wallheight[postx+1] = wallheight[postx+2] = wallheight[postx+3] = CalcHeight();

	switch(doorobjlist[doornum].lock) {
		case dr_normal:
			doorpage = DOORWALL;
			break;
		case dr_lock1:
		case dr_lock2:
		case dr_lock3:
		case dr_lock4:
			doorpage = DOORWALL+6;
			break;
		case dr_elevator:
			doorpage = DOORWALL+4;
			break;
	}

	wall = PM_GetPage(doorpage+1);
	ScalePost(wall, texture);
}

static void HitVertWall()
{
	short wallpic;
	unsigned texture;
	byte *wall;

	texture = (yintercept>>4)&0xfc0;
	
	if (xtilestep == -1) {
		texture = 0xfc0-texture;
		xintercept += TILEGLOBAL;
	}
	
	wallheight[postx] = wallheight[postx+1] = wallheight[postx+2] = wallheight[postx+3] = CalcHeight();

	if (tilehit & 0x40) { // check for adjacent doors
		ytile = yintercept>>TILESHIFT;
		if (tilemap[xtile-xtilestep][ytile] & 0x80)
			wallpic = DOORWALL+3;
		else
			wallpic = vertwall[tilehit & ~0x40];
	} else
		wallpic = vertwall[tilehit];
		
	wall = PM_GetPage(wallpic);
	ScalePost(wall, texture);
}

static void HitHorizWall()
{
	short wallpic;
	unsigned texture;
	byte *wall;

	texture = (xintercept >> 4) & 0xfc0;
	
	if (ytilestep == -1)
		yintercept += TILEGLOBAL;
	else
		texture = 0xfc0 - texture;
		
	wallheight[postx] = wallheight[postx+1] = wallheight[postx+2] = wallheight[postx+3] = CalcHeight();

	if (tilehit & 0x40) { // check for adjacent doors
		xtile = xintercept>>TILESHIFT;
		if (tilemap[xtile][ytile-ytilestep] & 0x80)
			wallpic = DOORWALL+2;
		else
			wallpic = horizwall[tilehit & ~0x40];
	} else
		wallpic = horizwall[tilehit];

  	  wall = PM_GetPage(wallpic);
  	  ScalePost(wall, texture,1);
}

static void HitHorizPWall()
{
	short wallpic;
	unsigned texture, offset;
	byte *wall;
	
	texture = (xintercept >> 4) & 0xfc0;
	
	offset = pwallpos << 10;
	
	if (ytilestep == -1)
		yintercept += TILEGLOBAL-offset;
	else {
		texture = 0xfc0-texture;
		yintercept += offset;
	}

	wallheight[postx] = wallheight[postx+1] = wallheight[postx+2] = wallheight[postx+3] = CalcHeight();

	wallpic = horizwall[tilehit&63];
	wall = PM_GetPage(wallpic);
	ScalePost(wall, texture);
}

static void HitVertPWall()
{
	short wallpic;
	unsigned texture, offset;
	byte *wall;
	
	texture = (yintercept >> 4) & 0xfc0;
	offset = pwallpos << 10;
	
	if (xtilestep == -1) {
		xintercept += TILEGLOBAL-offset;
		texture = 0xfc0-texture;
	} else
		xintercept += offset;

	wallheight[postx] = wallheight[postx+1] = wallheight[postx+2] = wallheight[postx+3] = CalcHeight();
	
	wallpic = vertwall[tilehit&63];

	wall = PM_GetPage(wallpic);
	ScalePost(wall, texture);
}

#define DEG90	900
#define DEG180	1800
#define DEG270	2700
#define DEG360	3600

static int samex(int intercept, int tile)
{
	if (xtilestep > 0) {
		if ((intercept>>TILESHIFT) >= tile)
			return 0;
		else
			return 1;
	} else {
		if ((intercept>>TILESHIFT) <= tile)
			return 0;
		else
			return 1;
	}
}

static int samey(int intercept, int tile)
{
	if (ytilestep > 0) {
		if ((intercept>>TILESHIFT) >= tile)
			return 0;
		else
			return 1;
	} else {
		if ((intercept>>TILESHIFT) <= tile)
			return 0;
		else
			return 1;
	}
}
static	unsigned xpartialup, xpartialdown, ypartialup, ypartialdown;
static	unsigned xpartial, ypartial;
static	int doorhit;
static	int angle;    /* ray angle through postx */
static	int midangle;
static	int focaltx, focalty;
static	int xstep, ystep;
static void AsmRefresh()
{
//	unsigned int columns = viewwidth / 4;
//	unsigned int startx=0;
//	unsigned int endx=columns;
//	unsigned int i;

	midangle = viewangle*(FINEANGLES/ANGLES);
	xpartialdown = (viewx&(TILEGLOBAL-1));
	xpartialup = TILEGLOBAL-xpartialdown;
	ypartialdown = (viewy&(TILEGLOBAL-1));
	ypartialup = TILEGLOBAL-ypartialdown;

	focaltx = viewx>>TILESHIFT;
	focalty = viewy>>TILESHIFT;

for (postx = 0; postx < viewwidth-1;postx+=4) {
	angle = midangle + pixelangle[postx];

	if (angle < 0) {
		/* -90 - -1 degree arc */
		angle += FINEANGLES;
		goto entry360;
	} else if (angle < DEG90) {
		/* 0-89 degree arc */
	entry90:
		xtilestep = 1;
		ytilestep = -1;
		xstep = finetangent[DEG90-1-angle];
		ystep = -finetangent[angle];
		xpartial = xpartialup;
		ypartial = ypartialdown;
	} else if (angle < DEG180) {
		/* 90-179 degree arc */
		xtilestep = -1;
		ytilestep = -1;
		xstep = -finetangent[angle-DEG90];
		ystep = -finetangent[DEG180-1-angle];
		xpartial = xpartialdown;
		ypartial = ypartialdown;
	} else if (angle < DEG270) {
		/* 180-269 degree arc */
		xtilestep = -1;
		ytilestep = 1;
		xstep = -finetangent[DEG270-1-angle];
		ystep = finetangent[angle-DEG180];
		xpartial = xpartialdown;
		ypartial = ypartialup;
	} else if (angle < DEG360) {
		/* 270-359 degree arc */
	entry360:
		xtilestep = 1;
		ytilestep = 1;
		xstep = finetangent[angle-DEG270];
		ystep = finetangent[DEG360-1-angle];
		xpartial = xpartialup;
		ypartial = ypartialup;
	} else {
		angle -= FINEANGLES;
		goto entry90;
	}
	
	yintercept = viewy + FixedByFrac(xpartial, ystep); // + xtilestep;
	xtile = focaltx + xtilestep;

	xintercept = viewx + FixedByFrac(ypartial, xstep); // + ytilestep;
	ytile = focalty + ytilestep;

/* CORE LOOP */

#define TILE(n) ((n)>>TILESHIFT)

	/* check intersections with vertical walls */
vertcheck:
	if (!samey(yintercept, ytile))
		goto horizentry;
		
vertentry:
	tilehit = tilemap[xtile][TILE(yintercept)];
	/* printf("vert: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n", postx, tilehit, xtile, ytile, xintercept, yintercept, xpartialup, xpartialdown, ypartialup, ypartialdown, xpartial, ypartial, doorhit, angle, midangle, focaltx, focalty, xstep, ystep); */
	
	if (tilehit) {
		if (tilehit & 0x80) {
			if (tilehit & 0x40) {
				/* vertpushwall */
				doorhit = yintercept + (signed)((signed)pwallpos * ystep) / 64;
			
				if (TILE(doorhit) != TILE(yintercept)) 
					goto passvert;
					
				yintercept = doorhit;
				xintercept = xtile << TILESHIFT;
				HitVertPWall();
			} else {
				/* vertdoor */
				doorhit = yintercept + (ystep >>1);

				if (TILE(doorhit) != TILE(yintercept))
					goto passvert;
				
				/* check door position */
				if ((doorhit&0xFFFF) < doorposition[tilehit&0x7f])
					goto passvert;
				
				yintercept = doorhit;
				xintercept = (xtile << TILESHIFT) + TILEGLOBAL/2;
				HitVertDoor();
			}
		} else {
			xintercept = xtile << TILESHIFT;
			HitVertWall();
		}
		continue;
	}
passvert:
	spotvis[xtile][TILE(yintercept)] = 1;
	xtile += xtilestep;
	yintercept += ystep;
	goto vertcheck;
	
horizcheck:
	/* check intersections with horizontal walls */
	
	if (!samex(xintercept, xtile))
		goto vertentry;

horizentry:
	tilehit = tilemap[TILE(xintercept)][ytile];
	/* printf("horz: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n", postx, tilehit, xtile, ytile, xintercept, yintercept, xpartialup, xpartialdown, ypartialup, ypartialdown, xpartial, ypartial, doorhit, angle, midangle, focaltx, focalty, xstep, ystep); */
	
	if (tilehit) {
		if (tilehit & 0x80) {
			if (tilehit & 0x40) {
				doorhit = xintercept + (signed)((signed)pwallpos * xstep) / 64;
		    	
				/* horizpushwall */
				if (TILE(doorhit) != TILE(xintercept))
					goto passhoriz;
				
				xintercept = doorhit;
				yintercept = ytile << TILESHIFT; 
				HitHorizPWall();
			} else {
				doorhit = xintercept + (xstep >> 1);
				
				if (TILE(doorhit) != TILE(xintercept))
					goto passhoriz;
				
				/* check door position */
				if ((doorhit&0xFFFF) < doorposition[tilehit&0x7f])
					goto passhoriz;
				
				xintercept = doorhit;
				yintercept = (ytile << TILESHIFT) + (TILEGLOBAL>>1);
				HitHorizDoor();
			}
		} else {
			yintercept = ytile << TILESHIFT;
			HitHorizWall();
		}
		continue;
	}
passhoriz:
	spotvis[TILE(xintercept)][ytile] = 1;
	ytile += ytilestep;
	xintercept += xstep;
	goto horizcheck;
}
}

/* ======================================================================== */

void FizzleFade(boolean abortable, int frames, byte color)
{
}

#if 0
static int xarr[1280];
static int yarr[1280];

static int myrand()
{
	return rand();
}

static void fillarray(int *arr, int len)
{
	int i;
	
	for (i = 0; i < len; i++)
		arr[i] = i;
}

static void randarray(int *arr, int len)
{
	int i, j, k;
	
	for (i = 0; i < len; i++) {
		j = myrand() % len;
		
		k = arr[i];
		arr[i] = arr[j];
		arr[j] = k;
	}
}
			
void FizzleFade(boolean abortable, int frames, byte color)
{
	boolean retr;
	int pixperframe;
	int x, y, xc, yc;
	int count, p, frame;
		
	count = viewwidth * viewheight;
	pixperframe = count / frames;
	
	srand(time(NULL));
		
	fillarray(xarr, viewwidth);
	randarray(xarr, viewwidth);
	
	fillarray(yarr, viewheight - 1);
	randarray(yarr, viewheight - 1);

	IN_StartAck();

	frame = 0;
	set_TimeCount(0);
	
	xc = 0;
	yc = 0;
	x = 0;
	y = 0;
	
	retr = false;
	do {
		if (abortable && IN_CheckAck())
			retr = true;
		else
		for (p = 0; p < pixperframe; p++) {
				
			gfxbuf[(xarr[x]+xoffset)+(yarr[y]+yoffset)*320] = color;
			
			count--;
			
			x++;
			if (x >= viewwidth)
				x = 0;
			
			y++;
			if (y >= (viewheight-1))
				y = 0;
			
			yc++;	
			if (yc >= (viewheight-1)) {
				yc = 0;
				y++;
				
				if (y >= (viewheight-1))
					y = 0;
			}
		}
		
		VW_UpdateScreen();
		
		frame++;
		while (get_TimeCount() < frame);
	} while (!retr && (count > 0));
	
	VW_UpdateScreen();
}
#endif
