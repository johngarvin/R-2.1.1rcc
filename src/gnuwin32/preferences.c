/*
 *  R : A Computer Language for Statistical Data Analysis
 *  file preferences.c
 *  Copyright (C) 2000  Guido Masarotto and Brian Ripley
 *                2004-5  R Core Development Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "win-nls.h"

#ifdef Win32
#define USE_MDI 1
#endif

#include <windows.h>
#include <ctype.h>  /* isspace */
#include "graphapp/ga.h"
#include "graphapp/graphapp.h"
#include "console.h"
#include "consolestructs.h"
#include "rui.h"

extern char fontname[LF_FACESIZE+1]; /* from console.c */
extern int consolex, consoley; /* from console.c */
extern int pagerMultiple, haveusedapager; /* from pager.c */
void editorsetfont(font f);

/*                configuration editor                        */


/* current state */

struct structGUI
{
    int MDI;
    int toolbar;
    int statusbar;
    int pagerMultiple;
    char font[50];
    int tt_font;
    int pointsize;
    char style[20];
    int crows, ccols, cx, cy, setWidthOnResize, prows, pcols,
	cbb, cbl, grx, gry;
    rgb bg, fg, user, hlt;
};
typedef struct structGUI *Gui;
static struct structGUI curGUI, newGUI;




extern char *ColorName[]; /* from graphapp/rgb.c */

static int cmatch(char *col, char **list)
{
    int i=0;
    char **pos = list;
    while(*pos != NULL) {
	if(strcmpi(*pos, col) == 0) return(i);
	i++; pos++;
    }
    return(-1);
}


static char *StyleList[] = {"normal", "bold", "italic", NULL};
static char *PointsList[] = {"6", "7", "8", "9", "10", "11", "12", "14", "16", "18", NULL};
static char *FontsList[] = {"Courier", "Courier New", "FixedSys", "FixedFont", "Lucida Console", "Terminal", "BatangChe", "DotumChe", "GulimChe", "MingLiU", "MS Gothic", "MS Mincho", "NSimSun", NULL};


static window wconfig;
static button bApply, bSave, bOK, bCancel;
static label l_mdi, l_mwin, l_font, l_point, l_style, l_crows, l_ccols,
    l_cx, l_cy, l_prows, l_pcols, l_grx, l_gry,
    l_cols, l_bgcol, l_fgcol, l_usercol, l_highlightcol, l_cbb, l_cbl;
static radiobutton rb_mdi, rb_sdi, rb_mwin, rb_swin;
static listbox f_font, f_style, d_point, bgcol, fgcol, usercol, highlightcol;
static checkbox toolbar, statusbar, tt_font, c_resize;
static field f_crows, f_ccols, f_prows, f_pcols, f_cx, f_cy, f_cbb,f_cbl,
    f_grx, f_gry;


static void getGUIstate(Gui p)
{
    p->MDI = ischecked(rb_mdi);
    p->toolbar = ischecked(toolbar);
    p->statusbar = ischecked(statusbar);
    p->pagerMultiple = ischecked(rb_mwin);
    strcpy(p->font, gettext(f_font));
    p->tt_font = ischecked(tt_font);
    p->pointsize = atoi(gettext(d_point));
    strcpy(p->style, gettext(f_style));
    p->crows = atoi(gettext(f_crows));
    p->ccols = atoi(gettext(f_ccols));
    p->cx = atoi(gettext(f_cx));
    p->cy = atoi(gettext(f_cy));
    p->setWidthOnResize = ischecked(c_resize);
    p->cbb = atoi(gettext(f_cbb));
    p->cbl = atoi(gettext(f_cbl));
    p->prows = atoi(gettext(f_prows));
    p->pcols = atoi(gettext(f_pcols));
    p->grx = atoi(gettext(f_grx));
    p->gry = atoi(gettext(f_gry));
    p->bg = nametorgb(gettext(bgcol));
    p->fg = nametorgb(gettext(fgcol));
    p->user = nametorgb(gettext(usercol));
    p->hlt = nametorgb(gettext(highlightcol));
}


