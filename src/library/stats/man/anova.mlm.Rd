\name{anova.mlm}
\alias{anova.mlm}
\alias{anova.mlmlist}
\title{Comparisons between multivariate linear models}
\description{
  Compute gereralized analysis of variance table for a list of
  multivariate linear models. At least two models must be given.
}
\usage{
## S3 method for class 'mlm'
anova.mlm(object, ...,
   test = c("Pillai", "Wilks", "Hotelling-Lawley", "Roy", " Spherical"),
   Sigma = diag(nrow = p),
   T = Thin.row(proj(M) - proj(X)), M = diag(nrow = p), X = ~0,
   idata = data.frame(index = seq(length = p)))
}
\arguments{
  \item{object}{An object of class \code{mlm}}
  \item{\dots}{Further objects of class \code{mlm}}
  \item{test}{Choice of test statistic (se below)}
  \item{Sigma}{(Only relevant if  \code{test=="Spherical"}). Covariance
    matrix assumed proportional to \code{Sigma}}
  \item{T}{Transformation matrix. By default computed from \code{M} and
    \code{X}} 
  \item{M}{Formula or matrix describing the outer projection (see below)}
  \item{X}{Formula or matrix describing the inner projection (see below)}
  \item{idata}{Data frame describing intra-block design}
}
\details{
  The \code{anova.mlm} method uses either a multivariate test statistic for
  the summary table, or a test based on sphericity assumptions (i.e.
  that the covariance is proportional to a given matrix).

  For the multivariate test, Wilks' statistic is most popular in the
  literature, but the default Pillai-Bartlett statistic is
  recommended by Hand and Taylor (1987).

  For the \code{"Spherical"} test, proportionality is usually with the
  identity matrix but a different matrix can be specified using \code{Sigma}).
  Corrections for asphericity known as the Greenhouse-Geisser,
  respectively Huynh-Feldt, epsilons are given and adjusted F tests are
  performed.

  It is common to transform the observations prior to testing. This
  typically involves 
  transformation to intra-block differences, but more complicated
  within-block designs can be encountered,
  making more elaborate transformations necessary. A
  transformation matrix \code{T} can be given directly or specified as
  the difference between two projections onto the spaces spanned by
  \code{M} and \code{X}, which in turn can be given as matrices or as
  model formulas with respect to \code{idata} (the tests will be
  invariant to parametrization of the quotient space \code{M/X}).
  
  Similar to \code{anova.lm} all test statistics use the SSD matrix from
  the largest model considered as the (generalized) denominator.
}
\value{
   An object of class \code{"anova"} inheriting from class \code{"data.frame"}
}
\note{The Huynh-Feldt epsilon differs from that calculated by SAS (as of
  v. 8.2) except when the DF is equal to the number of observations
minus one. This is believed to be a bug in SAS, not in R.}

\references{
  Hand, D. J. and Taylor, C. C.  (1987)
  \emph{Multivariate Analysis of Variance and Repeated Measures.}
  Chapman and Hall.
}

%% Probably use example from Baron/Li
\seealso{\code{\link{summary.manova}}}
\examples{
example(SSD) # Brings in the mlmfit and reacttime objects

mlmfit0 <- update(mlmfit,~0)

### Traditional tests of intrasubj. contrasts
## Using MANOVA techniques on contrasts:
anova(mlmfit, mlmfit0, X=~1)

## Assuming sphericity
anova(mlmfit, mlmfit0, X=~1, test="Spherical") 


### tests using intra-subject 3x2 design
idata <- data.frame(deg=gl(3,1,6,labels=c(0,4,8)),
                    noise=gl(2,3,6,labels=c("A","P")))

anova(mlmfit, mlmfit0, X = ~ deg + noise, idata = idata, test = "Spherical")
anova(mlmfit, mlmfit0, M = ~ deg + noise, X = ~ noise, idata = idata,
          test="Spherical" )
anova(mlmfit, mlmfit0, M = ~ deg + noise, X = ~ deg, idata = idata,
          test="Spherical" )

### There seems to be a strong interaction in these data
plot(colMeans(reacttime))
}
\keyword{regression}
\keyword{models}
\keyword{multivariate}