/*
 *  R : A Computer Language for Statistical Data Analysis
 *  Copyright (C) 1995, 1996  Robert Gentleman and Ross Ihaka
 *  Copyright (C) 1997--2002  Robert Gentleman, Ross Ihaka and the
 *			      R Development Core Team
 *  Copyright (C) 2003-4      The R Foundation
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
# include <config.h>
#endif

#include <Defn.h>
#include <R_ext/Random.h>
#include <R_ext/Applic.h>	/* for rcont2() */
#include <Rmath.h>		/* for rxxx functions */

static void invalid(SEXP call)
{
    errorcall(call, _("invalid arguments"));
}

static Rboolean random1(double (*f) (), double *a, int na, double *x, int n)
{
    Rboolean naflag = FALSE;
    double ai;
    int i;
    errno = 0;
    for (i = 0; i < n; i++) {
	ai = a[i % na];
	x[i] = f(ai);
	if (!R_FINITE(x[i])) naflag = 1;
    }
    return(naflag);
}

#define RAND1(num,name) \
	case num: \
		naflag = random1(name, REAL(a), na, REAL(x), n); \
		break


/* "do_random1" - random sampling from 1 parameter families. */
/* See switch below for distributions. */

SEXP do_random1(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    SEXP x, a;
    int i, n, na;
    Rboolean naflag = FALSE;
    checkArity(op, args);
    if (!isVector(CAR(args)) || !isNumeric(CADR(args)))
	invalid(call);
    if (LENGTH(CAR(args)) == 1) {
	n = asInteger(CAR(args));
	if (n == NA_INTEGER || n < 0)
	    invalid(call);
    }
    else n = LENGTH(CAR(args));
    PROTECT(x = allocVector(REALSXP, n));
    if (n == 0) {
	UNPROTECT(1);
	return(x);
    }
    na = LENGTH(CADR(args));
    if (na < 1) {
	for (i = 0; i < n; i++)
	    REAL(x)[i] = NA_REAL;
    }
    else {
	PROTECT(a = coerceVector(CADR(args), REALSXP));
	naflag = FALSE;
	GetRNGstate();
	switch (PRIMVAL(op)) {
	    RAND1(0, rchisq);
	    RAND1(1, rexp);
	    RAND1(2, rgeom);
	    RAND1(3, rpois);
	    RAND1(4, rt);
	    RAND1(5, rsignrank);
	default:
	    error(_("internal error in do_random1"));
	}
	if (naflag)
	    warningcall(call, _("NAs produced"));

	PutRNGstate();
	UNPROTECT(1);
    }
    UNPROTECT(1);
    return x;
}

static Rboolean random2(double (*f) (), double *a, int na, double *b, int nb,
		    double *x, int n)
{
    double ai, bi; int i;
    Rboolean naflag = FALSE;
    errno = 0;
    for (i = 0; i < n; i++) {
	ai = a[i % na];
	bi = b[i % nb];
	x[i] = f(ai, bi);
	if (!R_FINITE(x[i])) naflag = 1;
    }
    return(naflag);
}

#define RAND2(num,name) \
	case num: \
		random2(name, REAL(a), na, REAL(b), nb, REAL(x), n); \
		break

/* "do_random2" - random sampling from 2 parameter families. */
/* See switch below for distributions. */

