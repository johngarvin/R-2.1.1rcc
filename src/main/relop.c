/*
 *  R : A Computer Language for Statistical Data Analysis
 *  Copyright (C) 1995, 1996  Robert Gentleman and Ross Ihaka
 *  Copyright (C) 1997--2005  R Development Core Team
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

/* <UTF8> char here is handled as a whole.
   Collation is locale-specific if strcoll exists and works.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <Defn.h>
#include <Rmath.h>

static SEXP integer_relop(RELOP_TYPE code, SEXP s1, SEXP s2);
static SEXP real_relop(RELOP_TYPE code, SEXP s1, SEXP s2);
static SEXP complex_relop(RELOP_TYPE code, SEXP s1, SEXP s2, SEXP call);
static SEXP string_relop(RELOP_TYPE code, SEXP s1, SEXP s2);
static SEXP raw_relop(RELOP_TYPE code, SEXP s1, SEXP s2);

SEXP do_relop(SEXP call, SEXP op, SEXP args, SEXP env)
{
    SEXP ans;

    if (DispatchGroup("Ops", call, op, args, env, &ans))
	return ans;

    return do_relop_dflt(call, op, CAR(args), CADR(args));
}

SEXP do_relop_dflt(SEXP call, SEXP op, SEXP x, SEXP y)
{
    SEXP class=R_NilValue, dims, tsp=R_NilValue, xnames, ynames;
    int nx, ny, xarray, yarray, xts, yts;
    Rboolean mismatch = FALSE, iS;
    PROTECT_INDEX xpi, ypi;

    PROTECT_WITH_INDEX(x, &xpi);
    PROTECT_WITH_INDEX(y, &ypi);
    nx = length(x);
    ny = length(y);

    /* pre-test to handle the most common case quickly.
       Used to skip warning too ....
     */
    if (ATTRIB(x) == R_NilValue && ATTRIB(y) == R_NilValue &&
	TYPEOF(x) == REALSXP && TYPEOF(y) == REALSXP &&
	LENGTH(x) > 0 && LENGTH(y) > 0) {
	SEXP ans = real_relop(PRIMVAL(op), x, y);
	if (nx > 0 && ny > 0)
	    mismatch = ((nx > ny) ? nx % ny : ny % nx) != 0;
	if (mismatch)
	    warningcall(call, _("longer object length\n\
 \tis not a multiple of shorter object length"));
	UNPROTECT(2);
	return ans;
    }

    if ((iS = isSymbol(x)) || TYPEOF(x) == LANGSXP) {
	SEXP tmp = allocVector(STRSXP, 1);
	PROTECT(tmp);
	SET_STRING_ELT(tmp, 0, (iS) ? PRINTNAME(x) :
		       STRING_ELT(deparse1(x, 0, SIMPLEDEPARSE), 0));
	REPROTECT(x = tmp, xpi);
	UNPROTECT(1);
    }
    if ((iS = isSymbol(y)) || TYPEOF(y) == LANGSXP) {
	SEXP tmp = allocVector(STRSXP, 1);
	PROTECT(tmp);
	SET_STRING_ELT(tmp, 0, (iS) ? PRINTNAME(y) :
		       STRING_ELT(deparse1(y, 0, SIMPLEDEPARSE), 0));
	REPROTECT(y = tmp, ypi);
	UNPROTECT(1);
    }

    if (!isVector(x) || !isVector(y)) {
	if (isNull(x) || isNull(y)) {
	    UNPROTECT(2);
	    return allocVector(LGLSXP,0);
	}
	errorcall(call,
		  _("comparison (%d) is possible only for atomic and list types"),
		  PRIMVAL(op));
    }

    if (TYPEOF(x) == EXPRSXP || TYPEOF(y) == EXPRSXP)
	errorcall(call, _("comparison is not allowed for expressions"));

    /* ELSE :  x and y are both atomic or list */

    if (LENGTH(x) <= 0 || LENGTH(y) <= 0) {
	UNPROTECT(2);
	return allocVector(LGLSXP,0);
    }

    mismatch = FALSE;
    xarray = isArray(x);
    yarray = isArray(y);
    xts = isTs(x);
    yts = isTs(y);
    if (nx > 0 && ny > 0)
	mismatch = ((nx > ny) ? nx % ny : ny % nx) != 0;

    if (xarray || yarray) {
	if (xarray && yarray) {
	    if (!conformable(x, y))
		errorcall(call, _("non-conformable arrays"));
	    PROTECT(dims = getAttrib(x, R_DimSymbol));
	}
	else if (xarray) {
	    PROTECT(dims = getAttrib(x, R_DimSymbol));
	}
	else /*(yarray)*/ {
	    PROTECT(dims = getAttrib(y, R_DimSymbol));
	}
	PROTECT(xnames = getAttrib(x, R_DimNamesSymbol));
	PROTECT(ynames = getAttrib(y, R_DimNamesSymbol));
    }
    else {
	PROTECT(dims = R_NilValue);
	PROTECT(xnames = getAttrib(x, R_NamesSymbol));
	PROTECT(ynames = getAttrib(y, R_NamesSymbol));
    }
    if (xts || yts) {
	if (xts && yts) {
	    if (!tsConform(x, y))
		errorcall(call, _("non-conformable time series"));
	    PROTECT(tsp = getAttrib(x, R_TspSymbol));
	    PROTECT(class = getAttrib(x, R_ClassSymbol));
	}
	else if (xts) {
	    if (length(x) < length(y))
		ErrorMessage(call, ERROR_TSVEC_MISMATCH);
	    PROTECT(tsp = getAttrib(x, R_TspSymbol));
	    PROTECT(class = getAttrib(x, R_ClassSymbol));
	}
	else /*(yts)*/ {
	    if (length(y) < length(x))
		ErrorMessage(call, ERROR_TSVEC_MISMATCH);
	    PROTECT(tsp = getAttrib(y, R_TspSymbol));
	    PROTECT(class = getAttrib(y, R_ClassSymbol));
	}
    }
    if (mismatch)
	warningcall(call, _("longer object length\n\tis not a multiple of shorter object length"));

    if (isString(x) || isString(y)) {
	REPROTECT(x = coerceVector(x, STRSXP), xpi);
	REPROTECT(y = coerceVector(y, STRSXP), ypi);
	x = string_relop(PRIMVAL(op), x, y);
    }
    else if (isComplex(x) || isComplex(y)) {
	REPROTECT(x = coerceVector(x, CPLXSXP), xpi);
	REPROTECT(y = coerceVector(y, CPLXSXP), ypi);
	x = complex_relop(PRIMVAL(op), x, y, call);
    }
    else if (isReal(x) || isReal(y)) {
	REPROTECT(x = coerceVector(x, REALSXP), xpi);
	REPROTECT(y = coerceVector(y, REALSXP), ypi);
	x = real_relop(PRIMVAL(op), x, y);
    }
    else if (isInteger(x) || isInteger(y)) {
	REPROTECT(x = coerceVector(x, INTSXP), xpi);
	REPROTECT(y = coerceVector(y, INTSXP), ypi);
	x = integer_relop(PRIMVAL(op), x, y);
    }
    else if (isLogical(x) || isLogical(y)) {
	REPROTECT(x = coerceVector(x, LGLSXP), xpi);
	REPROTECT(y = coerceVector(y, LGLSXP), ypi);
	x = integer_relop(PRIMVAL(op), x, y);
    }
    else if (TYPEOF(x) == RAWSXP || TYPEOF(y) == RAWSXP) {
	REPROTECT(x = coerceVector(x, RAWSXP), xpi);
	REPROTECT(y = coerceVector(y, RAWSXP), ypi);
	x = raw_relop(PRIMVAL(op), x, y);
    } else errorcall(call, _("comparison of these types is not implemented"));


    PROTECT(x);
    if (dims != R_NilValue) {
	setAttrib(x, R_DimSymbol, dims);
	if (xnames != R_NilValue)
	    setAttrib(x, R_DimNamesSymbol, xnames);
	else if (ynames != R_NilValue)
	    setAttrib(x, R_DimNamesSymbol, ynames);
    }
    else {
	if (length(x) == length(xnames))
	    setAttrib(x, R_NamesSymbol, xnames);
	else if (length(x) == length(ynames))
	    setAttrib(x, R_NamesSymbol, ynames);
    }
    if (xts || yts) {
	setAttrib(x, R_TspSymbol, tsp);
	setAttrib(x, R_ClassSymbol, class);
	UNPROTECT(2);
    }

    UNPROTECT(6);
    return x;
}

