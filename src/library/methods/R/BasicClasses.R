.InitBasicClasses <-
  function(envir)
{
    ## setClass won't allow redefining basic classes,
    ## so make the list of these empty for now.
    assign(".BasicClasses", character(), envir)
    ## hide some functions that would break because the basic
    ## classes are not yet defined
    real.reconcileP <- reconcilePropertiesAndPrototype
    assign("reconcilePropertiesAndPrototype", function(name, properties, prototype, extends, where) {
        list(properties=properties, prototype = prototype, extends = extends)
    }, envir)
    clList = character()
    setClass("VIRTUAL", where = envir); clList <- c(clList, "VIRTUAL")
    setClass("oldClass", where = envir); clList <- c(clList, "oldClass")
    setClass("ANY", where = envir); clList <- c(clList, "ANY")
    setClass("vector", where = envir); clList <- c(clList, "vector")
    setClass("missing", where = envir); clList <- c(clList, "missing")
    vClasses <- c("logical", "numeric", "character",
                "complex", "integer", "single", "double", "raw",
                "expression", "list")
    for(.class in vClasses) {
        setClass(.class, prototype = newBasic(.class), where = envir)
    }
    ## there is a bug that makes is.null(expression()) TRUE.  Until it's fixed,
    ## the following kludge
    { setClass("expression", prototype = expression(TRUE), where = envir)
      .class <- getClass("expression", where = envir)
      .class@prototype <- newBasic("expression")
      assignClassDef("expression", .class, envir)
    }
    clList <- c(clList, vClasses)
    nullF <- function()NULL; environment(nullF) <- .GlobalEnv
    setClass("function", prototype = nullF, where = envir); clList <- c(clList, "function")

    setClass("language", where = envir); clList <- c(clList, "language")
    setClass("environment", prototype = new.env(), where = envir); clList <- c(clList, "environment")

    setClass("externalptr", prototype = .newExternalptr(), where = envir); clList <- c(clList, "externalptr")


    ## NULL is weird in that it has NULL as a prototype, but is not virtual
    nullClass <- newClassRepresentation(className="NULL", prototype = NULL, virtual=FALSE, package = "methods")
    assignClassDef("NULL", nullClass, where = envir); clList <- c(clList, "NULL")


    setClass("structure", where = envir); clList <- c(clList, "structure")
    stClasses <- c("matrix", "array") # classes that have attributes, but no class attr.
    for(.class in stClasses) {
        setClass(.class, prototype = newBasic(.class), where = envir)
    }
    ## "ts" will be defined below, because it has a formal contains, but its initialize
    ## method uses newBasic, so it needs to be in .BasicClasses
    clList <- c(clList, stClasses, "ts")
    assign(".BasicClasses", clList, envir)

    ## Now we can define the SClassExtension class and use it to instantiate some
    ## is() relations.
    .InitExtensions(envir)

    for(.class in vClasses)
        setIs(.class, "vector", where = envir)

    setIs("double", "numeric", where = envir)
    setIs("integer", "numeric", where = envir)

    setIs("structure", "vector", coerce = .gblEnv(function(object) as.vector(object)),
          replace = .gblEnv(function(from, to, value) {
              attributes(value) <- attributes(from)
              value
          }),
          where = envir)

    for(.class in stClasses)
        setIs(.class, "structure", where = envir)
    setIs("matrix", "array", where = envir)
    setIs("array", "matrix", test = .gblEnv(function(object) length(dim(object)) == 2),
          replace = .gblEnv(function(from, to, value) {
              if(is(value, "matrix"))
                  value
              else
                  stop("replacement value is not a matrix")
          }),
          where = envir)

    ## "ts" was an S3 class.  Because it has a consistent set of attributes (unlike array)
    ## it can be promoted to an S4 class with metadata, slot checking, etc.
    ## The initialize method uses newBasic(...), so should be consistent with the old code,
    ## (see def'n of BasicClasses above).
    setClass("ts", representation(.Data = "vector", tsp = "numeric"), contains = "vector",
             prototype = newBasic("ts"), where = envir)


    ## Some class definitions extending "language", delayed to here so
    ## setIs will work.
    setClass("name", "language", prototype = as.name("<UNDEFINED>"), where = envir); clList <- c(clList, "name")
    setClass("call", "language", prototype = quote("<undef>"()), where = envir); clList <- c(clList, "call")
    setClass("{", "language", prototype = quote({}), where = envir); clList <- c(clList, "{")
    setClass("if", "language", prototype = quote(if(NA) TRUE else FALSE), where = envir); clList <- c(clList, "if")
    setClass("<-", "language", prototype = quote("<undef>"<-NULL), where = envir); clList <- c(clList, "<-")
    setClass("for", "language", prototype = quote(for(NAME in logical()) NULL), where = envir); clList <- c(clList, "for")
    setClass("while", "language", prototype = quote(while(FALSE) NULL), where = envir); clList <- c(clList, "while")
    setClass("repeat", "language", prototype = quote(repeat{break}), where = envir); clList <- c(clList, "repeat")
    setClass("(", "language", prototype = quote((NULL)), where = envir); clList <- c(clList, "(")

    ## a virtual class used to allow NULL as an indicator that a possible function
    ## is not supplied (used, e.g., for the validity slot in classRepresentation
    setClass("OptionalFunction", where = envir)
    setIs("function", "OptionalFunction", where = envir)
    setIs("NULL", "OptionalFunction")
    assign(".BasicClasses", clList, envir)
    ## call setOldClass on some known old-style classes.  Ideally this would be done
    ## in the code that uses the classes, but that code doesn't know about the methods
    ## package.
    for(cl in .OldClassesList)
        setOldClass(cl, envir)
    ## some S3 classes have inheritance on an instance basis, that breaks the S4 contains
    ## model.  To emulate their (unfortunate) behavior requires a setIs with a test.
    for(cl in .OldIsList)
        .setOldIs(cl, envir)
    assign(".SealedClasses", c(clList,unique(unlist(.OldClassesList))),  envir)
    ## restore the true definition of the hidden functions
    assign("reconcilePropertiesAndPrototype", real.reconcileP, envir)
}

