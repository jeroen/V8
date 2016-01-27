# Internal function used for the JavaScript console.r API
#
# Provides: console.r.call("rnorm", {n:10})
r_call <- function(strfun, args = '{}'){
  FUN <- eval(parse(text=strfun))
  ARGS <- as.list(jsonlite::fromJSON(args))
  if(!is.function(FUN))
    stop("Argument is not a valid function")
  out <- do.call(FUN, ARGS)
  jsonlite::toJSON(out)
}

# Provides: console.r.get("iris")
r_get <- function(str, args = '{}'){
  x <- eval(parse(text = str))
  ARGS <- as.list(jsonlite::fromJSON(args))
  do.call(jsonlite::toJSON, c(list(x = x), ARGS))
}

# Provides: console.r.eval("rnorm(10)")
r_eval <- function(str, args = '{"print.eval":true}'){
  con <- textConnection(str)
  ARGS <- as.list(jsonlite::fromJSON(args))
  out <- do.call(source, c(list(file = con), ARGS))
  #tryCatch(toJSON(out$value), error = 'null')
  return('null')
}
