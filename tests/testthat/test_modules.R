context("ESM modules")

test_that("get modules", {
  skip_if(V8::engine_info()$numeric_version < "6.3")
  ctx <- V8::v8()
  ctx$source('modules/main.js')
  out <- ctx$eval('run_test()', await = TRUE)
  expect_error(ctx$eval('test_bad_path()', await = TRUE), "path")
  expect_equal(out, "579")
  expect_error(ctx$eval('test_syntax_error1()', await = TRUE), "SyntaxError")

})
