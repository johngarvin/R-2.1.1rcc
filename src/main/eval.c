/*
 *  R : A Computer Language for Statistical Data Analysis
 *  Copyright (C) 1995, 1996	Robert Gentleman and Ross Ihaka
 *  Copyright (C) 1998--2004	The R Development Core Team.
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

/* <UTF8> char here is either ASCII or handled as a whole */

#undef HASHING

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define ARGUSED(x) LEVELS(x)

#include "Defn.h"

extern Rboolean global_dump_stats;

#ifdef BYTECODE
static SEXP bcEval(SEXP, SEXP);
#endif

SEXP do_browser(SEXP, SEXP, SEXP, SEXP);

/*#define BC_PROFILING*/
#ifdef BC_PROFILING
static Rboolean bc_profiling = FALSE;
#endif

#ifdef R_PROFILING

/* BDR 2000-07-15
   Profiling is now controlled by the R function Rprof(), and should
   have negligible cost when not enabled.
*/

/* A simple mechanism for profiling R code.  When R_PROFILING is
   enabled, eval will write out the call stack every PROFSAMPLE
   microseconds using the SIGPROF handler triggered by timer signals
   from the ITIMER_PROF timer.  Since this is the same timer used by C
   profiling, the two cannot be used together.  Output is written to
   the file PROFOUTNAME.  This is a plain text file.  The first line
   of the file contains the value of PROFSAMPLE.  The remaining lines
   each give the call stack found at a sampling point with the inner
   most function first.

   To enable profiling, recompile eval.c with R_PROFILING defined.  It
   would be possible to selectively turn profiling on and off from R
   and to specify the file name from R as well, but for now I won't
   bother.

   The stack is traced by walking back along the context stack, just
   like the traceback creation in jump_to_toplevel.  One drawback of
   this approach is that it does not show BUILTIN's since they don't
   get a context.  With recent changes to pos.to.env it seems possible
   to insert a context around BUILTIN calls to that they show up in
   the trace.  Since there is a cost in establishing these contexts,
   they are only inserted when profiling is enabled.

   One possible advantage of not tracing BUILTIN's is that then
   profiling adds no cost when the timer is turned off.  This would be
   useful if we want to allow profiling to be turned on and off from
   within R.

   One thing that makes interpreting profiling output tricky is lazy
   evaluation.  When an expression f(g(x)) is profiled, lazy
   evaluation will cause g to be called inside the call to f, so it
   will appear as if g is called by f.

   L. T.  */

#ifdef Win32
# include <windows.h>		/* for CreateEvent, SetEvent */
# include <process.h>		/* for _beginthread, _endthread */
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# endif
# include <signal.h>
#endif /* not Win32 */

FILE *R_ProfileOutfile = NULL;
static int R_Profiling = 0;

#ifdef Win32
HANDLE MainThread;
HANDLE ProfileEvent;

static void doprof()
{
    RCNTXT *cptr;
    char buf[1100];

    buf[0] = '\0';
    SuspendThread(MainThread);
    for (cptr = R_GlobalContext; cptr; cptr = cptr->nextcontext) {
	if (((cptr->callflag & CTXT_FUNCTION) ||
	     (cptr->callflag & CTXT_BUILTIN))
	    && TYPEOF(cptr->call) == LANGSXP) {
	    SEXP fun = CAR(cptr->call);
	    if(strlen(buf) < 1000) {
		strcat(buf, TYPEOF(fun) == SYMSXP ? CHAR(PRINTNAME(fun)) :
		       "<Anonymous>");
		strcat(buf, " ");
	    }
	}
    }
    ResumeThread(MainThread);
    if(strlen(buf))
	fprintf(R_ProfileOutfile, "%s\n", buf);
}


/* Profiling thread main function */
static void __cdecl ProfileThread(void *pwait)
{
    int wait = *((int *)pwait);

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    while(WaitForSingleObject(ProfileEvent, wait) != WAIT_OBJECT_0) {
	doprof();
    }
}
#else /* not Win32 */
static void doprof(int sig)
{
    RCNTXT *cptr;
    int newline = 0;
    for (cptr = R_GlobalContext; cptr; cptr = cptr->nextcontext) {
	if (((cptr->callflag & CTXT_FUNCTION) ||
	     (cptr->callflag & CTXT_BUILTIN))
	    && TYPEOF(cptr->call) == LANGSXP) {
	    SEXP fun = CAR(cptr->call);
	    if (!newline) newline = 1;
	    fprintf(R_ProfileOutfile, "\"%s\" ",
		    TYPEOF(fun) == SYMSXP ? CHAR(PRINTNAME(fun)) :
		    "<Anonymous>");
	}
    }
    if (newline) fprintf(R_ProfileOutfile, "\n");
    signal(SIGPROF, doprof);
}

static void doprof_null(int sig)
{
    signal(SIGPROF, doprof_null);
}
#endif /* not Win32 */


static void R_EndProfiling()
{
#ifdef Win32
    SetEvent(ProfileEvent);
    CloseHandle(MainThread);
#else /* not Win32 */
    struct itimerval itv;

    itv.it_interval.tv_sec = 0;
    itv.it_interval.tv_usec = 0;
    itv.it_value.tv_sec = 0;
    itv.it_value.tv_usec = 0;
    setitimer(ITIMER_PROF, &itv, NULL);
    signal(SIGPROF, doprof_null);
#endif /* not Win32 */
    if(R_ProfileOutfile) fclose(R_ProfileOutfile);
    R_ProfileOutfile = NULL;
    R_Profiling = 0;
}

#if !defined(Win32) && defined(_R_HAVE_TIMING_)
double R_getClockIncrement(void);
#endif

static void R_InitProfiling(char * filename, int append, double dinterval)
{
#ifndef Win32
    struct itimerval itv;
#else
    int wait;
    HANDLE Proc = GetCurrentProcess();
#endif
    int interval;

    /* according to man setitimer, it waits until the next clock
       tick, usually 10ms, so avoid too small intervals here */
#if !defined(Win32) && defined(_R_HAVE_TIMING_)
    double clock_incr = R_getClockIncrement();
    int nclock = floor(dinterval/clock_incr + 0.5);
    interval = 1e6 * ((nclock > 1)?nclock:1) * clock_incr + 0.5;
#else
    interval = 1e6 * dinterval + 0.5;
#endif
    if(R_ProfileOutfile != NULL) R_EndProfiling();
    R_ProfileOutfile = fopen(filename, append ? "a" : "w");
    if (R_ProfileOutfile == NULL)
	error(_("Rprof: can't open profile file '%s'"), filename);
    fprintf(R_ProfileOutfile, "sample.interval=%d\n", interval);

#ifdef Win32
    /* need to duplicate to make a real handle */
    DuplicateHandle(Proc, GetCurrentThread(), Proc, &MainThread,
		    0, FALSE, DUPLICATE_SAME_ACCESS);
    wait = interval/1000;
    if(!(ProfileEvent = CreateEvent(NULL, FALSE, FALSE, NULL)) ||
       (_beginthread(ProfileThread, 0, &wait) == -1))
	R_Suicide("unable to create profiling thread");
    Sleep(wait/2); /* suspend this thread to ensure that the other one starts */
#else /* not Win32 */
    signal(SIGPROF, doprof);

    itv.it_interval.tv_sec = 0;
    itv.it_interval.tv_usec = interval;
    itv.it_value.tv_sec = 0;
    itv.it_value.tv_usec = interval;
    if (setitimer(ITIMER_PROF, &itv, NULL) == -1)
	R_Suicide("setting profile timer failed");
#endif /* not Win32 */
    R_Profiling = 1;
}

SEXP do_Rprof(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    char *filename;
    int append_mode;
    double dinterval;

#ifdef BC_PROFILING
    if (bc_profiling) {
	warning(_("can't use R profiling while byte code profiling"));
	return R_NilValue;
    }
#endif
    checkArity(op, args);
    if (!isString(CAR(args)) || (LENGTH(CAR(args))) != 1)
	errorcall(call, _("invalid 'filename' argument"));
    append_mode = asLogical(CADR(args));
    dinterval = asReal(CADDR(args));
    filename = R_ExpandFileName(CHAR(STRING_ELT(CAR(args), 0)));
    if (strlen(filename))
	R_InitProfiling(filename, append_mode, dinterval);
    else
	R_EndProfiling();
    return R_NilValue;
}
#else /* not R_PROFILING */
SEXP do_Rprof(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    error(_("R profiling is not available on this system"));
    return R_NilValue;		/* -Wall */
}
#endif /* not R_PROFILING */

/* NEEDED: A fixup is needed in browser, because it can trap errors,
 *	and currently does not reset the limit to the right value. */

static SEXP call_func_alloc(CCODE func, SEXP call, SEXP op, SEXP args, SEXP rho)
{
    SEXP stack;
    SEXP out;
    extern int global_alloc_stack_space_size;
    /*
    stack = (SEXP)alloca(global_alloc_stack_space_size);
    pushAllocStack(stack, global_alloc_stack_space_size, &allocVectorStack, &allocNodeStack);
    */
    out = func(call, op, args, rho);
    /*
    popAllocStack();
    */
    return out;
}

/* Return value of "e" evaluated in "rho". */

SEXP eval(SEXP e, SEXP rho)
{
    Rboolean fallback;
    SEXP op, tmp, val;
    static int evalcount = 0;

    /* The use of depthsave below is necessary because of the
       possibility of non-local returns from evaluation.  Without this
       an "expression too complex error" is quite likely. */

    int depthsave = R_EvalDepth++;

    /* We need to explicit set a NULL call here to circumvent attempts
       to deparse the call in the error-handler */
    if (R_EvalDepth > R_Expressions)
	errorcall(R_NilValue,
_("evaluation nested too deeply: infinite recursion / options(expressions=)?"));
    if (++evalcount > 100) {
	R_CheckUserInterrupt();
	evalcount = 0 ;
    }

    tmp = R_NilValue;		/* -Wall */

    R_Visible = 1;
    switch (TYPEOF(e)) {
    case NILSXP:
    case LISTSXP:
    case LGLSXP:
    case INTSXP:
    case REALSXP:
    case STRSXP:
    case CPLXSXP:
    case RAWSXP:
    case SPECIALSXP:
    case BUILTINSXP:
    case ENVSXP:
    case CLOSXP:
    case RCC_CLOSXP:
    case VECSXP:
    case EXTPTRSXP:
    case WEAKREFSXP:
    case EXPRSXP:
	tmp = e;
	/* Make sure constants in expressions are NAMED before being
           used as values.  Setting NAMED to 2 makes sure weird calls
           to assignment functions won't modify constants in
           expressions.  */
	if (NAMED(tmp) != 2) SET_NAMED(tmp, 2);
	break;
#ifdef BYTECODE
    case BCODESXP:
	    tmp = bcEval(e, rho);
	    break;
#endif
    case SYMSXP:
	R_Visible = 1;
	if (e == R_DotsSymbol)
	    error(_("... used in an incorrect context"));
	if( DDVAL(e) )
		tmp = ddfindVar(e,rho);
	else
		tmp = findVar(e, rho);
	if (tmp == R_UnboundValue)
	    error(_("Object \"%s\" not found"), CHAR(PRINTNAME(e)));
	/* if ..d is missing then ddfindVar will signal */
	else if (tmp == R_MissingArg && !DDVAL(e) ) {
	    char *n = CHAR(PRINTNAME(e));
	    if(*n) error(_("argument \"%s\" is missing, with no default"),
			 CHAR(PRINTNAME(e)));
	    else error(_("argument is missing, with no default"));
	}
	else if (TYPEOF(tmp) == PROMSXP) {
	    PROTECT(tmp);
	    tmp = eval(tmp, rho);
	    SET_NAMED(tmp, 2);
	    UNPROTECT(1);
	}
	else if (!isNull(tmp) && NAMED(tmp) < 1)
	    SET_NAMED(tmp, 1);
	break;
    case PROMSXP:
	if (PRVALUE(e) == R_UnboundValue) {
	    if(PRSEEN(e))
		errorcall(R_GlobalContext->call,
			  _("recursive default argument reference"));
	    SET_PRSEEN(e, 1);
	    fallback = getFallbackAlloc();
	    setFallbackAlloc(TRUE);
	    val = eval(PRCODE(e), PRENV(e));
	    setFallbackAlloc(fallback);
	    SET_PRSEEN(e, 0);
	    SET_PRVALUE(e, val);
	    /* allow GC to reclaim; useful for fancy games with delay() */
	    SET_PRENV(e, R_NilValue);
	}
	tmp = PRVALUE(e);
	break;
    case LANGSXP:
	if (TYPEOF(CAR(e)) == SYMSXP)
	    PROTECT(op = findFun(CAR(e), rho));
	else
	    PROTECT(op = eval(CAR(e), rho));
	if(TRACE(op) && R_current_trace_state()) {
	    Rprintf("trace: ");
	    PrintValue(e);
	}
	if (TYPEOF(op) == SPECIALSXP) {
	    int save = R_PPStackTop;
	    PROTECT(CDR(e));
	    R_Visible = 1 - PRIMPRINT(op);
	    tmp = PRIMFUN(op) (e, op, CDR(e), rho);
	    UNPROTECT(1);
	    if(save != R_PPStackTop) {
		Rprintf("stack imbalance in %s, %d then %d\n",
			PRIMNAME(op), save, R_PPStackTop);
	    }
	}
	else if (TYPEOF(op) == BUILTINSXP) {
	    int save = R_PPStackTop;
#ifdef R_PROFILING
	    if (R_Profiling) {
		RCNTXT cntxt;
		PROTECT(tmp = evalList(CDR(e), rho));
		R_Visible = 1 - PRIMPRINT(op);
		begincontext(&cntxt, CTXT_BUILTIN, e,
			     R_NilValue, R_NilValue, R_NilValue, R_NilValue);
		tmp = PRIMFUN(op) (e, op, tmp, rho);
		endcontext(&cntxt);
		UNPROTECT(1);
	    } else {
#endif /* R_PROFILING */
		PROTECT(tmp = evalList(CDR(e), rho));
		R_Visible = 1 - PRIMPRINT(op);
		tmp = PRIMFUN(op) (e, op, tmp, rho);
		UNPROTECT(1);
#ifdef R_PROFILING
	    }
#endif
	    if(save != R_PPStackTop) {
		Rprintf("stack imbalance in %s, %d then %d\n",
			PRIMNAME(op), save, R_PPStackTop);
	    }
	}
	else if (TYPEOF(op) == CLOSXP || TYPEOF(op) == RCC_CLOSXP) {
	    PROTECT(tmp = promiseArgs(CDR(e), rho));
	    tmp = applyClosure(e, op, tmp, rho, R_NilValue);
	    UNPROTECT(1);
	}
	else
	    error(_("attempt to apply non-function"));
	UNPROTECT(1);
	break;
    case DOTSXP:
	error(_("... used in an incorrect context"));
    default:
	UNIMPLEMENTED_TYPE("eval", e);
    }
    R_EvalDepth = depthsave;
    return (tmp);
}


/* Apply SEXP op of type CLOSXP to actuals */

SEXP applyClosureOld(SEXP call, SEXP op, SEXP arglist, SEXP rho, SEXP suppliedenv)
{
    SEXP body, formals, actuals, savedrho;
    volatile  SEXP newrho;
    SEXP f, a, tmp;
    RCNTXT cntxt;
    if (TYPEOF(op) == RCC_CLOSXP) {
      return applyRccClosure(call, op, arglist, rho, suppliedenv);
    }

    /* formals = list of formal parameters */
    /* actuals = values to be bound to formals */
    /* arglist = the tagged list of arguments */

    formals = FORMALS(op);
    body = BODY(op);
    savedrho = CLOENV(op);

    /*  Set up a context with the call in it so error has access to it */

    begincontext(&cntxt, CTXT_RETURN, call, savedrho, rho, arglist, op);

    /*  Build a list which matches the actual (unevaluated) arguments
	to the formal paramters.  Build a new environment which
	contains the matched pairs.  Ideally this environment sould be
	hashed.  */

    PROTECT(actuals = matchArgs(formals, arglist));
    PROTECT(newrho = NewEnvironment(formals, actuals, savedrho));

    /*  Use the default code for unbound formals.  FIXME: It looks like
	this code should preceed the building of the environment so that
        this will also go into the hash table.  */

    /* This piece of code is destructively modifying the actuals list,
       which is now also the list of bindings in the frame of newrho.
       This is one place where internal structure of environment
       bindings leaks out of envir.c.  It should be rewritten
       eventually so as not to break encapsulation of the internal
       environment layout.  We can live with it for now since it only
       happens immediately after the environment creation.  LT */

    f = formals;
    a = actuals;
    while (f != R_NilValue) {
	if (CAR(a) == R_MissingArg && CAR(f) != R_MissingArg) {
	    SETCAR(a, mkPROMISE(CAR(f), newrho));
	    SET_MISSING(a, 2);
	}
	f = CDR(f);
	a = CDR(a);
    }

    /*  Fix up any extras that were supplied by usemethod. */

    if (suppliedenv != R_NilValue) {
	for (tmp = FRAME(suppliedenv); tmp != R_NilValue; tmp = CDR(tmp)) {
	    for (a = actuals; a != R_NilValue; a = CDR(a))
		if (TAG(a) == TAG(tmp))
		    break;
	    if (a == R_NilValue)
		/* Use defineVar instead of earlier version that added
                   bindings manually */
		defineVar(TAG(tmp), CAR(tmp), newrho);
	}
    }

    /*  Terminate the previous context and start a new one with the
        correct environment. */

    endcontext(&cntxt);

    /*  If we have a generic function we need to use the sysparent of
	the generic as the sysparent of the method because the method
	is a straight substitution of the generic.  */

    if( R_GlobalContext->callflag == CTXT_GENERIC )
	begincontext(&cntxt, CTXT_RETURN, call,
		     newrho, R_GlobalContext->sysparent, arglist, op);
    else
	begincontext(&cntxt, CTXT_RETURN, call, newrho, rho, arglist, op);

    /* The default return value is NULL.  FIXME: Is this really needed
       or do we always get a sensible value returned?  */

    tmp = R_NilValue;

    /* Debugging */

    SET_DEBUG(newrho, DEBUG(op));
    if (DEBUG(op)) {
	Rprintf("debugging in: ");
	PrintValueRec(call,rho);
	/* Is the body a bare symbol (PR#6804) */
	if (!isSymbol(body) & !isVectorAtomic(body)){
		/* Find out if the body is function with only one statement. */
		if (isSymbol(CAR(body)))
			tmp = findFun(CAR(body), rho);
		else
			tmp = eval(CAR(body), rho);
		if((TYPEOF(tmp) == BUILTINSXP || TYPEOF(tmp) == SPECIALSXP)
		   && !strcmp( PRIMNAME(tmp), "for")
		   && !strcmp( PRIMNAME(tmp), "{")
		   && !strcmp( PRIMNAME(tmp), "repeat")
		   && !strcmp( PRIMNAME(tmp), "while")
			)
			goto regdb;
	}
	Rprintf("debug: ");
	PrintValue(body);
	do_browser(call,op,arglist,newrho);
    }

 regdb:

    /*  It isn't completely clear that this is the right place to do
	this, but maybe (if the matchArgs above reverses the
	arguments) it might just be perfect.  */

#ifdef  HASHING
#define HASHTABLEGROWTHRATE  1.2
    {
	SEXP R_NewHashTable(int, double);
	SEXP R_HashFrame(SEXP);
	int nargs = length(arglist);
	HASHTAB(newrho) = R_NewHashTable(nargs, HASHTABLEGROWTHRATE);
	newrho = R_HashFrame(newrho);
    }
#endif
#undef  HASHING

    /*  Set a longjmp target which will catch any explicit returns
	from the function body.  */

    if ((SETJMP(cntxt.cjmpbuf))) {
	if (R_ReturnedValue == R_RestartToken) {
	    cntxt.callflag = CTXT_RETURN;  /* turn restart off */
	    R_ReturnedValue = R_NilValue;  /* remove restart token */
	    PROTECT(tmp = eval(body, newrho));
	}
	else
	    PROTECT(tmp = R_ReturnedValue);
    }
    else {
	PROTECT(tmp = eval(body, newrho));
    }

    endcontext(&cntxt);

    if (DEBUG(op)) {
	Rprintf("exiting from: ");
	PrintValueRec(call, rho);
    }
    UNPROTECT(3);
    return (tmp);
}

/*
SEXP applyClosure(SEXP call, SEXP op, SEXP arglist, SEXP rho, SEXP suppliedenv)
{
    return applyClosureOpt(call, op, arglist, rho, suppliedenv, AC_DEFAULT, TYPEOF(CAR(call)) == SYMSXP ? CAR(call) : Rf_install("*anonymous*"));
}
*/

