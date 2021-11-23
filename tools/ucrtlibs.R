# Build against UCRT build of libv8
if(!file.exists('../windows/ucrt64/include/v8.h')){
  download.file('https://github.com/jeroen/V8/releases/download/v3.6.0/v8-9.1.269.38-win-ucrt.pkg.tar.xz', "ucrt64.tar.xz", quiet = TRUE)
  dir.create("../windows", showWarnings = FALSE)
  untar('ucrt64.tar.xz', exdir = "../windows")
  unlink('ucrt64.tar.xz')
}
