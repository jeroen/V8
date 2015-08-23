# Build against static libraries from curl website.
if(!file.exists("../windows/libv8-3.14.5.10/include/v8.h")){
  if(getRversion() < "3.3.0") setInternet2()
  download.file("https://github.com/rwinlib/libv8/archive/v3.14.5.10.zip", "lib.zip", quiet = TRUE)
  dir.create("../windows", showWarnings = FALSE)
  unzip("lib.zip", exdir = "../windows")
  unlink("lib.zip")
}

# libv8 is very sensitive to the compiler version
if(file.exists("gcc_version.txt") && file.exists("../windows/libv8-3.14.5.10/lib/i386-old")){
  gcc_version <- readLines("gcc_version.txt")
  is_old_gcc <- grepl("4.6.3", gcc_version[1], fixed = TRUE)
  if(is_old_gcc) {
    message("Found GCC 4.6.3 compiler. Using legacy libraries.")
    file.rename("../windows/libv8-3.14.5.10/lib/i386", "../windows/libv8-3.14.5.10/lib/i386-new")
    file.rename("../windows/libv8-3.14.5.10/lib/i386-old", "../windows/libv8-3.14.5.10/lib/i386")
  }
}
