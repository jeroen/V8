test_that("backend argument", {
  expect_silent(v8(backend = "native"))
  ctx <- v8()
  ctx$assign("x", pi)
  expect_identical(ctx$get("x"), pi)
  expect_error(v8(backend = "arrow"), "should be one of")
  skip_if_not_installed("jsonlite")
  expect_silent(v8(backend = "jsonlite"))
})

test_that("R to JS matches jsonlite", {
  skip_if_not_installed("jsonlite")
  values <- list(
    1, 1:3, c(TRUE, NA), c("a", NA), NA, NULL, list(a = NULL, b = 1), list(1, "a"),
    setNames(list(1, 2), c("a", "")), list(), setNames(list(), character()), matrix(1:6, 2),
    I(1), factor("a"), factor(c("a", NA)), as.Date("2020-01-01"), 1 + 2i,
    list(a = list(b = list(c = 1))), c(a = 1, b = 2), matrix(c("a", "b"), 1), array(1:8, c(2, 2, 2)),
    mtcars, iris, data.frame(a = 1:2, b = I(list(1:2, "x"))), list(df = data.frame(x = 1:2), v = I("a")),
    as.POSIXct("2020-01-01 10:00:00", tz = "UTC"), "é", list(x = NA),
    data.frame(
      a = c(1, NA, 2.5),
      b = c(1L, NA, 3L),
      l = c(TRUE, NA, FALSE),
      s = c("x", NA, "y"),
      f = factor(c("u", NA, "v")),
      d = as.Date("2020-01-01") + 0:2,
      t = as.POSIXct("2020-01-01 10:00:00", tz = "UTC") + 0:2
    )
  )
  ctx_json <- v8(backend = "jsonlite")
  ctx_native <- v8(backend = "native")
  for (x in values) {
    ctx_json$assign("x", x)
    ctx_native$assign("x", x)
    expect_identical(ctx_native$eval("JSON.stringify(x)"), ctx_json$eval("JSON.stringify(x)"))
  }
})

test_that("JS to R matches jsonlite", {
  skip_if_not_installed("jsonlite")
  values <- c(
    "1", "1.5", "true", "'a'", "null", "[1,2,3]", "[1,null]", "[true,1]", "[1,'a']", "[true,'a']",
    "[1.5,'a',null]", "[null,null]", "[]", "({})", "[[1,2],[3,4]]", "[[1,2],[3]]", "[[1,'a'],[2,'b']]",
    "[{a:{b:1}},{a:{b:2}}]", "[{a:[1,2]},{a:[3]}]", "[1,[2,3]]", "[{a:1},2]", "({a:[1,null],b:{c:'x'}})",
    "['NA',1]", "[1,'NA']", "[{a:1},{}]", "[[]]", "[{}]", "[[true,false],[null,true]]",
    "[[[1,2],[3,4]],[[5,6],[7,8]]]", "[1,2147483648]", "[{a:null},{a:[1]}]", "[{a:1},null]", "[[1,2],null]",
    "({'1':2})", "[null]", "[[null,null]]", "[{a:true,b:'x'},{a:1}]", "new Date(0)", "[new Date(0)]",
    "({d: new Date(0)})", "[{_row:'r1',a:1},{_row:'r2',a:2}]", "({f: function(){}, a: 1})",
    "[function(){}, 1]", "[{a:{b:{c:1}}}]", "[{a:'x'},{a:'x'},{a:'y'}]", "[[{a:1}],[{a:2}]]"
  )
  ctx_json <- v8(backend = "jsonlite")
  ctx_native <- v8(backend = "native")
  for (x in values) {
    expect_identical(ctx_native$get(x), ctx_json$get(x), label = x)
  }
  ctx_json$assign("mtcars", mtcars)
  ctx_native$assign("mtcars", mtcars)
  expect_identical(ctx_native$get("mtcars"), ctx_json$get("mtcars"))
})

test_that("native keeps doubles and special values", {
  ctx <- v8(backend = "native")
  ctx$assign("x", data.frame(a = c(pi, NaN, Inf, -Inf, NA)))
  expect_identical(ctx$eval("x.map(r => String(r.a)).join()"), "3.141592653589793,NaN,Infinity,-Infinity,undefined")
  expect_identical(ctx$get("x")$a, c(pi, NaN, Inf, -Inf, NA))
  ctx$assign("y", c(pi, NA, NaN))
  expect_identical(ctx$eval("JSON.stringify(y.map(String))"), '["3.141592653589793","null","NaN"]')
  expect_identical(ctx$get("y"), c(pi, NA, NaN))
  expect_identical(ctx$get("Infinity"), Inf)
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
  expect_identical(ctx$get("new Uint8Array([1, 2, 3])"), as.raw(1:3))
  expect_identical(ctx$get("({x: new Uint8Array([1, 2])})"), list(x = as.raw(1:2)))
  expect_null(ctx$get("undefined"))
  expect_null(ctx$get("console.log"))
})

