context("WASM")

test_that("Load WASM program", {
  skip_if_not(engine_info()$version > 6, message = "Your libv8 is too old for WASM")
  instance <- wasm(system.file('wasm/add.wasm', package = 'V8'))
  expect_equal(names(instance$exports), 'add')
  expect_equal(instance$exports$add(12, 30), 42)
})
