#' Embedded JavaScript
#'
#' A \emph{context} is an execution environment that allows separate, unrelated,
#' JavaScript code to run in a single instance of V8. You must explicitly specify
#' the context in which you want any JavaScript code to be run.
#'
#' @export
#' @aliases V8
#' @importFrom jsonlite fromJSON toJSON
#' @importFrom Rcpp sourceCpp
#' @useDynLib V8 V8_context_eval V8_make_context V8_context_validate
#' @examples # Create a new context
#' ct <- new_context();
#'
#' # Evaluate some code
#' ct$eval("foo=123")
#' ct$eval("bar=456")
#' ct$eval("foo+bar")
#'
#' # Functions and closures
#' ct$eval("JSON.stringify({x:Math.random()})")
#' ct$eval("(function(x){return x+1;})(123)")
#'
#' # Objects (via JSON only)
#' ct$assign("mydata", mtcars)
#' ct$get("mydata")
#'
#' # Validate syntax without evaluating
#' ct$validate("function foo(x){2*x}") #TRUE
#' ct$validate("foo = function(x){2*x}") #TRUE
#' ct$validate("function(x){2*x}") #FALSE
#'
#' # Load a JavaScript library
#' ct$source(system.file("js/underscore.js", package="V8"))
#'
#' # Call a function
#'
#'
new_context <- function() {
  this <- environment();
  context <- make_context();
  created <- Sys.time()
  eval <- function(src){
    context_eval(paste(src, collapse="\n"), context);
  }
  validate <- function(src){
    context_validate(paste(src, collapse="\n"), context)
  }
  call <- function(fun, ...){
    stopifnot(is.character(fun))
    stopifnot(this$validate(paste0("fun=", fun)));
    jsargs <- toJSON(unname(list(...)), auto_unbox=T);
    src <- paste0("fun=", fun, ";JSON.stringify(fun.apply(this,", jsargs, "));");
    out <- this$eval(src)
    fromJSON(out)
  }
  source <- function(file){
    this$eval(readLines(file, warn = FALSE))
  }
  get <- function(name){
    fromJSON(this$eval(c("JSON.stringify(", name, ")")))
  }
  assign <- function(x, value){
    invisible(this$eval(c(x, "=", toJSON(value))))
  }
  lockEnvironment(this, TRUE)
  #reg.finalizer(this, function(e){}, TRUE)
  structure(this, class="V8")
}

#' @export
`$.V8` <- function(x, y){
  if(!exists(y, x, inherits = FALSE)){
    stop("V8 object has no field '", y, "'")
  }
  get(y, x, inherits = FALSE)
}

#' @export
`[[.V8` <- `$.V8`

#' @export
print.V8 <- function(x, ...){
  cat("V8 context methods:\n  $eval(src)\n  $validate(src)\n  $source(file)\n  $get(name)\n  $assign(name, value)\n  $call(fun, ...)\n")
}

# Override default call argument.
stop <- function(..., call. = FALSE){
  base::stop(..., call. = call.)
}
