test_that("default backend is jsonlite", {

  expect_silent(ctx <- V8::v8())
  expect_silent(ctx <- V8::v8(backend = "jsonlite"))
  expect_error(ctx <- V8::v8(backend = "sqlite"), "should be one of")

  skip_if_not_installed("arrow")
  expect_silent(ctx <- V8::v8(backend = "arrow"))

})

test_that("backend arrow works", {

  skip_if_not_installed("arrow")
  ctx <- V8::v8(backend = "arrow")

  ctx$assign("mtcars", mtcars)
  got <- ctx$get("mtcars")

  expect_true(inherits(got, "tbl_df"))
  expect_equal(dim(got), c(32, 11))

  skip_if_offline()
  ctx$source("https://unpkg.com/underscore@1.13.7/underscore-min.js")
  ctx$call("_.filter", mtcars, V8::JS("function(x){return x.disp > 200}"))

})