SEXP do_random2(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    SEXP x, a, b;
    int i, n, na, nb;
    Rboolean naflag = FALSE;
    checkArity(op, args);
    if (!isVector(CAR(args)) ||
	!isNumeric(CADR(args)) ||
	!isNumeric(CADDR(args)))
	invalid(call);
    if (LENGTH(CAR(args)) == 1) {
	n = asInteger(CAR(args));
	if (n == NA_INTEGER || n < 0)
	    invalid(call);
    }
    else n = LENGTH(CAR(args));
    PROTECT(x = allocVector(REALSXP, n));
    if (n == 0) {
	UNPROTECT(1);
	return(x);
    }
    na = LENGTH(CADR(args));
    nb = LENGTH(CADDR(args));
    if (na < 1 || nb < 1) {
	for (i = 0; i < n; i++)
	    REAL(x)[i] = NA_REAL;
    }
    else {
	PROTECT(a = coerceVector(CADR(args), REALSXP));
	PROTECT(b = coerceVector(CADDR(args), REALSXP));
	naflag = 0;
	GetRNGstate();
	switch (PRIMVAL(op)) {
	    RAND2(0, rbeta);
	    RAND2(1, rbinom);
	    RAND2(2, rcauchy);
	    RAND2(3, rf);
	    RAND2(4, rgamma);
	    RAND2(5, rlnorm);
	    RAND2(6, rlogis);
	    RAND2(7, rnbinom);
	    RAND2(8, rnorm);
	    RAND2(9, runif);
	    RAND2(10, rweibull);
	    RAND2(11, rwilcox);
	    RAND2(12, rnchisq);
	default:
	    error(_("internal error in do_random2"));
	}
	if (naflag)
	    warningcall(call, _("NAs produced"));

	PutRNGstate();
	UNPROTECT(2);
    }
    UNPROTECT(1);
    return x;
}

static Rboolean random3(double (*f) (), double *a, int na, double *b, int nb,
			double *c, int nc, double *x, int n)
{
    double ai, bi, ci;
    int i;
    Rboolean naflag = FALSE;
    errno = 0;
    for (i = 0; i < n; i++) {
	ai = a[i % na];
	bi = b[i % nb];
	ci = c[i % nc];
	x[i] = f(ai, bi, ci);
	if (!R_FINITE(x[i])) naflag = TRUE;
    }
    return(naflag);
}

#define RAND3(num,name) \
	case num: \
		random3(name, REAL(a), na, REAL(b), nb, REAL(c), nc, REAL(x), n); \
		break


/* "do_random3" - random sampling from 3 parameter families. */
/* See switch below for distributions. */

SEXP do_random3(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    SEXP x, a, b, c;
    int i, n, na, nb, nc;
    Rboolean naflag = FALSE;
    checkArity(op, args);
    if (!isVector(CAR(args))) invalid(call);
    if (LENGTH(CAR(args)) == 1) {
	n = asInteger(CAR(args));
	if (n == NA_INTEGER || n < 0)
	    invalid(call);
    }
    else n = LENGTH(CAR(args));
    PROTECT(x = allocVector(REALSXP, n));
    if (n == 0) {
	UNPROTECT(1);
	return(x);
    }

    args = CDR(args); a = CAR(args);
    args = CDR(args); b = CAR(args);
    args = CDR(args); c = CAR(args);
    if (!isNumeric(a) || !isNumeric(b) || !isNumeric(c))
	invalid(call);
    na = LENGTH(a);
    nb = LENGTH(b);
    nc = LENGTH(c);
    if (na < 1 || nb < 1 || nc < 1) {
	for (i = 0; i < n; i++)
	    REAL(x)[i] = NA_REAL;
    }
    else {
	PROTECT(a = coerceVector(a, REALSXP));
	PROTECT(b = coerceVector(b, REALSXP));
	PROTECT(c = coerceVector(c, REALSXP));
	naflag = 0;
	GetRNGstate();
	switch (PRIMVAL(op)) {
	    RAND3(0, rhyper);
	default:
	    error(_("internal error in do_random3"));
	}
	if (naflag)
	    warningcall(call, _("NAs produced"));

	PutRNGstate();
	UNPROTECT(3);
    }
    UNPROTECT(1);
    return x;
}


/*
 *  Unequal Probability Sampling.
 *
 *  Modelled after Fortran code provided by:
 *    E. S. Venkatraman <venkat@biosta.mskcc.org>
 *  but with significant modifications in the
 *  "with replacement" case.
 */

/* Unequal probability sampling; with-replacement case */

