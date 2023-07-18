context("Error message")

# This test fails with the CRAN custom clang toolchains.
# The test is automatically removed by the autobrew script
test_that("SyntaxError from V8", {
  ctx <- V8::v8()
  expect_error(ctx$eval('var foo = }bla}'), 'SyntaxError', class = "std::invalid_argument")
})
