/*
 *  Mathlib : A C Library of Special Functions
 *  Copyright (C) 1998-2004 Ross Ihaka and the R Development Core Team
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "nmath.h"

#ifdef MATHLIB_STANDALONE
/*
 *  based on code in ../main/arithmetic.c
 *  used only in standalone Rmath lib.
 */

/* Include the header file defining finite() */
#ifdef HAVE_IEEE754_H
# include <ieee754.h>		/* newer Linuxen */
#else
# ifdef HAVE_IEEEFP_H
#  include <ieeefp.h>		/* others [Solaris], .. */
# endif
#endif

int R_finite(double x)
{
#ifdef HAVE_WORKING_ISFINITE
    return isfinite(x);
#elif HAVE_WORKING_FINITE
    return finite(x);
# else
/* neither finite nor isfinite work */
# ifdef _AIX
#  include <fp.h>
     return FINITE(x);
# else
    return (!isnan(x) & (x != ML_POSINF) & (x != ML_NEGINF));
# endif
#endif
}

/* C++ math header undefines any isnan macro. This file
   doesn't get C++ headers and so is safe. */
int R_isnancpp(double x)
{
	return (isnan(x) != 0);

}

static double myfmod(double x1, double x2)
{
    double q = x1 / x2;
    return x1 - floor(q) * x2;
}

#ifdef HAVE_WORKING_LOG
# define R_log	log
#else
double R_log(double x) { return(x > 0 ? log(x) : x < 0 ? ML_NAN : ML_NEGINF); }
#endif

double R_pow(double x, double y) /* = x ^ y */
{
    if(x == 1. || y == 0.)
	return(1.);
    if(x == 0.) {
	if(y > 0.) return(0.);
	/* y < 0 */return(ML_POSINF);
    }
    if (R_FINITE(x) && R_FINITE(y))
	return(pow(x,y));
    if (ISNAN(x) || ISNAN(y)) {
#ifdef IEEE_754
	return(x + y);
#else
	return(NA_REAL);
#endif
    }
    if(!R_FINITE(x)) {
	if(x > 0)		/* Inf ^ y */
	    return((y < 0.)? 0. : ML_POSINF);
	else {			/* (-Inf) ^ y */
	    if(R_FINITE(y) && y == floor(y)) /* (-Inf) ^ n */
		return((y < 0.) ? 0. : (myfmod(y,2.) ? x  : -x));
	}
    }
    if(!R_FINITE(y)) {
	if(x >= 0) {
	    if(y > 0)		/* y == +Inf */
		return((x >= 1)? ML_POSINF : 0.);
	    else		/* y == -Inf */
		return((x < 1) ? ML_POSINF : 0.);
	}
    }
    return(ML_NAN);		/* all other cases: (-Inf)^{+-Inf,
				   non-int}; (neg)^{+-Inf} */
}

double R_pow_di(double x, int n)
{
    double pow = 1.0;

    if (ISNAN(x)) return x;
    if (n != 0) {
	if (!R_FINITE(x)) return R_pow(x, (double)n);
	if (n < 0) { n = -n; x = 1/x; }
	for(;;) {
	    if(n & 01) pow *= x;
	    if(n >>= 1) x *= x; else break;
	}
    }
    return pow;
}

double NA_REAL = ML_NAN;
double R_PosInf = ML_POSINF, R_NegInf = ML_NEGINF;

#include <stdio.h>
#include <stdarg.h>
void REprintf(char *format, ...)
{
    va_list(ap);
    va_start(ap, format);
    fprintf(stderr, format, ap);
    va_end(ap);
}

#endif /* MATHLIB_STANDALONE */