SEXP applyClosure(SEXP call, SEXP op, SEXP arglist, SEXP rho, SEXP suppliedenv)
{
    SEXP body, formals, actuals, savedrho, funsxp;
    volatile  SEXP newrho;
    SEXP f, a, tmp;
    RCNTXT cntxt;

    if (global_dump_stats) {
	return applyClosureOpt(call, op, arglist, rho, suppliedenv, AC_DEFAULT, TYPEOF(CAR(call)) == SYMSXP ? CAR(call) : Rf_install("*anonymous*"));
    }

    /* formals = list of formal parameters */
    /* actuals = values to be bound to formals */
    /* arglist = the tagged list of arguments */

    if (TYPEOF(op) == RCC_CLOSXP) {
	formals = RCC_CLOSXP_FORMALS(op);
	funsxp = RCC_CLOSXP_FUN(op);
	savedrho = RCC_CLOSXP_CLOENV(op);
    } else {
	formals = FORMALS(op);
	body = BODY(op);
	savedrho = CLOENV(op);
    }
    formals = FORMALS(op);
    body = BODY(op);
    savedrho = CLOENV(op);

    /*  Set up a context with the call in it so error has access to it */

    begincontext(&cntxt, CTXT_RETURN, call, savedrho, rho, arglist, op);

    /*  Build a list which matches the actual (unevaluated) arguments
	to the formal paramters.  Build a new environment which
	contains the matched pairs.  Ideally this environment sould be
	hashed.  */

    PROTECT(actuals = matchArgs(formals, arglist));
    PROTECT(newrho = NewEnvironment(formals, actuals, savedrho));

    /*  Use the default code for unbound formals.  FIXME: It looks like
	this code should preceed the building of the environment so that
        this will also go into the hash table.  */

    /* This piece of code is destructively modifying the actuals list,
       which is now also the list of bindings in the frame of newrho.
       This is one place where internal structure of environment
       bindings leaks out of envir.c.  It should be rewritten
       eventually so as not to break encapsulation of the internal
       environment layout.  We can live with it for now since it only
       happens immediately after the environment creation.  LT */

    f = formals;
    a = actuals;
    while (f != R_NilValue) {
	if (CAR(a) == R_MissingArg && CAR(f) != R_MissingArg) {
	    SETCAR(a, mkPROMISE(CAR(f), newrho));
	    SET_MISSING(a, 2);
	}
	f = CDR(f);
	a = CDR(a);
    }

    /*  Fix up any extras that were supplied by usemethod. */

    if (suppliedenv != R_NilValue) {
	for (tmp = FRAME(suppliedenv); tmp != R_NilValue; tmp = CDR(tmp)) {
	    for (a = actuals; a != R_NilValue; a = CDR(a))
		if (TAG(a) == TAG(tmp))
		    break;
	    if (a == R_NilValue)
		/* Use defineVar instead of earlier version that added
                   bindings manually */
		defineVar(TAG(tmp), CAR(tmp), newrho);
	}
    }

    /*  Terminate the previous context and start a new one with the
        correct environment. */

    endcontext(&cntxt);

    /*  If we have a generic function we need to use the sysparent of
	the generic as the sysparent of the method because the method
	is a straight substitution of the generic.  */

    if( R_GlobalContext->callflag == CTXT_GENERIC )
	begincontext(&cntxt, CTXT_RETURN, call,
		     newrho, R_GlobalContext->sysparent, arglist, op);
    else
	begincontext(&cntxt, CTXT_RETURN, call, newrho, rho, arglist, op);

    /* The default return value is NULL.  FIXME: Is this really needed
       or do we always get a sensible value returned?  */

    tmp = R_NilValue;

    /* Debugging */

    if (TYPEOF(op) != RCC_CLOSXP) {
	SET_DEBUG(newrho, DEBUG(op));
	if (DEBUG(op)) {
	    Rprintf("debugging in: ");
	    PrintValueRec(call,rho);
	    /* Is the body a bare symbol (PR#6804) */
	    if (!isSymbol(body) & !isVectorAtomic(body)){
		/* Find out if the body is function with only one statement. */
		if (isSymbol(CAR(body)))
		    tmp = findFun(CAR(body), rho);
		else
		    tmp = eval(CAR(body), rho);
		if((TYPEOF(tmp) == BUILTINSXP || TYPEOF(tmp) == SPECIALSXP)
		   && !strcmp( PRIMNAME(tmp), "for")
		   && !strcmp( PRIMNAME(tmp), "{")
		   && !strcmp( PRIMNAME(tmp), "repeat")
		   && !strcmp( PRIMNAME(tmp), "while")
		   )
		    goto regdb;
	    }
	    Rprintf("debug: ");
	    PrintValue(body);
	    do_browser(call,op,arglist,newrho);
	}
    }

 regdb:

    /*  It isn't completely clear that this is the right place to do
	this, but maybe (if the matchArgs above reverses the
	arguments) it might just be perfect.  */

#ifdef  HASHING
#define HASHTABLEGROWTHRATE  1.2
    {
	SEXP R_NewHashTable(int, double);
	SEXP R_HashFrame(SEXP);
	int nargs = length(arglist);
	HASHTAB(newrho) = R_NewHashTable(nargs, HASHTABLEGROWTHRATE);
	newrho = R_HashFrame(newrho);
    }
#endif
#undef  HASHING

    /*  Set a longjmp target which will catch any explicit returns
	from the function body.  */

    if ((SETJMP(cntxt.cjmpbuf))) {
	if (R_ReturnedValue == R_RestartToken) {
	    cntxt.callflag = CTXT_RETURN;  /* turn restart off */
	    R_ReturnedValue = R_NilValue;  /* remove restart token */
	    if (TYPEOF(op) == RCC_CLOSXP) {
		PROTECT(tmp = RCC_FUNSXP_CFUN(funsxp) (actuals, newrho));
	    } else  {
		PROTECT(tmp = eval(body, newrho));
	    }
	}
	else
	    PROTECT(tmp = R_ReturnedValue);
    }
    else {
	if (TYPEOF(op) == RCC_CLOSXP) {
	    PROTECT(tmp = RCC_FUNSXP_CFUN(funsxp) (actuals, newrho));
	} else {
	    PROTECT(tmp = eval(body, newrho));
	}
    }

    endcontext(&cntxt);

    if (DEBUG(op)) {
	Rprintf("exiting from: ");
	PrintValueRec(call, rho);
    }
    UNPROTECT(3);
    return (tmp);
}

/* non-escaping symbols: these procedures are safe to call with pool
   allocation. This is because objects they allocate cannot escape
   except by their return value. */

/* UNSAFE functions: lapply, apply, eval, UseMethod, assign,
   postscript (calls gsetVar), axis */

static SEXP sym_dnorm;
static SEXP sym_pnorm;
static SEXP sym_qnorm;
static SEXP sym_dunif;
static SEXP sym_punif;
static SEXP sym_qunif;
static SEXP sym_sum;
static SEXP sym_vector;
static SEXP sym_append;
static SEXP sym_max;
static SEXP sym_any;
static SEXP sym_names;
static SEXP sym_names_default;
static SEXP sym_namesgets;
static SEXP sym_namesgets_default;
static SEXP sym_prod;
static SEXP sym_t;
static SEXP sym_t_default;
static SEXP sym_log;
static SEXP sym_matrix;
static SEXP sym_as_vector;
static SEXP sym_c;
static SEXP sym_unlist;
static SEXP sym_aperm;

static void init_symbols() {
    sym_dnorm = Rf_install("dnorm");
    sym_pnorm = Rf_install("pnorm");
    sym_qnorm = Rf_install("qnorm");
    sym_dunif = Rf_install("dunif");
    sym_punif = Rf_install("punif");
    sym_qunif = Rf_install("qunif");
    sym_sum = Rf_install("sum");
    sym_vector = Rf_install("vector");
    sym_append = Rf_install("append");
    sym_max = Rf_install("max");
    sym_any = Rf_install("any");
    sym_names = Rf_install("names");
    sym_names_default = Rf_install("names.default");
    sym_namesgets = Rf_install("names<-");
    sym_namesgets_default = Rf_install("names<-.default");
    sym_prod = Rf_install("prod");
    sym_t = Rf_install("t");
    sym_t_default = Rf_install("t.default");
    sym_log = Rf_install("log");
    sym_matrix = Rf_install("matrix");
    sym_as_vector = Rf_install("as.vector");
    sym_c = Rf_install("c");
    sym_unlist = Rf_install("unlist");
    sym_aperm = Rf_install("aperm");
}
 
Rboolean stack_alloc_safe(SEXP body, ApplyClosureOptions options, SEXP callee_sym)
{
    /* to avoid a lot of calls to Rf_install */
    static Rboolean init_done = FALSE;
    if (!init_done) {
	init_symbols();
	init_done = TRUE;
    }
    
    if (options & AC_RCC) {
	return (options & AC_STACK_CLOSURE);
	/* Safe only if the environment does not escape. This is
	   conservative. If the environment is not safe to stack
	   allocate, then neither are the things the environment
	   points to, which includes any object bound to a variable.
	   Temporary values that are never pointed to could be stack
	   allocated, but an unstackable environment is rare enough
	   that we might as well prohibit stack allocation altogether.
	   (The environment is determined to be unstackable only if
	   closure conversion determines that the closure might
	   escape.) */
    }
    if (callee_sym == sym_dnorm ||
	callee_sym == sym_pnorm ||
	callee_sym == sym_qnorm ||
	callee_sym == sym_dunif ||
	callee_sym == sym_punif ||
	callee_sym == sym_qunif ||
	callee_sym == sym_sum ||
	callee_sym == sym_vector ||
	callee_sym == sym_append ||
	callee_sym == sym_max ||
	callee_sym == sym_any ||
	callee_sym == sym_names ||
	callee_sym == sym_names_default ||
	callee_sym == sym_namesgets ||
	callee_sym == sym_namesgets_default ||
	callee_sym == sym_prod ||
	callee_sym == sym_t ||
	callee_sym == sym_t_default ||
	callee_sym == sym_log ||
	callee_sym == sym_matrix ||
	callee_sym == sym_as_vector ||
	callee_sym == sym_c ||
	callee_sym == sym_unlist ||
	callee_sym == sym_aperm
	)
    {
	return TRUE;
    }
    return FALSE;
}

#if 0
previously defined stack_alloc_unsafe; too many cases

    static SEXP sym_usemethod;
    static SEXP sym_eval;
    static SEXP sym_assign;
    static SEXP sym_postscript;

    sym_usemethod = Rf_install("UseMethod");
    sym_eval = Rf_install("eval");
    sym_assign = Rf_install("assign");
    sym_postscript = Rf_install("postscript");  /* calls gsetVar */

    if (!(options & AC_RCC) &&
	TYPEOF(body) == LANGSXP &&
	(CAR(body) == sym_usemethod ||
	 CAR(body) == sym_eval ||
	 CAR(body) == sym_assign ||
	 callee_sym == sym_postscript ||
	 strcmp(name, "axis") == 0
	 ))
    {
	return TRUE;
    }
    return FALSE;
#endif

#if 0
/* moved to duplicate.c */

/* For a variety of reasons, the duplication function for return
   values has to be different than duplicate(). */
SEXP mem_duplicate(SEXP x)
{
    int i;
    SEXP out;
    PROTECT(out = duplicate(x));
    if (TYPEOF(x) == CLOSXP) {
	SET_FORMALS(out, mem_duplicate(FORMALS(x)));
	SET_BODY(out, mem_duplicate(BODY(x)));
    } else if (TYPEOF(x) == STRSXP) {
	for(i = 0; i < LENGTH(x); i++) {
	    SET_STRING_ELT(out, i, mem_duplicate(STRING_ELT(x, i)));
	}
    } else if (TYPEOF(x) == PROMSXP) {
	SET_PRCODE(out, mem_duplicate(PRCODE(x)));
	SET_PRENV(out, mem_duplicate(PRENV(x)));
	SET_PRVALUE(out, mem_duplicate(PRVALUE(x)));
    }
    if (ATTRIB(x) != R_NilValue) {
	SET_ATTRIB(out, mem_duplicate(ATTRIB(x)));
    }
    UNPROTECT(1);
    return out;
}
#endif

/*
typedef enum {
    AC_RCC = 1,
    AC_MATCH_ARGS = 2,
    AC_CONTEXT = 4,
    AC_ENVIRONMENT = 8,
    AC_USEMETHOD = 16,
    AC_STACK_CLOSURE = 32,
    AC_DEFAULT = AC_MATCH_ARGS | AC_CONTEXT | AC_ENVIRONMENT | AC_USEMETHOD
} ApplyClosureOptions;
*/

/* Apply SEXP op of type CLOSXP to actuals */
SEXP applyClosureOpt(SEXP call, SEXP op, SEXP arglist, SEXP rho, SEXP suppliedenv, ApplyClosureOptions options, SEXP callee_sym)
{
    extern Rboolean global_stack_debug;
    SEXP body, formals, actuals, savedrho, funsxp;
    volatile  SEXP newrho;
    SEXP f, a, tmp;
    RCNTXT cntxt;
    SEXP stack_space;
    int nprotect = 0;
    int old_heap_alloc;
    const void * const allocStackTop = getAllocStackTop();
    const void * allocStackTop2;

    if (TYPEOF(op) == RCC_CLOSXP) {
      options |= AC_RCC;
    }

    /* formals = list of formal parameters */
    /* actuals = values to be bound to formals */
    /* arglist = the tagged list of arguments */

    if (options & AC_RCC) {
	formals = RCC_CLOSXP_FORMALS(op);
	funsxp = RCC_CLOSXP_FUN(op);
	savedrho = RCC_CLOSXP_CLOENV(op);
    } else {
	formals = FORMALS(op);
	body = BODY(op);
	savedrho = CLOENV(op);
    }

    /*
    if (name == NULL) {
	if (TYPEOF(CAR(call)) == SYMSXP) {
	    name = CHAR(PRINTNAME(CAR(call)));
	} else {
	    name = "";
	}
    }
    */

    if (global_dump_stats) {
      SEXP arg;
      char * source = (options & AC_RCC ? "C" : "I");
      fprintf(stderr, "Entering function %s %s [", CHAR(PRINTNAME(callee_sym)), source);
      for(arg = arglist; arg != R_NilValue; arg = CDR(arg)) {
	if (TYPEOF(CAR(arg)) == PROMSXP) {
	  fprintf(stderr, "P");
	} else {
	  fprintf(stderr, "N");
	}
      }
      fprintf(stderr, "]\n");
    }

    /*  Set up a context with the call in it so error has access to it */
    
    if (options & AC_CONTEXT) {
	begincontext(&cntxt, CTXT_RETURN, call, savedrho, rho, arglist, op);
    }

    if (options & AC_STACK_CLOSURE) {
	/* stack allocate matchArgs list (possibly), environment, promises */
	beginStackAlloc();
    } else {
	old_heap_alloc = getFallbackAlloc();
	setFallbackAlloc(TRUE);
    }

    /*  Build a list which matches the actual (unevaluated) arguments
	to the formal paramters.  Build a new environment which
	contains the matched pairs.  Ideally this environment sould be
	hashed.  */

    if (options & AC_MATCH_ARGS) {
      PROTECT(actuals = matchArgs(formals, arglist));
      nprotect++;
    } else {
      actuals = arglist;
    }

    if (options & AC_ENVIRONMENT) {
      PROTECT(newrho = NewEnvironment(formals, actuals, savedrho));
      nprotect++;
    } else {
      newrho = savedrho;
    }

    /*  Use the default code for unbound formals.  FIXME: It looks like
	this code should preceed the building of the environment so that
        this will also go into the hash table.  */

    /* This piece of code is destructively modifying the actuals list,
       which is now also the list of bindings in the frame of newrho.
       This is one place where internal structure of environment
       bindings leaks out of envir.c.  It should be rewritten
       eventually so as not to break encapsulation of the internal
       environment layout.  We can live with it for now since it only
       happens immediately after the environment creation.  LT */

    //    if (options & AC_MATCH_ARGS) {
	f = formals;
	a = actuals;
	while (f != R_NilValue) {
	    if (CAR(a) == R_MissingArg && CAR(f) != R_MissingArg) {
		SETCAR(a, mkPROMISE(CAR(f), newrho));
		SET_MISSING(a, 2);
	    }
	    f = CDR(f);
	    a = CDR(a);
	}
	//    }

    if (options & AC_STACK_CLOSURE) {
	/* we have already pushed the allocation stack space */
    } else {
	/* end the fallback alloc for the environment and args; alloc a new pool */
	setFallbackAlloc(old_heap_alloc);
	beginStackAlloc();
    }

    /*  Fix up any extras that were supplied by usemethod. */

    if (options & AC_USEMETHOD) {
	if (suppliedenv != R_NilValue) {
	    for (tmp = FRAME(suppliedenv); tmp != R_NilValue; tmp = CDR(tmp)) {
		for (a = actuals; a != R_NilValue; a = CDR(a))
		    if (TAG(a) == TAG(tmp))
			break;
		if (a == R_NilValue)
		    /* Use defineVar instead of earlier version that added
		       bindings manually */
		    defineVar(TAG(tmp), CAR(tmp), newrho);
	    }
	}
    }

    /*  Terminate the previous context and start a new one with the
        correct environment. */

    if (options & AC_CONTEXT) {
	endcontext(&cntxt);
    }

    /*  If we have a generic function we need to use the sysparent of
	the generic as the sysparent of the method because the method
	is a straight substitution of the generic.  */

    if (options & AC_CONTEXT) {
	if( R_GlobalContext->callflag == CTXT_GENERIC )
	    begincontext(&cntxt, CTXT_RETURN, call,
			 newrho, R_GlobalContext->sysparent, arglist, op);
	else
	    begincontext(&cntxt, CTXT_RETURN, call, newrho, rho, arglist, op);
    }

    /* The default return value is NULL.  FIXME: Is this really needed
       or do we always get a sensible value returned?  */

    tmp = R_NilValue;

    /* Debugging */

    if (!(options & AC_RCC)) {  /* can't get body of an RCC_CLOSXP */
	SET_DEBUG(newrho, DEBUG(op));
	if (DEBUG(op)) {
	    Rprintf("debugging in: ");
	    PrintValueRec(call,rho);
	    /* Is the body a bare symbol (PR#6804) */
	    if (!isSymbol(body) & !isVectorAtomic(body)){
		/* Find out if the body is function with only one statement. */
		if (isSymbol(CAR(body)))
		    tmp = findFun(CAR(body), rho);
		else
		    tmp = eval(CAR(body), rho);
		if((TYPEOF(tmp) == BUILTINSXP || TYPEOF(tmp) == SPECIALSXP)
		   && !strcmp( PRIMNAME(tmp), "for")
		   && !strcmp( PRIMNAME(tmp), "{")
		   && !strcmp( PRIMNAME(tmp), "repeat")
		   && !strcmp( PRIMNAME(tmp), "while")
		   )
		    goto regdb;
	    }
	    Rprintf("debug: ");
	    PrintValue(body);
	    do_browser(call,op,arglist,newrho);
	}
    }

 regdb:

    /*  It isn't completely clear that this is the right place to do
	this, but maybe (if the matchArgs above reverses the
	arguments) it might just be perfect.  */

#ifdef  HASHING
#define HASHTABLEGROWTHRATE  1.2
    {
	SEXP R_NewHashTable(int, double);
	SEXP R_HashFrame(SEXP);
	int nargs = length(arglist);
	HASHTAB(newrho) = R_NewHashTable(nargs, HASHTABLEGROWTHRATE);
	newrho = R_HashFrame(newrho);
    }
#endif
#undef  HASHING

    if (!stack_alloc_safe(body, options, callee_sym)) {
	old_heap_alloc = getFallbackAlloc();
	setFallbackAlloc(TRUE);
    }

    allocStackTop2 = getAllocStackTop();

    /*  Set a longjmp target which will catch any explicit returns
	from the function body.  */
    if ((options & AC_CONTEXT) && (SETJMP(cntxt.cjmpbuf))) {
	    if (R_ReturnedValue == R_RestartToken) {
		cntxt.callflag = CTXT_RETURN;  /* turn restart off */
		R_ReturnedValue = R_NilValue;  /* remove restart token */
		if (options & AC_RCC) {
		    PROTECT(tmp = RCC_FUNSXP_CFUN(funsxp) (actuals, newrho));
		} else {
		    PROTECT(tmp = eval(body, newrho));
		}
	    } else {
		PROTECT(tmp = R_ReturnedValue);
		/* recover from pushes that have happened */
		while (getAllocStackTop() != allocStackTop2) {
		    endStackAlloc();
		}
	    }
    } else {
	if (options & AC_RCC) {
	    PROTECT(tmp = RCC_FUNSXP_CFUN(funsxp) (actuals, newrho));
	} else {
	    PROTECT(tmp = eval(body, newrho));
	}
    }
    nprotect++;

    if (options & AC_CONTEXT) {
	endcontext(&cntxt);
    }

    if (getAllocStackTop() != allocStackTop2) {
	error(_("Allocation stack imbalance in eval"));
    }

    if (DEBUG(op)) {
	Rprintf("exiting from: ");
	PrintValueRec(call, rho);
    }

    if (!stack_alloc_safe(body, options, callee_sym)) {
        setFallbackAlloc(old_heap_alloc);
    } else if (getFallbackAlloc() == FALSE) {
	/* duplicate return value in parent pool */
	upAllocStack();
	tmp = duplicate(tmp);
	downAllocStack();
    }

    if (global_dump_stats) {
	fprintf(stderr, "Returning from function %s: pointers returned: ", CHAR(PRINTNAME(callee_sym)));
	printAllPointers(tmp);
	fprintf(stderr, "\n");
    }

    endStackAlloc();

    if (getAllocStackTop() != allocStackTop) {
	error(_("Allocation stack imbalance"));
    }

    UNPROTECT(nprotect);
    return (tmp);
}

