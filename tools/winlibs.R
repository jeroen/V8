# Build against static libraries from curl website.
if(!file.exists("../windows/v8-3.14.5/include/v8.h")){
  setInternet2()
  download.file("http://jeroenooms.github.io/V8/v8-3.14.5.zip", "lib.zip", quiet = TRUE)
  dir.create("../windows", showWarnings = FALSE)
  unzip("lib.zip", exdir = "../windows")
  unlink("lib.zip")
}
