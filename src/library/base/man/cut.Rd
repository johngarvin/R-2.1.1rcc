\name{cut}
\title{Convert Numeric to Factor}
\usage{
cut(x, \dots)

\method{cut}{default}(x, breaks, labels = NULL,
    include.lowest = FALSE, right = TRUE, dig.lab = 3, \dots)
}
\alias{cut}
\alias{cut.default}
\arguments{
  \item{x}{a numeric vector which is to be converted to a factor by cutting.}
  \item{breaks}{either a vector of cut points or number
    giving the number of intervals which \code{x} is to be cut into.}
  \item{labels}{labels for the levels of the resulting category.  By default,
    labels are constructed using \code{"(a,b]"} interval notation. If
    \code{labels = FALSE}, simple integer codes are returned instead of
    a factor.}.
  \item{include.lowest}{logical, indicating if an \sQuote{x[i]} equal to
    the lowest (or highest, for \code{right = FALSE}) \sQuote{breaks}
    value should be included.}
  \item{right}{logical, indicating if the intervals should be closed on
    the right (and open on the left) or vice versa.}
  \item{dig.lab}{integer which is used when labels are not given. It
    determines the number of digits used in formatting the break numbers.}
  \item{\dots}{further arguments passed to or from other methods.}
}
\description{
  \code{cut} divides the range of \code{x} into intervals
  and codes the values in \code{x} according to which
  interval they fall.
  The leftmost interval corresponds to level one,
  the next leftmost to level two and so on.
}
\value{
  A \code{\link{factor}} is returned, unless \code{labels = FALSE} which
  results in the mere integer level codes.
}
\details{
  If a \code{labels} parameter is specified, its values are used
  to name the factor levels. If none is specified, the factor
  level labels are constructed as \code{"(b1, b2]"}, \code{"(b2, b3]"}
  etc. for \code{right = TRUE} and as \code{"[b1, b2)"}, \ldots if
  \code{right = FALSE}.
  In this case, \code{dig.lab} indicates the minimum number  of digits
  should be used in formatting the numbers \code{b1}, \code{b2}, \ldots.
  A larger value (up to 12) will be used if needed to distinguish
  between any pair of  endpoints: if this fails labels such as
  \code{"Range3"} will be used.
}
\note{
  Instead of \code{table(cut(x, br))}, \code{hist(x, br, plot = FALSE)} is
  more efficient and less memory hungry.  Instead of \code{cut(*,
    labels = FALSE)}, \code{\link{findInterval}()} is more efficient.
}
\references{
  Becker, R. A., Chambers, J. M. and Wilks, A. R. (1988)
  \emph{The New S Language}.
  Wadsworth \& Brooks/Cole.
}
\seealso{
  \code{\link{split}} for splitting a variable according to a group factor;
  \code{\link{factor}}, \code{\link{tabulate}}, \code{\link{table}},
  \code{\link{findInterval}()}.
}
\examples{
Z <- rnorm(10000)
table(cut(Z, br = -6:6))
sum(table(cut(Z, br = -6:6, labels=FALSE)))
sum(   hist  (Z, br = -6:6, plot=FALSE)$counts)

cut(rep(1,5),4)#-- dummy
tx0 <- c(9, 4, 6, 5, 3, 10, 5, 3, 5)
x <- rep(0:8, tx0)
stopifnot(table(x) == tx0)

table( cut(x, b = 8))
table( cut(x, br = 3*(-2:5)))
table( cut(x, br = 3*(-2:5), right = FALSE))

##--- some values OUTSIDE the breaks :
table(cx  <- cut(x, br = 2*(0:4)))
table(cxl <- cut(x, br = 2*(0:4), right = FALSE))
which(is.na(cx));  x[is.na(cx)]  #-- the first 9  values  0
which(is.na(cxl)); x[is.na(cxl)] #-- the last  5  values  8


## Label construction:
y <- rnorm(100)
table(cut(y, breaks = pi/3*(-3:3)))
table(cut(y, breaks = pi/3*(-3:3), dig.lab=4))

table(cut(y, breaks =  1*(-3:3), dig.lab=4))
# extra digits don't "harm" here
table(cut(y, breaks =  1*(-3:3), right = FALSE))
#- the same, since no exact INT!

## sometimes the default dig.lab is not enough to be avoid confusion:
aaa <- c(1,2,3,4,5,2,3,4,5,6,7)
cut(aaa, 3)
cut(aaa, 3, dig.lab=4)
}
\keyword{category}
