\name{residuals}
\title{Extract Model Residuals}
\usage{
residuals(object, \dots)
resid(object, \dots)
}
\alias{residuals}
\alias{resid}
\alias{residuals.default}
\arguments{
  \item{object}{an object for which the extraction of model residuals is
    meaningful.}
  \item{\dots}{other arguments.}
}
\value{
  Residuals extracted from the object \code{object}.
}
\description{
  \code{residuals} is a generic function which extracts model residuals
  from objects returned by modeling functions.

  The abbreviated form \code{resid} is an alias for \code{residuals}.
  It is intended to encourage users to access object components through
  an accessor function rather than by directly referencing an object
  slot.

  All object classes which are returned by model fitting functions
  should provide a \code{residuals} method.  (Note that the method is
  \samp{residuals} and not \samp{resid}.)

  Methods can make use of \code{\link{naresid}} methods to compensate
  for the omission of missing values.  The default method does.
}
\references{
  Chambers, J. M. and Hastie, T. J. (1992)
  \emph{Statistical Models in S}.
  Wadsworth \& Brooks/Cole.
}
\seealso{
  \code{\link{coefficients}}, \code{\link{fitted.values}},
  \code{\link{glm}}, \code{\link{lm}}.
}
\keyword{models}
\keyword{regression}