static void ProbSampleReplace(int n, double *p, int *perm, int nans, int *ans)
{
    double rU;
    int i, j;
    int nm1 = n - 1;

    /* record element identities */
    for (i = 0; i < n; i++)
	perm[i] = i + 1;

    /* sort the probabilities into descending order */
    revsort(p, perm, n);

    /* compute cumulative probabilities */
    for (i = 1 ; i < n; i++)
	p[i] += p[i - 1];

    /* compute the sample */
    for (i = 0; i < nans; i++) {
	rU = unif_rand();
	for (j = 0; j < nm1; j++) {
	    if (rU <= p[j])
		break;
	}
	ans[i] = perm[j];
    }
}

/* Unequal probability sampling; without-replacement case */

static void ProbSampleNoReplace(int n, double *p, int *perm,
				int nans, int *ans)
{
    double rT, mass, totalmass;
    int i, j, k, n1;

    /* Record element identities */
    for (i = 0; i < n; i++)
	perm[i] = i + 1;

    /* Sort probabilities into descending order */
    /* Order element identities in parallel */
    revsort(p, perm, n);

    /* Compute the sample */
    totalmass = 1;
    for (i = 0, n1 = n-1; i < nans; i++, n1--) {
	rT = totalmass * unif_rand();
	mass = 0;
	for (j = 0; j < n1; j++) {
	    mass += p[j];
	    if (rT <= mass)
		break;
	}
	ans[i] = perm[j];
	totalmass -= p[j];
	for(k = j; k < n1; k++) {
	    p[k] = p[k + 1];
	    perm[k] = perm[k + 1];
	}
    }
}

/* Equal probability sampling; with-replacement case */

static void SampleReplace(int k, int n, int *y)
{
    int i;
    for (i = 0; i < k; i++)
	y[i] = n * unif_rand() + 1;
}

/* Equal probability sampling; without-replacement case */

static void SampleNoReplace(int k, int n, int *y, int *x)
{
    int i, j;
    for (i = 0; i < n; i++)
	x[i] = i;
    for (i = 0; i < k; i++) {
	j = n * unif_rand();
	y[i] = x[j] + 1;
	x[j] = x[--n];
    }
}

static void FixupProb(SEXP call, double *p, int n, int k, int replace)
{
    double sum;
    int i, npos;
    npos = 0;
    sum = 0.;
    for (i = 0; i < n; i++) {
	if (!R_FINITE(p[i]))
	    errorcall(call, _("NA in probability vector"));
	if (p[i] < 0)
	    errorcall(call, _("non-positive probability"));
	if (p[i] > 0) {
	    npos++;
	    sum += p[i];
	}
    }
    if (npos == 0 || (!replace && k > npos))
	errorcall(call, _("too few positive probabilities"));
    for (i = 0; i < n; i++)
	p[i] /= sum;
}

/* do_sample - equal probability sampling with/without replacement. */
/* Implements sample(n, k, r) - choose k elements from 1 to n */
/* with/without replacement according to r. */
SEXP do_sample(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    SEXP x, y, prob;
    int k, n, replace;
    checkArity(op, args);
    n = asInteger(CAR(args)); args = CDR(args);
    k = asInteger(CAR(args)); args = CDR(args);
    replace = asLogical(CAR(args)); args = CDR(args);
    prob = CAR(args);
    if (replace == NA_LOGICAL)
	errorcall(call, _("invalid third argument"));
    if (n == NA_INTEGER || n < 1)
	errorcall(call, _("invalid first argument"));
    if (k == NA_INTEGER || k < 0)
	errorcall(call, _("invalid second argument"));
    if (!replace && k > n)
	errorcall(call, _("cannot take a sample larger than the population\n when 'replace = FALSE'"));
    GetRNGstate();
    PROTECT(y = allocVector(INTSXP, k));
    if (!isNull(prob)) {
	prob = coerceVector(prob, REALSXP);
	if (NAMED(prob)) prob = duplicate(prob);
	PROTECT(prob);
	if (length(prob) != n)
	    errorcall(call, _("incorrect number of probabilities"));
	FixupProb(call, REAL(prob), n, k, replace);
	PROTECT(x = allocVector(INTSXP, n));
	if (replace)
	    ProbSampleReplace(n, REAL(prob), INTEGER(x), k, INTEGER(y));
	else
	    ProbSampleNoReplace(n, REAL(prob), INTEGER(x), k, INTEGER(y));
	UNPROTECT(2);
    }
    else {
	if (replace) SampleReplace(k, n, INTEGER(y));
	else {
	    x = allocVector(INTSXP, n);
	    SampleNoReplace(k, n, INTEGER(y), INTEGER(x));
	}
    }
    PutRNGstate();
    UNPROTECT(1);
    return y;
}

