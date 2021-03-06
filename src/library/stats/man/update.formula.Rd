\name{update.formula}
\alias{update.formula}
\title{Model Updating}
\usage{
\method{update}{formula}(old, new, ...)
}
\arguments{
  \item{old}{a model formula to be updated.}
  \item{new}{a formula giving a template which specifies how to update.}
  \item{\dots}{further arguments passed to or from other methods.}
}
\description{
  \code{update.formula} is used to update model formulae.
  This typically involves adding or dropping terms,
  but updates can be more general.
}
\details{
  The function works by first identifying the \emph{left-hand side}
  and \emph{right-hand side} of the \code{old} formula.
  It then examines the \code{new} formula and substitutes
  the \emph{lhs} of the \code{old} formula for any occurence
  of "\code{.}" on the left of \code{new}, and substitutes
  the \emph{rhs} of the \code{old} formula for any occurence
  of "\code{.}" on the right of \code{new}.
}
\value{
  The updated formula is returned.
}
\seealso{
  \code{\link{terms}}, \code{\link{model.matrix}}.
}
\examples{
update(y ~ x,    ~ . + x2) #> y ~ x + x2
update(y ~ x, log(.) ~ . ) #> log(y) ~ x
}
\keyword{models}
