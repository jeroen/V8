.onLoad <- function(libname, pkgname){
  # Test for development
  test <- new_context()
  test$source(system.file("js/underscore.js", package = pkgname))
  rm(test)
}
