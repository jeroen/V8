context("JS ArrayBuffers")

test_that("Reading and writing raw buffers", {

  ctx <- V8::v8()
  ctx$eval('
var data = [1,2,3]
var intArray = new Uint8Array( data )
var dataBuffer = new ArrayBuffer( 3 )
var floatArray = new Float32Array( [0, Math.PI])')

  expect_equal(ctx$get('data'), 1:3)
  expect_equal(ctx$get('dataBuffer'), raw(3))
  if (.Platform$endian == "little") {
    expect_equal(ctx$get('floatArray'), as.raw(c(0, 0, 0, 0, 219, 15, 73, 64)))
  } else {
    expect_equal(ctx$get('floatArray'), as.raw(c(0, 0, 0, 0, 64, 73, 15, 219)))
  }
  expect_equal(ctx$get('intArray'), as.raw(1:3))

  # Print methods
  expect_equal(ctx$eval('dataBuffer'), "[object ArrayBuffer]")
  expect_equal(ctx$eval('floatArray.buffer'), "[object ArrayBuffer]")
  expect_equal(ctx$eval('intArray.buffer'), "[object ArrayBuffer]")

})

test_that("Roundtrip ArrayBuffers", {
  ctx <- V8::v8()
  ctx$assign('iris', serialize(iris, NULL))
  expect_equal(unserialize(ctx$get('iris')), iris)
})

test_that("Large ArrayBuffer", {
  ctx <- V8::v8()
  x <- serialize(rnorm(1e6), NULL)
  ctx$assign('x', x)
  y <- ctx$get('x')
  expect_identical(x, y)
  rm(x); gc()
  expect_identical(ctx$get('x'), y)
})

test_that("ArrayBuffers from eval", {
  # Create a context and assign some data
  ctx <- v8()
  ctx$eval("bytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01,
    0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07,
    0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a, 0x09, 0x01,
    0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b
  ]);
  buffer = bytes.buffer")

  # Expected output
  bin <- as.raw(c(0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01,
                  0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01,
                  0x00, 0x07, 0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a,
                  0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b))
  ctx$assign('bin', bin)
  ctx$assign('mtcars', mtcars)

  # With serialize, no eval
  expect_equal(ctx$get('bytes'), bin)
  expect_equal(ctx$get('buffer'), bin)
  expect_equal(ctx$get('bin'), bin)
  expect_equal(ctx$get('mtcars'), mtcars)

  # With serialize and eval
  expect_equal(ctx$get('(function(x){return bytes})()'), bin)
  expect_equal(ctx$get('(function(x){return buffer})()'), bin)
  expect_equal(ctx$get('(function(x){return bin})()'), bin)
  expect_equal(ctx$get('(function(x){return mtcars})()'), mtcars)
  expect_null(ctx$get('console.log'))

  # Using call (serialize and eval)
  ctx$eval('function identity(x){return x;}')
  expect_equal(ctx$call('identity', JS('bytes')), bin)
  expect_equal(ctx$call('identity', JS('buffer')), bin)
  expect_equal(ctx$call('identity', JS('bin')), bin)
  expect_equal(ctx$call('identity', JS('mtcars')), mtcars)
  expect_null(ctx$call('identity', JS('console.log')))

  # Without serialize
  #expect_match(ctx$eval('bytes'), "^(\\d,?)+$")
  expect_match(ctx$eval('buffer'), "[object ArrayBuffer]", fixed = TRUE)
  #expect_match(ctx$eval('bin'), "[object ArrayBuffer]", fixed = TRUE)
  expect_match(ctx$eval('mtcars'), "[object Object]", fixed = TRUE)
  expect_match(ctx$eval('console.log'), 'function')
})
