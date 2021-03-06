\name{hist.POSIXt}
\alias{hist.POSIXt}
\alias{hist.Date}
\title{Histogram of a Date or Date-Time Object}
\description{
  Method for \code{\link{hist}} applied to date or date-time objects.
}
\usage{
\method{hist}{POSIXt}(x, breaks, \dots, plot = TRUE, freq = FALSE,
     start.on.monday = TRUE, format)

\method{hist}{Date}(x, breaks, \dots, plot = TRUE, freq = FALSE,
     start.on.monday = TRUE, format)
}
\synopsis{
hist.POSIXt(x, breaks, ..., xlab = deparse(substitute(x)),
            plot = TRUE, freq = FALSE,
            start.on.monday = TRUE, format)
hist.Date(x, breaks, ..., xlab = deparse(substitute(x)),
          plot = TRUE, freq = FALSE, start.on.monday = TRUE, format)
}
\arguments{
  \item{x}{an object inheriting from class \code{"POSIXt"} or \code{"Date"}.}
  \item{breaks}{
    a vector of cut points \emph{or} number giving the number of
    intervals which \code{x} is to be cut into \emph{or} an
    interval specification, one of \code{"days"}, \code{"weeks"},
    \code{"months"} or \code{"years"}, plus \code{"secs"}, \code{"mins"},
    \code{"hours"} for date-time objects.
 }
  \item{\dots}{graphical parameters, or arguments to
    \code{\link{hist.default}} such as \code{include.lowest},
      \code{right} and \code{labels}.}
  \item{plot}{logical.  If \code{TRUE} (default), a histogram is
    plotted, otherwise a list of breaks and counts is returned.}
  \item{freq}{logical; if \code{TRUE}, the
    histogram graphic is a representation of frequencies, i.e,
    the \code{counts} component of the result; if \code{FALSE},
    \emph{relative} frequencies (\dQuote{probabilities}) are plotted.}
  \item{start.on.monday}{logical. If \code{breaks = "weeks"}, should the
    week start on Mondays or Sundays?}
  \item{format}{for the x-axis labels. See \code{\link{strptime}}.}
}
\value{
  An object of class \code{"histogram"}: see \code{\link{hist}}.
}
\seealso{
  \code{\link{seq.POSIXt}}, \code{\link{axis.POSIXct}}, \code{\link{hist}}
}
\examples{
hist(.leap.seconds, "years", freq = TRUE)
hist(.leap.seconds,
     seq(ISOdate(1970, 1, 10), ISOdate(2002, 1, 1), "5 years"))

## 100 random dates in a 10-week period
random.dates <- as.Date("2001/1/1") + 70*runif(100)
hist(random.dates, "weeks", format = "\%d \%b")
}
\keyword{chron}
\keyword{dplot}
\keyword{hplot}
