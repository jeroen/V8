V8 <- function() {
  this <- environment();
  eval <- function(src){
    stopifnot(is.character(src))
  }
  call <- function(fun, ...){
    stopifnot(is.character(fun))
    stopifnot(jsvalidate(paste0("fun=", fun)));
    jsargs <- toJSON(unname(list(...)), auto_unbox=T);
    src <- paste0("fun=", fun, ";JSON.stringify(fun.apply(this,", jsargs, "));");
    out <- jseval(src)
    fromJSON(out)
  }
  source <- function(file){
    this$eval(readLines(file, warn = FALSE))
  }
  get <- function(name){
    fromJSON(this$eval("JSON.stringify(", name, ")"))
  }
  created <- Sys.time()
  lockEnvironment(this, TRUE)
  reg.finalizer(this, function(e){
    #destroy();
  }, TRUE)
  structure(this, class="V8")
}

`[[.V8` <- `$.V8` <- function(x, y){
  if(!exists(y, x, inherits = FALSE)){
    stop("V8 object has no field '", y, "'")
  }
  get(y, x, inherits = FALSE)
}

print.V8 <- function(x, ...){
  cat("V8 Engine methods:\n  $eval(src)\n  $source(file)\n  $call(fun, ...)\n  $get(name)\n")
}

stop <- function(..., call. = FALSE){
  base::stop(..., call. = call.)
}
