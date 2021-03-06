#include <windows.h>
#include "ga.h"
#include <stdlib.h> /* atof */
#include <ctype.h> /* tolower */


static window win;
static button bApply, bCancel;
static label l_var1, l_var2, l_lvl;
static field var1, var2, lvl;
static checkbox paired, varequal;
static listbox alt;

extern void R_ProcessEvents();

static int done = 0;
static char *v[2];


static char *alts[] = {"two.sided", "greater", "less", NULL};

/* keyboard shortcuts: CR or A/a accepts, ESC or C/c cancels */
static void hit_key(window w, int key)
{
    if(key == '\n' || tolower(key) == 'a') {
	flashcontrol(bApply);
	activatecontrol(bApply);
    }
    if(key == ESC || tolower(key) == 'c') {
	flashcontrol(bCancel);
	activatecontrol(bCancel);
    }
}

static void apply(button b)
{
    v[0] = gettext(var1);
    v[1] = gettext(var2);
    done = strlen(v[0]) && strlen(v[1]);
    if (!done) askok("all fields must be completed");
}

static void create_dialog()
{
    setcursor(ArrowCursor);  /* not `busy' cursor */
    win = newwindow("t-test entry", rect(0, 0, 200, 200),
			Titlebar | Centered | Modal);
    setbackground(win, dialog_bg());
    setkeydown(win, hit_key);
    bApply = newbutton("Apply", rect(20, 160, 50, 25), apply);
    bCancel = newbutton("Cancel", rect(120, 160, 50, 25), NULL);
    l_var1 = newlabel("x", rect(10, 10, 30, 20), AlignLeft);
    var1 = newfield("", rect(40, 10, 130, 20));
    l_var2 = newlabel("y", rect(10, 40, 30, 20), AlignLeft);
    var2 = newfield("", rect(40, 40, 130, 20));
    paired = newcheckbox("paired", rect(10, 70, 80, 20), NULL);
    varequal = newcheckbox("equal var", rect(110, 70, 80, 20), NULL);
    alt = newdroplist(alts , rect(30, 90, 120, 20), NULL);
    setlistitem(alt, 0);
    l_lvl = newlabel("confidence level", rect(20, 120, 90, 20), AlignLeft);
    lvl = newfield("0.95", rect(120, 120, 40, 20));
}

static void cancel(button b)
{
    /* need these set to something malloc'ed */
    v[0] = gettext(var1);
    v[1] = gettext(var2);
    done = 2;
}

/* just retrieve values from the dialog box and assemble call in
   interpreted code
*/
void menu_ttest(char **vars, int ints[], double level[])
{
    done = 0;
    create_dialog();
    setaction(bCancel, cancel);
    show(win);
    for(;;) {
	R_ProcessEvents();
	if(done > 0) break;
    }
    vars[0] = v[0]; vars[1] = v[1];
    ints[0] =  getlistitem(alt);
    ints[1] =  ischecked(paired);
    ints[2] =  ischecked(varequal);
    ints[3] = done;
    level[0] = atof(gettext(lvl));
    hide(win);
    delobj(bApply);
    delobj(win);
}


extern void consolecmd(window c, char *cmd);
extern window RConsole;


static void cancel2(button b)
{
    done = 2;
}

/* assemble call as string in C code */
void menu_ttest2()
{
    char cmd[256];

    done = 0;
    create_dialog();
    setaction(bCancel, cancel2);
    show(win);
    for(;;) {
	R_ProcessEvents();
	if(done > 0) break;
    }
    if(done == 1){
	sprintf(cmd, "t.test(x=%s, y=%s, alternative=\"%s\",\n      paired=%s, var.equal=%s, conf.level=%s)\n", v[0], v[1],
		alts[getlistitem(alt)],
		ischecked(paired) ? "TRUE" : "FALSE",
		ischecked(varequal) ? "TRUE" : "FALSE",
		gettext(lvl));
	consolecmd(RConsole, cmd);
    }    
    hide(win);
    delobj(bApply);
    delobj(win);
}


/* assemble and evaluate call in C code */
#include <Rinternals.h>
#include <R_ext/Parse.h>

SEXP menu_ttest3()
{
    char cmd[256];
    SEXP cmdSexp, cmdexpr, ans = R_NilValue;
    int i;
    ParseStatus status;
    
    done = 0;
    create_dialog();
    setaction(bCancel, cancel2);
    show(win);
    for(;;) {
	R_ProcessEvents();
	if(done > 0) break;
    }
    if(done == 1) {
	sprintf(cmd, "t.test(x=%s, y=%s, alternative=\"%s\",\n      paired=%s, var.equal=%s, conf.level=%s)\n", v[0], v[1],
		alts[getlistitem(alt)],
		ischecked(paired) ? "TRUE" : "FALSE",
		ischecked(varequal) ? "TRUE" : "FALSE",
		gettext(lvl));
    }    
    hide(win);
    delobj(bApply);
    delobj(win);
    if(done == 1) {
	PROTECT(cmdSexp = allocVector(STRSXP, 1));
	SET_STRING_ELT(cmdSexp, 0, mkChar(cmd));
	cmdexpr = PROTECT(R_ParseVector(cmdSexp, -1, &status));
	if (status != PARSE_OK) {
	    UNPROTECT(2);
	    error("invalid call %s", cmd);
	}
	/* Loop is needed here as EXPSEXP will be of length > 1 */
	for(i = 0; i < length(cmdexpr); i++)
	    ans = eval(VECTOR_ELT(cmdexpr, i), R_GlobalEnv);
	UNPROTECT(2);
    }
    return ans;
}
