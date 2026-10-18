context("V8 Callback to R")

ctx <- V8::v8()

test_that("console.r.get", {
  ctx$eval('//get a data frame
    var iris = console.r.get("iris")
    var iris_col = console.r.get("iris", {dataframe:"col"})
  ')
  expect_equal(ctx$get("iris.length"), nrow(iris))
  expect_equal(ctx$get("Object.keys(iris_col)"), names(iris))
})

test_that("console.r.assign", {
  ctx$eval('//assign to R session
    console.r.assign("iris2", iris)
    console.r.assign("iris3", iris, {simplifyVector:false})
  ')
  expect_is(iris2, "data.frame")
  expect_equal(length(iris2), 5)
  expect_equal(length(iris3), 150)
  rm(iris2, iris3, envir = globalenv())
})

test_that("console.r.call", {
  expect_equal(ctx$get("console.r.call('Sys.Date')"), as.character(Sys.Date()))
  expect_equal(length(ctx$get("console.r.call('rnorm', {n: 2,mean:10, sd:5})")), 2)
  expect_equal(length(ctx$get("console.r.call('rnorm', 3)")), 3)
  expect_equal(ctx$get("console.r.call('function(x){x^2}', {x:12})"), 144)
  expect_error(ctx$get("console.r.call('rnorm')"), "missing", class = "std::runtime_error")
})

test_that("console.r.eval", {
  expect_is(ctx$eval("console.r.eval('invisible(sessionInfo())')"), "character")
  expect_error(ctx$eval("console.r.eval('doesnotexists')"), "not found", class = "std::runtime_error")

  # setTimeLimit seems broken in R
  # expect_error(ctx$eval('console.r.eval("setTimeLimit(elapsed = 0.001); Sys.sleep(5)")'), 'elapsed')
})
