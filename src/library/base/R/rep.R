rep <- function(x, times, ...) UseMethod("rep")

rep.default <- function(x, times, length.out, each, ...)
{
    if (length(x) == 0)
        return(if(missing(length.out)) x else x[seq(len=length.out)])
    if (!missing(each)) {
        tm <- .Internal(rep(each, length(x)))
        nm <- names(x)
        x <- .Internal(rep(x, tm))
        if(!is.null(nm)) names(x) <- .Internal(rep(nm, tm))
        if(missing(length.out) && missing(times)) return(x)
    }
    if (!missing(length.out)) # takes precedence over times
	times <- ceiling(length.out/length(x))
    r <- .Internal(rep(x, times))
    if(!is.null(nm <- names(x))) names(r) <- .Internal(rep(nm, times))
    if (!missing(length.out))
	return(r[if(length.out > 0) 1:length.out else integer(0)])
    return(r)
}

rep.int <- function(x, times) .Internal(rep(x, times))
