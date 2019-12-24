#' Run JavaScript in a V8 context
#'
#' The [v8()] function (formerly called `new_context`) creates a
#' new V8 *context*. A context provides an execution environment that allows
#' separate, unrelated, JavaScript code to run in a single instance of V8, like a
#' tab in a browser.
#'
#' A V8 context cannot be saved or duplicated, but creating a new context and sourcing
#' code is very cheap. You can run as many parallel v8 contexts as you want. R packages
#' that use V8 can use a separate V8 context for each object or function call.
#'
#' The name of the global object (i.e. `global` in node and `window`
#' in browsers) can be set with the global argument. A context always have a global
#' scope, even when no name is set. When a context is initiated with `global = NULL`,
#' the global environment can be reached by evaluating `this` in the global scope,
#' for example: `ct$eval("Object.keys(this)")`.
#'
#' @section V8 Context Methods:
#' \Sexpr[results=rd, stage=build, echo=FALSE]{V8:::generate_rd()}
#'
#' The `ct$eval` method evaluates a string of JavaScript code in the same way
#' as `eval` in JavaScript. By default `eval()` returns a string with
#' console output; but when the `serialize` parameter is set to `TRUE` it
#' serializes the JavaScript return object to a JSON string or a raw buffer.
#'
#' The `ct$get`, `ct$assign` and `ct$call` functions automatically
#' convert arguments and return value between R and JavaScript (using JSON). To pass
#' literal JavaScript arguments that should not be converted to JSON, wrap them in
#' `JS()`, see examples.
#'
#' The `ct$validate` function is used to test
#' if a piece of code is valid JavaScript syntax within the context, and always
#' returns TRUE or FALSE.
#'
#' In an interactive R session you can use `ct$console()` to switch to an
#' interactive JavaScript console. Here you can use `console.log` to print
#' objects, and there is some support for JS tab-completion. This is mostly for
#' testing and debugging, it may not work perfectly in every IDE or R-frontend.
#'
#' @section Data Interchange:
#' JSON is used for data interchange between R and JavaScript. Therefore you can
#' (and should) only exchange data types that have a sensible JSON representation.
#' One exception is raw vectors which are converted to/from Uint8Array buffers, see
#' below. All other arguments and objects are automatically converted according to the mapping
#' described in [Ooms (2014)](http://arxiv.org/abs/1403.2805), and implemented
#' by the jsonlite package in [fromJSON()] and [toJSON()].
#'
#' As for version 3.0 of this R package, Raw vectors are converted to `Uint8Array`
#' typed arrays, and vice versa. This makes it possible to efficiently copy large chunks
#' binary data between R and JavaScript, which is useful for running [wasm]
#' or emscripten.
#'
#' @section Note about Linux and Legacy V8 engines:
#' This R package can be compiled against modern (V8 version 6+) libv8 API, or the legacy
#' libv8 API (V8 version 3.15 and below). You can check `V8::engine_info()` to see the version
#' that is running. The legacy version does not support modern JS (ES6) or WASM, but it is
#' still the default on older versions of Ubuntu and CentOS. The latest versions of all major
#' Linux distributions now provide a modern version of V8. For Ubuntu 16.04 and 18.04
#' we provide backports of libv8 (via libnode-dev), see the
#' [readme](https://github.com/jeroen/v8#backports-for-xenial-and-bionic) for details.
#'
#' @references A Mapping Between JSON Data and R Objects (Ooms, 2014): <http://arxiv.org/abs/1403.2805>
#' @export v8 new_context
#' @param global character vector indicating name(s) of the global environment. Use NULL for no name.
#' @param console expose `console` API (`console.log`, `console.warn`, `console.error`).
#' @param typed_arrays (deprecated) enable typed arrays in legacy libv8. Deprecated because
#' typed arrays are natively supported in recent versions of libv8.
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
#' outlist <- ctx$get("mydata", simplifyVector = FALSE)
#' outlist[1]
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
#' \dontrun{
#' #CoffeeScript
#' ct2 <- v8()
#' ct2$source("http://coffeescript.org/v1/browser-compiler/coffee-script.js")
#' jscode <- ct2$call("CoffeeScript.compile", "square = (x) -> x * x", list(bare = TRUE))
#' ct2$eval(jscode)
#' ct2$call("square", 9)
#'
#' # Interactive console
#' ct3 <- v8()
#' ct3$console()
#' # //this is JavaScript
#' # var test = [1,2,3]
#' # JSON.stringify(test)
#' # exit
#' }
#'
v8 <- function(global = "global", console = TRUE, typed_arrays = TRUE) {
  # Private fields
  private <- environment();

  # Low level evaluate
  evaluate_js <- function(src, serialize = FALSE){
    get_str_output(context_eval(join(src), private$context, serialize))
  }

  # Public methods
  this <- local({
    eval <- function(src, serialize = FALSE){
      # serialize=TRUE does not unserialize: user has to parse json/raw
      evaluate_js(src, serialize = serialize)
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
        if(is.raw(x)){
          raw_to_js(x)
        } else if(is.atomic(x) && inherits(x, "JS_EVAL")){
          as.character(x)
        } else {
          # To box or not. I'm not sure.
          toJSON(x, auto_unbox = auto_unbox)
        }
      }, character(1));
      jsargs <- paste(jsargs, collapse=",")
      src <- paste0("(", fun ,")(", jsargs, ");")
      get_json_output(evaluate_js(src, serialize = TRUE))
    }
    source <- function(file){
      if(is.character(file) && length(file) == 1 && grepl("^https?://", file)){
        file <- curl(file, open = "r")
        on.exit(close(file))
      }
      # Always assume UTF8, even on Windows.
      evaluate_js(readLines(file, encoding = "UTF-8", warn = FALSE))
    }
    get <- function(name, ...){
      stopifnot(is.character(name))
      get_json_output(evaluate_js(name, serialize = TRUE), ...)
    }
    assign <- function(name, value, auto_unbox = TRUE, ...){
      stopifnot(is.character(name))
      obj <- if(is.raw(value)) {
        write_array_buffer(name, value, private$context)
      } else if(inherits(value, "JS_EVAL")) {
        invisible(evaluate_js(paste("var", name, "=", value)))
      } else {
        invisible(evaluate_js(paste("var", name, "=", toJSON(value, auto_unbox = auto_unbox, ...))))
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
  if(is.raw(json)){
    return(json)
  } else if(is.null(json) || identical(json,"undefined")){
    invisible()
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
    ns <- ls(x)
    title <- sprintf("<V8 engine %s>", engine_info()$version)
    cat(title, "\n")
    lapply(ns, function(fn){
      cat(format_function(x[[fn]], fn), sep = "\n")
    })
    invisible()
  }
}

# Pretty format function headers
format_function <- function(fun, name = deparse(substitute(fun))){
  #header <- sub("\\{$", "", capture.output(fun)[1])
  header <- utils::head(deparse(args(fun), 100L), -1)
  header <- sub("^[ ]*", "   ", header)
  header[1] <- sub("^[ ]*function ?", paste0(" $", name), header[1])
  header
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

raw_to_js <- function(x){
  stopifnot(is.raw(x))
  paste0('new Uint8Array(', jsonlite::toJSON(as.integer(x)), ')')
}

generate_rd <- function(){
  out <- paste(utils::capture.output(print(v8())), collapse = "\n")
  paste("\\preformatted{", "## ctx <- v8()", out, "}\n", sep = "\n")
}