static int has_changed()
{
    Gui a=&curGUI, b=&newGUI;
    return a->MDI != b->MDI ||
	a->toolbar != b->toolbar ||
	a->statusbar != b->statusbar ||
	a->pagerMultiple != b->pagerMultiple ||
	strcmp(a->font, b->font) ||
	a->tt_font != b->tt_font ||
	a->pointsize != b->pointsize ||
	strcmp(a->style, b->style) ||
	a->crows != b->crows ||
	a->ccols != b->ccols ||
	a->cx != b->cx ||
	a->cy != b->cy ||
	a->cbb != b->cbb ||
	a->cbl != b->cbl ||
	a->setWidthOnResize != b->setWidthOnResize ||
	a->prows != b->prows ||
	a->pcols != b->pcols ||
	a->grx != b->grx ||
	a->gry != b->gry ||
	a->bg != b->bg ||
	a->fg != b->fg ||
	a->user != b->user ||
	a->hlt != b->hlt;
}


static void cleanup()
{
    hide(wconfig);
    delobj(l_mdi); delobj(rb_mdi); delobj(rb_sdi);
    delobj(toolbar); delobj(statusbar);
    delobj(l_mwin); delobj(rb_mwin); delobj(rb_swin);
    delobj(l_font); delobj(f_font); delobj(tt_font);
    delobj(l_point); delobj(d_point);
    delobj(l_style); delobj(f_style);
    delobj(l_crows); delobj(f_crows); delobj(l_ccols); delobj(f_ccols);
    delobj(c_resize);
    delobj(l_cx); delobj(f_cx); delobj(l_cy); delobj(f_cy);
    delobj(l_cbb); delobj(f_cbb); delobj(l_cbl); delobj(f_cbl);
    delobj(l_prows); delobj(f_prows); delobj(l_pcols); delobj(f_pcols);
    delobj(l_grx); delobj(f_grx); delobj(l_gry); delobj(f_gry);
    delobj(l_cols);
    delobj(l_bgcol); delobj(bgcol);
    delobj(l_fgcol); delobj(fgcol);
    delobj(l_usercol); delobj(usercol);
    delobj(l_highlightcol); delobj(highlightcol);
    delobj(bApply); delobj(bSave); delobj(bOK); delobj(bCancel);
    delobj(wconfig);
}


static void do_apply()
{
    rect r = getrect(RConsole);
    ConsoleData p = (ConsoleData) getdata(RConsole);
    int havenewfont = 0;

    getGUIstate(&newGUI);
    if(!has_changed()) return;

    if(newGUI.MDI != curGUI.MDI || newGUI.toolbar != curGUI.toolbar ||
       newGUI.statusbar != curGUI.statusbar)
	askok(G_("The overall console properties cannot be changed\non a running console.\n\nSave the preferences and restart Rgui to apply them.\n"));


/*  Set a new font? */
    if(strcmp(newGUI.font, curGUI.font) ||
       newGUI.pointsize != curGUI.pointsize ||
       strcmp(newGUI.style, curGUI.style))
    {
	char msg[LF_FACESIZE + 128];
	int sty = Plain;

	if(newGUI.tt_font) strcpy(fontname, "TT "); else strcpy(fontname, "");
	strcat(fontname,  newGUI.font);
	if (!strcmp(newGUI.style, "bold")) sty = Bold;
	if (!strcmp(newGUI.style, "italic")) sty = Italic;
	pointsize = newGUI.pointsize;
	fontsty = sty;

	/* Don't delete font: open pagers may be using it */
	if (strcmp(fontname, "FixedFont"))
	    consolefn = gnewfont(NULL, fontname, fontsty, pointsize, 0.0);
	else consolefn = FixedFont;
	if (!consolefn) {
	    sprintf(msg,
		    G_("Font %s-%d-%d  not found.\nUsing system fixed font"),
		    fontname, fontsty | FixedWidth, pointsize);
	    R_ShowMessage(msg);
	    consolefn = FixedFont;
	}
	/* if (!ghasfixedwidth(consolefn)) {
	    sprintf(msg,
		    G_("Font %s-%d-%d has variable width.\nUsing system fixed font"),
		    fontname, fontsty, pointsize);
	    R_ShowMessage(msg);
	    consolefn = FixedFont;
	    } */
	p->f = consolefn;
	FH = fontheight(p->f);
	FW = fontwidth(p->f);
	havenewfont = 1;
	editorsetfont(consolefn);
    }

/* resize console, possibly with new font */
    if (consoler != newGUI.crows || consolec != newGUI.ccols || havenewfont) {
	char buf[20];
	consoler = newGUI.crows;
	consolec = newGUI.ccols;
	r.width = (consolec + 1) * FW;
	r.height = (consoler + 1) * FH;
	resize(RConsole, r);
	sprintf(buf, "%d", ROWS); settext(f_crows, buf);
	sprintf(buf, "%d", COLS); settext(f_ccols, buf);
    }
    if (p->lbuf->dim != newGUI.cbb || p->lbuf->ms != newGUI.cbl)
	xbufgrow(p->lbuf, newGUI.cbb, newGUI.cbl);

/* Set colours and redraw */
    p->fg = consolefg = newGUI.fg;
    p->ufg = consoleuser = newGUI.user;
    p->bg = consolebg = newGUI.bg;
    drawconsole(RConsole, r);
    pagerhighlight = newGUI.hlt;

    if(haveusedapager &&
       (newGUI.prows != curGUI.prows || newGUI.pcols != curGUI.pcols))
	askok(G_("Changes in pager size will not apply to any open pagers"));
    pagerrow = newGUI.prows;
    pagercol = newGUI.pcols;

    if(newGUI.pagerMultiple != pagerMultiple) {
	if(!haveusedapager ||
	   askokcancel(G_("Do not change pager type if any pager is open\nProceed?"))
	   == YES)
	    pagerMultiple = newGUI.pagerMultiple;
	if(pagerMultiple) {
	    check(rb_mwin); uncheck(rb_swin);
	} else {check(rb_swin); uncheck(rb_mwin);}
    }

    setWidthOnResize = newGUI.setWidthOnResize;
    getGUIstate(&curGUI);
}

