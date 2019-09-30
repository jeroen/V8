context("ArrayBuffers")

test_that("ArrayBuffers", {
  # Create a context and assign some data
  ctx <- v8()
  ctx$eval("bytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01,
    0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07,
    0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a, 0x09, 0x01,
    0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b
  ]);
  buffer = bytes.buffer")
  ctx$assign('mtcars', mtcars)

  # Expected output
  bin <- as.raw(c(0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01,
           0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01,
           0x00, 0x07, 0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a,
           0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b))

  # With serialize, no eval
  expect_equal(ctx$read('bytes'), bin)
  expect_equal(ctx$read('buffer'), bin)
  expect_equal(ctx$read('mtcars'), mtcars)
  expect_error(ctx$read('console.log'))

  # With serialize and eval
  expect_equal(ctx$get('(function(x){return bytes})()'), bin)
  expect_equal(ctx$get('(function(x){return buffer})()'), bin)
  expect_equal(ctx$get('(function(x){return mtcars})()'), mtcars)
  expect_null(ctx$get('console.log'))

  # Using call (serialize and eval)
  ctx$eval('function identity(x){return x;}')
  expect_equal(ctx$call('identity', JS('bytes')), bin)
  expect_equal(ctx$call('identity', JS('buffer')), bin)
  expect_equal(ctx$call('identity', JS('mtcars')), mtcars)
  expect_null(ctx$call('identity', JS('console.log')))

  # Without serialize
  expect_match(ctx$eval('bytes'), "^(\\d,?)+$")
  expect_match(ctx$eval('buffer'), "[object ArrayBuffer]", fixed = TRUE)
  expect_match(ctx$eval('mtcars'), "[object Object]", fixed = TRUE)
  expect_match(ctx$eval('console.log'), 'function')
})

