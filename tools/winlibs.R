# Build against static libraries from curl website.
if(!file.exists("../windows/libv8-3.14.5.10/include/v8.h")){
  setInternet2()
  download.file("https://github.com/rwinlib/libv8/archive/v3.14.5.10.zip", "lib.zip", quiet = TRUE)
  dir.create("../windows", showWarnings = FALSE)
  unzip("lib.zip", exdir = "../windows")
  unlink("lib.zip")
}