/* Apply SEXP op of type RCC_CLOSXP to actuals */

/* **** FIXME: This code is factored out of applyClosure.  If we keep
   **** it we should modify this code and applyClosure share code when appropriate
   **** to avoid code drift. */

/* Changes from applyClosure:
   1. doesn't do object-oriented stuff with usemethod
   2. doesn't create a new context or end it after the call
   3. doesn't print debugging output of the function body
   4. doesn't do hashing even if HASHING is defined
   5. doesn't set a setjmp/longjmp target for explicit returns (we're
      assuming all calls to 'return' are compiled)
   6. calls RCC_FUNSXP_CFUN instead of eval-ing a function body
 */

SEXP applyRccClosure(SEXP call, SEXP op, SEXP arglist, SEXP rho, SEXP suppliedenv)
{
    SEXP funsxp, formals, actuals, savedrho;
    volatile  SEXP newrho;
    SEXP f, a, tmp;
    RCNTXT cntxt;

    /* formals = list of formal parameters */
    /* actuals = values to be bound to formals */
    /* arglist = the tagged list of arguments */

    formals = RCC_CLOSXP_FORMALS(op);
    funsxp = RCC_CLOSXP_FUN(op);
    savedrho = RCC_CLOSXP_CLOENV(op);


    /*  Build a list which matches the actual (unevaluated) arguments
	to the formal paramters.  Build a new environment which
	contains the matched pairs.  Ideally this environment sould be
	hashed.  */

    PROTECT(actuals = matchArgs(formals, arglist));
    PROTECT(newrho = NewEnvironment(formals, actuals, savedrho));

    /*  Use the default code for unbound formals.  FIXME: It looks like
	this code should preceed the building of the environment so that
        this will also go into the hash table.  */

    /* This piece of code is destructively modifying the actuals list,
       which is now also the list of bindings in the frame of newrho.
       This is one place where internal structure of environment
       bindings leaks out of envir.c.  It should be rewritten
       eventually so as not to break encapsulation of the internal
       environment layout.  We can live with it for now since it only
       happens immediately after the environment creation.  LT */

    f = formals;
    a = actuals;
    while (f != R_NilValue) {
	if (CAR(a) == R_MissingArg && CAR(f) != R_MissingArg) {
	    SETCAR(a, mkPROMISE(CAR(f), newrho));
	    SET_MISSING(a, 2);
	}
	f = CDR(f);
	a = CDR(a);
    }

#if 0
    /*  Fix up any extras that were supplied by usemethod. */

    if (suppliedenv != R_NilValue) {
	for (tmp = FRAME(suppliedenv); tmp != R_NilValue; tmp = CDR(tmp)) {
	    for (a = actuals; a != R_NilValue; a = CDR(a))
		if (TAG(a) == TAG(tmp))
		    break;
	    if (a == R_NilValue)
		/* Use defineVar instead of earlier version that added
                   bindings manually */
		defineVar(TAG(tmp), CAR(tmp), newrho);
	}
    }

    /*  Terminate the previous context and start a new one with the
        correct environment. */

    endcontext(&cntxt);

    /*  If we have a generic function we need to use the sysparent of
	the generic as the sysparent of the method because the method
	is a straight substitution of the generic.  */

    if( R_GlobalContext->callflag == CTXT_GENERIC )
	begincontext(&cntxt, CTXT_RETURN, call,
		     newrho, R_GlobalContext->sysparent, arglist, op);
    else
	begincontext(&cntxt, CTXT_RETURN, call, newrho, rho, arglist, op);

    /* The default return value is NULL.  FIXME: Is this really needed
       or do we always get a sensible value returned?  */

    tmp = R_NilValue;

    /* Debugging */

    SET_DEBUG(newrho, DEBUG(op));
    if (DEBUG(op)) {
	SEXP body = RCC_FUNSXP_BODYEXPR(funsxp);
	Rprintf("debugging in: ");
	PrintValueRec(call,rho);
	/* Is the body a bare symbol (PR#6804) */
	if (!isSymbol(body) & !isVectorAtomic(body)){
		/* Find out if the body is function with only one statement. */
		if (isSymbol(CAR(body)))
			tmp = findFun(CAR(body), rho);
		else
			tmp = eval(CAR(body), rho);
		if((TYPEOF(tmp) == BUILTINSXP || TYPEOF(tmp) == SPECIALSXP)
		   && !strcmp( PRIMNAME(tmp), "for")
		   && !strcmp( PRIMNAME(tmp), "{")
		   && !strcmp( PRIMNAME(tmp), "repeat")
		   && !strcmp( PRIMNAME(tmp), "while")
			)
			goto regdb;
	}
	Rprintf("debug: ");
	PrintValue(body);
	do_browser(call,op,arglist,newrho);
    }

 regdb:

    /*  It isn't completely clear that this is the right place to do
	this, but maybe (if the matchArgs above reverses the
	arguments) it might just be perfect.  */

#ifdef  HASHING
#define HASHTABLEGROWTHRATE  1.2
    {
	SEXP R_NewHashTable(int, double);
	SEXP R_HashFrame(SEXP);
	int nargs = length(arglist);
	HASHTAB(newrho) = R_NewHashTable(nargs, HASHTABLEGROWTHRATE);
	newrho = R_HashFrame(newrho);
    }
#endif
#undef  HASHING
#endif


#if 0
    if ((SETJMP(cntxt.cjmpbuf))) {
	if (R_ReturnedValue == R_RestartToken) {
	    cntxt.callflag = CTXT_RETURN;  /* turn restart off */
	    R_ReturnedValue = R_NilValue;  /* remove restart token */
	    PROTECT(tmp = eval(body, newrho));
	}
	else
	    PROTECT(tmp = R_ReturnedValue);
    }
    else {
	PROTECT(tmp = eval(body, newrho));
    }
#else
    PROTECT(tmp = RCC_FUNSXP_CFUN(funsxp) (actuals, newrho));
#endif

#if 0
    endcontext(&cntxt);
#endif

    if (DEBUG(op)) {
	Rprintf("exiting from: ");
	PrintValueRec(call, rho);
    }
    UNPROTECT(3);
    return (tmp);
}


/* Apply SEXP op of type RCC_CLOSXP to actuals, This version assumes
   no named or default arguments (all actual arguments are present and
   in the same order as the formal arguments). */

/* NB: This version is not used yet, but it will be useful when
   compile-time argument matching is done. */

/* **** FIXME: This code is factored out of applyClosure.  If we keep
   **** it we should modify this code and applyClosure share code when appropriate
   **** to avoid code drift. */

/* Changes from applyClosure:
   1. doesn't create a new environment (done ahead of time in the applyRccClosure caller)
   2. doesn't do object-oriented stuff with usemethod
   3. doesn't create a new context or end it after the call
   4. doesn't print debugging output of the function body
   5. doesn't do hashing even if HASHING is defined
   6. doesn't set a setjmp/longjmp target for explicit returns (we're
      assuming all calls to 'return' are compiled)
   7. calls RCC_FUNSXP_CFUN instead of eval-ing a function body

   Changes from applyRccClosure:
   1. doesn't handle named, default arguments
 */

/* TODO: call and rho are used only in debugging; suppliedenv is never
   used. Why not remove them? */

SEXP applyPlainRccClosure(SEXP call, SEXP op, SEXP arglist, SEXP rho, SEXP suppliedenv)
{
    SEXP funsxp, formals, actuals, savedrho;
    volatile  SEXP newrho;
    SEXP f, a, tmp;
    RCNTXT cntxt;

    /* formals = list of formal parameters */
    /* actuals = values to be bound to formals */
    /* arglist = the tagged list of arguments */

    formals = RCC_CLOSXP_FORMALS(op);
    funsxp = RCC_CLOSXP_FUN(op);
    savedrho = RCC_CLOSXP_CLOENV(op);

#if 0


    /*  Build a list which matches the actual (unevaluated) arguments
	to the formal paramters.  Build a new environment which
	contains the matched pairs.  Ideally this environment sould be
	hashed.  */

    PROTECT(actuals = matchArgs(formals, arglist));
#endif

    PROTECT(newrho = NewEnvironment(formals, arglist, savedrho));

#if 0

    /*  Use the default code for unbound formals.  FIXME: It looks like
	this code should preceed the building of the environment so that
        this will also go into the hash table.  */

    /* This piece of code is destructively modifying the actuals list,
       which is now also the list of bindings in the frame of newrho.
       This is one place where internal structure of environment
       bindings leaks out of envir.c.  It should be rewritten
       eventually so as not to break encapsulation of the internal
       environment layout.  We can live with it for now since it only
       happens immediately after the environment creation.  LT */

    f = formals;
    a = actuals;
    while (f != R_NilValue) {
	if (CAR(a) == R_MissingArg && CAR(f) != R_MissingArg) {
	    SETCAR(a, mkPROMISE(CAR(f), newrho));
	    SET_MISSING(a, 2);
	}
	f = CDR(f);
	a = CDR(a);
    }
#endif

#if 0
    /*  Fix up any extras that were supplied by usemethod. */

    if (suppliedenv != R_NilValue) {
	for (tmp = FRAME(suppliedenv); tmp != R_NilValue; tmp = CDR(tmp)) {
	    for (a = actuals; a != R_NilValue; a = CDR(a))
		if (TAG(a) == TAG(tmp))
		    break;
	    if (a == R_NilValue)
		/* Use defineVar instead of earlier version that added
                   bindings manually */
		defineVar(TAG(tmp), CAR(tmp), newrho);
	}
    }

    /*  Terminate the previous context and start a new one with the
        correct environment. */

    endcontext(&cntxt);

    /*  If we have a generic function we need to use the sysparent of
	the generic as the sysparent of the method because the method
	is a straight substitution of the generic.  */

    if( R_GlobalContext->callflag == CTXT_GENERIC )
	begincontext(&cntxt, CTXT_RETURN, call,
		     newrho, R_GlobalContext->sysparent, arglist, op);
    else
	begincontext(&cntxt, CTXT_RETURN, call, newrho, rho, arglist, op);

    /* The default return value is NULL.  FIXME: Is this really needed
       or do we always get a sensible value returned?  */

    tmp = R_NilValue;

    /* Debugging */

    SET_DEBUG(newrho, DEBUG(op));
    if (DEBUG(op)) {
	SEXP body = RCC_FUNSXP_BODYEXPR(funsxp);
	Rprintf("debugging in: ");
	PrintValueRec(call,rho);
	/* Is the body a bare symbol (PR#6804) */
	if (!isSymbol(body) & !isVectorAtomic(body)){
		/* Find out if the body is function with only one statement. */
		if (isSymbol(CAR(body)))
			tmp = findFun(CAR(body), rho);
		else
			tmp = eval(CAR(body), rho);
		if((TYPEOF(tmp) == BUILTINSXP || TYPEOF(tmp) == SPECIALSXP)
		   && !strcmp( PRIMNAME(tmp), "for")
		   && !strcmp( PRIMNAME(tmp), "{")
		   && !strcmp( PRIMNAME(tmp), "repeat")
		   && !strcmp( PRIMNAME(tmp), "while")
			)
			goto regdb;
	}
	Rprintf("debug: ");
	PrintValue(body);
	do_browser(call,op,arglist,newrho);
    }

 regdb:

    /*  It isn't completely clear that this is the right place to do
	this, but maybe (if the matchArgs above reverses the
	arguments) it might just be perfect.  */

#ifdef  HASHING
#define HASHTABLEGROWTHRATE  1.2
    {
	SEXP R_NewHashTable(int, double);
	SEXP R_HashFrame(SEXP);
	int nargs = length(arglist);
	HASHTAB(newrho) = R_NewHashTable(nargs, HASHTABLEGROWTHRATE);
	newrho = R_HashFrame(newrho);
    }
#endif
#undef  HASHING
#endif


#if 0
    if ((SETJMP(cntxt.cjmpbuf))) {
	if (R_ReturnedValue == R_RestartToken) {
	    cntxt.callflag = CTXT_RETURN;  /* turn restart off */
	    R_ReturnedValue = R_NilValue;  /* remove restart token */
	    PROTECT(tmp = eval(body, newrho));
	}
	else
	    PROTECT(tmp = R_ReturnedValue);
    }
    else {
	PROTECT(tmp = eval(body, newrho));
    }
#else
    PROTECT(tmp = RCC_FUNSXP_CFUN(funsxp) (arglist, newrho));
#endif

#if 0
    endcontext(&cntxt);
#endif

    if (DEBUG(op)) {
	Rprintf("exiting from: ");
	PrintValueRec(call, rho);
    }
    UNPROTECT(2);
    return (tmp);
}

SEXP applyPlainRccClosureArgs(SEXP op, ...) {
  SEXP funsxp, savedrho, tmp;
  va_list ap;

  funsxp = RCC_CLOSXP_FUN(op);
  savedrho = RCC_CLOSXP_CLOENV(op);

  va_start(ap, op);
  tmp = RCC_FUNSXP_CFUN(funsxp) (ap, savedrho);
  va_end(ap);
  return (tmp);
}

/* **** FIXME: This code is factored out of applyClosure.  If we keep
   **** it we should change applyClosure to run through this routine
   **** to avoid code drift. */
static SEXP R_execClosure(SEXP call, SEXP op, SEXP arglist, SEXP rho,
			  SEXP newrho)
{
    SEXP body, tmp;
    RCNTXT cntxt;

    body = BODY(op);

    begincontext(&cntxt, CTXT_RETURN, call, newrho, rho, arglist, op);

    /* The default return value is NULL.  FIXME: Is this really needed
       or do we always get a sensible value returned?  */

    tmp = R_NilValue;

    /* Debugging */

    SET_DEBUG(newrho, DEBUG(op));
    if (DEBUG(op)) {
	Rprintf("debugging in: ");
	PrintValueRec(call,rho);
	/* Find out if the body is function with only one statement. */
	if (isSymbol(CAR(body)))
	    tmp = findFun(CAR(body), rho);
	else
	    tmp = eval(CAR(body), rho);
	if((TYPEOF(tmp) == BUILTINSXP || TYPEOF(tmp) == SPECIALSXP)
	   && !strcmp( PRIMNAME(tmp), "for")
	   && !strcmp( PRIMNAME(tmp), "{")
	   && !strcmp( PRIMNAME(tmp), "repeat")
	   && !strcmp( PRIMNAME(tmp), "while")
	   )
	    goto regdb;
	Rprintf("debug: ");
	PrintValue(body);
	do_browser(call,op,arglist,newrho);
    }

 regdb:

    /*  It isn't completely clear that this is the right place to do
	this, but maybe (if the matchArgs above reverses the
	arguments) it might just be perfect.  */

#ifdef  HASHING
#define HASHTABLEGROWTHRATE  1.2
    {
	SEXP R_NewHashTable(int, double);
	SEXP R_HashFrame(SEXP);
	int nargs = length(arglist);
	HASHTAB(newrho) = R_NewHashTable(nargs, HASHTABLEGROWTHRATE);
	newrho = R_HashFrame(newrho);
    }
#endif
#undef  HASHING

    /*  Set a longjmp target which will catch any explicit returns
	from the function body.  */

    if ((SETJMP(cntxt.cjmpbuf))) {
	if (R_ReturnedValue == R_RestartToken) {
	    cntxt.callflag = CTXT_RETURN;  /* turn restart off */
	    R_ReturnedValue = R_NilValue;  /* remove restart token */
	    PROTECT(tmp = eval(body, newrho));
	}
	else
	    PROTECT(tmp = R_ReturnedValue);
    }
    else {
	PROTECT(tmp = eval(body, newrho));
    }

    endcontext(&cntxt);

    if (DEBUG(op)) {
	Rprintf("exiting from: ");
	PrintValueRec(call, rho);
    }
    UNPROTECT(1);
    return (tmp);
}

/* **** FIXME: Temporary code to execute S4 methods in a way that
   **** preserves lexical scope. */

static SEXP R_dot_Generic = NULL;
static SEXP R_dot_Method = NULL;
static SEXP R_dot_Methods = NULL;
static SEXP R_dot_defined = NULL;
static SEXP R_dot_target = NULL;

SEXP R_execMethod(SEXP op, SEXP rho)
{
    SEXP call, arglist, callerenv, newrho, next, val;
    RCNTXT *cptr;

    if (R_dot_Generic == NULL) {
	R_dot_Generic = install(".Generic");
	R_dot_Method = install(".Method");
	R_dot_Methods = install(".Methods");
	R_dot_defined = install(".defined");
	R_dot_target = install(".target");
    }

    /* create a new environment frame enclosed by the lexical
       environment of the method */
    PROTECT(newrho = Rf_NewEnvironment(R_NilValue, R_NilValue, CLOENV(op)));

    /* copy the bindings for the formal environment from the top frame
       of the internal environment of the generic call to the new
       frame.  need to make sure missingness information is preserved
       and the environments for any default expression promises are
       set to the new environment.  should move this to envir.c where
       it can be done more efficiently. */
    for (next = FORMALS(op); next != R_NilValue; next = CDR(next)) {
	SEXP symbol =  TAG(next);
	R_varloc_t loc;
	int missing;
	loc = R_findVarLocInFrame(rho,symbol);
	if(loc == NULL)
	    error(_("could not find symbol \"%s\" in environment of the generic function"),
		  CHAR(PRINTNAME(symbol)));
	missing = R_GetVarLocMISSING(loc);
	val = R_GetVarLocValue(loc);
	SET_FRAME(newrho, CONS(val, FRAME(newrho)));
	SET_TAG(FRAME(newrho), symbol);
	if (missing) {
	    SET_MISSING(FRAME(newrho), missing);
	    if (TYPEOF(val) == PROMSXP && PRENV(val) == rho) {
		SEXP deflt;
		SET_PRENV(val, newrho);
		/* find the symbol in the method, copy its expression
		 * to the promise */ 
		for(deflt = CAR(op); deflt != R_NilValue; deflt = CDR(deflt)) {
		    if(TAG(deflt) == symbol)
		        break;
		}
		if(deflt == R_NilValue)
		    error(_("Symbol \"%s\" not in environment of method"),
			  CHAR(PRINTNAME(symbol)));
		SET_PRCODE(val, CAR(deflt));
	    }
	}
    }

    /* copy the bindings of the spacial dispatch variables in the top
       frame of the generic call to the new frame */
    defineVar(R_dot_defined, findVarInFrame(rho, R_dot_defined), newrho);
    defineVar(R_dot_Method, findVarInFrame(rho, R_dot_Method), newrho);
    defineVar(R_dot_target, findVarInFrame(rho, R_dot_target), newrho);

    /* copy the bindings for .Generic and .Methods.  We know (I think)
       that they are in the second frame, so we could use that. */
    defineVar(R_dot_Generic, findVar(R_dot_Generic, rho), newrho);
    defineVar(R_dot_Methods, findVar(R_dot_Methods, rho), newrho);

    /* Find the calling context.  Should be R_GlobalContext unless
       profiling has inserted a CTXT_BUILTIN frame. */
    cptr = R_GlobalContext;
    if (cptr->callflag & CTXT_BUILTIN)
	cptr = cptr->nextcontext;

    /* The calling environment should either be the environment of the
       generic, rho, or the environment of the caller of the generic,
       the current sysparent. */
    callerenv = cptr->sysparent; /* or rho? */

    /* get the rest of the stuff we need from the current context,
       execute the method, and return the result */
    call = cptr->call;
    arglist = cptr->promargs;
    val = R_execClosure(call, op, arglist, callerenv, newrho);
    UNPROTECT(1);
    return val;
}

