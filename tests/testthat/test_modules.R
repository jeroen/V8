
writeLines(
  text = "
// mathFuns.js

// Exported functions
export function add(a, b) {
  return a + b;
}

export function subtract(a, b) {
  return a - b;
}",
"mathFuns.js"
)

writeLines(
  text = "
// mathCons.js

// You can also export constants
export const PI = 3.14159;",
  "mathCons.js"
)

writeLines(
  text = "
// main.js

// Import the module
export { add, subtract } from './mathFuns.js';
export { PI } from './mathCons.js';",
  "main.js"
)

test_that("test modules", {
  ctx <- V8::v8()
  
  ctx$eval('var x = add(5, 3)', src_module = "main.js")
  expect_true(8 == ctx$eval('x'))
  
  ctx$eval('var x = subtract(10, 4)', src_module = "main.js")
  expect_true(6 == ctx$eval('x'))
  
  ctx$eval('var x = PI', src_module = "main.js")
  expect_true(round(pi, 5) == ctx$eval('x'))
  
  ctx <- V8::v8()
  ctx$eval('', src_module = "main.js")
  expect_true(round(pi, 5) == ctx$eval('PI'))
})

unlink("main.js")
unlink("mathCons.js")
unlink("mathFuns.js")

writeLines(
  text = "
const text = await Promise.resolve('Hey there');
console.log('outside: ' + text)
", 
  "runme.mjs"
)

test_that("await works", {
  ctx <- V8::v8()
  expect_equal(capture_output(ctx$eval('', src_module = "runme.mjs")), "outside: Hey there")
  expect_error(ctx$source("runme.mjs"), "await is only valid in async functions and the top level bodies of modules")
})

unlink("runme.mjs")