static void save(button b)
{
    char *file, buf[256], *p;
    FILE *fp;

    setuserfilter("All files (*.*)\0*.*\0\0");
    strcpy(buf, getenv("R_USER"));
    file = askfilesavewithdir(G_("Select directory for file 'Rconsole'"),
			      "Rconsole", buf);
    if(!file) return;
    strcpy(buf, file);
    p = buf + strlen(buf) - 2;
    if(!strncmp(p, ".*", 2)) *p = '\0';

    fp = fopen(buf, "w");
    if(fp == NULL) {
	MessageBox(0, "Cannot open file to fp",
		   "Configuration Save Error",
		   MB_TASKMODAL | MB_ICONSTOP | MB_OK);
	return;
    }

    fprintf(fp, "%s\n%s\n%s\n\n%s\n%s\n",
	    "# Optional parameters for the console and the pager",
	    "# The system-wide copy is in rwxxxx/etc.",
	    "# A user copy can be installed in `R_USER'.",
	    "## Style",
	    "# This can be `yes' (for MDI) or `no' (for SDI).");
    fprintf(fp, "MDI = %s\n",  ischecked(rb_mdi)?"yes":"no");
    fprintf(fp, "%s\n%s%s\n%s%s\n\n",
	    "# the next two are only relevant for MDI",
	    "toolbar = ", ischecked(toolbar)?"yes":"no",
	    "statusbar = ", ischecked(statusbar)?"yes":"no");

    fprintf(fp, "%s\n%s\n%s\n%s\n%s\n",
	    "## Font.",
	    "# Please use only fixed width font.",
	    "# If font=FixedFont the system fixed font is used; in this case",
	    "# points and style are ignored. If font begins with \"TT \", only",
	    "# True Type fonts are searched for.");
    fprintf(fp, "font = %s%s\npoints = %s\nstyle = %s # Style can be normal, bold, italic\n\n\n",
	    ischecked(tt_font)?"TT ":"",
	    gettext(f_font),
	    gettext(d_point),
	    gettext(f_style));
    fprintf(fp, "# Dimensions (in characters) of the console.\n");
    fprintf(fp, "rows = %s\ncolumns = %s\n",
	    gettext(f_crows), gettext(f_ccols));
    fprintf(fp, "# Dimensions (in characters) of the internal pager.\n");
    fprintf(fp, "pgrows = %s\npgcolumns = %s\n",
	    gettext(f_prows), gettext(f_pcols));
    fprintf(fp, "# should options(width=) be set to the console width?\n");
    fprintf(fp, "setwidthonresize = %s\n\n",
	    ischecked(c_resize) ? "yes" : "no");
    fprintf(fp, "# memory limits for the console scrolling buffer, in bytes and lines\n");
    fprintf(fp, "bufbytes = %s\nbuflines = %s\n\n",
	    gettext(f_cbb), gettext(f_cbl));
    fprintf(fp, "# Initial position of the console (pixels, relative to the workspace for MDI)\n");
    fprintf(fp, "xconsole = %s\nyconsole = %s\n\n",
	    gettext(f_cx), gettext(f_cy));
    fprintf(fp, "%s\n%s\n%s\n%s\n%s\n%s\n\n",
	    "# Dimension of MDI frame in pixels",
	    "# Format (w*h+xorg+yorg) or use -ve w and h for offsets from right bottom",
	    "# This will come up maximized if w==0",
	    "# MDIsize = 0*0+0+0",
	    "# MDIsize = 1000*800+100+0",
	    "# MDIsize = -50*-50+50+50  # 50 pixels space all round");
    fprintf(fp, "%s\n%s\n%s\npagerstyle = %s\n\n\n",
	    "# The internal pager can displays help in a single window",
	    "# or in multiple windows (one for each topic)",
	    "# pagerstyle can be set to `singlewindow' or `multiplewindows'",
	    ischecked(rb_mwin) ? "multiplewindows" : "singlewindow");

    fprintf(fp, "## Colours for console and pager(s)\n# (see rwxxxx/etc/rgb.txt for the known colours).\n");
    fprintf(fp, "background = %s\n", gettext(bgcol));
    fprintf(fp, "normaltext = %s\n", gettext(fgcol));
    fprintf(fp, "usertext = %s\n", gettext(usercol));
    fprintf(fp, "highlight = %s\n", gettext(highlightcol));
    fprintf(fp, "\n\n%s\n%s\nxgraphics = %s\nygraphics = %s\n",
	    "## Initial position of the graphics window",
	    "## (pixels, <0 values from opposite edge)",
	    gettext(f_grx), gettext(f_gry));
    fclose(fp);
}

