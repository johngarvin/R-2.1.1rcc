\name{browseURL}
\alias{browseURL}
\title{Load URL into a WWW Browser}
\description{
  Load a given URL into a WWW browser.
}
\usage{
browseURL(url, browser = getOption("browser"))
}
\arguments{
  \item{url}{a non-empty character string giving the URL to be loaded.}
  \item{browser}{a non-empty character string giving the name of the
    program to be used as hypertext browser.  It should be in the PATH,
    or a full path specified.
#ifdef windows
    
    Under Windows \code{NULL} is also allowed (and is the default), and
    implies that the file association mechanism will be used.
#endif
  }
}
#ifdef unix
\details{
  If \code{browser} supports remote control and \R knows how to perform
  it, the URL is opened in any already running browser or a new one if
  necessary.  This mechanism currently is available for browsers which
  support the \code{"-remote openURL(...)"} interface (which includes
  Netscape 4.x, 6.2.x (but not 6.0/1), 7.1, Opera 5/6, Mozilla >= 0.9.5
  and Mozilla Firefox),
  Galeon, KDE konqueror (via kfmclient) and the GNOME interface to
  Mozilla.  Netscape 7.0 and Opera 7 behave slightly differently,
  and you will need
  to open them first.  Note that the type of browser is determined from
  its name, so this mechanism will only be used if the browser is
  installed under its canonical name.

  Because \code{"-remote"} will use any browser displaying on the X
  server (whatever machine it is running on), the remote control
  mechanism is only used if \code{DISPLAY} points to the local host.
  This may not allow displaying more than one URL at a time from a
  remote host.
}
#endif
#ifdef windows
\examples{
\dontrun{browseURL("http://www.r-project.org")
browseURL("file://c|/R/rw1060/doc/html/index.html",
          browser="C:/Program Files/Netscape/Netscape/netscp.exe")
}}
#endif
\keyword{file}
