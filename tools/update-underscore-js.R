# set the underscore.js version number
version <- "1.13.6"

# download the raw underscore-min.js, replaces existing file in inst/js
download_version <- function(version) {
  download.file(
    file.path("https://raw.githubusercontent.com/jashkenas/underscore", version, "underscore-min.js"),
    file.path("inst/js/underscore.js")
  )
}

download_version(version)