test_that("native round trips", {
  ctx <- v8(backend = "native")
  iris_chr <- iris
  iris_chr$Species <- as.character(iris_chr$Species)
  values <- list(
    list(mtcars, mtcars),
    list(iris, iris_chr),
    list(c(pi, exp(1)), c(pi, exp(1))),
    list(list(a = 1:3, b = list(c = "x", d = c(TRUE, FALSE))), list(a = 1:3, b = list(c = "x", d = c(TRUE, FALSE)))),
    list(matrix(c(0.5, 1.5, 2.5, 3.5, 4.5, 5.5), 2), matrix(c(0.5, 1.5, 2.5, 3.5, 4.5, 5.5), 2)),
    list(as.raw(0:255), as.raw(0:255)),
    list("\U0001F600", "\U0001F600"),
    list(c(a = "x", b = NA), c("x", NA))
  )
  for (x in values) {
    ctx$assign("x", x[[1]])
    expect_equal(ctx$get("x"), x[[2]])
  }
})

test_that("native call", {
  ctx <- v8(backend = "native")
  out <- ctx$call("function(x){return x.filter(r => r.disp > 400)}", mtcars)
  expect_identical(rownames(out), rownames(mtcars)[mtcars$disp > 400])
  expect_equal(ctx$call("Math.max", 1, 5), 5)
  expect_identical(ctx$call("function(x, y){return x.length + y.length}", iris, mtcars), 182L)
  expect_identical(ctx$call("function(x){return x.a + x.b}", list(a = 1, b = 2)), 3L)
  expect_identical(ctx$call("function(x){return x}", I(1)), 1L)
  expect_identical(ctx$eval("Object.keys(this).filter(k => k.startsWith('V8_arg_')).length"), "0")
})

test_that("native errors", {
  ctx <- v8(backend = "native")
  expect_error(ctx$assign("x", as.difftime(5, units = "mins")), "difftime")
  expect_error(ctx$assign("x", function() 1), "closure")
  expect_error(ctx$get("(function(){ const a = {}; a.a = a; return a; })()"), "circular")
})

test_that("toJSON options match jsonlite", {
  skip_if_not_installed("jsonlite")
  t <- as.POSIXct("2020-01-01 10:00:00", tz = "Europe/Berlin")
  df <- data.frame(a = c(1.5, NA), b = c("x", NA))
  cases <- list(
    list(df, list(na = "null")),
    list(df, list(na = "string")),
    list(c(1, NA, NaN, Inf), list(na = "null")),
    list(c(1, NA, NaN, Inf), list(na = "string")),
    list(c(TRUE, NA), list(na = "string")),
    list(factor(c("a", NA)), list(na = "string")),
    list(df, list(dataframe = "columns", na = "null")),
    list(df, list(dataframe = "values", na = "null")),
    list(mtcars[1:2, 1:2], list(dataframe = "columns")),
    list(mtcars[1:2, 1:2], list(dataframe = "values")),
    list(mtcars[1:2, 1:2], list(rownames = FALSE)),
    list(data.frame(a = 1:2), list(rownames = TRUE)),
    list(matrix(1:6, 2), list(matrix = "columnmajor")),
    list(array(1:8, c(2, 2, 2)), list(matrix = "columnmajor")),
    list(list(a = NULL), list(null = "null")),
    list(factor(c("a", "b")), list(factor = "integer")),
    list(as.Date("2020-01-02"), list(Date = "epoch")),
    list(data.frame(d = as.Date("2020-01-01")), list(Date = "epoch")),
    list(t, list(POSIXt = "ISO8601")),
    list(t, list(POSIXt = "ISO8601", UTC = TRUE)),
    list(t, list(POSIXt = "epoch")),
    list(t, list(POSIXt = "mongo")),
    list(t, list(UTC = TRUE)),
    list(t, list(time_format = "%H:%M")),
    list(as.raw(c(1, 2, 3, 255)), list(raw = "hex")),
    list(as.raw(1:3), list(raw = "int")),
    list(as.raw(1:5), list(raw = "base64")),
    list(list(r = as.raw(1:4)), list(raw = "mongo")),
    list(c(1 + 2i, 3 - 1i), list(complex = "list")),
    list(pi, list(digits = 2)),
    list(c(123.456, 0.001234), list(digits = I(2))),
    list(c(pi, 1e-10, 2.5), list(digits = 4)),
    list(list(x = structure("[1,2]", class = "json")), list(json_verbatim = TRUE)),
    list(list(a = 1:2, b = "x"), list(auto_unbox = FALSE))
  )
  ctx_json <- v8(backend = "jsonlite")
  ctx_native <- v8(backend = "native")
  for (cs in cases) {
    do.call(ctx_json$assign, c(list(name = "x", value = cs[[1]]), cs[[2]]))
    do.call(ctx_native$assign, c(list(name = "x", value = cs[[1]]), cs[[2]]))
    expect_identical(ctx_native$eval("JSON.stringify(x)"), ctx_json$eval("JSON.stringify(x)"),
                     label = deparse(cs[[2]]))
  }
  expect_error(ctx_native$assign("x", 1, foo = TRUE), "Unsupported option")
})

