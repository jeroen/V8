.onAttach <- function(libname, pkg){
  packageStartupMessage(paste("Using V8 engine", version()))
}

.onLoad <- function(libname, pkgname){
  # Test for development
  # ct <- v8()
  # ct$source(system.file("js/underscore.js", package = pkgname))
  # rm(ct)
}
