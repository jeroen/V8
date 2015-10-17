# Build against static libraries.
if(!file.exists("../windows/libv8-3.15/include/v8.h")){
  if(getRversion() < "3.3.0") setInternet2()
  exception_output <- paste(readLines("exceptions.txt"), collapse = "\n")
  is_sjlj <- grepl("personality_sj", exception_output, fixed = TRUE)
  if(isTRUE(is_sjlj)){
    download.file("https://github.com/rwinlib/libv8/archive/v3.15-sjlj.zip", "lib.zip", quiet = TRUE)
  } else {
    download.file("https://github.com/rwinlib/libv8/archive/v3.15.zip", "lib.zip", quiet = TRUE)
  }
  dir.create("../windows", showWarnings = FALSE)
  unzip("lib.zip", exdir = "../windows")
  unlink("lib.zip")
  if(isTRUE(is_sjlj)){
    setwd("../windows/")
    file.rename("libv8-3.15-sjlj", "libv8-3.15")
  }
}
