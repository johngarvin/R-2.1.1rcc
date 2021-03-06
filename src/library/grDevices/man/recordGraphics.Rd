\name{recordGraphics}
\alias{recordGraphics}
%- Also NEED an '\alias' for EACH other topic documented here.
\title{ Record graphics operations }
\description{
  Records arbitrary code on the graphics engine display list.
  Useful for encapsulating calculations with graphical output
  that depends on the calculations.
  Intended \emph{only} for expert use.
}
\usage{
recordGraphics(expr, list, env)
}
%- maybe also 'usage' for other objects documented here.
\arguments{
  \item{expr}{object of mode \code{\link{expression}} or \code{call} or
    an \dQuote{unevaluated expression}.}
  \item{list}{a list defining the environment in which \code{expr} is to
    be evaluated.}
  \item{env}{An \code{\link{environment}} specifying
    where \R looks for objects not found in
    \code{envir}.}
}
\value{
  The value from evaluating \code{expr}.
}
\details{
  The code in \code{expr} is evaluated in an environment constructed
  from \code{list}, with \code{env} as the parent of that environment.

  All three arguments are saved on the graphics engine display list
  so that on a device resize or copying between devices, the original
  evaluation environment can be recreated and the code can be
  reevaluated to reproduce the graphical output.
}
\section{Warning }{
  This function is not intended for general use.  Incorrect or
  improper use
  of this function could lead to unintended and/or undesirable
  results.

  An example of acceptable use is querying the current state
  of a graphics device or graphics system setting and then
  calling a graphics function.

  An example of improper use would be calling the \code{assign} function
  to performing assignments in the
  global environment.
} 
\seealso{\code{\link{eval}} }
\examples{
plot(1:10)
# This rectangle remains 1inch wide when the device is resized
recordGraphics(
  {
    rect(4, 2,
         4 + diff(par("usr")[1:2])/par("pin")[1], 3)
  },
  list(),
  getNamespace("graphics"))
}
\keyword{ device }% at least one, from doc/KEYWORDS
