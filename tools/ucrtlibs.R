# Build against UCRT build of libv8
if(!file.exists('../windows/ucrt64/include/v8.h')){
  download.file('https://github.com/r-windows/rtools-packages/releases/download/v8-9.0.257.17/ucrt64.tar.xz', "ucrt64.tar.xz", quiet = TRUE)
  dir.create("../windows", showWarnings = FALSE)
  untar('ucrt64.tar.xz', exdir = "../windows")
  unlink('ucrt64.tar.xz')
}
