\name{substituteDirect}
\alias{substituteDirect}
\title{SubstituteDirect}
\description{
  Substitute for the variables named in the second argument the
  corresponding objects, substituting into \code{object}.  The argument
  \code{frame} is a named list; if omitted, the environment of the
  caller is used.

  This function differs from the ordinary \code{substitute} in that it
  treats its first argument in the standard S way, by evaluating it.  In
  contrast, \code{substitute} does not evaluate its first argument.

  The goal is to replace this with an \code{eval=} argument to
  \code{\link{substitute}}.
}
\usage{
substituteDirect(object, frame, cleanFunction=TRUE)
}
\keyword{internal}
