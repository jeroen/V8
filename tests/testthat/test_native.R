test_that("backend argument", {
  expect_silent(v8(backend = "jsonlite"))
  expect_silent(v8(backend = "native"))
  expect_error(v8(backend = "arrow"), "should be one of")
})

test_that("native assign matches jsonlite", {
  df <- data.frame(
    a = c(1, NA, 2.5),
    b = c(1L, NA, 3L),
    l = c(TRUE, NA, FALSE),
    s = c("x", NA, "y"),
    f = factor(c("u", NA, "v")),
    d = as.Date("2020-01-01") + 0:2,
    t = as.POSIXct("2020-01-01 10:00:00", tz = "UTC") + 0:2
  )
  ctx_json <- v8()
  ctx_native <- v8(backend = "native")
  ctx_json$assign("df", df)
  ctx_native$assign("df", df)
  expect_identical(ctx_native$eval("JSON.stringify(df)"), ctx_json$eval("JSON.stringify(df)"))

  ctx_json$assign("mtcars", mtcars)
  ctx_native$assign("mtcars", mtcars)
  expect_identical(ctx_native$get("mtcars"), ctx_json$get("mtcars"))
})

test_that("native keeps doubles and special values", {
  ctx <- v8(backend = "native")
  ctx$assign("x", data.frame(a = c(pi, NaN, Inf, -Inf, NA)))
  expect_identical(ctx$eval("x.map(r => String(r.a)).join()"), "3.141592653589793,NaN,Infinity,-Infinity,undefined")
  expect_identical(ctx$get("x")$a, c(pi, NaN, Inf, -Inf, NA))
})

test_that("native get", {
  ctx <- v8(backend = "native")
  out <- ctx$get('[{"a": 1, "b": "x", "c": true}, {"a": 2.5, "d": null}, {}]')
  expect_identical(out, data.frame(
    a = c(1, 2.5, NA),
    b = c("x", NA, NA),
    c = c(TRUE, NA, NA),
    d = c(NA, NA, NA)
  ))
  expect_identical(ctx$get('[{"a": 1}, {"a": 2}]')$a, 1:2)
  expect_identical(ctx$get('[{"a": 2147483648}]')$a, 2147483648)
  expect_identical(ctx$get('[{"_row": "r1", "a": 1}]'), data.frame(a = 1L, row.names = "r1"))
})

test_that("native get falls back to json", {
  ctx <- v8(backend = "native")
  expect_identical(ctx$get("[1, 2, 3]"), 1:3)
  expect_identical(ctx$get("({a: 1})"), list(a = 1L))
  expect_identical(ctx$get("[]"), list())
  expect_identical(ctx$get('[{"a": 1}, {"a": "x"}]'), data.frame(a = c("1", "x")))
  expect_identical(ctx$get('[{"a": [1, 2]}]'), v8()$get('[{"a": [1, 2]}]'))
  expect_identical(ctx$get("[new Date(0)]"), "1970-01-01T00:00:00.000Z")
  expect_identical(ctx$get('[{"a": 1}]', simplifyDataFrame = FALSE), list(list(a = 1L)))
})

test_that("native call", {
  ctx <- v8(backend = "native")
  out <- ctx$call("function(x){return x.filter(r => r.disp > 400)}", mtcars)
  expect_identical(rownames(out), rownames(mtcars)[mtcars$disp > 400])
  expect_equal(ctx$call("Math.max", 1, 5), 5)
  expect_identical(ctx$call("function(x, y){return x.length + y.length}", iris, mtcars), 182L)
  expect_identical(ctx$eval("Object.keys(this).filter(k => k.startsWith('V8_arg_')).length"), "0")
})

test_that("native unsupported columns use json", {
  ctx <- v8(backend = "native")
  df <- data.frame(id = 1:2)
  df$l <- list(1:2, "a")
  ctx$assign("df", df)
  ctx_json <- v8()
  ctx_json$assign("df", df)
  expect_identical(ctx$eval("JSON.stringify(df)"), ctx_json$eval("JSON.stringify(df)"))
})

test_that("utf8 strings", {
  ctx <- v8(backend = "native")
  latin <- iconv("café", "UTF-8", "latin1")
  df <- data.frame(s = c("été", latin, NA, "\U0001F600"))
  df$f <- factor(df$s)
  ctx$assign("df", df)
  out <- ctx$get("df")
  expect_identical(out$s, enc2utf8(df$s))
  expect_identical(out$f, enc2utf8(as.character(df$f)))
})

test_that("ArrayBufferView offset", {
  ctx <- v8()
  expect_identical(ctx$get("new Uint8Array([1, 2, 3, 4, 5]).subarray(1, 3)"), as.raw(2:3))
})

test_that("special column names", {
  df <- data.frame(a = c(1.5, NA), b = c("x", "x"))
  names(df) <- c("__proto__", "it's \"quoted\"\n")
  ctx <- v8(backend = "native")
  ctx$assign("df", df)
  expect_identical(ctx$eval("Object.keys(df[0]).length"), "2")
  expect_identical(ctx$eval("Object.getPrototypeOf(df[0]) === Object.prototype"), "true")
  expect_identical(ctx$get("df"), df)
  ctx$assign("df2", df[1, ])
  expect_identical(ctx$eval("Object.keys(df2[0]).length"), "2")
})
