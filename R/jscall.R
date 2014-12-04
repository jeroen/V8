jscall <- function(fun, ...) {
  stopifnot(V8::jsvalidate(paste0("fun=", fun)));
  jsargs <- jsonlite::toJSON(unname(list(...)), auto_unbox=T);
  src <- paste0("fun=", fun, ";JSON.stringify(fun.apply(this,", jsargs, "));");
  out <- V8::jseval(src)
  jsonlite::fromJSON(out)
}

#jscall("function(x,y){return x+y}", 10, 38)