static void apply(button b) /* button callback */
{
    do_apply(); /* to be used outside button callbacks */
}

static void cancel(button b)
{
    cleanup();
    show(RConsole);
}

static void ok(button b)
{
    getGUIstate(&newGUI);
    do_apply(); /* It is more usual to have an OK button which applies the new preferences and exits, rather than prompting first */
    cleanup();
    show(RConsole);
}

static void cMDI(button b)
{
    enable(toolbar);
    enable(statusbar);
}

static void cSDI(button b)
{
    disable(toolbar);
    disable(statusbar);
}


void Rgui_configure()
{
    char buf[100], *style;
    rect r;
    ConsoleData p = (ConsoleData) getdata(RConsole);

    wconfig = newwindow(G_("Rgui Configuration Editor"), rect(0, 0, 550, 450),
			Titlebar | Centered | Modal);
    setbackground(wconfig, dialog_bg());
    l_mdi = newlabel("Single or multiple windows",
		      rect(10, 10, 150, 20), AlignLeft);
    rb_mdi = newradiobutton("MDI", rect(150, 10 , 70, 20), cMDI);
    rb_sdi = newradiobutton("SDI", rect(220, 10 , 70, 20), cSDI);


    toolbar = newcheckbox("MDI toolbar", rect(300, 10, 100, 20), NULL);
    if(RguiMDI & RW_TOOLBAR) check(toolbar);
    statusbar = newcheckbox("MDI statusbar", rect(420, 10, 130, 20), NULL);
    if(RguiMDI & RW_STATUSBAR) check(statusbar);
    if(RguiMDI & RW_MDI) {
	check(rb_mdi); cMDI(rb_mdi);
    } else {
	check(rb_sdi); cSDI(rb_sdi);
    }

    l_mwin = newlabel("Pager style", rect(10, 50, 90, 20), AlignLeft);
    newradiogroup();
    rb_mwin = newradiobutton("multiple windows", rect(150, 50, 150, 20), NULL);
    rb_swin = newradiobutton("single window", rect(320, 50 , 150, 20), NULL);
    if(pagerMultiple) check(rb_mwin); else check(rb_swin);

/* Font, pointsize, style */

    l_font = newlabel("Font", rect(10, 100, 40, 20), AlignLeft);

    f_font = newdropfield(FontsList, rect(50, 100, 120, 20), NULL);
    tt_font = newcheckbox("TrueType only", rect(180, 100, 110, 20), NULL);
    {
	char *pf;
	if ((strlen(fontname) > 1) &&
	    (fontname[0] == 'T') && (fontname[1] == 'T')) {
	    check(tt_font);
	    for (pf = fontname+2; isspace(*pf) ; pf++);
	} else pf = fontname;
	setlistitem(f_font, cmatch(pf, FontsList));
    }

    l_point = newlabel("size", rect(310, 100, 30, 20), AlignLeft);
    d_point = newdropfield(PointsList, rect(345, 100, 50, 20), NULL);
    sprintf(buf, "%d", pointsize);
    setlistitem(d_point, cmatch(buf, PointsList));
    l_style = newlabel("style", rect(410, 100, 40, 20), AlignLeft);
    f_style = newdropfield(StyleList, rect(450, 100, 80, 20), NULL);
    style = "normal";
    if (fontsty & Italic) style = "italic";
    if (fontsty & Bold) style = "Bold";
    setlistitem(f_style, cmatch(style, StyleList));

/* Console size, set widthonresize */
    l_crows = newlabel("Console   rows", rect(10, 150, 100, 20), AlignLeft);
    sprintf(buf, "%d", ROWS);
    f_crows = newfield(buf, rect(110, 150, 30, 20));
    l_ccols = newlabel("columns", rect(150, 150, 60, 20), AlignLeft);
    sprintf(buf, "%d", COLS);
    f_ccols = newfield(buf, rect(220, 150, 30, 20));
    r = GetCurrentWinPos(RConsole);
    l_cx = newlabel("Initial left", rect(270, 150, 70, 20), AlignLeft);
    sprintf(buf, "%d", r.x);
    f_cx = newfield(buf, rect(350, 150, 40, 20));
    l_cy = newlabel("top", rect(430, 150, 30, 20), AlignLeft);
    sprintf(buf, "%d", r.y);
    f_cy = newfield(buf, rect(480, 150, 40, 20));

    c_resize = newcheckbox("set options(width) on resize?",
			   rect(50, 175, 200, 20), NULL);
    if(setWidthOnResize) check(c_resize);

    l_cbb = newlabel("buffer bytes", rect(270, 175, 70, 20), AlignLeft);
    sprintf(buf, "%ld", p->lbuf->dim);
    f_cbb = newfield(buf, rect(350, 175, 60, 20));
    l_cbl = newlabel("lines", rect(430, 175, 50, 20), AlignLeft);
    sprintf(buf, "%d", p->lbuf->ms);
    f_cbl = newfield(buf, rect(480, 175, 40, 20));

/* Pager size */
    l_prows = newlabel("Pager   rows", rect(10, 210, 100, 20), AlignLeft);
    sprintf(buf, "%d", pagerrow);
    f_prows = newfield(buf, rect(110, 210, 30, 20));
    l_pcols = newlabel("columns", rect(150, 210, 60, 20), AlignLeft);
    sprintf(buf, "%d", pagercol);
    f_pcols = newfield(buf, rect(220, 210, 30, 20));

/* Graphics window */
    l_grx = newlabel("Graphics windows: initial left",
		    rect(10, 250, 190, 20), AlignLeft);
    sprintf(buf, "%d", Rwin_graphicsx);
    f_grx = newfield(buf, rect(200, 250, 40, 20));
    l_gry = newlabel("top", rect(270, 250, 30, 20), AlignLeft);
    sprintf(buf, "%d", Rwin_graphicsy);
    f_gry = newfield(buf, rect(300, 250, 40, 20));

/* Font colours */
    l_cols = newlabel("Console and Pager Colours",
		      rect(10, 300, 520, 20), AlignCenter);
    l_bgcol = newlabel("Background", rect(10, 330, 100, 20), AlignCenter);
    bgcol = newlistbox(ColorName, rect(10, 350, 100, 50), NULL);
    l_fgcol = newlabel("Output text", rect(150, 330, 100, 20), AlignCenter);
    fgcol = newlistbox(ColorName, rect(150, 350, 100, 50), NULL);
    l_usercol = newlabel("User input", rect(290, 330, 100, 20), AlignCenter);
    usercol = newlistbox(ColorName, rect(290, 350, 100, 50), NULL);
    l_highlightcol = newlabel("Titles in pager", rect(430, 330, 100, 20),
			      AlignCenter);
    highlightcol = newlistbox(ColorName, rect(430, 350, 100, 50), NULL);
    setlistitem(bgcol, rgbtonum(consolebg));
    setlistitem(fgcol, rgbtonum(consolefg));
    setlistitem(usercol, rgbtonum(consoleuser));
    setlistitem(highlightcol, rgbtonum(pagerhighlight));

    bApply = newbutton(G_("Apply"), rect(50, 410, 70, 25), apply);
    bSave = newbutton(G_("Save"), rect(130, 410, 70, 25), save);
    bOK = newbutton(G_("OK"), rect(350, 410, 70, 25), ok);
    bCancel = newbutton(G_("Cancel"), rect(430, 410, 70, 25), cancel);
    show(wconfig);
    getGUIstate(&curGUI);
}
