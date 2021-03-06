\name{name}
\alias{name}
\alias{is.symbol}
\alias{as.symbol}
\alias{as.name}
\alias{is.name}
\title{Variable Names or Symbols, respectively}
\description{
  \code{as.symbol} coerces its argument to be a \emph{symbol}, or
  equivalently, a \emph{name}.  The argument must be of mode
  \code{"character"}.  \code{as.name} is an alias for \code{as.symbol}.

  \code{is.symbol} (and \code{is.name} equivalently) returns \code{TRUE}
  or \code{FALSE} depending on whether its argument is a symbol (i.e.,
  name) or not.
}
\usage{
as.symbol(x)
is.symbol(y)

as.name(x)
is.name(y)
}
\arguments{
  \item{x, y}{objects to be coerced or tested.} 
}
\note{
  The term \dQuote{symbol} is from the LISP background of \R, whereas
  \dQuote{name} has been the standard S term for this.
}
\details{
  \code{is.symbol} is generic: you can write methods to handle
  specific classes of objects, see \link{InternalMethods}.
}
\references{
  Becker, R. A., Chambers, J. M. and Wilks, A. R. (1988)
  \emph{The New S Language}.
  Wadsworth \& Brooks/Cole.
}
\seealso{
  \code{\link{call}}, \code{\link{is.language}}.
  For the internal object mode, \code{\link{typeof}}.
}
\examples{
an <- as.name("arrg")
is.name(an) # TRUE
mode(an)   # name
typeof(an) # symbol
}
\keyword{programming}
\keyword{attribute}
