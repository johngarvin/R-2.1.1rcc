getCRANmirrors <- function()
    read.csv(file.path(R.home(), "doc", "CRAN_mirrors.csv"),
             as.is=TRUE)


mirror2html <- function(mirrors = NULL, file="mirrors.html",
                        head = "mirrors-head.html",
                        foot = "mirrors-foot.html")    
{
    if(is.null(mirrors)){
        mirrors <- getCRANmirrors()
    }

    z <- NULL
    if(file.exists(head))
        z <- readLines(head)

    z <- c(z, "<dl>")
    
    for(country in unique(mirrors$Country)){
        m = mirrors[mirrors$Country==country,]
        z <- c(z, paste("<dt>", country, "</dt>", sep=""),
               "<dd>",
               "<table border=0 width=90%>")

        for(k in 1:nrow(m)){
            z <- c(z, "<tr>",
                   "<td width=45%>",
                   "<a href=\"", m$URL[k], "\" target=\"_top\">",
                   m$URL[k], "</a>",
                   "</td>\n",
                   "<td>", m$Host[k], "</td>",
                   "</tr>")
        }
        z <- c(z, "</table>", "</dd>")
    }

    z <- c(z, "</dl>")
    
    if(file.exists(foot))
        z <- c(z, readLines(foot))

    if(file!="")
        writeLines(z, file)

    invisible(z)
}
  
checkCRAN <-function(method)
{
    master <- CRAN.packages(CRAN="http://cran.R-project.org", method=method)
    m <- getCRANmirrors()

    z <- list()
    for(url in as.character(m$URL))
        z[[url]] = CRAN.packages(url, method=method)

    lapply(z, function(a) all.equal(a, master))
}
       
                

    

    
