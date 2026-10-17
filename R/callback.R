# Internal function used for the JavaScript console.r API
#
# Provides: console.r.call("rnorm", {n:10})
r_call <- function(strfun, args = '{}'){
  no_jumps({
    FUN <- eval(parse(text=strfun))
    ARGS <- as.list(jsonlite::fromJSON(args))
    if(!is.function(FUN))
      stop("Argument is not a valid function")
    out <- do.call(FUN, ARGS)
    jsonlite::toJSON(out)
  })
}

# Provides: console.r.get("iris")
r_get <- function(str, args = '{}'){
  no_jumps({
    x <- eval(parse(text = str))
    ARGS <- as.list(jsonlite::fromJSON(args))
    do.call(jsonlite::toJSON, c(list(x = x), ARGS))
  })
}

# Provides: console.r.eval("rnorm(10)")
r_eval <- function(str, args = '{"print.eval":true}'){
  no_jumps({
    con <- textConnection(str)
    ARGS <- as.list(jsonlite::fromJSON(args))
    do.call(source, c(list(file = con), ARGS))
    #tryCatch(toJSON(out$value), error = 'null')
    return('null')
  })
}

# Provides: console.r.assign("test", [1,2,3])
r_assign <- function(name, value, args = '{}'){
  no_jumps({
    ARGS <- as.list(jsonlite::fromJSON(args))
    VAL <- do.call(jsonlite::fromJSON, c(list(txt = value), ARGS))
    asgn <- get("assign", "package:base")
    asgn(name, VAL, globalenv())
    return('null')
  })
}

r_call_native <- function(strfun, args = NULL, unused = NULL){
  no_jumps({
    FUN <- eval(parse(text = strfun))
    if(!is.function(FUN))
      stop("Argument is not a valid function")
    native_encode(do.call(FUN, as.list(args)))
  })
}

r_get_native <- function(str, unused1 = NULL, unused2 = NULL){
  no_jumps({
    native_encode(eval(parse(text = str)))
  })
}

r_eval_native <- function(str, args = NULL, unused = NULL){
  no_jumps({
    con <- textConnection(str)
    if(is.null(args))
      args <- list(print.eval = TRUE)
    do.call(source, c(list(file = con), as.list(args)))
    NULL
  })
}

r_assign_native <- function(name, value = NULL, unused = NULL){
  no_jumps({
    asgn <- get("assign", "package:base")
    asgn(name, value, globalenv())
    NULL
  })
}

no_jumps <- function(...){
  tryCatch(..., error = function(e){
    structure(e$message, class = 'cb_error')
  }, interrupt = function(e){
    structure("User interruption during R evaluation", class = 'cb_error')
  })
}