static SEXP EnsureLocal(SEXP symbol, SEXP rho)
{
    SEXP vl;

    if ((vl = findVarInFrame3(rho, symbol, TRUE)) != R_UnboundValue) {
	vl = eval(symbol, rho);	/* for promises */
	if(NAMED(vl) == 2) {
	    PROTECT(vl = duplicate(vl));
	    defineVar(symbol, vl, rho);
	    UNPROTECT(1);
	}
	return vl;
    }

    vl = eval(symbol, ENCLOS(rho));
    if (vl == R_UnboundValue)
	error(_("Object \"%s\" not found"), CHAR(PRINTNAME(symbol)));

    PROTECT(vl = duplicate(vl));
    defineVar(symbol, vl, rho);
    UNPROTECT(1);
    SET_NAMED(vl, 1);
    return vl;
}


/* Note: If val is a language object it must be protected */
/* to prevent evaluation.  As an example consider */
/* e <- quote(f(x=1,y=2); names(e) <- c("","a","b") */

static SEXP replaceCall(SEXP fun, SEXP val, SEXP args, SEXP rhs)
{
    SEXP tmp, ptmp;
    PROTECT(fun);
    PROTECT(args);
    PROTECT(rhs);
    PROTECT(val);
    ptmp = tmp = allocList(length(args)+3);
    UNPROTECT(4);
    SETCAR(ptmp, fun); ptmp = CDR(ptmp);
    SETCAR(ptmp, val); ptmp = CDR(ptmp);
    while(args != R_NilValue) {
	SETCAR(ptmp, CAR(args));
        SET_TAG(ptmp, TAG(args));
	ptmp = CDR(ptmp);
	args = CDR(args);
    }
    SETCAR(ptmp, rhs);
    SET_TAG(ptmp, install("value"));
    SET_TYPEOF(tmp, LANGSXP);
    return tmp;
}


static SEXP assignCall(SEXP op, SEXP symbol, SEXP fun,
		       SEXP val, SEXP args, SEXP rhs)
{
    PROTECT(op);
    PROTECT(symbol);
    val = replaceCall(fun, val, args, rhs);
    UNPROTECT(2);
    return lang3(op, symbol, val);
}


/* It might be a tad more efficient to make the non-error part of this
   into a macro, especially for while loops. */
static Rboolean asLogicalNoNA(SEXP s, SEXP call)
{
    Rboolean cond = asLogical(s);
    if (length(s) > 1)
	warningcall(call, 
		    _("the condition has length > 1 and only the first element will be used"));
    if (cond == NA_LOGICAL) {
	char *msg = length(s) ? (isLogical(s) ?
				 _("missing value where TRUE/FALSE needed") :
				 _("argument is not interpretable as logical")) :
	    _("argument is of length zero");
	errorcall(call, msg);
    }
    return cond;
}


SEXP do_if(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    SEXP Cond = eval(CAR(args), rho);

    if (asLogicalNoNA(Cond, call))
	return (eval(CAR(CDR(args)), rho));
    else if (length(args) > 2)
	return (eval(CAR(CDR(CDR(args))), rho));
    R_Visible = 0;
    return R_NilValue;
}


#define BodyHasBraces(body) \
    ((isLanguage(body) && CAR(body) == R_BraceSymbol) ? 1 : 0)

#define DO_LOOP_DEBUG(call, op, args, rho, bgn) do { \
    if (bgn && DEBUG(rho)) { \
	Rprintf("debug: "); \
	PrintValue(CAR(args)); \
	do_browser(call,op,args,rho); \
    } } while (0)


SEXP do_for(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    int dbg;
    volatile int i, n, bgn;
    SEXP sym, body;
    volatile SEXP ans, v, val;
    RCNTXT cntxt;
    PROTECT_INDEX vpi, api;

    sym = CAR(args);
    val = CADR(args);
    body = CADDR(args);

    if ( !isSymbol(sym) ) errorcall(call, _("non-symbol loop variable"));

    PROTECT(args);
    PROTECT(rho);
    PROTECT(val = eval(val, rho));
    defineVar(sym, R_NilValue, rho);
    if (isList(val) || isNull(val)) {
	n = length(val);
	PROTECT_WITH_INDEX(v = R_NilValue, &vpi);
    }
    else {
	n = LENGTH(val);
	PROTECT_WITH_INDEX(v = allocVector(TYPEOF(val), 1), &vpi);
    }
    ans = R_NilValue;

    dbg = DEBUG(rho);
    bgn = BodyHasBraces(body);

    PROTECT_WITH_INDEX(ans, &api);
    begincontext(&cntxt, CTXT_LOOP, R_NilValue, rho, R_NilValue, R_NilValue,
		 R_NilValue);
    switch (SETJMP(cntxt.cjmpbuf)) {
    case CTXT_BREAK: goto for_break;
    case CTXT_NEXT: goto for_next;
    }
    for (i = 0; i < n; i++) {
	DO_LOOP_DEBUG(call, op, args, rho, bgn);
	switch (TYPEOF(val)) {
	case LGLSXP:
	    REPROTECT(v = allocVector(TYPEOF(val), 1), vpi);
	    LOGICAL(v)[0] = LOGICAL(val)[i];
	    setVar(sym, v, rho);
	    break;
	case INTSXP:
	    REPROTECT(v = allocVector(TYPEOF(val), 1), vpi);
	    INTEGER(v)[0] = INTEGER(val)[i];
	    setVar(sym, v, rho);
	    break;
	case REALSXP:
	    REPROTECT(v = allocVector(TYPEOF(val), 1), vpi);
	    REAL(v)[0] = REAL(val)[i];
	    setVar(sym, v, rho);
	    break;
	case CPLXSXP:
	    REPROTECT(v = allocVector(TYPEOF(val), 1), vpi);
	    COMPLEX(v)[0] = COMPLEX(val)[i];
	    setVar(sym, v, rho);
	    break;
	case STRSXP:
	    REPROTECT(v = allocVector(TYPEOF(val), 1), vpi);
	    SET_STRING_ELT(v, 0, STRING_ELT(val, i));
	    setVar(sym, v, rho);
	    break;
	case EXPRSXP:
	case VECSXP:
	    setVar(sym, VECTOR_ELT(val, i), rho);
	    break;
	case LISTSXP:
	    setVar(sym, CAR(val), rho);
	    val = CDR(val);
	    break;
	default: errorcall(call, _("bad for() loop sequence"));
	}
	REPROTECT(ans = eval(body, rho), api);
    for_next:
	; /* needed for strict ISO C compilance, according to gcc 2.95.2 */
    }
 for_break:
    endcontext(&cntxt);
    UNPROTECT(5);
    R_Visible = 0;
    SET_DEBUG(rho, dbg);
    return ans;
}


SEXP do_while(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    int dbg;
    volatile int bgn;
    volatile SEXP t, body;
    RCNTXT cntxt;
    PROTECT_INDEX tpi;

    checkArity(op, args);

    dbg = DEBUG(rho);
    body = CADR(args);
    bgn = BodyHasBraces(body);

    t = R_NilValue;
    PROTECT_WITH_INDEX(t, &tpi);
    begincontext(&cntxt, CTXT_LOOP, R_NilValue, rho, R_NilValue, R_NilValue,
		 R_NilValue);
    if (SETJMP(cntxt.cjmpbuf) != CTXT_BREAK) {
	while (asLogicalNoNA(eval(CAR(args), rho), call)) {
	    DO_LOOP_DEBUG(call, op, args, rho, bgn);
	    REPROTECT(t = eval(body, rho), tpi);
	}
    }
    endcontext(&cntxt);
    UNPROTECT(1);
    R_Visible = 0;
    SET_DEBUG(rho, dbg);
    return t;
}


SEXP do_repeat(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    int dbg;
    volatile int bgn;
    volatile SEXP t, body;
    RCNTXT cntxt;
    PROTECT_INDEX tpi;

    checkArity(op, args);

    dbg = DEBUG(rho);
    body = CAR(args);
    bgn = BodyHasBraces(body);

    t = R_NilValue;
    PROTECT_WITH_INDEX(t, &tpi);
    begincontext(&cntxt, CTXT_LOOP, R_NilValue, rho, R_NilValue, R_NilValue,
		 R_NilValue);
    if (SETJMP(cntxt.cjmpbuf) != CTXT_BREAK) {
	for (;;) {
	    DO_LOOP_DEBUG(call, op, args, rho, bgn);
	    REPROTECT(t = eval(body, rho), tpi);
	}
    }
    endcontext(&cntxt);
    UNPROTECT(1);
    R_Visible = 0;
    SET_DEBUG(rho, dbg);
    return t;
}


SEXP do_break(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    findcontext(PRIMVAL(op), rho, R_NilValue);
    return R_NilValue;
}


SEXP do_paren(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    checkArity(op, args);
    return CAR(args);
}


SEXP do_begin(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    SEXP s;
    if (args == R_NilValue) {
	s = R_NilValue;
    }
    else {
	while (args != R_NilValue) {
	    if (DEBUG(rho)) {
		Rprintf("debug: ");
		PrintValue(CAR(args));
		do_browser(call,op,args,rho);
	    }
	    s = eval(CAR(args), rho);
	    args = CDR(args);
	}
    }
    return s;
}


SEXP do_return(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    SEXP a, v, vals;
    int nv = 0;

    /* We do the evaluation here so that we can tag any untagged
       return values if they are specified by symbols. */

    /* this used to crash with missing args, so keep them and check later */
    PROTECT(vals = evalListKeepMissing(args, rho));
    a = args;
    v = vals;
    while (!isNull(a)) {
	nv += 1;
	if (CAR(a) == R_DotsSymbol)
	    error(_("... not allowed in return"));
	if (isNull(TAG(a)) && isSymbol(CAR(a)))
	    SET_TAG(v, CAR(a));
	a = CDR(a);
	v = CDR(v);
    }
    switch(nv) {
    case 0:
	v = R_NilValue;
	break;
    case 1:
	v = CAR(vals);
	break;
    default:
	warningcall(call, _("multi-argument returns are deprecated"));
	for (v = vals; v != R_NilValue; v = CDR(v)) {
	    if (CAR(v) == R_MissingArg)
		error(_("empty expression in return value"));
	    if (NAMED(CAR(v)))
		SETCAR(v, duplicate(CAR(v)));
	}
	v = PairToVectorList(vals);
	break;
    }
    UNPROTECT(1);

    findcontext(CTXT_BROWSER | CTXT_FUNCTION, rho, v);

    return R_NilValue; /*NOTREACHED*/
}


static SEXP forcePromise(SEXP e);

SEXP do_function(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    SEXP rval;

    if (TYPEOF(op) == PROMSXP) {
	op = forcePromise(op);
	SET_NAMED(op, 2);
    }
    if (length(args) < 2)
	WrongArgCount("lambda");
    CheckFormals(CAR(args));
    rval = mkCLOSXP(CAR(args), CADR(args), rho);
    setAttrib(rval, R_SourceSymbol, CADDR(args));
    return rval;
}


/*
 *  Assignments for complex LVAL specifications. This is the stuff that
 *  nightmares are made of ...	Note that "evalseq" preprocesses the LHS
 *  of an assignment.  Given an expression, it builds a list of partial
 *  values for the exression.  For example, the assignment x$a[3] <- 10
 *  with LHS x$a[3] yields the (improper) list:
 *
 *	 (eval(x$a[3])	eval(x$a)  eval(x)  .  x)
 *
 *  (Note the terminating symbol).  The partial evaluations are carried
 *  out efficiently using previously computed components.
 */

/*
  For complex superassignment  x[y==z]<<-w
  we want x required to be nonlocal, y,z, and w permitted to be local or nonlocal.
*/

static SEXP evalseq(SEXP expr, SEXP rho, int forcelocal,  R_varloc_t tmploc)
{
    SEXP val, nval, nexpr;
    if (isNull(expr))
	error(_("invalid (NULL) left side of assignment"));
    if (isSymbol(expr)) {
	PROTECT(expr);
	if(forcelocal) {
	    nval = EnsureLocal(expr, rho);
	}
	else {/* now we are down to the target symbol */
	  nval = eval(expr, ENCLOS(rho));
	}
	UNPROTECT(1);
	return CONS(nval, expr);
    }
    else if (isLanguage(expr)) {
	PROTECT(expr);
	PROTECT(val = evalseq(CADR(expr), rho, forcelocal, tmploc));
	R_SetVarLocValue(tmploc, CAR(val));
	PROTECT(nexpr = LCONS(R_GetVarLocSymbol(tmploc), CDDR(expr)));
	PROTECT(nexpr = LCONS(CAR(expr), nexpr));
	nval = eval(nexpr, rho);
	UNPROTECT(4);
	return CONS(nval, val);
    }
    else error(_("Target of assignment expands to non-language object"));
    return R_NilValue;	/*NOTREACHED*/
}

/* Main entry point for complex assignments */
/* We have checked to see that CAR(args) is a LANGSXP */

static const char * const asym[] = {":=", "<-", "<<-", "="};


static SEXP applydefine(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    SEXP expr, lhs, rhs, saverhs, tmp, tmp2;
    R_varloc_t tmploc;
    char buf[32];

    expr = CAR(args);

    /*  It's important that the rhs get evaluated first because
	assignment is right associative i.e.  a <- b <- c is parsed as
	a <- (b <- c).  */

    PROTECT(saverhs = rhs = eval(CADR(args), rho));

    /*  FIXME: We need to ensure that this works for hashed
        environments.  This code only works for unhashed ones.  the
        syntax error here is a delibrate marker so I don't forget that
        this needs to be done.  The code used in "missing" will help
        here.  */

    /*  FIXME: This strategy will not work when we are working in the
	data frame defined by the system hash table.  The structure there
	is different.  Should we special case here?  */

    /*  We need a temporary variable to hold the intermediate values
	in the computation.  For efficiency reasons we record the
	location where this variable is stored.  */

    if (rho == R_BaseNamespace)
	errorcall(call, _("cannot do complex assignments in base namespace"));
    if (rho == R_NilValue)
	errorcall(call, _("cannot do complex assignments in NULL environment"));
    defineVar(R_TmpvalSymbol, R_NilValue, rho);
    tmploc = R_findVarLocInFrame(rho, R_TmpvalSymbol);

    /*  Do a partial evaluation down through the LHS. */
    lhs = evalseq(CADR(expr), rho,
                  PRIMVAL(op)==1 || PRIMVAL(op)==3, tmploc);

    PROTECT(lhs);
    PROTECT(rhs); /* To get the loop right ... */

    while (isLanguage(CADR(expr))) {
	if (TYPEOF(CAR(expr)) != SYMSXP)
	    error(_("invalid function in complex assignment"));
	if(strlen(CHAR(PRINTNAME(CAR(expr)))) + 3 > 32)
	    error(_("overlong name in '%s'"), CHAR(PRINTNAME(CAR(expr))));
	sprintf(buf, "%s<-", CHAR(PRINTNAME(CAR(expr))));
	tmp = install(buf);
	UNPROTECT(1);
	R_SetVarLocValue(tmploc, CAR(lhs));
	PROTECT(tmp2 = mkPROMISE(rhs, rho));
	SET_PRVALUE(tmp2, rhs);
	PROTECT(rhs = replaceCall(tmp, R_GetVarLocSymbol(tmploc), CDDR(expr),
				  tmp2));
	rhs = eval(rhs, rho);
	UNPROTECT(2);
	PROTECT(rhs);
	lhs = CDR(lhs);
	expr = CADR(expr);
    }
    if (TYPEOF(CAR(expr)) != SYMSXP)
	error(_("invalid function in complex assignment"));
    if(strlen(CHAR(PRINTNAME(CAR(expr)))) + 3 > 32)
	error(_("overlong name in '%s'"), CHAR(PRINTNAME(CAR(expr))));
    sprintf(buf, "%s<-", CHAR(PRINTNAME(CAR(expr))));
    R_SetVarLocValue(tmploc, CAR(lhs));
    PROTECT(tmp = mkPROMISE(CADR(args), rho));
    SET_PRVALUE(tmp, rhs);
    PROTECT(expr = assignCall(install(asym[PRIMVAL(op)]), CDR(lhs),
			      install(buf), R_GetVarLocSymbol(tmploc),
			      CDDR(expr), tmp));
    expr = eval(expr, rho);
    UNPROTECT(5);
    unbindVar(R_TmpvalSymbol, rho);
    if (global_dump_stats && (PRIMVAL(op)==0 || PRIMVAL(op)==2)) {
	fprintf(stderr, "Nonlocal complex assignment: pointers: ");
	printAllPointers(saverhs);
	fprintf(stderr, "\n");
    }
#ifdef CONSERVATIVE_COPYING
    return duplicate(saverhs);
#else
    /* we do not duplicate the value, so to be conservative mark the
       value as NAMED = 2 */
    SET_NAMED(saverhs, 2);
    return saverhs;
#endif
}

/* Defunct in in 1.5.0
SEXP do_alias(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    checkArity(op,args);
    Rprintf(".Alias is deprecated; there is no replacement \n");
    SET_NAMED(CAR(args), 0);
    return CAR(args);
}
*/

/*  Assignment in its various forms  */

SEXP do_set(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    SEXP s;
    if (length(args) != 2)
	WrongArgCount(asym[PRIMVAL(op)]);
    if (isString(CAR(args)))
	SETCAR(args, install(CHAR(STRING_ELT(CAR(args), 0))));

    switch (PRIMVAL(op)) {
    case 1: case 3:					/* <-, = */
	if (isSymbol(CAR(args))) {
	    s = eval(CADR(args), rho);
#ifdef CONSERVATIVE_COPYING
	    if (NAMED(s))
	    {
		SEXP t;
		PROTECT(s);
		t = duplicate(s);
		UNPROTECT(1);
		s = t;
	    }
	    PROTECT(s);
	    R_Visible = 0;
	    defineVar(CAR(args), s, rho);
	    UNPROTECT(1);
	    SET_NAMED(s, 1);
#else
	    switch (NAMED(s)) {
	    case 0: SET_NAMED(s, 1); break;
	    case 1: SET_NAMED(s, 2); break;
	    }
	    R_Visible = 0;
	    defineVar(CAR(args), s, rho);
#endif
	    return (s);
	}
	else if (isLanguage(CAR(args))) {
	    R_Visible = 0;
	    return applydefine(call, op, args, rho);
	}
	else errorcall(call, 
		       _("invalid (do_set) left-hand side to assignment"));
    case 2:						/* <<- */
	if (isSymbol(CAR(args))) {
	    s = eval(CADR(args), rho);
	    if (NAMED(s))
		s = duplicate(s);
	    PROTECT(s);
	    R_Visible = 0;
	    setVar(CAR(args), s, ENCLOS(rho));
	    UNPROTECT(1);
	    SET_NAMED(s, 1);
	    if (global_dump_stats) {
		fprintf(stderr, "Nonlocal assignment: pointers assigned: ");
		printAllPointers(s);
		fprintf(stderr, "\n");
	    }
	    return s;
	}
	else if (isLanguage(CAR(args)))
	    return applydefine(call, op, args, rho);
	else error(_("invalid assignment left-hand side"));

    default:
	UNIMPLEMENTED("do_set");

    }
    return R_NilValue;/*NOTREACHED*/
}


/* Evaluate each expression in "el" in the environment "rho".  This is */
/* a naturally recursive algorithm, but we use the iterative form below */
/* because it is does not cause growth of the pointer protection stack, */
/* and because it is a little more efficient. */

SEXP evalList(SEXP el, SEXP rho)
{
    SEXP ans, h, tail;

    PROTECT(ans = tail = CONS(R_NilValue, R_NilValue));

    while (el != R_NilValue) {

	/* If we have a ... symbol, we look to see what it is bound to.
	 * If its binding is Null (i.e. zero length)
	 *	we just ignore it and return the cdr with all its expressions evaluated;
	 * if it is bound to a ... list of promises,
	 *	we force all the promises and then splice
	 *	the list of resulting values into the return value.
	 * Anything else bound to a ... symbol is an error
	*/
	if (CAR(el) == R_DotsSymbol) {
	    h = findVar(CAR(el), rho);
	    if (TYPEOF(h) == DOTSXP || h == R_NilValue) {
		while (h != R_NilValue) {
		    SETCDR(tail, CONS(eval(CAR(h), rho), R_NilValue));
		    SET_TAG(CDR(tail), CreateTag(TAG(h)));
		    tail = CDR(tail);
		    h = CDR(h);
		}
	    }
	    else if (h != R_MissingArg)
		error(_("... used in an incorrect context"));
	}
	else if (CAR(el) != R_MissingArg) {
	    SETCDR(tail, CONS(eval(CAR(el), rho), R_NilValue));
	    tail = CDR(tail);
	    SET_TAG(tail, CreateTag(TAG(el)));
	}
	el = CDR(el);
    }
    UNPROTECT(1);
    return CDR(ans);
}/* evalList() */


/* A slight variation of evaluating each expression in "el" in "rho". */
/* This is a naturally recursive algorithm, but we use the iterative */
/* form below because it is does not cause growth of the pointer */
/* protection stack, and because it is a little more efficient. */