/* i1 = i % n1; i2 = i % n2;
 * this macro is quite a bit faster than having real modulo calls
 * in the loop (tested on Intel and Sparc)
 */
#define mod_iterate(n1,n2,i1,i2) for (i=i1=i2=0; i<n; \
	i1 = (++i1 == n1) ? 0 : i1,\
	i2 = (++i2 == n2) ? 0 : i2,\
	++i)

#define mod_iterate1(n1,n2,i2) \
  for (i=i2=0; i<n1; i2 = (++i2 == n2) ? 0 : i2, ++i)

#define NATST_INTEGER(x) (x == NA_INTEGER)
#define NATST_COMPLEX(x) (ISNAN(x.r) || ISNAN(x.i)) 
#define NATST_FALSE(x) 0

#define RELOP_EQOP(x,y) (x == y) 
#define RELOP_NEOP(x,y) (x != y) 
#define RELOP_LTOP(x,y) (x < y) 
#define RELOP_GTOP(x,y) (x > y) 
#define RELOP_LEOP(x,y) (x <= y) 
#define RELOP_GEOP(x,y) (x >= y) 

#define RELOP_COMPLEX(JOIN_OP,PRIM_OP,x,y) \
  ((x.r PRIM_OP y.r) JOIN_OP (x.i PRIM_OP y.i))

