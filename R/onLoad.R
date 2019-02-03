.onLoad <- function(libname, pkgname){
  if(is.function(start_v8_isolate))
    start_v8_isolate()
  # Test for development
  # ct <- v8()
  # ct$source(system.file("js/underscore.js", package = pkgname))
  # rm(ct)
}
