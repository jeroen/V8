context("WASM")

test_that("Load WASM program", {
  skip_if_not(engine_info()$version > 6)
  instance <- wasm(system.file('wasm/add.wasm', package = 'V8'))
  expect_equal(names(instance$exports), 'add')
  expect_equal(instance$exports$add(12, 30), 42)
})
