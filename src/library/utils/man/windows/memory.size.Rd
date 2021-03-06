\name{memory.size}
\alias{memory.size}
\alias{memory.limit}
\title{Report on Memory Allocation}
\description{
  \code{memory.size} reports the current or maximum memory allocation of
  the \code{malloc} function used in this version of \R.

  \code{memory.limit} reports or increases the limit in force
  on the total allocation.
}
\usage{
memory.size(max = FALSE)

memory.limit(size = NA)
}
\arguments{
  \item{max}{logical. If true the maximum amount of memory obtained from
    the OS is reported, otherwise the amount currently in use.}
  \item{size}{numeric. If \code{NA} report the memory size, otherwise
    request a new limit, in Mb.}
}
\details{
  Command-line flag \code{--max-mem-size} sets the maximum value of
  obtainable memory (including a very small amount of housekeeping
  overhead).

  Memory limits can only be increased.
}
\value{
  Size in bytes.
}
\examples{
memory.size()
memory.size(TRUE)
round(memory.limit()/1048576.0, 2)
}
\keyword{utilities}
