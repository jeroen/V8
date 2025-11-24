
# requires curl

encoding_ver <- "0.7.0"
arrow_ver    <- "21.1.0"


#### jsdiff ####
dir_js_enc <- "inst/js/encoding"
if (dir.exists(dir_js_enc)) unlink(dir_js_enc, recursive = TRUE)
dir.create(dir_js_enc)

dir_js_arr <- "inst/js/apache_arrow"
if (dir.exists(dir_js_arr)) unlink(dir_js_arr, recursive = TRUE)
dir.create(dir_js_arr)

encoding_js <- sprintf("https://unpkg.com/text-encoding@%s/lib/encoding.js", encoding_ver)
curl::curl_download(encoding_js, paste0(dir_js_enc, "/encoding.js"))

arrow_js <- sprintf("https://unpkg.com/apache-arrow@%s/Arrow.es2015.min.js", arrow_ver)
curl::curl_download(arrow_js, paste0(dir_js_arr, "/diff.min.js"))

lic_js_enc <- sprintf("https://unpkg.com/text-encoding@%s/LICENSE.md", encoding_ver)
curl::curl_download(lic_js_enc, paste0(dir_js_enc, "/LICENSE.md"))

arr_js_enc <- sprintf("https://unpkg.com/apache-arrow@%s/LICENSE.txt", arrow_ver)
curl::curl_download(arr_js_enc, paste0(dir_js_arr, "/LICENSE.txt"))
