\name{tsdiag}
\alias{tsdiag}
\alias{tsdiag.arima0}
\alias{tsdiag.Arima}
\alias{tsdiag.StructTS}
\title{Diagnostic Plots for Time-Series Fits}
\description{
  A generic function to plot time-series diagnostics.
}
\usage{
tsdiag(object, gof.lag, \dots)
}
\arguments{
  \item{object}{a fitted time-series model}
  \item{gof.lag}{the maximum number of lags for a Portmanteau
    goodness-of-fit test}
  \item{\dots}{further arguments to be passed to particular methods}
}
\details{
  This is a generic function. It will generally plot the residuals,
  often standadized, the autocorrelation function of the residuals, and
  the p-values of a Portmanteau test for all lags up to \code{gof.lag}.

  The methods for \code{\link{arima}} and \code{\link{StructTS}} objects
  plots residuals scaled by the estimate of their (individual) variance,
  and use the Ljung--Box version of the portmanteau test.
}
\value{
  None. Diagnostics are plotted.
}

\seealso{
  \code{\link{arima}}, \code{\link{StructTS}}, \code{\link{Box.test}}
}

\examples{
\dontrun{fit <- arima(lh, c(1,0,0))
tsdiag(fit)

## see also examples(arima)

(fit <- StructTS(log10(JohnsonJohnson), type="BSM"))
tsdiag(fit)
}}
\keyword{ts}