SEXP do_rmultinom(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    SEXP prob, ans, nms;
    int n, size, k, i, ik;
    checkArity(op, args);
    n	 = asInteger(CAR(args)); args = CDR(args);/* n= #{samples} */
    size = asInteger(CAR(args)); args = CDR(args);/* X ~ Multi(size, prob) */
    if (n == NA_INTEGER || n < 0)
	errorcall(call, _("invalid first argument 'n'"));
    if (size == NA_INTEGER || size < 0)
	errorcall(call, _("invalid second argument 'size'"));
    prob = CAR(args);
    prob = coerceVector(prob, REALSXP);
    k = length(prob);/* k = #{components or classes} = X-vector length */
    if (NAMED(prob)) prob = duplicate(prob);/*as `do_sample' -- need this line? */
    PROTECT(prob);
    FixupProb(call, REAL(prob), k, 0, 1);/*check and make sum = 1 */
    GetRNGstate();
    PROTECT(ans = allocMatrix(INTSXP, k, n));/* k x n : natural for columnwise store */
    for(i=ik = 0; i < n; i++, ik += k)
	rmultinom(size, REAL(prob), k, &INTEGER(ans)[ik]);
    PutRNGstate();
    if(!isNull(nms = getAttrib(prob, R_NamesSymbol))) {
	SEXP dimnms;
	PROTECT(nms);
	PROTECT(dimnms = allocVector(VECSXP, 2));
	SET_VECTOR_ELT(dimnms, 0, nms);
	setAttrib(ans, R_DimNamesSymbol, dimnms);
	UNPROTECT(2);
    }
    UNPROTECT(2);
    return ans;
}

SEXP
R_r2dtable(SEXP n, SEXP r, SEXP c)
{
    int nr, nc, *row_sums, *col_sums, i, *jwork;
    int n_of_samples, n_of_cases;
    double *fact;
    SEXP ans, tmp;

    nr = length(r);
    nc = length(c);

    /* Note that the R code in r2dtable() also checks for missing and
       negative values.
       Should maybe do the same here ...
    */
    if(!isInteger(n) || (length(n) == 0) ||
       !isInteger(r) || (nr <= 1) ||
       !isInteger(c) || (nc <= 1))
	error(_("invalid arguments"));

    n_of_samples = INTEGER(n)[0];
    row_sums = INTEGER(r);
    col_sums = INTEGER(c);

    /* Compute total number of cases as the sum of the row sums.
       Note that the R code in r2dtable() also checks whether this is
       the same as the sum of the col sums.
       Should maybe do the same here ...
    */
    n_of_cases = 0;
    jwork = row_sums;
    for(i = 0; i < nr; i++)
	n_of_cases += *jwork++;

    /* Log-factorials from 0 to n_of_cases.
       (I.e., lgamma(1), ..., lgamma(n_of_cases + 1).)
    */
    fact = (double *) R_alloc(n_of_cases + 1, sizeof(double));
    fact[0] = 0.;
    for(i = 1; i <= n_of_cases; i++)
	fact[i] = lgammafn((double) (i + 1));

    jwork = (int *) R_alloc(nc, sizeof(int));

    PROTECT(ans = allocVector(VECSXP, n_of_samples));

    GetRNGstate();

    for(i = 0; i < n_of_samples; i++) {
	PROTECT(tmp = allocMatrix(INTSXP, nr, nc));
	rcont2(&nr, &nc, row_sums, col_sums, &n_of_cases, fact,
	       jwork, INTEGER(tmp));
	SET_VECTOR_ELT(ans, i, tmp);
	UNPROTECT(1);
    }

    PutRNGstate();

    UNPROTECT(1);

    return(ans);
}
