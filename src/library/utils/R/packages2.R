install.packages <-
    function(pkgs, lib, repos = CRAN,
             contriburl = contrib.url(repos, type),
             CRAN = getOption("repos"),
             method, available = NULL, destdir = NULL,
             installWithVers = FALSE, dependencies = FALSE,
             type = getOption("pkgType"))
{
    explode_bundles <- function(a)
    {
        contains <- .find_bundles(a, FALSE)
        extras <- unlist(lapply(names(contains), function(x)
                                paste(contains[[x]], " (", x, ")", sep="")))
        sort(as.vector(c(a[, 1], extras)))
    }

    implode_bundles <- function(pkgs)
    {
    	bundled <- grep(".* \\(.*\\)$", pkgs)
    	if (length(bundled)) {
    	    bundles <- unique(gsub(".* \\((.*)\\)$", "\\1", pkgs[bundled]))
    	    pkgs <- c(pkgs[-bundled], bundles)
    	}
    	pkgs
    }

    if(missing(pkgs) || !length(pkgs)) {
        if(.Platform$OS.type == "windows" || .Platform$GUI == "AQUA") {
            if(is.null(available))
                available <- available.packages(contriburl = contriburl,
                                                method = method)
            if(NROW(available)) {
                a <- explode_bundles(available)
                pkgs <- implode_bundles(select.list(a, multiple = TRUE, title = "Packages"))
            }
            if(!length(pkgs)) stop("no packages were specified")
        } else if(.Platform$OS.type == "unix" &&
                  capabilities("tcltk") && capabilities("X11")) {
            if(is.null(available))
                available <- available.packages(contriburl = contriburl,
                                                method = method)
            if(NROW(available)) {
                a <- explode_bundles(available)
                pkgs <- implode_bundles(tcltk::tk_select.list(a, multiple = TRUE,
                                              title ="Packages"))
            }
            if(!length(pkgs)) stop("no packages were specified")
        } else
            stop("no packages were specified")
    }

    if(missing(lib) || is.null(lib)) {
        lib <- .libPaths()[1]
        if(length(.libPaths()) > 1)
            warning(gettextf("argument 'lib' is missing: using %s", lib),
                    immediate. = TRUE, domain = NA)
    }

    if(.Platform$OS.type == "windows") {
        if(type == "mac.binary")
            stop("cannot install MacOS X binary packages on Windows")

        if(type == "win.binary") {      # include local .zip files
            .install.winbinary(pkgs = pkgs, lib = lib, contriburl = contriburl,
                               method = method, available = available,
                               destdir = destdir,
                               installWithVers = installWithVers,
                               dependencies = dependencies)
            return(invisible())
        }
    } else {
        if(type == "mac.binary") {
            if(!length(grep("darwin", R.version$platform)))
                stop("cannot install MacOS X binary packages on this plaform")
            .install.macbinary(pkgs = pkgs, lib = lib, contriburl = contriburl,
                               method = method, available = available,
                               destdir = destdir,
                               installWithVers = installWithVers,
                               dependencies = dependencies)
            return(invisible())
        }

        if(type == "win.binary")
            stop("cannot install Windows binary packages on this plaform")

        if(!file.exists(file.path(R.home(), "bin", "INSTALL")))
            stop("This version of R is not set up to install source packages\nIf it was installed from an RPM, you may need the R-devel RPM")
    }

    if(is.null(repos) & missing(contriburl)) {
        update <- cbind(pkgs, lib) # for side-effect of recycling to same length
        cmd0 <- paste(file.path(R.home(),"bin","R"), "CMD INSTALL")
        if (installWithVers)
            cmd0 <- paste(cmd0, "--with-package-versions")
        for(i in 1:nrow(update)) {
            cmd <- paste(cmd0, "-l", shQuote(update[i, 2]),
                         shQuote(update[i, 1]))
            if(system(cmd) > 0)
                warning(gettextf(
                 "installation of package '%s' had non-zero exit status",
                                update[i, 1]), domain = NA)
        }
        return(invisible())
    }

    oneLib <- length(lib) == 1
    tmpd <- destdir
    nonlocalcran <- length(grep("^file:", contriburl)) < length(contriburl)
    if(is.null(destdir) && nonlocalcran) {
        tmpd <- file.path(tempdir(), "downloaded_packages")
        if (!file.exists(tmpd) && !dir.create(tmpd))
            stop(gettextf("Unable to create temporary directory '%s'", tmpd),
                 domain = NA)
    }

    depends <- is.character(dependencies) ||
    (is.logical(dependencies) && dependencies)
    if(depends && is.logical(dependencies))
        dependencies <-  c("Depends", "Imports", "Suggests")
    if(depends && !oneLib) {
        warning("Do not know which element of 'lib' to install dependencies into\nskipping dependencies")
        depends <- FALSE
    }
    if(is.null(available))
        available <- available.packages(contriburl = contriburl,
                                        method = method)
    bundles <- .find_bundles(available)
    for(bundle in names(bundles))
        pkgs[ pkgs %in% bundles[[bundle]] ] <- bundle
    if(depends) { # check for dependencies, recursively
        p0 <- p1 <- unique(pkgs) # this is ok, as 1 lib only
        have <- .packages(all.available = TRUE)
	repeat {
	    if(any(miss <- ! p1 %in% row.names(available))) {
		cat(sprintf(ngettext(sum(miss),
				     "dependency %s is not available",
				     "dependencies %s are not available"),
		    paste(sQuote(p1[miss]), collapse=", ")), "\n\n", sep ="")
		flush.console()
	    }
	    p1 <- p1[!miss]
	    deps <- as.vector(available[p1, dependencies])
	    deps <- .clean_up_dependencies(deps, available)
	    if(!length(deps)) break
	    toadd <- deps[! deps %in% c("R", have, pkgs)]
	    if(length(toadd) == 0) break
	    pkgs <- c(toadd, pkgs)
	    p1 <- toadd
	}
        for(bundle in names(bundles))
            pkgs[ pkgs %in% bundles[[bundle]] ] <- bundle
        pkgs <- unique(pkgs)
        pkgs <- pkgs[pkgs %in% row.names(available)]
        if(length(pkgs) > length(p0)) {
            added <- setdiff(pkgs, p0)
            cat(ngettext(length(added),
                         "also installing the dependency ",
                         "also installing the dependencies "),
                paste(sQuote(added), collapse=", "), "\n\n", sep="")
            flush.console()
        }
    }

    pkgs <- unique(pkgs) # in case ask for more than one from a bundle
    foundpkgs <- download.packages(pkgs, destdir = tmpd, available = available,
                                   contriburl = contriburl, method = method,
                                   type = "source")

    ## at this point pkgs may contain duplicates,
    ## the same pkg in different libs
    if(!is.null(foundpkgs)) {
        update <- unique(cbind(pkgs, lib))
        colnames(update) <- c("Package", "LibPath")
        found <- pkgs %in% foundpkgs[, 1]
        files <- foundpkgs[match(pkgs[found], foundpkgs[, 1]), 2]
        update <- cbind(update[found, , drop=FALSE], file = files)
        if(nrow(update) > 1) {
            upkgs <- unique(pkgs <- update[, 1])
            DL <- .make_dependency_list(upkgs, available)
            p0 <- .find_install_order(upkgs, DL)
            ## can't use update[p0, ] due to possible multiple matches
            update <- update[sort.list(match(pkgs, p0)), ]
        }
        cmd0 <- paste(file.path(R.home(),"bin","R"), "CMD INSTALL")
        if (installWithVers)
            cmd0 <- paste(cmd0, "--with-package-versions")
        for(i in 1:nrow(update)) {
            cmd <- paste(cmd0, "-l", shQuote(update[i, 2]), update[i, 3])
            status <- system(cmd)
            if(status > 0)
                warning(gettextf(
                 "installation of package '%s' had non-zero exit status",
                                 update[i, 1]), domain = NA)
        }
        if(!is.null(tmpd) && is.null(destdir))
            cat("\n", gettextf("The downloaded packages are in\n\t%s",
                               normalizePath(tmpd)), "\n", sep = "")
        link.html.help(verbose = TRUE)
    } else if(!is.null(tmpd) && is.null(destdir)) unlink(tmpd, TRUE)

    invisible()
}
