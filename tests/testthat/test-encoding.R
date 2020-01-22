test_that("Encoding is preserved", {
  ctx <- v8()
  ctx$eval("function I(x){return x;}")
  str_native <- enc2native("\u00C0\u00CB\u00D0")
  str_utf <- enc2utf8("\u00C0\u00CB\u00D0")
  expect_equal(ctx$call('I', str_native), str_utf)
  expect_equal(ctx$call('I', str_utf), str_utf)
})