test_that("fromJSON options match jsonlite", {
  skip_if_not_installed("jsonlite")
  cases <- list(
    list("[1,2]", list(simplifyVector = FALSE)),
    list("[{a:1},{a:2}]", list(simplifyVector = FALSE)),
    list("[{a:1},{a:2}]", list(simplifyDataFrame = FALSE)),
    list("[[1,2],[3,4]]", list(simplifyMatrix = FALSE)),
    list("[{a:{b:1,c:2}},{a:{b:3,c:4}}]", list(flatten = TRUE)),
    list("({x: [{a:{b:1}}]})", list(flatten = TRUE)),
    list("({a:[1,2], b:{c:null}})", list(simplifyVector = FALSE)),
    list("[null, 1]", list(simplifyVector = FALSE)),
    list("[[{a:1}]]", list(simplifyDataFrame = FALSE))
  )
  ctx_json <- v8(backend = "jsonlite")
  ctx_native <- v8(backend = "native")
  for (cs in cases) {
    expect_identical(do.call(ctx_native$get, c(list(cs[[1]]), cs[[2]])),
                     do.call(ctx_json$get, c(list(cs[[1]]), cs[[2]])), label = cs[[1]])
  }
  expect_identical(ctx_native$call("function(){return [1, 2]}", simplify = FALSE), list(1L, 2L))
  expect_error(ctx_native$get("1", foo = TRUE), "Unsupported option")
})

test_that("native callbacks", {
  ctx <- v8(backend = "native")
  expect_identical(ctx$get("console.r.call('Sys.Date')"), as.character(Sys.Date()))
  expect_length(ctx$get("console.r.call('rnorm', {n: 2, mean: 10, sd: 5})"), 2)
  expect_identical(ctx$get("console.r.call('function(x){x^2}', {x: 12})"), 144L)
  ctx$assign("m", head(mtcars, 2))
  expect_identical(ctx$get("console.r.get('head(mtcars, 2)')"), ctx$get("m"))
  expect_identical(ctx$eval("JSON.stringify(console.r.get('list(a = 1:3, b = \"x\")'))"), '{"a":[1,2,3],"b":["x"]}')
  ctx$eval("console.r.assign('native_cb_test', [{a: 1, b: 'x'}, {a: 2}])")
  expect_identical(get("native_cb_test", envir = globalenv()), data.frame(a = 1:2, b = c("x", NA)))
  rm("native_cb_test", envir = globalenv())
  expect_null(ctx$get("console.r.eval('invisible(1)')"))
  expect_error(ctx$get("console.r.call('stop', ['boom'])"), "boom")
})

test_that("list columns", {
  ctx <- v8(backend = "native")
  df <- data.frame(id = 1:2)
  df$l <- list(1:2, "a")
  ctx$assign("df", df)
  expect_identical(ctx$eval("JSON.stringify(df)"), '[{"id":1,"l":[1,2]},{"id":2,"l":"a"}]')
  expect_identical(ctx$get("df")$l, list(1:2, "a"))
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

test_that("ArrayBufferView offset", {
  ctx <- v8()
  expect_identical(ctx$get("new Uint8Array([1, 2, 3, 4, 5]).subarray(1, 3)"), as.raw(2:3))
})

test_that("na option does not bind to name", {
  df <- data.frame(a = c(1.5, NA))
  ctx <- v8()
  ctx$assign("x", df, na = "null")
  expect_identical(ctx$eval("JSON.stringify(x)"), '[{"a":1.5},{"a":null}]')
  ctx$assign("y", df, FALSE, na = "string")
  expect_identical(ctx$eval("JSON.stringify(y)"), '[{"a":1.5},{"a":"NA"}]')
  skip_if_not_installed("jsonlite")
  ctx_json <- v8(backend = "jsonlite")
  ctx_json$assign("x", df, na = "null")
  expect_identical(ctx_json$eval("JSON.stringify(x)"), '[{"a":1.5},{"a":null}]')
})
