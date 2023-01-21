.onAttach <- function(libname, pkg){
  ver <- version()
  packageStartupMessage(paste("Using V8 engine", ver))
  if(isTRUE(v8_version_numeric() < 6)){
    warning("This system has a very old version of libv8. Some packages may not work.", call. = FALSE)
  }
}
