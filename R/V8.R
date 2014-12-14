#' Run JavaScript in a V8 context
#'
#' A \emph{context} is an execution environment that allows separate, unrelated,
#' JavaScript code to run in a single instance of V8. You must explicitly specify
#' the context in which you want any JavaScript code to be run.
#'
#' The \code{ct$eval} method evaluates a string of raw code in the same way
#' as \code{eval} would do in JavaScript. It returns a string with console output.
#' The \code{ct$get}, \code{ct$assign} and \code{ct$call} functions
#' on the other hand automatically convert arguments and return value from/to JSON,
#' unless an argument has been wrapped in \code{I()}, see examples.
#' The \code{ct$validate} function is used to test if a piece of code is valid
#' JavaScript syntax within the context, and always returns TRUE or FALSE.
#'
#' JSON is used for all data interchange beteen R and JavaScript. Therefore you can
#' (and should) only exchange data types that have a sensible JSON representation.
#' All aguments and objects are automatically converted according to the mapping
#' described in \href{http://arxiv.org/abs/1403.2805}{Ooms (2014)}, and implemented
#' by the jsonlite package in \code{\link{fromJSON}} and \code{\link{toJSON}}.
#'
#' @references A Mapping Between JSON Data and R Objects (Ooms, 2014): \url{http://arxiv.org/abs/1403.2805}
#' @export
#' @aliases V8
#' @importFrom jsonlite fromJSON toJSON
#' @importFrom Rcpp sourceCpp
#' @useDynLib V8
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
#' # Assign JavaScript
#' ct$assign("foo", I("function(x){return x*x}"))
#' ct$assign("bar", I("foo(9)"))
#' ct$get("bar")
#'
#' # Validate script without evaluating
#' ct$validate("function foo(x){2*x}") #TRUE
#' ct$validate("foo = function(x){2*x}") #TRUE
#' ct$validate("function(x){2*x}") #FALSE
#'
#' # Use a JavaScript library
#' ct$source(system.file("js/underscore.js", package="V8"))
#' ct$call("_.filter", mtcars, I("function(x){return x.mpg < 15}"))
#'
#' # Example from underscore manual
#' ct$eval("_.templateSettings = {interpolate: /\\{\\{(.+?)\\}\\}/g}")
#' ct$eval("var template = _.template('Hello {{ name }}!')")
#' ct$call("template", list(name = "Mustache"))
#'
#' # Call anonymous function
#' ct$call("function(x, y){return x * y}", 123, 3)
#'
#' \dontrun{fun with CoffeeScript
#' ct2 <- new_context()
#' ct2$source("http://coffeescript.org/extras/coffee-script.js")
#' jscode <- ct2$call("CoffeeScript.compile", "square = (x) -> x * x", list(bare = TRUE))
#' ct2$eval(jscode)
#' ct2$call("square", 9)
#' }
#'
new_context <- function() {
  # Internal variables
  context <- make_context();
  created <- Sys.time();

  # Exported variables
  this <- local({
    eval <- function(src){
      if(length(src) > 1){
        src <- join(src)
      }
      get_str_output(context_eval_safe(src, context));
    }
    validate <- function(src){
      context_validate_safe(join(src), context)
    }
    call <- function(fun, ...){
      stopifnot(is.character(fun))
      stopifnot(this$validate(c("fun=", fun)));
      jsargs <- list(...);
      if(!is.null(names(jsargs))){
        stop("Named arguments are not supported in JavaScript.")
      }
      jsargs <- vapply(jsargs, function(x){
        if(is.atomic(x) && is(x, "AsIs")){
          as.character(x)
        } else {
          # To box or not. I'm not sure.
          toJSON(x, auto_unbox = TRUE)
        }
      }, character(1));
      jsargs <- paste(jsargs, collapse=",")
      src <- paste0("JSON.stringify((", fun ,")(", jsargs, "));");
      out <- this$eval(src)
      get_json_output(out)
    }
    source <- function(file){
      this$eval(readLines(file, warn = FALSE))
    }
    get <- function(name){
      stopifnot(is.character(name))
      get_json_output(this$eval(c("JSON.stringify(", name, ")")))
    }
    assign <- function(name, value){
      stopifnot(is.character(name))
      obj <- if(is(value, "AsIs")){
        invisible(this$eval(paste(name, "=", value)))
      } else {
        invisible(this$eval(paste(name, "=", toJSON(value))))
      }
    }
    reset <- function(){
      context <<- make_context();
    }
    #reg.finalizer(environment(), function(e){}, TRUE)
    lockEnvironment(environment(), TRUE)
    structure(environment(), class=c("V8", "environment"))
  })
}

get_json_output <- function(json){
  if(identical(json,"undefined")){
    invisible()
  } else {
    fromJSON(json)
  }
}

get_str_output <- function(str){
  if(identical(str, "undefined")){
    invisible(str)
  } else {
    return(str)
  }
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
`[.V8` <- `$.V8`

#' @export
print.V8 <- function(x, ...){
  if(context_null(get("context", x))){
    cat("This context has been disposed.")
  } else {
    cat("V8 context methods:\n  $eval(src)\n  $validate(src)\n  $source(file)\n  $get(name)\n  $assign(name, value)\n  $call(fun, ...)\n  $reset()\n")
  }
}

join <- function (str){
  paste(str, collapse="\n")
}

# Override default call argument.
stop <- function(..., call. = FALSE){
  base::stop(..., call. = call.)
}
