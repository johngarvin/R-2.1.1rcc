example <-
function(topic, package = NULL, lib.loc = NULL, local = FALSE,
	 echo = TRUE, verbose = getOption("verbose"), setRNG = FALSE,
	 prompt.echo = paste(abbreviate(topic, 6), "> ", sep = ""))
{
    topic <- substitute(topic)
    if(!is.character(topic))
	topic <- deparse(topic)[1]
    INDICES <- .find.package(package, lib.loc, verbose = verbose)
    file <- index.search(topic, INDICES, "AnIndex", "R-ex")
    if(file == "") {
	warning(gettextf("no help file found for '%s'", topic), domain = NA)
	return(invisible())
    }
    packagePath <- dirname(dirname(file))
    if(length(file) > 1) {
	packagePath <- packagePath[1]
	warning(gettextf("more than one help file found: using package '%s'",
                basename(packagePath)), domain = NA)
	file <- file[1]
    }
    pkg <- basename(packagePath)
    lib <- dirname(packagePath)
    zfile <- zip.file.extract(file, "Rex.zip")
    if(zfile != file) on.exit(unlink(zfile))
    if(!file.exists(zfile)) {
	warning(gettextf("'%s' has a help file but no examples file", topic),
                domain = NA)
	return(invisible())
    }
    if(pkg != "base")
	library(pkg, lib = lib, character.only = TRUE)
    if(!is.logical(setRNG) || setRNG) {
	## save current RNG state:
	if((exists(".Random.seed", envir = .GlobalEnv))) {
	    oldSeed <- get(".Random.seed", envir = .GlobalEnv)
	    on.exit(assign(".Random.seed", oldSeed, envir = .GlobalEnv))
	} else {
	    oldRNG <- RNGkind()
	    on.exit(RNGkind(oldRNG[1], oldRNG[2]))
	}
	## set RNG
	if(is.logical(setRNG)) { # i.e. == TRUE: use the same as R CMD check
	    ## see ../../../../share/perl/massage-Examples.pl
	    RNGkind("default", "default")
	    set.seed(1)
	} else eval(setRNG)
    }
    encoding <-
        if(length(enc <- localeToCharset()) > 1)
            c(enc[-length(enc)], "latin1")
        else ""
    ## peek at the file, but note we can't usefully translate to C.
    zz <- readLines(zfile, n=1)
    if(length(grep("^### Encoding: ", zz)) > 0 &&
       !identical(Sys.getlocale("LC_CTYPE"), "C"))
        encoding <- substring(zz, 15)
    source(zfile, local, echo = echo, prompt.echo = prompt.echo,
	   verbose = verbose, max.deparse.length = 250,
           encoding = encoding)
}