SEXP evalListKeepMissing(SEXP el, SEXP rho)
{
    SEXP ans, h, tail;

    PROTECT(ans = tail = CONS(R_NilValue, R_NilValue));

    while (el != R_NilValue) {

	/* If we have a ... symbol, we look to see what it is bound to.
	 * If its binding is Null (i.e. zero length)
	 *	we just ignore it and return the cdr with all its expressions evaluated;
	 * if it is bound to a ... list of promises,
	 *	we force all the promises and then splice
	 *	the list of resulting values into the return value.
	 * Anything else bound to a ... symbol is an error
	*/
	if (CAR(el) == R_DotsSymbol) {
	    h = findVar(CAR(el), rho);
	    if (TYPEOF(h) == DOTSXP || h == R_NilValue) {
		while (h != R_NilValue) {
		    if (CAR(h) == R_MissingArg)
			SETCDR(tail, CONS(R_MissingArg, R_NilValue));
		    else
			SETCDR(tail, CONS(eval(CAR(h), rho), R_NilValue));
		    SET_TAG(CDR(tail), CreateTag(TAG(h)));
		    tail = CDR(tail);
		    h = CDR(h);
		}
	    }
	    else if(h != R_MissingArg)
		error(_("... used in an incorrect context"));
	}
	else if (CAR(el) == R_MissingArg) {
	    SETCDR(tail, CONS(R_MissingArg, R_NilValue));
	    tail = CDR(tail);
	    SET_TAG(tail, CreateTag(TAG(el)));
	}
	else {
	    SETCDR(tail, CONS(eval(CAR(el), rho), R_NilValue));
	    tail = CDR(tail);
	    SET_TAG(tail, CreateTag(TAG(el)));
	}
	el = CDR(el);
    }
    UNPROTECT(1);
    return CDR(ans);
}


/* Create a promise to evaluate each argument.	Although this is most */
/* naturally attacked with a recursive algorithm, we use the iterative */
/* form below because it is does not cause growth of the pointer */
/* protection stack, and because it is a little more efficient. */

SEXP promiseArgs(SEXP el, SEXP rho)
{
    SEXP ans, h, tail;

    PROTECT(ans = tail = CONS(R_NilValue, R_NilValue));

    while(el != R_NilValue) {

	/* If we have a ... symbol, we look to see what it is bound to.
	 * If its binding is Null (i.e. zero length)
	 * we just ignore it and return the cdr with all its
	 * expressions promised; if it is bound to a ... list
	 * of promises, we repromise all the promises and then splice
	 * the list of resulting values into the return value.
	 * Anything else bound to a ... symbol is an error
	 */

	/* Is this double promise mechanism really needed? */

	if (CAR(el) == R_DotsSymbol) {
	    h = findVar(CAR(el), rho);
	    if (TYPEOF(h) == DOTSXP || h == R_NilValue) {
		while (h != R_NilValue) {
		    SETCDR(tail, CONS(mkPROMISE(CAR(h), rho), R_NilValue));
		    SET_TAG(CDR(tail), CreateTag(TAG(h)));
		    tail = CDR(tail);
		    h = CDR(h);
		}
	    }
	    else if (h != R_MissingArg)
		error(_("... used in an incorrect context"));
	}
	else if (CAR(el) == R_MissingArg) {
	    SETCDR(tail, CONS(R_MissingArg, R_NilValue));
	    tail = CDR(tail);
	    SET_TAG(tail, CreateTag(TAG(el)));
	}
	else {
	    SETCDR(tail, CONS(mkPROMISE(CAR(el), rho), R_NilValue));
	    tail = CDR(tail);
	    SET_TAG(tail, CreateTag(TAG(el)));
	}
	el = CDR(el);
    }
    UNPROTECT(1);
    return CDR(ans);
}


/* Check that each formal is a symbol */

void CheckFormals(SEXP ls)
{
    if (isList(ls)) {
	for (; ls != R_NilValue; ls = CDR(ls))
	    if (TYPEOF(TAG(ls)) != SYMSXP)
		goto err;
	return;
    }
 err:
    error(_("invalid formal argument list for \"function\""));
}



/* "eval" and "eval.with.vis" : Evaluate the first argument */
/* in the environment specified by the second argument. */

SEXP do_eval(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    SEXP encl, x, xptr;
    volatile SEXP expr, env, tmp;

    int frame;
    RCNTXT cntxt;

    checkArity(op, args);
    expr = CAR(args);
    env = CADR(args);
    encl = CADDR(args);
    if ( !isNull(encl) && !isEnvironment(encl) )
	errorcall(call, _("invalid third argument"));
    switch(TYPEOF(env)) {
    case NILSXP:
        env = encl;     /* so eval(expr, NULL, encl) works */
    case ENVSXP:
	PROTECT(env);	/* so we can unprotect 2 at the end */
	break;
    case LISTSXP:
	env = NewEnvironment(R_NilValue, duplicate(CADR(args)), encl);
	PROTECT(env);
	break;
    case VECSXP:
	x = VectorToPairList(CADR(args));
	for (xptr = x ; xptr != R_NilValue ; xptr = CDR(xptr))
	    SET_NAMED(CAR(xptr) , 2);
	env = NewEnvironment(R_NilValue, x, encl);
	PROTECT(env);
	break;
    case INTSXP:
    case REALSXP:
	if (length(env) != 1)
	    errorcall(call, _("numeric 'envir' arg not of length one"));
	frame = asInteger(env);
	if (frame == NA_INTEGER)
	    errorcall(call, _("invalid environment"));
	PROTECT(env = R_sysframe(frame, R_GlobalContext));
	break;
    default:
	errorcall(call, _("invalid second argument"));
    }
    if(isLanguage(expr) || isSymbol(expr) || isByteCode(expr)) {
	PROTECT(expr);
	begincontext(&cntxt, CTXT_RETURN, call, env, rho, args, op);
	if (!SETJMP(cntxt.cjmpbuf))
	    expr = eval(expr, env);
	else {
	    expr = R_ReturnedValue;
	    if (expr == R_RestartToken) {
		cntxt.callflag = CTXT_RETURN;  /* turn restart off */
		errorcall(call, _("restarts not supported in eval"));
	    }
	}
	endcontext(&cntxt);
	UNPROTECT(1);
    }
    else if (isExpression(expr)) {
	int i, n;
	PROTECT(expr);
	n = LENGTH(expr);
	tmp = R_NilValue;
	begincontext(&cntxt, CTXT_RETURN, call, env, rho, args, op);
	if (!SETJMP(cntxt.cjmpbuf))
	    for(i=0 ; i<n ; i++)
		tmp = eval(VECTOR_ELT(expr, i), env);
	else {
	    tmp = R_ReturnedValue;
	    if (tmp == R_RestartToken) {
		cntxt.callflag = CTXT_RETURN;  /* turn restart off */
		errorcall(call, _("restarts not supported in eval"));
	    }
	}
	endcontext(&cntxt);
	UNPROTECT(1);
	expr = tmp;
    }
    else if( TYPEOF(expr) == PROMSXP ) {
        expr = eval(expr, rho);
    }
    if (PRIMVAL(op)) { /* eval.with.vis(*) : */
	PROTECT(expr);
	PROTECT(env = allocVector(VECSXP, 2));
	PROTECT(encl = allocVector(STRSXP, 2));
	SET_STRING_ELT(encl, 0, mkChar("value"));
	SET_STRING_ELT(encl, 1, mkChar("visible"));
	SET_VECTOR_ELT(env, 0, expr);
	SET_VECTOR_ELT(env, 1, ScalarLogical(R_Visible));
	setAttrib(env, R_NamesSymbol, encl);
	expr = env;
	UNPROTECT(3);
    }
    UNPROTECT(1);
    return expr;
}


SEXP do_recall(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    RCNTXT *cptr;
    SEXP s, ans ;
    cptr = R_GlobalContext;
    /* get the args supplied */
    while (cptr != NULL) {
	if (cptr->callflag == CTXT_RETURN && cptr->cloenv == rho)
	    break;
	cptr = cptr->nextcontext;
    }
    args = cptr->promargs;
    /* get the env recall was called from */
    s = R_GlobalContext->sysparent;
    while (cptr != NULL) {
	if (cptr->callflag == CTXT_RETURN && cptr->cloenv == s)
	    break;
	cptr = cptr->nextcontext;
    }
    if (cptr == NULL)
	error(_("'Recall' called from outside a closure"));
    if( TYPEOF(CAR(cptr->call)) == SYMSXP)
	PROTECT(s = findFun(CAR(cptr->call), cptr->sysparent));
    else
	PROTECT(s = eval(CAR(cptr->call), cptr->sysparent));
    ans = applyClosure(cptr->call, s, args, cptr->sysparent, R_NilValue);
    UNPROTECT(1);
    return ans;
}


SEXP EvalArgs(SEXP el, SEXP rho, int dropmissing)
{
    if(dropmissing) return evalList(el, rho);
    else return evalListKeepMissing(el, rho);
}


/* DispatchOrEval is used in internal functions which dispatch to
 * object methods (e.g. "[" or "[[").  The code either builds promises
 * and dispatches to the appropriate method, or it evaluates the
 * (unevaluated) arguments it comes in with and returns them so that
 * the generic built-in C code can continue.

 * To call this an ugly hack would be to insult all existing ugly hacks
 * at large in the world.
 */
int DispatchOrEval(SEXP call, SEXP op, char *generic, SEXP args, SEXP rho,
		   SEXP *ans, int dropmissing, int argsevald)
{
#define AVOID_PROMISES_IN_DISPATCH_OR_EVAL
#ifdef AVOID_PROMISES_IN_DISPATCH_OR_EVAL
/* DispatchOrEval is called very frequently, most often in cases where
   no dispatching is needed and the isObject or the string-based
   pre-test fail.  To avoid degrading performance it is therefore
   necessary to avoid creating promises in these cases.  The pre-test
   does require that we look at the first argument, so that needs to
   be evaluated.  The complicating factor is that the first argument
   might come in with a "..." and that there might be other arguments
   in the "..." as well.  LT */

    SEXP x = R_NilValue;
    int dots = FALSE, nprotect = 0;;

    if( argsevald )
        {PROTECT(x = CAR(args)); nprotect++;}
    else {
	/* Find the object to dispatch on, dropping any leading
	   ... arguments with missing or empty values.  If there are no
	   arguments, R_NilValue is used. */
	for (; args != R_NilValue; args = CDR(args)) {
	    if (CAR(args) == R_DotsSymbol) {
		SEXP h = findVar(R_DotsSymbol, rho);
		if (TYPEOF(h) == DOTSXP) {
		    /* just a consistency check */
		    if (TYPEOF(CAR(h)) != PROMSXP)
			error(_("value in ... is not a promise"));
		    dots = TRUE;
		    x = eval(CAR(h), rho);
		break;
		}
		else if (h != R_NilValue && h != R_MissingArg)
		    error(_("... used in an incorrect context"));
	    }
	    else {
		dots = FALSE;
		x = eval(CAR(args), rho);
		break;
	    }
	}
	PROTECT(x); nprotect++;
    }
	/* try to dispatch on the object */
    if( isObject(x) ) {
	char *pt;
	/* try for formal method */
	if(R_has_methods(op)) {
	    SEXP value, argValue;
	    /* create a promise to pass down to applyClosure  */
	    if(!argsevald) {
		argValue = promiseArgs(args, rho);
		SET_PRVALUE(CAR(argValue), x);
	    }
	    else
		argValue = args;
	    PROTECT(argValue); nprotect++;
	    value = R_possible_dispatch(call, op, argValue, rho);
	    if(value) {
		*ans = value;
		UNPROTECT(nprotect);
		return 1;
	    }
	    else {
		/* go on, with the evaluated args.  Not guaranteed to have
		   the same semantics as if the arguments were not
		   evaluated, in special cases (e.g., arg values that are
		   LANGSXP).
		   The use of the promiseArgs is supposed to prevent
		   multiple evaluation after the call to possible_dispatch.
		*/
		if (dots)
		    argValue = EvalArgs(argValue, rho, dropmissing);
		else {
		    argValue = CONS(x, EvalArgs(CDR(argValue), rho, dropmissing));
		    SET_TAG(argValue, CreateTag(TAG(args)));
		}
		PROTECT(args = argValue); nprotect++;
		argsevald = 1;
	    }
	}
	if (TYPEOF(CAR(call)) == SYMSXP)
	    pt = Rf_strrchr(CHAR(PRINTNAME(CAR(call))), '.');
	else
	    pt = NULL;

	if (pt == NULL || strcmp(pt,".default")) {
	    RCNTXT cntxt;
	    SEXP pargs;
	    PROTECT(pargs = promiseArgs(args, rho)); nprotect++;
	    SET_PRVALUE(CAR(pargs), x);
	    begincontext(&cntxt, CTXT_RETURN, call, rho, rho, pargs, op);
	    if(usemethod(generic, x, call, pargs, rho, rho, R_NilValue, ans))
	    {
		endcontext(&cntxt);
		UNPROTECT(nprotect);
		return 1;
	    }
	    endcontext(&cntxt);
	}
    }
    if(!argsevald) {
    if (dots)
	/* The first call argument was ... and may contain more than the
	   object, so it needs to be evaluated here.  The object should be
	   in a promise, so evaluating it again should be no problem. */
	*ans = EvalArgs(args, rho, dropmissing);
    else {
	PROTECT(*ans = CONS(x, EvalArgs(CDR(args), rho, dropmissing)));
	SET_TAG(*ans, CreateTag(TAG(args)));
	UNPROTECT(1);
    }
    }
    else *ans = args;
#else
    SEXP x;
    RCNTXT cntxt;

    /* NEW */
    PROTECT(args = promiseArgs(args, rho)); nprotect++;
    PROTECT(x = eval(CAR(args),rho)); nprotect++;

    if( isObject(x)) {
	char *pt;
	if (TYPEOF(CAR(call)) == SYMSXP)
	    pt = Rf_strrchr(CHAR(PRINTNAME(CAR(call))), '.');
	else
	    pt = NULL;

	if (pt == NULL || strcmp(pt,".default")) {
	    /* PROTECT(args = promiseArgs(args, rho)); */
	    SET_PRVALUE(CAR(args), x);
	    begincontext(&cntxt, CTXT_RETURN, call, rho, rho, args, op);
	    if(usemethod(generic, x, call, args, rho, rho, R_NilValue, ans)) {
		endcontext(&cntxt);
		UNPROTECT(nprotect);
		return 1;
	    }
	    endcontext(&cntxt);
	}
    }
    /* else PROTECT(args); */
    PROTECT(*ans = CONS(x, EvalArgs(CDR(args), rho, dropmissing)));
    SET_TAG(*ans, CreateTag(TAG(args)));
    UNPROTECT(1);
#endif
    UNPROTECT(nprotect);
    return 0;
}


/* gr needs to be protected on return from this function */
static void findmethod(SEXP class, char *group, char *generic,
		       SEXP *sxp,  SEXP *gr, SEXP *meth, int *which,
		       char *buf, SEXP rho)
{
    int len, whichclass;

    len = length(class);

    /* Need to interleave looking for group and generic methods */
    /* eg if class(x) is "foo" "bar" then x>3 should invoke */
    /* "Ops.foo" rather than ">.bar" */
    for (whichclass = 0 ; whichclass < len ; whichclass++) {
	if(strlen(generic) +
	   strlen(CHAR(STRING_ELT(class, whichclass))) + 2 > 512)
	    error(_("class name too long in %s"), generic);
	sprintf(buf, "%s.%s", generic, CHAR(STRING_ELT(class, whichclass)));
	*meth = install(buf);
	*sxp = R_LookupMethod(*meth, rho, rho, R_NilValue);
	if (isFunction(*sxp)) {
	    *gr = mkString("");
	    break;
	}
	if(strlen(group) +
	   strlen(CHAR(STRING_ELT(class, whichclass))) + 2 > 512)
	    error(_("class name too long in %s"), group);
	sprintf(buf, "%s.%s", group, CHAR(STRING_ELT(class, whichclass)));
	*meth = install(buf);
	*sxp = R_LookupMethod(*meth, rho, rho, R_NilValue);
	if (isFunction(*sxp)) {
	    *gr = mkString(group);
	    break;
	}
    }
    *which = whichclass;
}

int DispatchGroup(char* group, SEXP call, SEXP op, SEXP args, SEXP rho,
		  SEXP *ans)
{
    int i, j, nargs, lwhich, rwhich, set;
    SEXP lclass, s, t, m, lmeth, lsxp, lgr, newrho;
    SEXP rclass, rmeth, rgr, rsxp;
    char lbuf[512], rbuf[512], generic[128], *pt;

    /* pre-test to avoid string computations when there is nothing to
       dispatch on because either there is only one argument and it
       isn't an object or there are two or more arguments but neither
       of the first two is an object -- both of these cases would be
       rejected by the code following the string examination code
       below */
    if (args != R_NilValue && ! isObject(CAR(args)) &&
        (CDR(args) == R_NilValue || ! isObject(CADR(args))))
	return 0;
    /* try for formal method */
    if(R_has_methods(op)) {
      SEXP value;
      value = R_possible_dispatch(call, op, args, rho);
      if(value) {
	*ans = value;
	return 1;
      }
      /*else to on to look for S3 methods */
    }

    /* check whether we are processing the default method */
    if ( isSymbol(CAR(call)) ) {
	if(strlen(CHAR(PRINTNAME(CAR(call)))) >= 512)
	   error(_("call name too long in %s"), CHAR(PRINTNAME(CAR(call))));
	sprintf(lbuf, "%s", CHAR(PRINTNAME(CAR(call))) );
	pt = strtok(lbuf, ".");
	pt = strtok(NULL, ".");

	if( pt != NULL && !strcmp(pt, "default") )
	    return 0;
    }

    if( !strcmp(group, "Ops") )
	nargs = length(args);
    else
	nargs = 1;

    if( nargs == 1 && !isObject(CAR(args)) )
	return 0;

    if(!isObject(CAR(args)) && !isObject(CADR(args)))
	return 0;

    if(strlen(PRIMNAME(op)) >= 128)
	error(_("generic name too long in %s"), PRIMNAME(op));
    sprintf(generic, "%s", PRIMNAME(op) );

    lclass = getAttrib(CAR(args), R_ClassSymbol);

    if( nargs == 2 )
	rclass = getAttrib(CADR(args), R_ClassSymbol);
    else
	rclass = R_NilValue;

    lsxp = R_NilValue; lgr = R_NilValue; lmeth = R_NilValue;
    rsxp = R_NilValue; rgr = R_NilValue; rmeth = R_NilValue;

    findmethod(lclass, group, generic, &lsxp, &lgr, &lmeth, &lwhich,
	       lbuf, rho);
    PROTECT(lgr);

    if( nargs == 2 )
	findmethod(rclass, group, generic, &rsxp, &rgr, &rmeth,
		   &rwhich, rbuf, rho);
    else
	rwhich=0;

    PROTECT(rgr);

    if( !isFunction(lsxp) && !isFunction(rsxp) ) {
	UNPROTECT(2);
	return 0; /* no generic or group method so use default*/
    }

    if( lsxp!=rsxp ) {
	if( isFunction(lsxp) && isFunction(rsxp) ) {
	    warning(_("Incompatible methods (\"%s\", \"%s\") for \"%s\""),
		    CHAR(PRINTNAME(lmeth)), CHAR(PRINTNAME(rmeth)), generic);
	    UNPROTECT(2);
	    return 0;
	}
	/* if the right hand side is the one */
	if( !isFunction(lsxp) ) { /* copy over the righthand stuff */
	    lsxp=rsxp;
	    lmeth=rmeth;
	    lgr=rgr;
	    lclass=rclass;
	    lwhich=rwhich;
	    strcpy(lbuf, rbuf);
	}
    }

    /* we either have a group method or a class method */

    PROTECT(newrho = allocSExp(ENVSXP));
    PROTECT(m = allocVector(STRSXP,nargs));
    s = args;
    for (i = 0 ; i < nargs ; i++) {
	t = getAttrib(CAR(s), R_ClassSymbol);
	set = 0;
	if (isString(t)) {
	    for (j = 0 ; j < length(t) ; j++) {
		if (!strcmp(CHAR(STRING_ELT(t, j)),
			    CHAR(STRING_ELT(lclass, lwhich)))) {
		    SET_STRING_ELT(m, i, mkChar(lbuf));
		    set = 1;
		    break;
		}
	    }
	}
	if( !set )
	    SET_STRING_ELT(m, i, R_BlankString);
	s = CDR(s);
    }

    defineVar(install(".Method"), m, newrho);
    UNPROTECT(1);
    PROTECT(t=mkString(generic));
    defineVar(install(".Generic"), t, newrho);
    UNPROTECT(1);
    defineVar(install(".Group"), lgr, newrho);
    set=length(lclass)-lwhich;
    PROTECT(t = allocVector(STRSXP, set));
    for(j=0 ; j<set ; j++ )
	SET_STRING_ELT(t, j, duplicate(STRING_ELT(lclass, lwhich++)));
    defineVar(install(".Class"), t, newrho);
    UNPROTECT(1);
    if (R_UseNamespaceDispatch) {
	defineVar(install(".GenericCallEnv"), rho, newrho);
	defineVar(install(".GenericDefEnv"), R_NilValue, newrho);
    }

    PROTECT(t = LCONS(lmeth,CDR(call)));

    /* the arguments have been evaluated; since we are passing them */
    /* out to a closure we need to wrap them in promises so that */
    /* they get duplicated and things like missing/substitute work. */

    PROTECT(s = promiseArgs(CDR(call), rho));
    if (length(s) != length(args))
	errorcall(call, _("dispatch error"));
    for (m = s ; m != R_NilValue ; m = CDR(m), args = CDR(args) )
	SET_PRVALUE(CAR(m), CAR(args));

    *ans = applyClosure(t, lsxp, s, rho, newrho);
    UNPROTECT(5);
    return 1;
}

