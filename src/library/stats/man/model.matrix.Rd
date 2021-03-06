\name{model.matrix}
\alias{model.matrix}
\alias{model.matrix.default}
\alias{model.matrix.lm}
%\alias{model.matrix.lm.null}
%\alias{model.matrix.glm.null}
\title{Construct Design Matrices}
\usage{
model.matrix(object, \dots)

\method{model.matrix}{default}(object, data = environment(object),
             contrasts.arg = NULL, xlev = NULL, \dots)
}
\arguments{
  \item{object}{an object of an appropriate class. For the default
    method, a model formula or terms object.}
  \item{data}{a data frame created with \code{\link{model.frame}}.}
  \item{contrasts.arg}{A list, whose entries are contrasts suitable for
    input to the \code{\link{contrasts}} replacement function and
    whose names are the names of columns of \code{data} containing
    \code{\link{factor}}s.}
  \item{xlev}{to be used as argument of \code{\link{model.frame}} if
    \code{data} has no \code{"terms"} attribute.}
  \item{\dots}{further arguments passed to or from other methods.}
}
\description{
  \code{model.matrix} creates a design matrix.
}
\details{
  \code{model.matrix} creates a design matrix from the description given
  in \code{terms(formula)}, using the data in \code{data} which must
  contain columns with the same names as would be created by a call to
  \code{model.frame(formula)} or, more precisely, by evaluating
  \code{attr(terms(formula), "variables")}.  There may be other columns
  and the order is not important.

  If \code{contrasts.arg} is specified for a factor it overrides the
  default factor coding for that variable and any \code{"contrasts"}
  attribute set by \code{\link{C}} or \code{\link{contrasts}}.

  In interactions, the variable whose levels vary fastest is the first
  one to appear in the formula (and not in the term), so in \code{~ a +
    b + b:a} the interaction will have \code{a} varying fastest.

  By convention, if the response variable also appears on the
  right-hand side of the formula it is dropped (with a warning),
  although interactions involving the term are retained.
}
\value{
  The design matrix for a regression model with the specified formula
  and data.

  There is an attribute \code{"assign"}, an integer vector with an entry
  for each column in the matrix giving the term in the formula which
  gave rise to the column.
  
  If there are any factors in terms in the model, there is an attribute
  \code{"contrasts"}, a named list with an entry for each factor.  This
  specifies the contrasts that would be used in terms in which the
  factor is coded by contrasts (in some terms dummy coding may be used),
  either as a character vector naming a function or as a numeric matrix.
}
\references{
  Chambers, J. M. (1992)
  \emph{Data for models.}
  Chapter 3 of \emph{Statistical Models in S}
  eds J. M. Chambers and T. J. Hastie, Wadsworth \& Brooks/Cole.
}
\seealso{
  \code{\link{model.frame}}, \code{\link{model.extract}},
  \code{\link{terms}}
}
\examples{
ff <- log(Volume) ~ log(Height) + log(Girth)
str(m <- model.frame(ff, trees))
mat <- model.matrix(ff, m)

dd <- data.frame(a = gl(3,4), b = gl(4,1,12))# balanced 2-way
options("contrasts")
model.matrix(~ a + b, dd)
model.matrix(~ a + b, dd, contrasts = list(a="contr.sum"))
model.matrix(~ a + b, dd, contrasts = list(a="contr.sum", b="contr.poly"))
m.orth <- model.matrix(~a+b, dd, contrasts = list(a="contr.helmert"))
crossprod(m.orth)# m.orth is  ALMOST  orthogonal
}
\keyword{models}
