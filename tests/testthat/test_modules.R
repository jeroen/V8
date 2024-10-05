context("ESM modules")

test_that("get modules", {
  ctx <- V8::v8()
  ctx$source('modules/main.js')
  out <- ctx$eval('run_test()', await = TRUE)
  expect_equal(out, "579")
})
