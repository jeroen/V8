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
  expect_equal(ctx$get('floatArray'), as.raw(c(0, 0, 0, 0, 219, 15, 73, 64)))
  expect_equal(ctx$get('intArray'), as.raw(1:3))
})

test_that("Roundtrip ArrayBuffers", {
  ctx <- V8::v8()
  ctx$assign('iris', serialize(iris, NULL))
  expect_equal(unserialize(ctx$get('iris')), iris)
})