### The following methods are not currently installed.  (Tradeoff between intuition
### of users that new("matrix", ...) should be like matrix(...) vs
### consistency of new().  Relevant when new class has basic class as its data part.
###
### To install the methods below, uncomment the last line of the function
### .InitMethodDefinitions
.InitBasicClassMethods <- function(where) {
    ## methods to initialize "informal" classes by using the
    ## functions of the same name.
    ##
    ## These methods are designed to be inherited or extended
    setMethod("initialize", "matrix",
              function(object, data =   NA, nrow = 1, ncol = 1,
                       byrow = FALSE, dimnames = NULL, ...) {
                  if(nargs() < 2) # guaranteed to be called with object from new
                      object
                  else if(is.matrix(data) && nargs() == 2 + length(list(...)))
                      .mergeAttrs(data, object, list(...))
                  else {
                      value <- matrix(data, nrow, ncol, byrow, dimnames)
                      .mergeAttrs(value, object, list(...))
                  }
              })
    setMethod("initialize", "array",
              function(object, data =   NA, dim = length(data), dimnames = NULL, ...) {
                  if(nargs() < 2) # guaranteed to be called with object from new
                      object
                  else if(is.array(data) && nargs() == 2 + length(list(...)))
                      .mergeAttrs(data, object, list(...))
                  else {
                      value <- array(data, nrow, ncol, byrow, dimnames)
                      .mergeAttrs(value, object, list(...))
                  }
              })
    ## the "ts" method supports most of the arguments to ts()
    ## but NOT the class argument (!), and it won't work right
    ## if people set up "mts" objects with "ts" class (!, again)
    setMethod("initialize", "ts",
              function(object, data =   NA, start = 1, end = numeric(0), frequency = 1,
    deltat = 1, ts.eps = getOption("ts.eps"), names = NULL, ...) {
                  if(nargs() < 2) # guaranteed to be called with object from new
                      object
                  else if(is.ts(data) && nargs() == 2 + length(list(...)))
                      .mergeAttrs(data, object, list(...))
                  else {
                      value <- ts(data, start, end, frequency,
                                  deltat, ts.eps, names = names)
                      .mergeAttrs(value, object, list(...))
                  }
              })
}

## .OldClassList is a purely heuristic list of known old-style classes, with emphasis
## on old-style class inheritiance.  Used in .InitBasicClasses to call setOldClass for
## each known class pattern.
.OldClassesList <-
    list(
         c("anova", "data.frame"),
         c("mlm", "lm"),
         c("aov", "lm"), # see also .OldIsList
         c("maov", "mlm", "lm"),
         "POSIXt", "POSIXct", "POSIXlt", # see .OldIsList
         "dump.frames",
         c("ordered", "factor"),
         c("glm.null", "glm", "lm"),
         c("anova.glm.null", "anova.glm"),
         "hsearch",
         "integrate",
         "packageInfo",
         "libraryIQR",
         "packageIQR",
         "mtable",
         "table",
         "summary.table",
### can't do this while "ts" is treated like "matrix":         c("mts", "ts"),
### instead:
         "mts",
         "recordedplot",
         "socket",
         "packageIQR",
         "density",
         "formula",
         "logLik",
         "rle"
)

# These relations sometimes hold, sometimes not:  have to look in the S3
# class attribute to test.
.OldIsList <- list(
                   c("POSIXt", "POSIXct"),
                   c("POSIXt", "POSIXlt"),
                   c("aov","mlm")
                   )
