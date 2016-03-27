# Build against mingw-w64 build of libv8 3.15.10
if(!file.exists("../windows/libv8-3.15/include/v8.h")){
  if(getRversion() < "3.3.0") setInternet2()
  download.file("https://github.com/rwinlib/libv8/archive/v3.15.10.zip", "lib.zip", quiet = TRUE)
  dir.create("../windows", showWarnings = FALSE)
  unzip("lib.zip", exdir = "../windows")
  unlink("lib.zip")
}