#ifdef BYTECODE
static int R_bcVersion = 4;
static int R_bcMinVersion = 4;

static SEXP R_AddSym = NULL;
static SEXP R_SubSym = NULL;
static SEXP R_MulSym = NULL;
static SEXP R_DivSym = NULL;
static SEXP R_ExptSym = NULL;
static SEXP R_SqrtSym = NULL;
static SEXP R_ExpSym = NULL;
static SEXP R_EqSym = NULL;
static SEXP R_NeSym = NULL;
static SEXP R_LtSym = NULL;
static SEXP R_LeSym = NULL;
static SEXP R_GeSym = NULL;
static SEXP R_GtSym = NULL;
static SEXP R_AndSym = NULL;
static SEXP R_OrSym = NULL;
static SEXP R_NotSym = NULL;
static SEXP R_SubsetSym = NULL;
static SEXP R_SubassignSym = NULL;
static SEXP R_CSym = NULL;
static SEXP R_Subset2Sym = NULL;
static SEXP R_Subassign2Sym = NULL;
static SEXP FakeCall0 = NULL;
static SEXP FakeCall1 = NULL;
static SEXP FakeCall2 = NULL;
static SEXP R_TrueValue = NULL;
static SEXP R_FalseValue = NULL;

#if defined(__GNUC__) && ! defined(BC_PROFILING) && (! defined(NO_THREADED_CODE))
# define THREADED_CODE
#endif

void R_initialize_bcode(void)
{
  R_AddSym = install("+");
  R_SubSym = install("-");
  R_MulSym = install("*");
  R_DivSym = install("/");
  R_ExptSym = install("^");
  R_SqrtSym = install("sqrt");
  R_ExpSym = install("exp");
  R_EqSym = install("==");
  R_NeSym = install("!=");
  R_LtSym = install("<");
  R_LeSym = install("<=");
  R_GeSym = install(">=");
  R_GtSym = install(">");
  R_AndSym = install("&");
  R_OrSym = install("|");
  R_NotSym = install("!");
  R_SubsetSym = install("[");
  R_SubassignSym = install("[<-");
  R_CSym = install("c");
  R_Subset2Sym = install("[[");
  R_Subassign2Sym = install("[[<-");
  FakeCall0 = CONS(R_NilValue, R_NilValue);
  FakeCall1 = CONS(R_NilValue, FakeCall0);
  FakeCall2 = CONS(R_NilValue, FakeCall1);
  R_PreserveObject(FakeCall2);
  R_TrueValue = mkTrue();
  SET_NAMED(R_TrueValue, 2);
  R_PreserveObject(R_TrueValue);
  R_FalseValue = mkFalse();
  SET_NAMED(R_FalseValue, 2);
  R_PreserveObject(R_FalseValue);
#ifdef THREADED_CODE
  bcEval(NULL, NULL);
#endif
}

enum {
  BCMISMATCH_OP,
  RETURN_OP,
  GOTO_OP,
  BRIFNOT_OP,
  POP_OP,
  DUP_OP,
  PRINTVALUE_OP,
  STARTLOOPCNTXT_OP,
  ENDLOOPCNTXT_OP,
  DOLOOPNEXT_OP,
  DOLOOPBREAK_OP,
  STARTFOR_OP,
  STEPFOR_OP,
  ENDFOR_OP,
  SETLOOPVAL_OP,
  INVISIBLE_OP,
  LDCONST_OP,
  LDNULL_OP,
  LDTRUE_OP,
  LDFALSE_OP,
  GETVAR_OP,
  DDVAL_OP,
  SETVAR_OP,
  GETFUN_OP,
  GETGLOBFUN_OP,
  GETSYMFUN_OP,
  GETBUILTIN_OP,
  GETINTLBUILTIN_OP,
  CHECKFUN_OP,
  MAKEPROM_OP,
  DOMISSING_OP,
  SETTAG_OP,
  DODOTS_OP,
  PUSHARG_OP,
  PUSHCONSTARG_OP,
  PUSHNULLARG_OP,
  PUSHTRUEARG_OP,
  PUSHFALSEARG_OP,
  CALL_OP,
  CALLBUILTIN_OP,
  CALLSPECIAL_OP,
  MAKECLOSURE_OP,
  UMINUS_OP,
  UPLUS_OP,
  ADD_OP,
  SUB_OP,
  MUL_OP,
  DIV_OP,
  EXPT_OP,
  SQRT_OP,
  EXP_OP,
  EQ_OP,
  NE_OP,
  LT_OP,
  LE_OP,
  GE_OP,
  GT_OP,
  AND_OP,
  OR_OP,
  NOT_OP,
  DOTSERR_OP,
  STARTASSIGN_OP,
  ENDASSIGN_OP,
  STARTSUBSET_OP,
  DFLTSUBSET_OP,
  STARTSUBASSIGN_OP,
  DFLTSUBASSIGN_OP,
  STARTC_OP,
  DFLTC_OP,
  STARTSUBSET2_OP,
  DFLTSUBSET2_OP,
  STARTSUBASSIGN2_OP,
  DFLTSUBASSIGN2_OP,
  DOLLAR_OP,
  DOLLARGETS_OP,
  ISNULL_OP,
  ISLOGICAL_OP,
  ISINTEGER_OP,
  ISDOUBLE_OP,
  ISCOMPLEX_OP,
  ISCHARACTER_OP,
  ISSYMBOL_OP,
  ISOBJECT_OP,
  ISNUMERIC_OP,
  NVECELT_OP,
  NMATELT_OP,
  SETNVECELT_OP,
  SETNMATELT_OP,
  OPCOUNT
};


SEXP R_unary(SEXP, SEXP, SEXP);
SEXP R_binary(SEXP, SEXP, SEXP, SEXP);
SEXP do_math1(SEXP, SEXP, SEXP, SEXP);
SEXP do_relop_dflt(SEXP, SEXP, SEXP, SEXP);
SEXP do_logic(SEXP, SEXP, SEXP, SEXP);
SEXP do_subset_dflt(SEXP, SEXP, SEXP, SEXP);
SEXP do_subassign_dflt(SEXP, SEXP, SEXP, SEXP);
SEXP do_c_dflt(SEXP, SEXP, SEXP, SEXP);
SEXP do_subset2_dflt(SEXP, SEXP, SEXP, SEXP);
SEXP do_subassign2_dflt(SEXP, SEXP, SEXP, SEXP);
SEXP R_subset3_dflt(SEXP, SEXP);
SEXP R_subassign3_dflt(SEXP, SEXP, SEXP, SEXP);

#define DO_FAST_RELOP2(op,a,b) do { \
    double __a__ = (a), __b__ = (b); \
    SEXP val; \
    if (ISNAN(__a__) || ISNAN(__b__)) val = ScalarLogical(NA_LOGICAL); \
    else val = (__a__ op __b__) ? R_TrueValue : R_FalseValue; \
    R_BCNodeStackTop[-2] = val; \
    R_BCNodeStackTop--; \
    NEXT(); \
} while (0)

# define FastRelop2(op,opval,opsym) do { \
    SEXP x = R_BCNodeStackTop[-2]; \
    SEXP y = R_BCNodeStackTop[-1]; \
    if (ATTRIB(x) == R_NilValue && ATTRIB(y) == R_NilValue) { \
	if (TYPEOF(x) == REALSXP && LENGTH(x) == 1 && \
	    TYPEOF(y) == REALSXP && LENGTH(y) == 1) \
	    DO_FAST_RELOP2(op, REAL(x)[0], REAL(y)[0]); \
	else if (TYPEOF(x) == INTSXP && LENGTH(x) == 1 && \
		 TYPEOF(y) == REALSXP && LENGTH(y) == 1) { \
	    double xd = INTEGER(x)[0] == NA_INTEGER ? NA_REAL : INTEGER(x)[0];\
            DO_FAST_RELOP2(op, xd, REAL(y)[0]); \
	} \
	else if (TYPEOF(x) == REALSXP && LENGTH(x) == 1 && \
		 TYPEOF(y) == INTSXP && LENGTH(y) == 1) { \
	    double yd = INTEGER(y)[0] == NA_INTEGER ? NA_REAL : INTEGER(y)[0];\
	    DO_FAST_RELOP2(op, REAL(x)[0], yd); \
	} \
	else if (TYPEOF(x) == INTSXP && LENGTH(x) == 1 && \
		 TYPEOF(y) == INTSXP && LENGTH(y) == 1) { \
	    double xd = INTEGER(x)[0] == NA_INTEGER ? NA_REAL : INTEGER(x)[0];\
	    double yd = INTEGER(y)[0] == NA_INTEGER ? NA_REAL : INTEGER(y)[0];\
	    DO_FAST_RELOP2(op, xd, yd); \
	} \
    } \
    Relop2(opval, opsym); \
} while (0)

static SEXP cmp_relop(SEXP call, int opval, SEXP opsym, SEXP x, SEXP y)
{
    SEXP op = SYMVALUE(opsym);
    if (TYPEOF(op) == PROMSXP) {
	op = forcePromise(op);
	SET_NAMED(op, 2);
    }
    if (isObject(x) || isObject(y)) {
	SEXP args, ans;
	args = CONS(x, CONS(y, R_NilValue));
	PROTECT(args);
	if (DispatchGroup("Ops", call, op, args, R_GlobalEnv, &ans)) {
	    UNPROTECT(1);
	    return ans;
	}
	UNPROTECT(1);
    }
    return do_relop_dflt(R_NilValue, op, x, y);
}

static SEXP cmp_arith1(SEXP call, SEXP op, SEXP x)
{
  if (isObject(x)) {
    SEXP args, ans;
    args = CONS(x, R_NilValue);
    PROTECT(args);
    if (DispatchGroup("Ops", call, op, args, R_GlobalEnv, &ans)) {
      UNPROTECT(1);
      return ans;
    }
    UNPROTECT(1);
  }
  return R_unary(R_NilValue, op, x);
}

static SEXP cmp_arith2(SEXP call, int opval, SEXP opsym, SEXP x, SEXP y)
{
    SEXP op = SYMVALUE(opsym);
    if (TYPEOF(op) == PROMSXP) {
	op = forcePromise(op);
	SET_NAMED(op, 2);
    }
    if (isObject(x) || isObject(y)) {
	SEXP args, ans;
	args = CONS(x, CONS(y, R_NilValue));
	PROTECT(args);
	if (DispatchGroup("Ops", call, op, args, R_GlobalEnv, &ans)) {
	    UNPROTECT(1);
	    return ans;
	}
	UNPROTECT(1);
    }
    return R_binary(R_NilValue, op, x, y);
}

#define Builtin1(do_fun,which) do { \
  R_BCNodeStackTop[-1] = CONS(R_BCNodeStackTop[-1], R_NilValue); \
  R_BCNodeStackTop[-1] = do_fun(FakeCall1, SYMVALUE(which), \
                                R_BCNodeStackTop[-1], R_NilValue); \
  NEXT(); \
} while(0)

#define NewBuiltin1(do_fun,which) do { \
  SEXP x = R_BCNodeStackTop[-1]; \
  R_BCNodeStackTop[-1] = do_fun(FakeCall1, SYMVALUE(which), x); \
  NEXT(); \
} while(0)

#define Builtin2(do_fun,which) do { \
  SEXP tmp = CONS(R_BCNodeStackTop[-1], R_NilValue); \
  R_BCNodeStackTop[-2] = CONS(R_BCNodeStackTop[-2], tmp); \
  R_BCNodeStackTop--; \
  R_BCNodeStackTop[-1] = do_fun(FakeCall2, SYMVALUE(which), \
                                R_BCNodeStackTop[-1], R_NilValue); \
  NEXT(); \
} while(0)

#define NewBuiltin2(do_fun,opval,opsym) do { \
  SEXP x = R_BCNodeStackTop[-2]; \
  SEXP y = R_BCNodeStackTop[-1]; \
  R_BCNodeStackTop[-2] = do_fun(FakeCall2, opval, opsym, x, y); \
  R_BCNodeStackTop--; \
  NEXT(); \
} while(0)

#define Arith1(which) NewBuiltin1(cmp_arith1,which)
#define Arith2(opval,opsym) NewBuiltin2(cmp_arith2,opval,opsym)
#define Math1(which) Builtin1(do_math1,which)
#define Relop2(opval,opsym) NewBuiltin2(cmp_relop,opval,opsym)

# define DO_FAST_BINOP(op,a,b) do { \
    SEXP val = allocVector(REALSXP, 1); \
    REAL(val)[0] = (a) op (b); \
    R_BCNodeStackTop[-2] = val; \
    R_BCNodeStackTop--; \
    NEXT(); \
} while (0)
# define FastBinary(op,opval,opsym) do { \
    SEXP x = R_BCNodeStackTop[-2]; \
    SEXP y = R_BCNodeStackTop[-1]; \
    if (ATTRIB(x) == R_NilValue && ATTRIB(y) == R_NilValue) { \
	if (TYPEOF(x) == REALSXP && LENGTH(x) == 1 && \
	    TYPEOF(y) == REALSXP && LENGTH(y) == 1) \
	    DO_FAST_BINOP(op, REAL(x)[0], REAL(y)[0]); \
	else if (TYPEOF(x) == INTSXP && LENGTH(x) == 1 && \
		 INTEGER(x)[0] != NA_INTEGER && \
		 TYPEOF(y) == REALSXP && LENGTH(y) == 1) \
	    DO_FAST_BINOP(op, INTEGER(x)[0], REAL(y)[0]); \
	else if (TYPEOF(x) == REALSXP && LENGTH(x) == 1 && \
		 TYPEOF(y) == INTSXP && LENGTH(y) == 1 && \
		 INTEGER(y)[0] != NA_INTEGER) \
	    DO_FAST_BINOP(op, REAL(x)[0], INTEGER(y)[0]); \
    } \
    Arith2(opval, opsym); \
} while (0)

#define BCNPUSH(v) do { \
  SEXP __value__ = (v); \
  SEXP *__ntop__ = R_BCNodeStackTop + 1; \
  if (__ntop__ > R_BCNodeStackEnd) nodeStackOverflow(); \
  __ntop__[-1] = __value__; \
  R_BCNodeStackTop = __ntop__; \
} while (0)

#define BCNPOP() (R_BCNodeStackTop--, R_BCNodeStackTop[0])
#define BCNPOP_IGNORE_VALUE() R_BCNodeStackTop--

#define BCNSTACKCHECK(n)  do { \
  if (R_BCNodeStackTop + 1 > R_BCNodeStackEnd) nodeStackOverflow(); \
} while (0)

#define BCIPUSHPTR(v)  do { \
  void *__value__ = (v); \
  IStackval *__ntop__ = R_BCIntStackTop + 1; \
  if (__ntop__ > R_BCIntStackEnd) intStackOverflow(); \
  *__ntop__[-1].p = __value__; \
  R_BCIntStackTop = __ntop__; \
} while (0)

#define BCIPUSHINT(v)  do { \
  int __value__ = (v); \
  IStackval *__ntop__ = R_BCIntStackTop + 1; \
  if (__ntop__ > R_BCIntStackEnd) intStackOverflow(); \
  __ntop__[-1].i = __value__; \
  R_BCIntStackTop = __ntop__; \
} while (0)

#define BCIPOPPTR() ((--R_BCIntStackTop)->p)
#define BCIPOPINT() ((--R_BCIntStackTop)->i)

#define BCCONSTS(e) BCODE_CONSTS(e)

static void nodeStackOverflow()
{
    error(_("node stack overflow"));
}

#ifdef BC_INT_STACK
static void intStackOverflow()
{
    error(_("integer stack overflow"));
}
#endif

static SEXP bytecodeExpr(SEXP e)
{
    if (isByteCode(e)) {
	if (LENGTH(BCCONSTS(e)) > 0)
	    return VECTOR_ELT(BCCONSTS(e), 0);
	else return R_NilValue;
    }
    else return e;
}

SEXP R_PromiseExpr(SEXP p)
{
    return bytecodeExpr(PRCODE(p));
}

SEXP R_ClosureExpr(SEXP p)
{
    return bytecodeExpr(BODY(p));
}

static SEXP forcePromise(SEXP e)
{
  if (PRVALUE(e) == R_UnboundValue) {
    SEXP val;
    if(PRSEEN(e))
      errorcall(R_GlobalContext->call, 
		_("recursive default argument reference"));
    SET_PRSEEN(e, 1);
    val = eval(PRCODE(e), PRENV(e));
    SET_PRSEEN(e, 0);
    SET_PRVALUE(e, val);
  }
  return PRVALUE(e);
}

#ifdef THREADED_CODE
typedef union { void *v; int i; } BCODE;

struct { void *addr; int argc; } opinfo[OPCOUNT];

#define OP(name,n) \
  case name##_OP: opinfo[name##_OP].addr = (__extension__ &&op_##name); \
    opinfo[name##_OP].argc = (n); \
    goto loop; \
    op_##name

#define BEGIN_MACHINE  NEXT(); init: { int which = 0; loop: switch(which++)
#define LASTOP } value = R_NilValue; goto done
#define INITIALIZE_MACHINE() if (body == NULL) goto init

#define NEXT() (__extension__ ({goto *(*pc++).v;}))
#define GETOP() (*pc++).i

#define BCCODE(e) (BCODE *) INTEGER(BCODE_CODE(e))
#else
typedef int BCODE;

#define OP(name,argc) case name##_OP

#ifdef BC_PROFILING
#define BEGIN_MACHINE  loop: current_opcode = *pc; switch(*pc++)
#else
#define BEGIN_MACHINE  loop: switch(*pc++)
#endif
#define LASTOP  default: error(_("Bad opcode"))
#define INITIALIZE_MACHINE()

#define NEXT() goto loop
#define GETOP() *pc++

#define BCCODE(e) INTEGER(BCODE_CODE(e))
#endif

#define DO_GETVAR(dd) do { \
  SEXP symbol = VECTOR_ELT(constants, GETOP()); \
  value = (dd) ? ddfindVar(symbol, rho) : findVar(symbol, rho); \
  R_Visible = 1; \
  if (value == R_UnboundValue) \
    error(_("Object \"%s\" not found"), CHAR(PRINTNAME(symbol))); \
  else if (value == R_MissingArg) { \
    char *n = CHAR(PRINTNAME(symbol)); \
    if(*n) error(_("argument \"%s\" is missing, with no default"), n); \
    else error(_("argument is missing, with no default")); \
  } \
  else if (TYPEOF(value) == PROMSXP) { \
    value = forcePromise(value); \
    SET_NAMED(value, 2); \
  } \
  else if (!isNull(value) && NAMED(value) < 1) \
    SET_NAMED(value, 1); \
  BCNPUSH(value); \
  NEXT(); \
} while (0)

#define PUSHCALLARG(v) PUSHCALLARG_CELL(CONS(v, R_NilValue))

#define PUSHCALLARG_CELL(c) do { \
  SEXP __cell__ = (c); \
  if (R_BCNodeStackTop[-2] == R_NilValue) R_BCNodeStackTop[-2] = __cell__; \
  else SETCDR(R_BCNodeStackTop[-1], __cell__); \
  R_BCNodeStackTop[-1] = __cell__; \
} while (0)

/* making sure the constant is NAMED can be done at assembly time
   once duplicate is set up to not copy the constant portion of code
   and once load is set to make the constants NAMED--basically once
   there is a proper code data type with appropriate support. */
#define DO_LDCONST(v) do { \
  v = VECTOR_ELT(constants, GETOP()); \
  if (! NAMED(v)) SET_NAMED(v, 1); \
} while (0)

