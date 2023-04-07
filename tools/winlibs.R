# Download a suitable libv8 build
if(grepl('Windows Server 2008', osVersion)){
  stop("Windows Vista/2008 is no longer supported")
}
if(!file.exists('../windows/ucrt64/include/v8.h')){
  cdn <- ifelse(isTRUE(grepl('ucrt', R.version$crt)),
           'https://github.com/jeroen/V8/releases/download/v3.6.0/v8-9.1.269.38-win-ucrt.pkg.tar.xz',
           'https://github.com/jeroen/V8/releases/download/v3.6.0/v8-9.1.269.38-win-msvcrt.tar.xz')
  download.file(cdn, "libv8.tar.xz", quiet = TRUE)
  dir.create("../windows", showWarnings = FALSE)
  untar('libv8.tar.xz', exdir = "../windows", tar = 'internal')
  unlink('libv8.tar.xz')
  if(isTRUE(grepl('ucrt', R.version$crt))){
    file.rename("../windows/ucrt64", "../windows/mingw64")
  }
}
