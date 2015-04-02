## ----, echo = FALSE, message = FALSE-------------------------------------
knitr::opts_chunk$set(comment = "")
library(V8)

## ------------------------------------------------------------------------
# Create a new context
ct <- new_context();

# Evaluate some code
ct$eval("var foo = 123")
ct$eval("var bar = 456")
ct$eval("foo + bar")

## ------------------------------------------------------------------------
# Create some JSON
cat(ct$eval("JSON.stringify({x:Math.random()})"))

# Simple closure
ct$eval("(function(x){return x+1;})(123)")

## ----, eval=FALSE--------------------------------------------------------
#  ct$source(system.file("js/underscore.js", package="V8"))
#  ct$source("https://cdnjs.cloudflare.com/ajax/libs/crossfilter/1.3.11/crossfilter.min.js")

## ----echo=FALSE, results='hide'------------------------------------------
ct$source(system.file("js/underscore.js", package="V8"))
ct$source(system.file("js/crossfilter.js", package="V8"))

## ------------------------------------------------------------------------
ct$assign("mydata", mtcars)
ct$get("mydata")

## ------------------------------------------------------------------------
ct$assign("foo", JS("function(x){return x*x}"))
ct$assign("bar", JS("foo(9)"))
ct$get("bar")

## ------------------------------------------------------------------------
ct$call("_.filter", mtcars, JS("function(x){return x.mpg < 15}"))

## ----, eval=FALSE--------------------------------------------------------
#  # Load some data
#  data(diamonds, package = "ggplot2")
#  ct$assign("diamonds", diamonds)
#  ct$console()

## ----, eval=FALSE--------------------------------------------------------
#  output <- ct$get("output")
#  print(output)

## ----, eval=FALSE--------------------------------------------------------
#  ct <- new_context()
#  ct$source("https://cdnjs.cloudflare.com/ajax/libs/crossfilter/1.3.11/crossfilter.min.js")
#  ct$eval('var cf = crossfilter || console.error("failed to load crossfilter!")')

## ------------------------------------------------------------------------
ct <- new_context(typed_arrays = FALSE);
ct$get(JS("Object.keys(global)"))

## ------------------------------------------------------------------------
ct <- new_context(typed_arrays = TRUE);
ct$get(JS("Object.keys(global)"))

## ------------------------------------------------------------------------
ct2 <- new_context(global = NULL, console = FALSE)
ct2$get(JS("Object.keys(this).length"))
ct2$assign("cars", cars)
ct2$eval("var foo = 123")
ct2$eval("function test(x){x+1}")
ct2$get(JS("Object.keys(this).length"))
ct2$get(JS("Object.keys(this)"))

## ------------------------------------------------------------------------
ct2$eval("var __global__ = this")
ct2$eval("(function(){var bar = [1,2,3,4]; __global__.bar = bar; })()")
ct2$get("bar")

## ------------------------------------------------------------------------
ct$validate("function foo(x){2*x}")
ct$validate("foo = function(x){2*x}")

## ------------------------------------------------------------------------
ct$validate("function(x){2*x}")

## ------------------------------------------------------------------------
ct$validate("(function(x){2*x})")
ct$validate("!function(x){2*x}")

