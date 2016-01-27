callback <- function(strfun, json = "{}"){
  FUN <- eval(parse(text=strfun))
  ARGS <- as.list(jsonlite::fromJSON(json))
  if(!is.function(FUN))
    stop("Argument is not a valid function")
  out <- do.call(FUN, ARGS)
  jsonlite::toJSON(out)
}
