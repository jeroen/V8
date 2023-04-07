# Build against mingw-w64 build of libv8
VERSION <- commandArgs(TRUE)
LIBDIR <- paste0("../windows/libv8-", VERSION)
if(!file.exists(paste0(LIBDIR, "/include/v8.h"))){
  if(getRversion() < "3.3.0") stop("This library requires R 3.3 or newer")
  download.file(sprintf("https://github.com/rwinlib/libv8/archive/v%s.zip", VERSION), "v8.zip", quiet = TRUE)
  dir.create("../windows", showWarnings = FALSE)
  unzip("v8.zip", exdir = "../windows")
  setwd(LIBDIR)
  unzip("lib-4.9.3.zip")
  if(getRversion() >= "3.6.0") unzip("lib.zip")
  unlink("v8.zip")
}
