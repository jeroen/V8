context("Error message")

test_that("SyntaxError from V8", {
  ctx <- V8::v8()
  expect_error(ctx$eval('var foo = {bla}'), 'SyntaxError')
})
