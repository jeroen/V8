.onLoad <- function(libname, pkgname){
  underscore <- system.file("js/underscore.js", package = pkgname)
  jseval(paste(readLines(underscore, warn = FALSE), collapse = "\n"))
}
