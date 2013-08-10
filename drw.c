/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include "drw.h"
#include "util.h"

Drw *
drw_create(Display *dpy, int screen, Window root, unsigned int w, unsigned int h) {
	Drw *drw = (Drw *)calloc(1, sizeof(Drw));
	if(!drw)
		return NULL;
	drw->dpy = dpy;
	drw->screen = screen;
	drw->root = root;
	drw->w = w;
	drw->h = h;
	drw->drawable = XCreatePixmap(dpy, root, w, h, DefaultDepth(dpy, screen));
	drw->gc = XCreateGC(dpy, root, 0, NULL);
	XSetLineAttributes(dpy, drw->gc, 1, LineSolid, CapButt, JoinMiter);
	return drw;
}

void
drw_resize(Drw *drw, unsigned int w, unsigned int h) {
	if(!drw)
		return;
	drw->w = w;
	drw->h = h;
	if(drw->drawable != 0)
		XFreePixmap(drw->dpy, drw->drawable);
	drw->drawable = XCreatePixmap(drw->dpy, drw->root, w, h, DefaultDepth(drw->dpy, drw->screen));
}

void
drw_free(Drw *drw) {
	XFreePixmap(drw->dpy, drw->drawable);
	XFreeGC(drw->dpy, drw->gc);
	free(drw);
}

Fnt *
drw_font_create(Drw *drw, const char *fontname) {
	Fnt *font;

	font = (Fnt *)calloc(1, sizeof(Fnt));
	if(!font)
		return NULL;

	if(!(font->xfont = XftFontOpenName(drw->dpy, drw->screen, fontname))
	&& !(font->xfont = XftFontOpenName(drw->dpy, drw->screen,"fixed")))
		die("error, cannot load font: '%s'\n", fontname);

	font->ascent = font->xfont->ascent;
	font->descent = font->xfont->descent;

	font->h = font->ascent + font->descent;
	return font;
}

void
drw_font_free(Display *dpy, Fnt *font) {
	if(!font)
		return;
	free(font);
}

XftColor *
drw_clr_create(Drw *drw, const char *clrname) {
	XftColor *color;

	if(!drw)
		return NULL;

    color = (XftColor *)calloc(1, sizeof(XftColor));

	if(!XftColorAllocName(drw->dpy, DefaultVisual(drw->dpy, drw->screen),
                          DefaultColormap(drw->dpy, drw->screen),
                          clrname, color))
		die("error, cannot allocate color '%s'\n", clrname);
	return color;
}

void
drw_clr_free(Drw *drw, XftColor *clr) {
	if(clr)
	    XftColorFree(drw->dpy, DefaultVisual(drw->dpy, drw->screen),
                     DefaultColormap(drw->dpy, drw->screen), clr);
}

void
drw_setfont(Drw *drw, Fnt *font) {
	if(drw)
		drw->font = font;
}

void
drw_setscheme(Drw *drw, ClrScheme *scheme) {
	if(drw && scheme) 
		drw->scheme = scheme;
}

void
drw_rect(Drw *drw, int x, int y, unsigned int w, unsigned int h, int filled, int empty, int invert) {
	int dx;

	if(!drw || !drw->font || !drw->scheme)
		return;
	XSetForeground(drw->dpy, drw->gc, invert ? drw->scheme->bg->pixel
                                             : drw->scheme->fg->pixel);
	dx = (drw->font->ascent + drw->font->descent + 2) / 4;
	if(filled)
		XFillRectangle(drw->dpy, drw->drawable, drw->gc, x+1, y+1, dx+1, dx+1);
	else if(empty)
		XDrawRectangle(drw->dpy, drw->drawable, drw->gc, x+1, y+1, dx, dx);
}

void
drw_text(Drw *drw, int x, int y, unsigned int w, unsigned int h, const char *text, int invert) {
	char buf[256];
	int i, tx, ty, th, len, olen;
	Extnts tex;
	XftDraw *d;

	if(!drw || !drw->scheme)
		return;
	XSetForeground(drw->dpy, drw->gc, invert ? drw->scheme->fg->pixel
                                             : drw->scheme->bg->pixel);
	XFillRectangle(drw->dpy, drw->drawable, drw->gc, x, y, w, h);
	if(!text || !drw->font)
		return;
	olen = strlen(text);
	drw_font_getexts(drw, text, olen, &tex);
	th = drw->font->ascent + drw->font->descent;
	ty = y + (h / 2) - (th / 2) + drw->font->ascent;
	tx = x + (h / 2);
	/* shorten text if necessary */
	for(len = MIN(olen, sizeof buf); len && (tex.w > w - tex.h || w < tex.h); len--)
		drw_font_getexts(drw, text, len, &tex);
	if(!len)
		return;
	memcpy(buf, text, len);
	if(len < olen)
		for(i = len; i && i > len - 3; buf[--i] = '.');

	d = XftDrawCreate(drw->dpy, drw->drawable,
                      DefaultVisual(drw->dpy, drw->screen),
                      DefaultColormap(drw->dpy, drw->screen));
	XftDrawStringUtf8(d, invert ? drw->scheme->bg : drw->scheme->fg,
                      drw->font->xfont, tx, ty, (XftChar8 *) buf, len);
	XftDrawDestroy(d);
}

void
drawline(Drw *drw, int x, int y) {
	XGCValues gcv;

	gcv.foreground = drw->scheme->fg->pixel;
	XChangeGC(drw->dpy, drw->gc, GCForeground, &gcv);
	XDrawLine(drw->dpy, drw->drawable, drw->gc, x, y, x, y + (drw->font->ascent + drw->font->descent + 2));
}

void
drw_map(Drw *drw, Window win, int x, int y, unsigned int w, unsigned int h) {
	if(!drw)
		return;
	XCopyArea(drw->dpy, drw->drawable, win, drw->gc, x, y, w, h, x, y);
	XSync(drw->dpy, False);
}


void
drw_font_getexts(Drw *drw, const char *text, unsigned int len, Extnts *tex) {
    XGlyphInfo ext;

	if(!drw->font || !text)
		return;
    tex->h = drw->font->ascent + drw->font->descent;
    XftTextExtentsUtf8(drw->dpy, drw->font->xfont, (FcChar8 *)text, len, &ext);
    tex->w = ext.xOff;
}

unsigned int
drw_font_getexts_width(Drw *drw, const char *text, unsigned int len) {
	Extnts tex;

	if(!drw->font)
		return -1;
	drw_font_getexts(drw, text, len, &tex);
	return tex.w;
}

Cur *
drw_cur_create(Drw *drw, int shape) {
	Cur *cur = (Cur *)calloc(1, sizeof(Cur));

	if(!drw || !cur)
		return NULL;
	cur->cursor = XCreateFontCursor(drw->dpy, shape);
	return cur;
}

void
drw_cur_free(Drw *drw, Cur *cursor) {
	if(!drw || !cursor)
		return;
	XFreeCursor(drw->dpy, cursor->cursor);
	free(cursor);
}