#define RELOP_COMPLEX_EQOP(x,y) RELOP_COMPLEX(&&,==,x,y)		
#define RELOP_COMPLEX_NEOP(x,y) RELOP_COMPLEX(||,!=,x,y)		

#define relop_macro(NATST,TYPE,RELOP)					\
  if (n2 == 1) { /* special case: rhs operand is singleton */		\
    x2 = TYPE(s2)[0];							\
    if (NATST(x2)) {							\
      for (i = 0; i < n; i++) LOGICAL(ans)[i] = NA_LOGICAL;		\
    } else {								\
      for (i = 0; i < n1; i++) {					\
	x1 = TYPE(s1)[i];						\
	if (NATST(x1)) LOGICAL(ans)[i] = NA_LOGICAL;			\
	else LOGICAL(ans)[i] = RELOP(x1,x2);				\
      }									\
    }									\
  } else if (n1 == n2) { /* special case: operands same length */	\
    for (i = 0; i < n1; i++) {						\
      x1 = TYPE(s1)[i];							\
      x2 = TYPE(s2)[i];							\
      if (NATST(x1) || NATST(x2)) LOGICAL(ans)[i] = NA_LOGICAL;		\
      else LOGICAL(ans)[i] = RELOP(x1,x2);				\
    }									\
  } else { /* general case */						\
    mod_iterate1(n1, n2, i2) {						\
      x1 = TYPE(s1)[i];							\
      x2 = TYPE(s2)[i2];						\
      if (NATST(x1) || NATST(x2)) LOGICAL(ans)[i] = NA_LOGICAL;		\
      else LOGICAL(ans)[i] = RELOP(x1,x2);				\
    }									\
  }

#define swap(TYPE,a,b) { TYPE tmp = a; a = b; b = tmp; }

#define relop_postamble \
  UNPROTECT(2);		\
  return ans;		

#define relop_preamble(CTYPE)			    \
  int i, i1, i2, n, n1, n2;			    \
  CTYPE x1, x2;					    \
  SEXP ans;					    \
  n1 = LENGTH(s1);				    \
  n2 = LENGTH(s2);				    \
  n = (n1 > n2) ? n1 : n2;			    \
  PROTECT(s1);					    \
  PROTECT(s2);					    \
  ans = allocVector(LGLSXP, n);			    \
  /* swap operands so longest is lhs */		    \
  if (n1 < n2) {				    \
    swap(int,n1,n2); swap(SEXP,s1,s2);		    \
    /* reverse operator, if asymmetric */	    \
    switch(code) {				    \
    case LTOP: code = GTOP; break;		    \
    case GTOP: code = LTOP; break;		    \
    case LEOP: code = GEOP; break;		    \
    case GEOP: code = LEOP; break;		    \
    default: break;				    \
    }						    \
  }						    \

#define generic_relop(code,CTYPE,RTYPE,NATST,s1,s2)	   \
  {							   \
    relop_preamble(CTYPE);				   \
    switch (code) {					   \
    case EQOP: relop_macro(NATST,RTYPE,RELOP_EQOP); break; \
    case NEOP: relop_macro(NATST,RTYPE,RELOP_NEOP); break; \
    case LTOP: relop_macro(NATST,RTYPE,RELOP_LTOP); break; \
    case GTOP: relop_macro(NATST,RTYPE,RELOP_GTOP); break; \
    case LEOP: relop_macro(NATST,RTYPE,RELOP_LEOP); break; \
    case GEOP: relop_macro(NATST,RTYPE,RELOP_GEOP); break; \
    }							   \
    relop_postamble;					   \
  }

static SEXP integer_relop(RELOP_TYPE code, SEXP s1, SEXP s2)
{
  generic_relop(code,int,INTEGER,NATST_INTEGER,s1,s2);
}

static SEXP real_relop(RELOP_TYPE code, SEXP s1, SEXP s2)
{
  generic_relop(code,double,REAL,ISNAN,s1,s2);
}

