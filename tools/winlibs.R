if(!file.exists('../windows/libv8/include/v8.h')){
  unlink("../windows", recursive = TRUE)
  url <- if(grepl("aarch", R.version$platform)){
    "https://github.com/r-windows/bundles/releases/download/v8-11.9.169.6/v8-11.9.169.6-clang-aarch64.tar.xz"
  } else if(grepl("clang", Sys.getenv('R_COMPILED_BY'))){
    "https://github.com/r-windows/bundles/releases/download/v8-11.9.169.6/v8-11.9.169.6-clang-x86_64.tar.xz"
  } else if(getRversion() >= "4.3") {
    "https://github.com/r-windows/bundles/releases/download/v8-11.9.169.6/v8-11.9.169.6-ucrt-x86_64.tar.xz"
  } else if(getRversion() >= "4.2") {
    "https://github.com/r-windows/bundles/releases/download/v8-11.8.172.13/v8-9.1.269.38-win-ucrt.tar.xz"
  } else {
    "https://github.com/r-windows/bundles/releases/download/v8-11.8.172.13/v8-9.1.269.38-win-msvcrt.tar.xz"
  }
  download.file(url, basename(url), quiet = TRUE)
  dir.create("../windows", showWarnings = FALSE)
  untar(basename(url), exdir = "../windows", tar = 'internal')
  unlink(basename(url))
  setwd("../windows")
  file.rename(list.files(), 'libv8')
}