static int tryDispatch(char *generic, SEXP call, SEXP x, SEXP rho, SEXP *pv)
{
  RCNTXT cntxt;
  SEXP pargs;
  int dispatched = FALSE;

  PROTECT(pargs = promiseArgs(CDR(call), rho));
  SET_PRVALUE(CAR(pargs), x);
  begincontext(&cntxt, CTXT_RETURN, call, rho, rho, pargs, R_NilValue);/**** FIXME: put in op */
  if (usemethod(generic, x, call, pargs, rho, rho, R_NilValue, pv))
    dispatched = TRUE;
  endcontext(&cntxt);
  UNPROTECT(1);
  return dispatched;
}

#define DO_STARTDISPATCH(generic) do { \
  SEXP call = VECTOR_ELT(constants, GETOP()); \
  int label = GETOP(); \
  value = R_BCNodeStackTop[-1]; \
  if (isObject(value) && tryDispatch(generic, call, value, rho, &value)) {\
    R_BCNodeStackTop[-1] = value; \
    BC_CHECK_SIGINT(); \
    pc = codebase + label; \
  } \
  else { \
    SEXP tag = TAG(CDR(call)); \
    SEXP cell = CONS(value, R_NilValue); \
    BCNSTACKCHECK(3); \
    R_BCNodeStackTop[0] = call; \
    R_BCNodeStackTop[1] = cell; \
    R_BCNodeStackTop[2] = cell; \
    R_BCNodeStackTop += 3; \
    if (tag != R_NilValue) \
      SET_TAG(cell, CreateTag(tag)); \
  } \
  NEXT(); \
} while (0)

#define DO_DFLTDISPATCH(fun, symbol) do { \
  SEXP call = R_BCNodeStackTop[-3]; \
  SEXP args = R_BCNodeStackTop[-2]; \
  value = fun(call, symbol, args, rho); \
  R_BCNodeStackTop -= 3; \
  R_BCNodeStackTop[-1] = value; \
  NEXT(); \
} while (0)

#define DO_ISTEST(fun) do { \
  R_BCNodeStackTop[-1] = fun(R_BCNodeStackTop[-1]) ? \
                         R_TrueValue : R_FalseValue; \
  NEXT(); \
} while(0)
#define DO_ISTYPE(type) do { \
  R_BCNodeStackTop[-1] = TYPEOF(R_BCNodeStackTop[-1]) == type ? \
                         mkTrue() : mkFalse(); \
  NEXT(); \
} while (0)
#define isNumericOnly(x) (isNumeric(x) && ! isLogical(x))

#ifdef BC_PROFILING
#define NO_CURRENT_OPCODE -1
static int current_opcode = NO_CURRENT_OPCODE;
static int opcode_counts[OPCOUNT];
#endif

#define BC_COUNT_DELTA 1000

#define BC_CHECK_SIGINT() do { \
  if (++evalcount > BC_COUNT_DELTA) { \
      R_CheckUserInterrupt(); \
      evalcount = 0; \
  } \
} while (0)

static void loopWithContect(volatile SEXP code, volatile SEXP rho)
{
    RCNTXT cntxt;
    begincontext(&cntxt, CTXT_LOOP, R_NilValue, rho, R_NilValue, R_NilValue,
		 R_NilValue);
    if (SETJMP(cntxt.cjmpbuf) != CTXT_BREAK)
	bcEval(code, rho);
    endcontext(&cntxt);
}

static void checkVectorSubscript(SEXP vec, int k)
{
    switch (TYPEOF(vec)) {
    case REALSXP:
    case INTSXP:
    case LGLSXP:
    case CPLXSXP:
    case STRSXP:
    case VECSXP:
    case EXPRSXP:
    case RAWSXP:
	if (k < 0 || k >= LENGTH(vec))
	    error(_("subscript out of bounds"));
	break;
    default: error(_("not a vector object"));
    }
}

static SEXP numVecElt(SEXP vec, SEXP idx)
{
    int i = asInteger(idx) - 1;
    if (OBJECT(vec))
	error(_("can only handle simple real vectors"));
    checkVectorSubscript(vec, i);
    switch (TYPEOF(vec)) {
    case REALSXP: return ScalarReal(REAL(vec)[i]);
    case INTSXP: return ScalarInteger(INTEGER(vec)[i]);
    case LGLSXP: return ScalarLogical(LOGICAL(vec)[i]);
    case CPLXSXP: return ScalarComplex(COMPLEX(vec)[i]);
    case RAWSXP: return ScalarRaw(RAW(vec)[i]);
    default:
	error(_("not a simple vector"));
	return R_NilValue; /* keep -Wall happy */
    }
}

static SEXP numMatElt(SEXP mat, SEXP idx, SEXP jdx)
{
    SEXP dim;
    int k, nrow;
    int i = asInteger(idx);
    int j = asInteger(jdx);

    if (OBJECT(mat))
	error(_("can only handle simple real vectors"));

    dim = getAttrib(mat, R_DimSymbol);
    if (mat == R_NilValue || TYPEOF(dim) != INTSXP || LENGTH(dim) != 2)
	error(_("incorrect number of subscripts"));
    nrow = INTEGER(dim)[0];
    k = i - 1 + nrow * (j - 1);
    checkVectorSubscript(mat, k);

    switch (TYPEOF(mat)) {
    case REALSXP: return ScalarReal(REAL(mat)[k]);
    case INTSXP: return ScalarInteger(INTEGER(mat)[k]);
    case LGLSXP: return ScalarLogical(LOGICAL(mat)[k]);
    case CPLXSXP: return ScalarComplex(COMPLEX(mat)[k]);
    default:
	error(_("not a simple matrix"));
	return R_NilValue; /* keep -Wall happy */
    }
}

static SEXP setNumVecElt(SEXP vec, SEXP idx, SEXP value)
{
    int i = asInteger(idx) - 1;
    if (OBJECT(vec))
	error(_("can only handle simple real vectors"));
    checkVectorSubscript(vec, i);
    if (NAMED(vec) > 1)
	vec = duplicate(vec);
    PROTECT(vec);
    switch (TYPEOF(vec)) {
    case REALSXP: REAL(vec)[i] = asReal(value); break;
    case INTSXP: INTEGER(vec)[i] = asInteger(value); break;
    case LGLSXP: LOGICAL(vec)[i] = asLogical(value); break;
    case CPLXSXP: COMPLEX(vec)[i] = asComplex(value); break;
    default: error(_("not a simple vector"));
    }
    UNPROTECT(1);
    return vec;
}

static SEXP setNumMatElt(SEXP mat, SEXP idx, SEXP jdx, SEXP value)
{
    SEXP dim;
    int k, nrow;
    int i = asInteger(idx);
    int j = asInteger(jdx);

    if (OBJECT(mat))
	error(_("can only handle simple real vectors"));

    dim = getAttrib(mat, R_DimSymbol);
    if (mat == R_NilValue || TYPEOF(dim) != INTSXP || LENGTH(dim) != 2)
	error(_("incorrect number of subscripts"));
    nrow = INTEGER(dim)[0];
    k = i - 1 + nrow * (j - 1);
    checkVectorSubscript(mat, k);

    if (NAMED(mat) > 1)
	mat = duplicate(mat);

    PROTECT(mat);
    switch (TYPEOF(mat)) {
    case REALSXP: REAL(mat)[k] = asReal(value); break;
    case INTSXP: INTEGER(mat)[k] = asInteger(value); break;
    case LGLSXP: LOGICAL(mat)[k] = asLogical(value); break;
    case CPLXSXP: COMPLEX(mat)[k] = asComplex(value); break;
    default: error(_("not a simple matrix"));
    }
    UNPROTECT(1);
    return mat;
}

static SEXP bcEval(SEXP body, SEXP rho)
{
  SEXP value, constants;
  BCODE *pc, *codebase;
  int ftype = 0;
  SEXP *oldntop = R_BCNodeStackTop;
  static int evalcount = 0;
#ifdef BC_INT_STACK
  IStackval *olditop = R_BCIntStackTop;
#endif
#ifdef BC_PROFILING
  int old_current_opcode = current_opcode;
#endif

  BC_CHECK_SIGINT();

  INITIALIZE_MACHINE();
  codebase = pc = BCCODE(body);
  constants = BCCONSTS(body);

  /* check version */
  {
      int version = GETOP();
      if (version < R_bcMinVersion || version > R_bcVersion) {
	  if (version >= 2) {
	      static Rboolean warned = FALSE;
	      if (! warned) {
		  warned = TRUE;
		  warning(_("bytecode version mismatch; using eval"));
	      }
	      return eval(bytecodeExpr(body), rho);
	  }
	  else if (version < R_bcMinVersion)
	      error(_("bytecode version is too old"));
	  else error(_("bytecode version is too new"));
      }
  }

  BEGIN_MACHINE {
    OP(BCMISMATCH, 0): error(_("byte code version mismatch"));
    OP(RETURN, 0): value = R_BCNodeStackTop[-1]; goto done;
    OP(GOTO, 1):
      {
	int label = GETOP();
	BC_CHECK_SIGINT();
	pc = codebase + label;
	NEXT();
      }
    OP(BRIFNOT, 1):
      {
	int label = GETOP(), cond;
	value = BCNPOP();
	cond = asLogical(value);
	if (cond == NA_LOGICAL)
	  error(isLogical(value)
		? _("missing value where logical needed")
		: _("argument of if(*) is not interpretable as logical"));
	if (! cond) {
	    BC_CHECK_SIGINT();
	    pc = codebase + label;
	}
	NEXT();
      }
    OP(POP, 0): BCNPOP_IGNORE_VALUE(); NEXT();
    OP(DUP, 0): value = R_BCNodeStackTop[-1]; BCNPUSH(value); NEXT();
    OP(PRINTVALUE, 0): PrintValue(BCNPOP()); NEXT();
    OP(STARTLOOPCNTXT, 1):
	{
	    SEXP code = VECTOR_ELT(constants, GETOP());
	    loopWithContect(code, rho);
	    NEXT();
	}
    OP(ENDLOOPCNTXT, 0): value = R_NilValue; goto done;
    OP(DOLOOPNEXT, 0): findcontext(CTXT_NEXT, rho, R_NilValue);
    OP(DOLOOPBREAK, 0): findcontext(CTXT_BREAK, rho, R_NilValue);
    OP(STARTFOR, 2):
      {
	SEXP seq = R_BCNodeStackTop[-1];
	SEXP symbol = VECTOR_ELT(constants, GETOP());
	int label = GETOP();

	defineVar(symbol, R_NilValue, rho);
	BCNPUSH((SEXP) R_findVarLocInFrame(rho, symbol));

	value = allocVector(INTSXP, 2);
	INTEGER(value)[0] = -1;
	if (isVector(seq))
	  INTEGER(value)[1] = LENGTH(seq);
	else if (isList(seq) || isNull(seq))
	  INTEGER(value)[1] = length(seq);
	else error(_("invalid sequence argument in for loop"));
	BCNPUSH(value);

	BCNPUSH(R_NilValue);

	BC_CHECK_SIGINT();
	pc = codebase + label;
	NEXT();
      }
    OP(STEPFOR, 1):
      {
	int label = GETOP();
	int i = ++(INTEGER(R_BCNodeStackTop[-2])[0]);
	int n = INTEGER(R_BCNodeStackTop[-2])[1];
	if (i < n) {
	  SEXP seq = R_BCNodeStackTop[-4];
	  SEXP cell = R_BCNodeStackTop[-3];
	  switch (TYPEOF(seq)) {
	  case LGLSXP:
	  case INTSXP:
	    value = allocVector(TYPEOF(seq), 1);
	    INTEGER(value)[0] = INTEGER(seq)[i];
	    break;
	  case REALSXP:
	    value = allocVector(TYPEOF(seq), 1);
	    REAL(value)[0] = REAL(seq)[i];
	    break;
	  case CPLXSXP:
	    value = allocVector(TYPEOF(seq), 1);
	    COMPLEX(value)[0] = COMPLEX(seq)[i];
	    break;
	  case STRSXP:
	    value = allocVector(TYPEOF(seq), 1);
	    SET_STRING_ELT(value, 0, STRING_ELT(seq, i));
	    break;
	  case EXPRSXP:
	  case VECSXP:
	    value = VECTOR_ELT(seq, i);
	    break;
	  case LISTSXP:
	    value = CAR(seq);
	    R_BCNodeStackTop[-4] = CDR(seq);
	    break;
	  }
	  R_SetVarLocValue((R_varloc_t) cell, value);
	  BC_CHECK_SIGINT();
	  pc = codebase + label;
	}
	NEXT();
      }
    OP(ENDFOR, 0):
      {
	value = R_BCNodeStackTop[-1];
	R_BCNodeStackTop -= 3;
	R_BCNodeStackTop[-1] = value;
	NEXT();
      }
    OP(SETLOOPVAL, 0): value = BCNPOP(); R_BCNodeStackTop[-1] = value; NEXT();
    OP(INVISIBLE,0): R_Visible = 0; NEXT();
    OP(LDCONST, 1): DO_LDCONST(value); BCNPUSH(value); NEXT();
    OP(LDNULL, 0):  BCNPUSH(R_NilValue); NEXT();
    OP(LDTRUE, 0):  BCNPUSH(R_TrueValue); NEXT();
    OP(LDFALSE, 0):  BCNPUSH(R_FalseValue); NEXT();
    OP(GETVAR, 1): DO_GETVAR(FALSE);
    OP(DDVAL, 1): DO_GETVAR(TRUE);
    OP(SETVAR, 1):
      {
	SEXP symbol = VECTOR_ELT(constants, GETOP());
	value = R_BCNodeStackTop[-1];
	switch (NAMED(value)) {
	case 0: SET_NAMED(value, 1); break;
	case 1: SET_NAMED(value, 2); break;
	}
	defineVar(symbol, value, rho);
	NEXT();
      }
    OP(GETFUN, 1):
      {
	/* get the function */
	SEXP symbol = VECTOR_ELT(constants, GETOP());
	value = findFun(symbol, rho);
	if(TRACE(value)) {
	  Rprintf("trace: ");
	  PrintValue(symbol);
	}

	/* initialize the function type register, push the function, and
	   push space for creating the argument list. */
	ftype = TYPEOF(value);
	BCNSTACKCHECK(3);
	R_BCNodeStackTop[0] = value;
	R_BCNodeStackTop[1] = R_NilValue;
	R_BCNodeStackTop[2] = R_NilValue;
	R_BCNodeStackTop += 3;
	NEXT();
      }
    OP(GETGLOBFUN, 1):
      {
	/* get the function */
	SEXP symbol = VECTOR_ELT(constants, GETOP());
	value = findFun(symbol, R_GlobalEnv);
	if(TRACE(value)) {
	  Rprintf("trace: ");
	  PrintValue(symbol);
	}

	/* initialize the function type register, push the function, and
	   push space for creating the argument list. */
	ftype = TYPEOF(value);
	BCNSTACKCHECK(3);
	R_BCNodeStackTop[0] = value;
	R_BCNodeStackTop[1] = R_NilValue;
	R_BCNodeStackTop[2] = R_NilValue;
	R_BCNodeStackTop += 3;
	NEXT();
      }
    OP(GETSYMFUN, 1):
      {
	/* get the function */
	SEXP symbol = VECTOR_ELT(constants, GETOP());
	value = SYMVALUE(symbol);
	if (TYPEOF(value) == PROMSXP) {
	    value = forcePromise(value);
	    SET_NAMED(value, 2);
	}
	if(TRACE(value)) {
	  Rprintf("trace: ");
	  PrintValue(symbol);
	}

	/* initialize the function type register, push the function, and
	   push space for creating the argument list. */
	ftype = TYPEOF(value);
	BCNSTACKCHECK(3);
	R_BCNodeStackTop[0] = value;
	R_BCNodeStackTop[1] = R_NilValue;
	R_BCNodeStackTop[2] = R_NilValue;
	R_BCNodeStackTop += 3;
	NEXT();
      }
    OP(GETBUILTIN, 1):
      {
	/* get the function */
	SEXP symbol = VECTOR_ELT(constants, GETOP());
	value = SYMVALUE(symbol);
	if (TYPEOF(value) == PROMSXP) {
	    value = forcePromise(value);
	    SET_NAMED(value, 2);
	}
	if (TYPEOF(value) != BUILTINSXP)
	  error(_("not a BUILTIN function"));
	if(TRACE(value)) {
	  Rprintf("trace: ");
	  PrintValue(symbol);
	}

	/* push the function and push space for creating the argument list. */
	ftype = TYPEOF(value);
	BCNSTACKCHECK(3);
	R_BCNodeStackTop[0] = value;
	R_BCNodeStackTop[1] = R_NilValue;
	R_BCNodeStackTop[2] = R_NilValue;
	R_BCNodeStackTop += 3;
	NEXT();
      }
    OP(GETINTLBUILTIN, 1):
      {
	/* get the function */
	SEXP symbol = VECTOR_ELT(constants, GETOP());
	value = INTERNAL(symbol);
	if (TYPEOF(value) != BUILTINSXP)
	  error(_("not a BUILTIN function"));

	/* push the function and push space for creating the argument list. */
	ftype = TYPEOF(value);
	BCNSTACKCHECK(3);
	R_BCNodeStackTop[0] = value;
	R_BCNodeStackTop[1] = R_NilValue;
	R_BCNodeStackTop[2] = R_NilValue;
	R_BCNodeStackTop += 3;
	NEXT();
      }
    OP(CHECKFUN, 0):
      {
	/* check then the value on the stack is a function */
	value = R_BCNodeStackTop[-1];
	if (TYPEOF(value) != CLOSXP && TYPEOF(value) != BUILTINSXP &&
	    TYPEOF(value) != RCC_CLOSXP &&
	    TYPEOF(value) != SPECIALSXP)
	  error(_("attempt to apply non-function"));

	/* initialize the function type register, and push space for
	   creating the argument list. */
	ftype = TYPEOF(value);
	BCNSTACKCHECK(2);
	R_BCNodeStackTop[0] = R_NilValue;
	R_BCNodeStackTop[1] = R_NilValue;
	R_BCNodeStackTop += 2;
	NEXT();
      }
    OP(MAKEPROM, 1):
      {
	SEXP code = VECTOR_ELT(constants, GETOP());
	if (ftype != SPECIALSXP) {
	  if (ftype == BUILTINSXP)
	    value = bcEval(code, rho);
	  else
	    value = mkPROMISE(code, rho);
          PUSHCALLARG(value);
	}
	NEXT();
      }
    OP(DOMISSING, 0):
      {
	if (ftype != SPECIALSXP)
	  PUSHCALLARG(R_MissingArg);
	NEXT();
      }
    OP(SETTAG, 1):
      {
	SEXP tag = VECTOR_ELT(constants, GETOP());
	SEXP cell = R_BCNodeStackTop[-1];
	if (ftype != SPECIALSXP && cell != R_NilValue)
	  SET_TAG(cell, CreateTag(tag));
	NEXT();
      }
    OP(DODOTS, 0):
      {
	if (ftype != SPECIALSXP) {
	  SEXP h = findVar(R_DotsSymbol, rho);
	  if (TYPEOF(h) == DOTSXP || h == R_NilValue) {
	    for (; h != R_NilValue; h = CDR(h)) {
	      SEXP val, cell;
	      if (ftype == BUILTINSXP) val = eval(CAR(h), rho);
	      else val = mkPROMISE(CAR(h), rho);
	      cell = CONS(val, R_NilValue);
	      PUSHCALLARG_CELL(cell);
	      if (TAG(h) != R_NilValue) SET_TAG(cell, CreateTag(TAG(h)));
	    }
	  }
	  else if (h != R_MissingArg)
	    error(_("... used in an incorrect context"));
	}
	NEXT();
      }
    OP(PUSHARG, 0): PUSHCALLARG(BCNPOP()); NEXT();
    OP(PUSHCONSTARG, 1): DO_LDCONST(value); PUSHCALLARG(value); NEXT();
    OP(PUSHNULLARG, 0): PUSHCALLARG(R_NilValue); NEXT();
    OP(PUSHTRUEARG, 0): PUSHCALLARG(R_TrueValue); NEXT();
    OP(PUSHFALSEARG, 0): PUSHCALLARG(R_FalseValue); NEXT();
    OP(CALL, 1):
      {
	SEXP fun = R_BCNodeStackTop[-3];
	SEXP call = VECTOR_ELT(constants, GETOP());
	SEXP args = R_BCNodeStackTop[-2];
	switch (ftype) {
	case BUILTINSXP:
	  R_Visible = 1 - PRIMPRINT(fun);
	  value = PRIMFUN(fun) (call, fun, args, rho);
	  break;
	case SPECIALSXP:
	  R_Visible = 1 - PRIMPRINT(fun);
	  value = PRIMFUN(fun) (call, fun, CDR(call), rho);
	  break;
	case RCC_CLOSXP:
	case CLOSXP:
	  value = applyClosure(call, fun, args, rho, R_NilValue);
	  break;
	default: error(_("bad function"));
	}
	R_BCNodeStackTop -= 2;
	R_BCNodeStackTop[-1] = value;
	NEXT();
      }
    OP(CALLBUILTIN, 1):
      {
	SEXP fun = R_BCNodeStackTop[-3];
	SEXP call = VECTOR_ELT(constants, GETOP());
	SEXP args = R_BCNodeStackTop[-2];
	if (TYPEOF(fun) != BUILTINSXP)
	  error(_("not a BUILTIN function"));
	R_Visible = 1 - PRIMPRINT(fun);
	value = PRIMFUN(fun) (call, fun, args, rho);
	R_BCNodeStackTop -= 2;
	R_BCNodeStackTop[-1] = value;
	NEXT();
      }
    OP(CALLSPECIAL, 1):
      {
	SEXP call = VECTOR_ELT(constants, GETOP());
	SEXP symbol = CAR(call);
	SEXP fun = SYMVALUE(symbol);
	if (TYPEOF(value) == PROMSXP) {
	    value = forcePromise(value);
	    SET_NAMED(value, 2);
	}
	if(TRACE(fun)) {
	  Rprintf("trace: ");
	  PrintValue(symbol);
	}
	if (TYPEOF(fun) != SPECIALSXP)
	  error(_("not a SPECIAL function"));
	R_Visible = 1 - PRIMPRINT(fun);
	value = PRIMFUN(fun) (call, fun, CDR(call), rho);
	BCNPUSH(value);
	NEXT();
      }
    OP(MAKECLOSURE, 1):
      {
	SEXP fb = VECTOR_ELT(constants, GETOP());
	SEXP forms = VECTOR_ELT(fb, 0);
	SEXP body = VECTOR_ELT(fb, 1);
	value = mkCLOSXP(forms, body, rho);
	BCNPUSH(value);
	NEXT();
      }
    OP(UMINUS, 0): Arith1(R_SubSym);
    OP(UPLUS, 0): Arith1(R_AddSym);
    OP(ADD, 0): FastBinary(+, PLUSOP, R_AddSym);
    OP(SUB, 0): FastBinary(-, MINUSOP, R_SubSym);
    OP(MUL, 0): FastBinary(*, TIMESOP, R_MulSym);
    OP(DIV, 0): FastBinary(/, DIVOP, R_DivSym);
    OP(EXPT, 0): Arith2(POWOP, R_ExptSym);
    OP(SQRT, 0): Math1(R_SqrtSym);
    OP(EXP, 0): Math1(R_ExpSym);
    OP(EQ, 0): FastRelop2(==, EQOP, R_EqSym);
    OP(NE, 0): FastRelop2(!=, NEOP, R_NeSym);
    OP(LT, 0): FastRelop2(<, LTOP, R_LtSym);
    OP(LE, 0): FastRelop2(<=, LEOP, R_LeSym);
    OP(GE, 0): FastRelop2(>=, GEOP, R_GeSym);
    OP(GT, 0): FastRelop2(>, GTOP, R_GtSym);
    OP(AND, 0): Builtin2(do_logic, R_AndSym);
    OP(OR, 0): Builtin2(do_logic, R_OrSym);
    OP(NOT, 0): Builtin1(do_logic, R_NotSym);
    OP(DOTSERR, 0): error(_("... used in an incorrect context"));
    OP(STARTASSIGN, 2):
      {
	SEXP symbol = VECTOR_ELT(constants, GETOP());
	SEXP valsym = VECTOR_ELT(constants, GETOP());
	EnsureLocal(symbol, rho);
	value = R_BCNodeStackTop[-1];
	defineVar(valsym, value, rho); /**** not adjusting NAMED OK? */
	/* right-hand side value is now on top of stack */
	NEXT();
      }
    OP(ENDASSIGN, 2):
      {
	SEXP symbol = VECTOR_ELT(constants, GETOP());
	SEXP valsym = VECTOR_ELT(constants, GETOP());
	value = BCNPOP();
	switch (NAMED(value)) {
	case 0: SET_NAMED(value, 1); break;
	case 1: SET_NAMED(value, 2); break;
	}
	defineVar(symbol, value, rho);
	unbindVar(valsym, rho);
	/* original right-hand side value is now on top of stack again */
	NEXT();
      }
    OP(STARTSUBSET, 2): DO_STARTDISPATCH("[");
    OP(DFLTSUBSET, 0): DO_DFLTDISPATCH(do_subset_dflt, R_SubsetSym);
    OP(STARTSUBASSIGN, 2): DO_STARTDISPATCH("[<-");
    OP(DFLTSUBASSIGN, 0): DO_DFLTDISPATCH(do_subassign_dflt, R_SubassignSym);
    OP(STARTC, 2): DO_STARTDISPATCH("c");
    OP(DFLTC, 0): DO_DFLTDISPATCH(do_c_dflt, R_CSym);
    OP(STARTSUBSET2, 2): DO_STARTDISPATCH("[[");
    OP(DFLTSUBSET2, 0): DO_DFLTDISPATCH(do_subset2_dflt, R_Subset2Sym);
    OP(STARTSUBASSIGN2, 2): DO_STARTDISPATCH("[[<-");
    OP(DFLTSUBASSIGN2, 0):
      DO_DFLTDISPATCH(do_subassign2_dflt, R_Subassign2Sym);
    OP(DOLLAR, 2):
      {
	int dispatched = FALSE;
	SEXP call = VECTOR_ELT(constants, GETOP());
	SEXP symbol = VECTOR_ELT(constants, GETOP());
	SEXP x = R_BCNodeStackTop[-1];
	if (isObject(x)) {
	  RCNTXT cntxt;
	  SEXP pargs, str;
	  PROTECT(pargs = promiseArgs(CDR(call), rho));
	  SET_PRVALUE(CAR(pargs), x);
	  str = allocVector(STRSXP, 1);
	  SET_STRING_ELT(str, 0, PRINTNAME(symbol));
	  SET_PRVALUE(CADR(pargs), str);
	  begincontext(&cntxt, CTXT_RETURN, call, rho, rho, pargs, R_NilValue);/**** FIXME: put in op */
	  if (usemethod("$", x, call, pargs, rho, rho, R_NilValue, &value))
	    dispatched = TRUE;
	  endcontext(&cntxt);
	  UNPROTECT(1);
	}
	if (dispatched)
	  R_BCNodeStackTop[-1] = value;
	else
	  R_BCNodeStackTop[-1] = R_subset3_dflt(x, PRINTNAME(symbol));
	NEXT();
      }
    OP(DOLLARGETS, 2):
      {
	int dispatched = FALSE;
	SEXP call = VECTOR_ELT(constants, GETOP());
	SEXP symbol = VECTOR_ELT(constants, GETOP());
	SEXP x = R_BCNodeStackTop[-1];
	value = R_BCNodeStackTop[-2];
	if (isObject(x)) {
	  RCNTXT cntxt;
	  SEXP pargs, str;
	  PROTECT(pargs = promiseArgs(CDR(call), rho));
	  SET_PRVALUE(CAR(pargs), x);
	  str = allocVector(STRSXP, 1);
	  SET_STRING_ELT(str, 0, PRINTNAME(symbol));
	  SET_PRVALUE(CADR(pargs), str);
	  SET_PRVALUE(CADDR(pargs), value);
	  begincontext(&cntxt, CTXT_RETURN, call, rho, rho, pargs, R_NilValue);/**** FIXME: put in op */
	  if (usemethod("$<-", x, call, pargs, rho, rho, R_NilValue, &value))
	    dispatched = TRUE;
	  endcontext(&cntxt);
	  UNPROTECT(1);
	}
	R_BCNodeStackTop--;
	if (dispatched)
	  R_BCNodeStackTop[-1] = value;
	else
	  R_BCNodeStackTop[-1] = R_subassign3_dflt(call, x, symbol, value);
	NEXT();
      }
    OP(ISNULL, 0): DO_ISTEST(isNull);
    OP(ISLOGICAL, 0): DO_ISTYPE(LGLSXP);
    OP(ISINTEGER, 0): DO_ISTYPE(INTSXP);
    OP(ISDOUBLE, 0): DO_ISTYPE(REALSXP);
    OP(ISCOMPLEX, 0): DO_ISTYPE(CPLXSXP);
    OP(ISCHARACTER, 0): DO_ISTYPE(STRSXP);
    OP(ISSYMBOL, 0): DO_ISTYPE(SYMSXP);
    OP(ISOBJECT, 0): DO_ISTEST(OBJECT);
    OP(ISNUMERIC, 0): DO_ISTEST(isNumericOnly);
    OP(NVECELT, 2): {
	SEXP vec = R_BCNodeStackTop[-2];
	SEXP idx = R_BCNodeStackTop[-1];
	value = numVecElt(vec, idx);
	R_BCNodeStackTop--;
	R_BCNodeStackTop[-1] = value;
	NEXT();
    }
    OP(NMATELT, 3): {
	SEXP mat = R_BCNodeStackTop[-3];
	SEXP idx = R_BCNodeStackTop[-2];
	SEXP jdx = R_BCNodeStackTop[-1];
	value = numMatElt(mat, idx, jdx);
	R_BCNodeStackTop -= 2;
	R_BCNodeStackTop[-1] = value;
	NEXT();
    }
    OP(SETNVECELT, 3): {
	SEXP vec = R_BCNodeStackTop[-3];
	SEXP idx = R_BCNodeStackTop[-2];
	value = R_BCNodeStackTop[-1];
	value = setNumVecElt(vec, idx, value);
	R_BCNodeStackTop -= 2;
	R_BCNodeStackTop[-1] = value;
	NEXT();
    }
    OP(SETNMATELT, 4): {
	SEXP mat = R_BCNodeStackTop[-4];
	SEXP idx = R_BCNodeStackTop[-3];
	SEXP jdx = R_BCNodeStackTop[-2];
	value = R_BCNodeStackTop[-1];
	value = setNumMatElt(mat, idx, jdx, value);
	R_BCNodeStackTop -= 3;
	R_BCNodeStackTop[-1] = value;
	NEXT();
    }
    LASTOP;
  }

 done:
  R_BCNodeStackTop = oldntop;
#ifdef BC_INT_STACK
  R_BCIntStackTop = olditop;
#endif
#ifdef BC_PROFILING
  current_opcode = old_current_opcode;
#endif
  return value;
}