static SEXP complex_relop(RELOP_TYPE code, SEXP s1, SEXP s2, SEXP call)
{
  relop_preamble(Rcomplex);
  if (code != EQOP && code != NEOP) {
    errorcall(call, _("invalid comparison with complex values"));
  }
  switch (code) {					   
  case EQOP: relop_macro(NATST_COMPLEX,COMPLEX,RELOP_COMPLEX_EQOP); break;   
  case NEOP: relop_macro(NATST_COMPLEX,COMPLEX,RELOP_COMPLEX_NEOP); break;   
  default: break; /* never reached */
  }
  relop_postamble;
}

#if defined(Win32) && defined(SUPPORT_UTF8)
#define STRCOLL Rstrcoll
#else

#ifdef HAVE_STRCOLL
#define STRCOLL strcoll
#else
#define STRCOLL strcmp
#endif

#endif

static SEXP string_relop(RELOP_TYPE code, SEXP s1, SEXP s2)
{
    int i, n, n1, n2;
    SEXP ans;

    n1 = LENGTH(s1);
    n2 = LENGTH(s2);
    n = (n1 > n2) ? n1 : n2;
    PROTECT(s1);
    PROTECT(s2);
    ans = allocVector(LGLSXP, n);

    switch (code) {
    case EQOP:
	for (i = 0; i < n; i++) {
	    if ((STRING_ELT(s1, i % n1) == NA_STRING) ||
		(STRING_ELT(s2, i % n2) == NA_STRING))
		LOGICAL(ans)[i] = NA_LOGICAL;
 	    else
	    if (strcmp(CHAR(STRING_ELT(s1, i % n1)),
		       CHAR(STRING_ELT(s2, i % n2))) == 0)
		LOGICAL(ans)[i] = 1;
	    else
		LOGICAL(ans)[i] = 0;
	}
	break;
    case NEOP:
	for (i = 0; i < n; i++) {
	    if ((STRING_ELT(s1, i % n1) == NA_STRING) ||
		(STRING_ELT(s2, i % n2) == NA_STRING))
		LOGICAL(ans)[i] = NA_LOGICAL;
 	    else
	    if (streql(CHAR(STRING_ELT(s1, i % n1)),
		       CHAR(STRING_ELT(s2, i % n2))) != 0)
		LOGICAL(ans)[i] = 0;
	    else
		LOGICAL(ans)[i] = 1;
	}
	break;
    case LTOP:
	for (i = 0; i < n; i++) {
	    if ((STRING_ELT(s1, i % n1) == NA_STRING) ||
		(STRING_ELT(s2, i % n2) == NA_STRING))
		LOGICAL(ans)[i] = NA_LOGICAL;
 	    else
	    if (STRCOLL(CHAR(STRING_ELT(s1, i % n1)),
		       CHAR(STRING_ELT(s2, i % n2))) < 0)
		LOGICAL(ans)[i] = 1;
	    else
		LOGICAL(ans)[i] = 0;
	}
	break;
    case GTOP:
	for (i = 0; i < n; i++) {
	    if ((STRING_ELT(s1, i % n1) == NA_STRING) ||
		(STRING_ELT(s2, i % n2) == NA_STRING))
		LOGICAL(ans)[i] = NA_LOGICAL;
 	    else
	    if (STRCOLL(CHAR(STRING_ELT(s1, i % n1)),
		       CHAR(STRING_ELT(s2, i % n2))) > 0)
		LOGICAL(ans)[i] = 1;
	    else
		LOGICAL(ans)[i] = 0;
	}
	break;
    case LEOP:
	for (i = 0; i < n; i++) {
	    if ((STRING_ELT(s1, i % n1) == NA_STRING) ||
		(STRING_ELT(s2, i % n2) == NA_STRING))
		LOGICAL(ans)[i] = NA_LOGICAL;
 	    else
	    if (STRCOLL(CHAR(STRING_ELT(s1, i % n1)),
		       CHAR(STRING_ELT(s2, i % n2))) <= 0)
		LOGICAL(ans)[i] = 1;
	    else
		LOGICAL(ans)[i] = 0;
	}
	break;
    case GEOP:
	for (i = 0; i < n; i++) {
	    if ((STRING_ELT(s1, i % n1) == NA_STRING) ||
		(STRING_ELT(s2, i % n2) == NA_STRING))
		LOGICAL(ans)[i] = NA_LOGICAL;
 	    else
	    if (STRCOLL(CHAR(STRING_ELT(s1, i % n1)),
		       CHAR(STRING_ELT(s2, i % n2))) >= 0)
		LOGICAL(ans)[i] = 1;
	    else
		LOGICAL(ans)[i] = 0;
	}
	break;
    }
    UNPROTECT(2);
    return ans;
}

static SEXP raw_relop(RELOP_TYPE code, SEXP s1, SEXP s2)
{
  generic_relop(code,Rbyte,RAW,NATST_FALSE,s1,s2);
}
