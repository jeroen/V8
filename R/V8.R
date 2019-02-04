#' Run JavaScript in a V8 context
#'
#' The \code{\link{v8}} function (formerly called \code{new_context}) creates a
#' new V8 \emph{context}. A context provides an execution environment that allows
#' separate, unrelated, JavaScript code to run in a single instance of V8, like a
#' tab in a browser.
#'
#' V8 contexts cannot be serialized but creating a new contexts and sourcing code
#' is very cheap. You can run as many parallel v8 contexts as you want. R packages
#' that use V8 can use a separate V8 context for each object or function call.
#'
#' The \code{ct$eval} method evaluates a string of raw code in the same way
#' as \code{eval} would do in JavaScript. It returns a string with console output.
#' The \code{ct$get}, \code{ct$assign} and \code{ct$call} functions
#' on the other hand automatically convert arguments and return value from/to JSON,
#' unless an argument has been wrapped in \code{JS()}, see examples.
#' The \code{ct$validate} function is used to test if a piece of code is valid
#' JavaScript syntax within the context, and always returns TRUE or FALSE.
#'
#' JSON is used for all data interchange between R and JavaScript. Therefore you can
#' (and should) only exchange data types that have a sensible JSON representation.
#' All arguments and objects are automatically converted according to the mapping
#' described in \href{http://arxiv.org/abs/1403.2805}{Ooms (2014)}, and implemented
#' by the jsonlite package in \code{\link{fromJSON}} and \code{\link{toJSON}}.
#'
#' The name of the global object (i.e. \code{global} in node and \code{window}
#' in browsers) can be set with the global argument. A context always have a global
#' scope, even when no name is set. When a context is initiated with \code{global = NULL},
#' the global environment can be reached by evaluating \code{this} in the global scope,
#' for example: \code{ct$eval("Object.keys(this)")}.
#' @section Methods:
#' \describe{
#'   \item{\code{console()}}{ starts an interactive console}
#'   \item{\code{eval(src)}}{ evaluates a string with JavaScript source code}
#'   \item{\code{validate(src)}}{ test if a string of JavaScript code is syntactically valid}
#'   \item{\code{source(file)}}{ evaluates a file with JavaScript code}
#'   \item{\code{get(name, ...)}}{ convert a JavaScript to R via JSON. Optional arguments (\code{...}) are passed to \link[jsonlite]{fromJSON} to set JSON coercion options.}
#'   \item{\code{assign(name, value)}}{ copy an R object to JavaScript via JSON}
#'   \item{\code{call(fun, ...)}}{ call a JavaScript function with arguments \code{...}. Arguments which are not wrapped in \code{JS()} automatically get converted to JSON}
#'   \item{\code{reset()}}{ resets the context (removes all objects)}
#' }
#' @references A Mapping Between JSON Data and R Objects (Ooms, 2014): \url{http://arxiv.org/abs/1403.2805}
#' @export v8 new_context
#' @param global character vector indicating name(s) of the global environment. Use NULL for no name.
#' @param console expose \code{console} API (\code{console.log}, \code{console.warn}, \code{console.error}).
#' @param typed_arrays enable support for typed arrays (part of ECMA6). This adds a bunch of additional
#' functions to the global namespace.
#' @aliases V8 v8 new_context
#' @rdname V8
#' @name V8
#' @importFrom jsonlite fromJSON toJSON
#' @importFrom curl curl
#' @importFrom Rcpp sourceCpp
#' @importFrom utils head loadhistory savehistory tail
#' @useDynLib V8
#' @examples # Create a new context
#' ctx <- v8();
#'
#' # Evaluate some code
#' ctx$eval("var foo = 123")
#' ctx$eval("var bar = 456")
#' ctx$eval("foo+bar")
#'
#' # Functions and closures
#' ctx$eval("JSON.stringify({x:Math.random()})")
#' ctx$eval("(function(x){return x+1;})(123)")
#'
#' # Objects (via JSON only)
#' ctx$assign("mydata", mtcars)
#' ctx$get("mydata")
#' ctx$get("mydata", simplifyVector = FALSE)
#'
#' # Assign JavaScript
#' ctx$assign("foo", JS("function(x){return x*x}"))
#' ctx$assign("bar", JS("foo(9)"))
#' ctx$get("bar")
#'
#' # Validate script without evaluating
#' ctx$validate("function foo(x){2*x}") #TRUE
#' ctx$validate("foo = function(x){2*x}") #TRUE
#' ctx$validate("function(x){2*x}") #FALSE
#'
#' # Use a JavaScript library
#' ctx$source(system.file("js/underscore.js", package="V8"))
#' ctx$call("_.filter", mtcars, JS("function(x){return x.mpg < 15}"))
#'
#' # Example from underscore manual
#' ctx$eval("_.templateSettings = {interpolate: /\\{\\{(.+?)\\}\\}/g}")
#' ctx$eval("var template = _.template('Hello {{ name }}!')")
#' ctx$call("template", list(name = "Mustache"))
#'
#' # Call anonymous function
#' ctx$call("function(x, y){return x * y}", 123, 3)
#'
#' \dontrun{CoffeeScript
#' ct2 <- v8()
#' ct2$source("http://coffeescript.org/extras/coffee-script.js")
#' jscode <- ct2$call("CoffeeScript.compile", "square = (x) -> x * x", list(bare = TRUE))
#' ct2$eval(jscode)
#' ct2$call("square", 9)
#'
#' # Interactive console
#' ct3 <- v8()
#' ct3$console()
#' //this is JavaScript
#' var test = [1,2,3]
#' JSON.stringify(test)
#' exit}
#'
v8 <- function(global = "global", console = TRUE, typed_arrays = TRUE) {
  # Private fields
  private <- environment();

  # Public methods
  this <- local({
    eval <- function(src){
      get_str_output(context_eval(join(src), private$context));
    }
    validate <- function(src){
      context_validate(join(src), private$context)
    }
    call <- function(fun, ..., auto_unbox = TRUE){
      stopifnot(is.character(fun))
      stopifnot(this$validate(c("fun=", fun)));
      jsargs <- list(...);
      if(!is.null(names(jsargs))){
        stop("Named arguments are not supported in JavaScript.")
      }
      jsargs <- vapply(jsargs, function(x){
        if(is.atomic(x) && inherits(x, "JS_EVAL")){
          as.character(x)
        } else {
          # To box or not. I'm not sure.
          toJSON(x, auto_unbox = auto_unbox)
        }
      }, character(1));
      jsargs <- paste(jsargs, collapse=",")
      src <- paste0("JSON.stringify((", fun ,")(", jsargs, "));");
      out <- this$eval(src)
      get_json_output(out)
    }
    source <- function(file){
      if(is.character(file) && length(file) == 1 && grepl("^https?://", file)){
        file <- curl(file, open = "r")
        on.exit(close(file))
      }
      # Always assume UTF8, even on Windows.
      this$eval(readLines(file, encoding = "UTF-8", warn = FALSE))
    }
    get <- function(name, ...){
      stopifnot(is.character(name))
      get_json_output(this$eval(c("JSON.stringify(", name, ")")), ...)
    }
    assign <- function(name, value, auto_unbox = TRUE, ...){
      stopifnot(is.character(name))
      obj <- if(inherits(value, "JS_EVAL")){
        invisible(this$eval(paste("var", name, "=", value)))
      } else {
        invisible(this$eval(paste("var", name, "=", toJSON(value, auto_unbox = auto_unbox, ...))))
      }
    }
    reset <- function(){
      private$context <- make_context(private$console);
      private$created <- Sys.time();
      if(length(global)){
        context_eval(paste("var", global, "= this;", collapse = "\n"), private$context)
      }
      if(isTRUE(typed_arrays)){
        context_enable_typed_arrays(private$context)
      }
      invisible()
    }
    console <- function(){
      this$eval("")
      message("This is V8 version ", version(), ". Press ESC or CTRL+C to exit.")
      on.exit(message("Exiting V8 console."))
      buffer <- character();

      # OSX R.app does not support savehistory
      has_history <- !inherits(try(savehistory(tempfile()), silent=T), "try-error")
      if(has_history){
        savehistory()
        on.exit(loadhistory(), add = TRUE)
        histfile <- ".V8history"
        if(file.exists(histfile)){
          loadhistory(histfile)
        } else {
          file.create(histfile)
        }
      }

      # Set custom tab completer
      rc.options(custom.completer = function(env){
        env$comps <- tab_complete(this, env$token)
      })
      on.exit({rc.options(custom.completer = NULL)}, add = TRUE)

      # REPL
      repeat {
        prompt <- ifelse(length(buffer), "  ", "~ ")
        if(nchar(line <- readline(prompt))){
          buffer <- c(buffer, line)
        }
        if(identical(buffer, "exit")) break;
        if(length(buffer) && (this$validate(buffer) || !nchar(line))){
          if(has_history){
            write(buffer, histfile, append = TRUE)
            loadhistory(histfile)
          }
          tryCatch(
            cat(undefined_to_null(this$eval(buffer))),
            error = function(e){
              message(e$message)
            }
          )
          buffer <- character();
        }
      }
    }
    #reg.finalizer(environment(), function(e){}, TRUE)
    reset()
    lockEnvironment(environment(), TRUE)
    structure(environment(), class=c("V8", "environment"))
  })
}

# For backward compatibility
new_context <- v8

undefined_to_null <- function(str){
  if(identical(str,"undefined")){
    invisible()
  } else {
    paste0(str, "\n")
  }
}

get_json_output <- function(json, ...){
  if(identical(json,"undefined")){
    invisible(NULL)
  } else {
    fromJSON(json, ...)
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
names.V8 <- function(x, ...){
  ls(x, ...)
}

#' @export
print.V8 <- function(x, ...){
  if(context_null(get("context", x))){
    cat("This context has been disposed.")
  } else {
    cat("V8 context methods:\n  $console()\n  $eval(src)\n  $validate(src)\n  $source(file)\n  $get(name)\n  $assign(name, value)\n  $call(fun, ...)\n  $reset()\n")
  }
}

join <- function (str){
  paste(str, collapse="\n")
}

# Override default call argument.
stop <- function(x, ..., call. = FALSE){
  if(inherits(x, "condition"))
    base::stop(x, ...)
  base::stop(x, ..., call. = call.)
}

#' @rdname V8
#' @export
engine_info <- function(){
  list (
    version = version()
  )
}
