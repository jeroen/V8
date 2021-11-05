.onAttach <- function(libname, pkg){
  ver <- version()
  packageStartupMessage(paste("Using V8 engine", ver))
  if(ver < 6){
    warning("This system has a very old version of libv8. Some packages may not work.", call. = FALSE)
  }
}

.onLoad <- function(libname, pkgname){
  # Test for development
  # ct <- v8()
  # ct$source(system.file("js/underscore.js", package = pkgname))
  # rm(ct)
}
