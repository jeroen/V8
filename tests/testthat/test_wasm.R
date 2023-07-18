context("WASM")

test_that("Load WASM program", {
  instance <- wasm(system.file('wasm/add.wasm', package = 'V8'))
  expect_equal(names(instance$exports), 'add')
  expect_equal(instance$exports$add(12, 30), 42)
})
