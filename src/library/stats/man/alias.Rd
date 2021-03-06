\name{alias}
\title{Find Aliases (Dependencies) in a Model}
\usage{
alias(object, \dots)

\method{alias}{formula}(object, data, \dots)

\method{alias}{lm}(object, complete = TRUE, partial = FALSE,
      partial.pattern = FALSE, \dots)
}
\alias{alias}
\alias{alias.formula}
\alias{alias.lm}
%\alias{print.mtable}
\arguments{
  \item{object}{A fitted model object, for example from \code{lm} or
    \code{aov}, or a formula for \code{alias.formula}.}
  \item{data}{Optionally, a data frame to search for the objects
    in the formula.}
  \item{complete}{Should information on complete aliasing be included?}
  \item{partial}{Should information on partial aliasing be included?}
  \item{partial.pattern}{Should partial aliasing be presented in a
    schematic way? If this is done, the results are presented in a
    more compact way, usually giving the deciles of the coefficients.}
  \item{\dots}{further arguments passed to or from other methods.}
}
\description{
  Find aliases (linearly dependent terms) in a linear model specified by
  a formula.
}
\details{
  Although the main method is for class \code{"lm"}, \code{alias} is
  most useful for experimental designs and so is used with fits from
  \code{aov}.
  Complete aliasing refers to effects in linear models that cannot be estimated
  independently of the terms which occur earlier in the model and so
  have their coefficients omitted from the fit. Partial aliasing refers
  to effects that can be estimated less precisely because of
  correlations induced by the design.
}
\value{
  A list (of \code{\link[base]{class} "listof"}) containing components
  \item{Model}{Description of the model; usually the formula.}
  \item{Complete}{A matrix with columns corresponding to effects that
    are linearly dependent on the rows; may be of class \code{"mtable"}
    which has its own \code{\link{print}} method.}
  \item{Partial}{The correlations of the estimable effects, with a zero
    diagonal.}
}
\note{The aliasing pattern may depend on the contrasts in use: Helmert
  contrasts are probably most useful.

  The defaults are different from those in S.
}
\references{
  Chambers, J. M., Freeny, A and Heiberger, R. M. (1992)
  \emph{Analysis of variance; designed experiments.}
  Chapter 5 of \emph{Statistical Models in S}
  eds J. M. Chambers and T. J. Hastie, Wadsworth \& Brooks/Cole.
}
\author{
  The design was inspired by the S function of the same name described
  in Chambers \emph{et al.} (1992).
}
\examples{
had.VR <- "package:MASS" \%in\% search()
## The next line is for fractions() which gives neater results
if(!had.VR) res <- require(MASS)
## From Venables and Ripley (2002) p.165.
N <- c(0,1,0,1,1,1,0,0,0,1,1,0,1,1,0,0,1,0,1,0,1,1,0,0)
P <- c(1,1,0,0,0,1,0,1,1,1,0,0,0,1,0,1,1,0,0,1,0,1,1,0)
K <- c(1,0,0,1,0,1,1,0,0,1,0,1,0,1,1,0,0,0,1,1,1,0,1,0)
yield <- c(49.5,62.8,46.8,57.0,59.8,58.5,55.5,56.0,62.8,55.8,69.5,55.0,
           62.0,48.8,45.5,44.2,52.0,51.5,49.8,48.8,57.2,59.0,53.2,56.0)
npk <- data.frame(block=gl(6,4), N=factor(N), P=factor(P),
                  K=factor(K), yield=yield)

op <- options(contrasts=c("contr.helmert", "contr.poly"))
npk.aov <- aov(yield ~ block + N*P*K, npk)
alias(npk.aov)
if(!had.VR && res) detach(package:MASS)
options(op)# reset
}
\keyword{models}