#ifdef THREADED_CODE
SEXP R_bcEncode(SEXP bytes)
{
    SEXP code;
    BCODE *pc;
    int *ipc, i, n, m, v;

    m = (sizeof(BCODE) + sizeof(int) - 1) / sizeof(int);

    n = LENGTH(bytes);
    ipc = INTEGER(bytes);

    v = ipc[0];
    if (v < R_bcMinVersion || v > R_bcVersion) {
	code = allocVector(INTSXP, m * 2);
	pc = (BCODE *) CHAR(code);
	pc[0].i = v;
	pc[1].v = opinfo[BCMISMATCH_OP].addr;
	return code;
    }
    else {
	code = allocVector(INTSXP, m * n);
	pc = (BCODE *) CHAR(code);

	for (i = 0; i < n; i++) pc[i].i = ipc[i];

	/* install the current version number */
	pc[0].i = R_bcVersion;

	for (i = 1; i < n;) {
	    int op = pc[i].i;
	    pc[i].v = opinfo[op].addr;
	    i += opinfo[op].argc + 1;
	}

	return code;
    }
}

static int findOp(void *addr)
{
    int i;

    for (i = 0; i < OPCOUNT; i++)
	if (opinfo[i].addr == addr)
	    return i;
    error(_("cannot find index for threaded code address"));
    return 0; /* not reached */
}

SEXP R_bcDecode(SEXP code) {
    int n, i, j, *ipc;
    BCODE *pc;
    SEXP bytes;

    n = LENGTH(code);
    pc = (BCODE *) CHAR(code);

    bytes = allocVector(INTSXP, n);
    ipc = INTEGER(bytes);

    /* copy the version number */
    ipc[0] = pc[0].i;

    for (i = 1; i < n;) {
	int op = findOp(pc[i].v);
	int argc = opinfo[op].argc;
	ipc[i] = op;
	i++;
	for (j = 0; j < argc; j++, i++)
	    ipc[i] = pc[i].i;
    }

    return bytes;
}
#else
SEXP R_bcEncode(SEXP x) { return x; }
SEXP R_bcDecode(SEXP x) { return duplicate(x); }
#endif

SEXP do_mkcode(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    SEXP bytes, consts, ans;

    checkArity(op, args);
    bytes = CAR(args);
    consts = CADR(args);
    ans = CONS(R_bcEncode(bytes), consts);
    SET_TYPEOF(ans, BCODESXP);
    return ans;
}

SEXP do_bcclose(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    SEXP forms, body, env;

    checkArity(op, args);
    forms = CAR(args);
    body = CADR(args);
    env = CADDR(args);

    CheckFormals(forms);

    if (! isByteCode(body))
	errorcall(call, _("invalid environment"));

    if (!isNull(env) && !isEnvironment(env))
	errorcall(call, _("invalid environment"));

    return mkCLOSXP(forms, body, env);
}

SEXP do_is_builtin_internal(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    SEXP symbol, i;

    checkArity(op, args);
    symbol = CAR(args);

    if (!isSymbol(symbol))
	errorcall(call, _("invalid symbol"));

    if ((i = INTERNAL(symbol)) != R_NilValue && TYPEOF(i) == BUILTINSXP)
	return R_TrueValue;
    else
	return R_FalseValue;
}

static SEXP disassemble(SEXP bc)
{
  SEXP ans, dconsts;
  int i;
  SEXP code = BCODE_CODE(bc);
  SEXP consts = BCODE_CONSTS(bc);
  SEXP expr = BCODE_EXPR(bc);
  int nc = LENGTH(consts);

  PROTECT(ans = allocVector(VECSXP, expr != R_NilValue ? 4 : 3));
  SET_VECTOR_ELT(ans, 0, install(".Code"));
  SET_VECTOR_ELT(ans, 1, R_bcDecode(code));
  SET_VECTOR_ELT(ans, 2, allocVector(VECSXP, nc));
  if (expr != R_NilValue)
      SET_VECTOR_ELT(ans, 3, duplicate(expr));

  dconsts = VECTOR_ELT(ans, 2);
  for (i = 0; i < nc; i++) {
    SEXP c = VECTOR_ELT(consts, i);
    if (isByteCode(c))
      SET_VECTOR_ELT(dconsts, i, disassemble(c));
    else
      SET_VECTOR_ELT(dconsts, i, duplicate(c));
  }

  UNPROTECT(1);
  return ans;
}

SEXP do_disassemble(SEXP call, SEXP op, SEXP args, SEXP rho)
{
  SEXP code;

  checkArity(op, args);
  code = CAR(args);
  if (! isByteCode(code))
    errorcall(call, _("argument is not a byte code object"));
  return disassemble(code);
}

SEXP do_bcversion(SEXP call, SEXP op, SEXP args, SEXP rho)
{
  SEXP ans = allocVector(INTSXP, 1);
  INTEGER(ans)[0] = R_bcVersion;
  return ans;
}

#include "Fileio.h"

SEXP do_loadfile(SEXP call, SEXP op, SEXP args, SEXP env)
{
    SEXP file, s;
    FILE *fp;

    checkArity(op, args);

    PROTECT(file = coerceVector(CAR(args), STRSXP));

    if (! isValidStringF(file))
	errorcall(call, _("bad file name"));

    fp = R_fopen(R_ExpandFileName(CHAR(STRING_ELT(file,0))), "rb");
    if (!fp)
	errorcall(call, _("unable to open 'file'"));
    s = R_LoadFromFile(fp, 0);
    fclose(fp);

    UNPROTECT(1);
    return s;
}

SEXP do_savefile(SEXP call, SEXP op, SEXP args, SEXP env)
{
    FILE *fp;

    checkArity(op, args);

    if (!isValidStringF(CADR(args)))
	errorcall(call, _("'file' must be non-empty string"));
    if (TYPEOF(CADDR(args)) != LGLSXP)
	errorcall(call, _("'ascii' must be logical"));

    fp = R_fopen(R_ExpandFileName(CHAR(STRING_ELT(CADR(args), 0))), "wb");
    if (!fp)
	errorcall(call, _("unable to open 'file'"));

    R_SaveToFileV(CAR(args), fp, INTEGER(CADDR(args))[0], 0);

    fclose(fp);
    return R_NilValue;
}

#define R_COMPILED_EXTENSION ".Rc"

/* neither of these functions call R_ExpandFileName -- the caller
   should do that if it wants to */
char *R_CompiledFileName(char *fname, char *buf, size_t bsize)
{
    char *basename, *ext;

    /* find the base name and the extension */
    basename = Rf_strrchr(fname, FILESEP[0]);
    if (basename == NULL) basename = fname;
    ext = Rf_strrchr(basename, '.');

    if (ext != NULL && strcmp(ext, R_COMPILED_EXTENSION) == 0) {
	/* the supplied file name has the compiled file extension, so
	   just copy it to the buffer and return the buffer pointer */
	if (snprintf(buf, bsize, "%s", fname) < 0)
	    error(_("R_CompiledFileName: buffer too small"));
	return buf;
    }
    else if (ext == NULL) {
	/* if the requested file has no extention, make a name that
           has the extenrion added on to the expanded name */
	if (snprintf(buf, bsize, "%s%s", fname, R_COMPILED_EXTENSION) < 0)
	    error(_("R_CompiledFileName: buffer too small"));
	return buf;
    }
    else {
	/* the supplied file already has an extention, so there is no
	   corresponding compiled file name */
	return NULL;
    }
}

FILE *R_OpenCompiledFile(char *fname, char *buf, size_t bsize)
{
    char *cname = R_CompiledFileName(fname, buf, bsize);

    if (cname != NULL && R_FileExists(cname) &&
	(strcmp(fname, cname) == 0 ||
	 ! R_FileExists(fname) ||
	 R_FileMtime(cname) > R_FileMtime(fname)))
	/* the compiled file cname exists, and either fname does not
	   exist, or it is the same as cname, or both exist and cname
	   is newer */
	return R_fopen(buf, "rb");
    else return NULL;
}

SEXP do_putconst(SEXP call, SEXP op, SEXP args, SEXP env)
{
    SEXP code, c, ans;
    int i, n;

    checkArity(op, args);
    code = CAR(args);
    if (TYPEOF(code) != VECSXP)
	error(_("code must be a generic vector"));
    c = CADR(args);

    n = LENGTH(code);
    ans = allocVector(VECSXP, n + 1);
    for (i = 0; i < n; i++)
	SET_VECTOR_ELT(ans, i, VECTOR_ELT(code, i));
    SET_VECTOR_ELT(ans, n, c);

    return ans;
}

#ifdef BC_PROFILING
SEXP R_getbcprofcounts()
{
    SEXP val;
    int i;

    val = allocVector(INTSXP, OPCOUNT);
    for (i = 0; i < OPCOUNT; i++)
	INTEGER(val)[i] = opcode_counts[i];
    return val;
}

static void dobcprof(int sig)
{
    if (current_opcode >= 0 && current_opcode < OPCOUNT)
	opcode_counts[current_opcode]++;
    signal(SIGPROF, dobcprof);
}

SEXP R_startbcprof()
{
    struct itimerval itv;
    int interval;
    double dinterval = 0.02;
    int i;

    if (R_Profiling)
	error(_("profile timer in use"));
    if (bc_profiling)
	error(_("already byte code profiling"));

    /* according to man setitimer, it waits until the next clock
       tick, usually 10ms, so avoid too small intervals here */
    interval = 1e6 * dinterval + 0.5;

    /* initialize the profile data */
    current_opcode = NO_CURRENT_OPCODE;
    for (i = 0; i < OPCOUNT; i++)
	opcode_counts[i] = 0;

    signal(SIGPROF, dobcprof);

    itv.it_interval.tv_sec = 0;
    itv.it_interval.tv_usec = interval;
    itv.it_value.tv_sec = 0;
    itv.it_value.tv_usec = interval;
    if (setitimer(ITIMER_PROF, &itv, NULL) == -1)
	error(_("setting profile timer failed"));

    bc_profiling = TRUE;

    return R_NilValue;
}

static void dobcprof_null(int sig)
{
    signal(SIGPROF, dobcprof_null);
}

SEXP R_stopbcprof()
{
    struct itimerval itv;

    if (! bc_profiling)
	error(_("not byte code profiling"));

    itv.it_interval.tv_sec = 0;
    itv.it_interval.tv_usec = 0;
    itv.it_value.tv_sec = 0;
    itv.it_value.tv_usec = 0;
    setitimer(ITIMER_PROF, &itv, NULL);
    signal(SIGPROF, dobcprof_null);

    bc_profiling = FALSE;

    return R_NilValue;
}
#else
SEXP R_getbcprofcounts() { return R_NilValue; }
SEXP R_startbcprof() { return R_NilValue; }
SEXP R_stopbcprof() { return R_NilValue; }
#endif
#endif
