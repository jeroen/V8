## ---- echo = FALSE, message = FALSE--------------------------------------
knitr::opts_chunk$set(comment = "")
library(V8)

## ------------------------------------------------------------------------
ct <- v8()
ct$source(system.file("js/underscore.js", package="V8"))
ct$call("_.filter", mtcars, JS("function(x){return x.mpg < 15}"))

## ----eval=FALSE----------------------------------------------------------
#  ct <- v8()
#  ct$source("~/Desktop/bundle.js")

## ----echo=FALSE, results='hide'------------------------------------------
ct <- v8()
ct$source("beautify.js")

## ------------------------------------------------------------------------
ct$get(JS('Object.keys(global)'))

## ------------------------------------------------------------------------
test <- "(function(x,y){x = x || 1; y = y || 1; return y * x;})(4, 9)"
pretty_test <- ct$call("beautify.js_beautify", test, list(indent_size = 2))
cat(pretty_test)

## ------------------------------------------------------------------------
html <- "<ul><li>one</li><li>two</li><li>three</li></ul>"
cat(ct$call("beautify.html_beautify", html))

