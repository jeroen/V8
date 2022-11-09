test_that("underscore.js can be loaded", {
  ctx <- v8()
  expect_silent(ctx$source(system.file("js/underscore.js", package = "V8")))
})

test_that("the current version of underscore.js is loaded", {
  ctx <- v8()
  ctx$source(system.file("js/underscore.js", package = "V8"))
  expect_equal("1.13.6", ctx$call("function(x) { return _.VERSION }"))
})

test_that("underscore.js filter works", {
  ctx <- v8()
  ctx$source(system.file("js/underscore.js", package="V8"))
  expect_equal(
    rownames(ctx$call("_.filter", mtcars, JS("function(x) { return x.mpg === 14.3 }"))),
    "Duster 360"
  )
})


