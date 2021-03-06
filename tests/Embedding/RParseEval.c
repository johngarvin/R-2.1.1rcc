#include <R.h>
#include <Rinternals.h>
#include <Rdefines.h>
#include <R_ext/Parse.h>

#include "embeddedRCall.h"

int
main(int argc, char *argv[])
{
    SEXP e, tmp;
    int n, hadError;
    ParseStatus status;

    init_R(argc, argv);

    PROTECT(tmp = NEW_CHARACTER(1));
    SET_STRING_ELT(tmp, 0, 
		   COPY_TO_USER_STRING("{plot(1:10, pch=\"+\");print(1:10)}"));
    e = R_ParseVector(tmp, 1, &status);

    PROTECT(e);
    Rf_PrintValue(e);
    n = GET_LENGTH(e);

    Test_tryEval(VECTOR_ELT(e,0), &hadError);  

    UNPROTECT(2);

    return(0);
}
