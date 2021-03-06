require(tcltk) || stop("tcltk support is absent")
local({

    tt <- tktoplevel()
    tkwm.title(tt, "R FAQ")
    txt <- tktext(tt, bg="white", font="courier")
    scr <- tkscrollbar(tt, repeatinterval=5,
                       command=function(...)tkyview(txt,...))
    ## Safest to make sure scr exists before setting yscrollcommand
    tkconfigure(txt, yscrollcommand=function(...)tkset(scr,...))
    tkpack(txt, side="left", fill="both", expand=TRUE)
    tkpack(scr, side="right", fill="y")

    chn <- tclopen(file.path(R.home(), "FAQ"))
    tkinsert(txt, "end", tclread(chn))
    tclclose(chn)
    
    tkconfigure(txt, state="disabled")
    tkmark.set(txt,"insert","0.0")
    tkfocus(txt)

    cat("******************************************************\n",
        "The source for this demo can be found in the file:\n",
        file.path(system.file(package = "tcltk"), "demo", "tkfaq.R"),
        "\n******************************************************\n")
})
