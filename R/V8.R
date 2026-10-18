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
#' convert arguments and return value between R and JavaScript. To pass
#' literal JavaScript arguments that should not be converted, wrap them in
#' `JS()`, see examples.
#'
#' If a call to `ct$eval()`,`ct$get()`, or `ct$call()` returns a JavaScript promise,
#' you can set `await = TRUE` to wait for the promise to be resolved. It will then
#' return the result of the promise, or an error in case the promise is rejected.
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
#' Arguments and return values are converted between R and JavaScript according
#' to the mapping described in [Ooms (2014)](https://arxiv.org/abs/1403.2805), the
#' same mapping that the jsonlite package implements. Values are copied directly
#' between R and JavaScript, without going through JSON text. Vectors, lists,
#' matrices and data frames are converted in both directions, data frames column
#' by column.
#'
#' The options of `toJSON()` from jsonlite can be passed to `ct$assign()` and
#' as an options object to `console.r.get()`: `dataframe`, `matrix`, `Date`,
#' `POSIXt`, `factor`, `complex`, `raw`, `null`, `na`, `auto_unbox`, `digits`,
#' `UTC`, `rownames`, `keep_vec_names`, `json_verbatim` and `time_format`.
#' The options of `fromJSON()` can be passed to `ct$get()` and as an options
#' object to `console.r.assign()`: `simplifyVector`, `simplifyDataFrame`,
#' `simplifyMatrix` and `flatten`. `ct$call()` passes `simplify` on as
#' `simplifyVector`.
#'
#' Some defaults differ from jsonlite: doubles keep full precision (jsonlite
#' rounds to 4 decimal digits unless `digits` is set), `NA` in vectors becomes
#' `null` (jsonlite writes the string `"NA"` for numbers), `NaN` and `Inf` stay
#' numbers, raw vectors become `Uint8Array` also inside lists, and whole numbers
#' from JavaScript are returned as integers whenever they fit.
#'
#' With `backend = "jsonlite"` (always the case in WebAssembly/webR), values are
#' converted to and from JSON text with the jsonlite package instead, as in
#' earlier versions of V8.
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
#' @references A Mapping Between JSON Data and R Objects (Ooms, 2014): <https://arxiv.org/abs/1403.2805>
#' @export v8 new_context
#' @param global character vector indicating name(s) of the global environment. Use NULL for no name.
#' @param console expose `console` API (`console.log`, `console.warn`, `console.error`).
#' @param ... ignored parameters for past/future versions.
#' @param backend how values are converted between R and JavaScript: `"native"`
#' (default) or `"jsonlite"`, see the data interchange section. The default can be
#' changed with the `V8.backend` option.
#' @aliases V8 v8 new_context
#' @rdname V8
#' @name V8
#' @import Rcpp
#' @importFrom curl curl
#' @importFrom utils head loadhistory savehistory tail
#' @useDynLib V8, .registration = TRUE
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
#' ctx$source("https://cdnjs.cloudflare.com/ajax/libs/underscore.js/1.13.6/underscore-min.js")
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
v8 <- function(global = "global", console = TRUE, ..., backend = getOption("V8.backend", "native")) {
  # Private fields
  private <- environment();
  backend <- match.arg(backend, c("native", "jsonlite"))
  if(identical(R.version$os, "emscripten"))
    backend <- "jsonlite"
  if(backend == "jsonlite" && !requireNamespace("jsonlite", quietly = TRUE))
    stop("The jsonlite backend requires the jsonlite package.")
  native <- backend == "native"

  # Low level evaluate
  evaluate_js <- function(src, serialize = FALSE, await = FALSE){
    get_str_output(context_eval(join(src), private$context, serialize, await))
  }

  # Public methods
  this <- local({
    eval <- function(src, serialize = FALSE, await = FALSE){
      # serialize=TRUE does not unserialize: user has to parse json/raw
      evaluate_js(src, serialize = serialize, await = await)
    }
    validate <- function(src){
      context_validate(join(src), private$context)
    }
    call <- function(fun, ..., auto_unbox = TRUE, await = FALSE, simplify = TRUE){
      stopifnot(is.character(fun))
      stopifnot(this$validate(c("fun=", fun)));
      jsargs <- list(...);
      if(!is.null(names(jsargs))){
        stop("Named arguments are not supported in JavaScript.")
      }
      if(private$native){
        key <- "__V8_call_args__"
        options <- list(auto_unbox = auto_unbox)
        values <- list()
        exprs <- character(length(jsargs))
        for(i in seq_along(jsargs)){
          x <- jsargs[[i]]
          if(is.atomic(x) && inherits(x, "JS_EVAL")){
            exprs[i] <- as.character(x)
          } else {
            values[length(values) + 1] <- list(native_encode(x, options))
            exprs[i] <- sprintf("V8_args[%d]", length(values) - 1)
          }
        }
        src <- paste0("(", fun, ")(", paste(exprs, collapse = ","), ");")
        if(length(values)){
          write_native(key, values, options, private$context)
          src <- sprintf("(function(V8_args){delete this['%s']; return (%s)(%s);})(this['%s']);",
                         key, fun, paste(exprs, collapse = ","), key)
        }
        out <- context_eval(join(src), private$context, TRUE, await, TRUE, list(simplifyVector = simplify))
        return(if(is.null(out)) invisible() else out)
      }
      jsargs <- vapply(jsargs, function(x){
        if(is.raw(x)){
          raw_to_js(x)
        } else if(is.atomic(x) && inherits(x, "JS_EVAL")){
          as.character(x)
        } else {
          # To box or not. I'm not sure.
          jsonlite::toJSON(x, auto_unbox = auto_unbox)
        }
      }, character(1));
      jsargs <- paste(jsargs, collapse=",")
      src <- paste0("(", fun ,")(", jsargs, ");")
      get_json_output(evaluate_js(src, serialize = TRUE, await = await), simplifyVector = simplify)
    }
    source <- function(file){
      if(is.character(file) && length(file) == 1 && grepl("^https?://", file)){
        file <- curl(file, open = "r")
        on.exit(close(file))
      }
      # Always assume UTF8, even on Windows.
      evaluate_js(readLines(file, encoding = "UTF-8", warn = FALSE))
    }
    get <- function(name, ..., await = FALSE){
      stopifnot(is.character(name))
      if(private$native){
        options <- if(missing(...)) list() else from_json_options(list(...))
        out <- context_eval(join(name), private$context, TRUE, await, TRUE, options)
        if(isTRUE(options$flatten))
          out <- flatten_frames(out)
        return(if(is.null(out)) invisible() else out)
      }
      get_json_output(evaluate_js(name, serialize = TRUE, await = await), ...)
    }
    assign <- function(name, value, auto_unbox = TRUE, ...){
      # jsonlite's 'na' option partially matches 'name', so bind arguments again exactly
      call_names <- names(sys.call())
      if("na" %in% call_names && !("name" %in% call_names)){
        args <- lapply(as.list(sys.call())[-1], base::eval, envir = parent.frame())
        tags <- names(args)
        positional <- args[tags == ""]
        named <- args[tags != ""]
        for(arg in c("name", "value", "auto_unbox")){
          if(is.null(named[[arg]]) && length(positional)){
            named[[arg]] <- positional[[1]]
            positional <- positional[-1]
          }
        }
        return(do.call(this$assign, c(named, positional)))
      }
      stopifnot(is.character(name))
      obj <- if(is.raw(value)) {
        write_array_buffer(name, value, private$context)
      } else if(inherits(value, "JS_EVAL")) {
        invisible(evaluate_js(paste("var", name, "=", value)))
      } else if(private$native) {
        options <- if(missing(...)) list(auto_unbox = auto_unbox) else to_json_options(c(list(auto_unbox = auto_unbox), list(...)))
        invisible(write_native(name, native_encode(value, options), options, private$context))
      } else {
        invisible(evaluate_js(paste("var", name, "=", jsonlite::toJSON(value, auto_unbox = auto_unbox, ...))))
      }
    }
    reset <- function(){
      private$context <- make_context(private$console, private$native);
      private$created <- Sys.time();
      if(length(global)){
        context_eval(paste("var", global, "= this;", collapse = "\n"), private$context)
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
  } else if(!length(json) || identical(json,"undefined")){
    invisible()
  } else {
    jsonlite::fromJSON(json, ...)
  }
}

to_json_options <- function(options){
  allowed <- c("dataframe", "matrix", "Date", "POSIXt", "factor", "complex", "raw", "null", "na",
               "auto_unbox", "digits", "pretty", "force", "UTC", "rownames", "keep_vec_names",
               "json_verbatim", "always_decimal", "time_format")
  if(length(options) && (is.null(names(options)) || any(names(options) == "")))
    stop("Options must be named.")
  unknown <- setdiff(names(options), allowed)
  if(length(unknown))
    stop("Unsupported option: ", paste(unknown, collapse = ", "))
  options$dataframe <- match.arg(options$dataframe, c("rows", "columns", "values"))
  options$matrix <- match.arg(options$matrix, c("rowmajor", "columnmajor"))
  options$Date <- match.arg(options$Date, c("ISO8601", "epoch"))
  options$POSIXt <- match.arg(options$POSIXt, c("string", "ISO8601", "epoch", "mongo"))
  options$factor <- match.arg(options$factor, c("string", "integer"))
  options$complex <- match.arg(options$complex, c("string", "list"))
  options$null <- match.arg(options$null, c("list", "null"))
  if(!is.null(options$raw))
    options$raw <- match.arg(options$raw, c("base64", "hex", "mongo", "int", "js"))
  if(!is.null(options$na))
    options$na <- match.arg(options$na, c("null", "string"))
  options
}

from_json_options <- function(options){
  allowed <- c("simplifyVector", "simplifyDataFrame", "simplifyMatrix", "flatten", "bigint_as_char")
  if(length(options) && (is.null(names(options)) || any(names(options) == "")))
    stop("Options must be named.")
  unknown <- setdiff(names(options), allowed)
  if(length(unknown))
    stop("Unsupported option: ", paste(unknown, collapse = ", "))
  options
}

base64_encode <- function(x){
  chars <- c(LETTERS, letters, 0:9, "+", "/")
  pad <- (3 - length(x) %% 3) %% 3
  m <- matrix(c(as.integer(x), integer(pad)), nrow = 3)
  v <- m[1, ] * 65536L + m[2, ] * 256L + m[3, ]
  idx <- rbind(v %/% 262144L, (v %/% 4096L) %% 64L, (v %/% 64L) %% 64L, v %% 64L)
  out <- chars[as.vector(idx) + 1L]
  if(pad > 0)
    out[seq(length(out) - pad + 1, length(out))] <- "="
  paste(out, collapse = "")
}

native_encode <- function(x, options = list()){
  if(is.null(options$digits) && is.null(options$raw) && !isTRUE(options$keep_vec_names) &&
     !identical(options$factor, "integer") && native_plain(x))
    return(x)
  if(is.data.frame(x)){
    out <- lapply(unclass(x), native_encode, options = options)
    rn <- attr(x, "row.names")
    if(isTRUE(options$rownames)){
      out[["_row"]] <- if(is.character(rn)) rn else seq_len(nrow(x))
    } else if(!isFALSE(options$rownames) && is.character(rn)){
      out[["_row"]] <- rn
    }
    return(structure(out, row.names = .set_row_names(nrow(x)), class = "data.frame"))
  }
  if(inherits(x, "POSIXlt"))
    x <- as.POSIXct(x)
  if(inherits(x, "POSIXct")){
    posixt <- if(is.null(options$POSIXt)) "string" else options$POSIXt
    if(posixt == "epoch" || posixt == "mongo"){
      ms <- floor(as.numeric(x) * 1000)
      if(posixt == "epoch")
        return(ms)
      return(structure(list(`$date` = ms), row.names = .set_row_names(length(ms)), class = "data.frame"))
    }
    format <- options$time_format
    if(is.null(format)){
      format <- if(posixt == "string") "" else if(isTRUE(options$UTC)) "%Y-%m-%dT%H:%M:%SZ" else "%Y-%m-%dT%H:%M:%S"
    }
    u <- unique(x)
    text <- if(isTRUE(options$UTC)) format(u, format = format, tz = "UTC") else format(u, format = format)
    out <- text[match(unclass(x), unclass(u))]
    return(if(inherits(x, "AsIs")) I(out) else out)
  }
  if(inherits(x, "Date")){
    if(identical(options$Date, "epoch"))
      return(as.numeric(x))
    u <- unique(x)
    out <- format(u)[match(unclass(x), unclass(u))]
    return(if(inherits(x, "AsIs")) I(out) else out)
  }
  if(is.factor(x))
    return(if(identical(options$factor, "integer")) as.integer(x) else x)
  if(inherits(x, "json"))
    return(if(isTRUE(options$json_verbatim)) x else as.character(x))
  other <- setdiff(oldClass(x), c("AsIs", "JS_EVAL"))
  if(length(other))
    stop("No native conversion for class: ", paste(other, collapse = ", "))
  if(is.list(x)){
    x[] <- lapply(x, native_encode, options = options)
    return(x)
  }
  if(is.complex(x)){
    if(identical(options$complex, "list"))
      return(list(real = Re(x), imaginary = Im(x)))
    out <- as.character(x)
    dim(out) <- dim(x)
    return(if(inherits(x, "AsIs")) I(out) else out)
  }
  if(is.raw(x) && !is.null(options$raw) && options$raw != "js"){
    return(switch(options$raw,
      int = as.integer(x),
      hex = as.character(as.hexmode(as.integer(x))),
      base64 = base64_encode(x),
      mongo = list(`$binary` = base64_encode(x), `$type` = "5")
    ))
  }
  if(is.double(x) && length(options$digits) == 1 && !is.na(options$digits)){
    if(inherits(options$digits, "AsIs")){
      x[] <- signif(x, options$digits)
    } else {
      rounded <- round(x, options$digits)
      small <- !is.na(x) & rounded == 0 & x != 0
      rounded[small] <- signif(x[small], options$digits)
      x[] <- rounded
    }
  }
  if(isTRUE(options$keep_vec_names) && !is.null(names(x)) && is.null(dim(x)))
    return(native_encode(as.list(x), options))
  x
}

flatten_frames <- function(x){
  if(is.data.frame(x)){
    out <- list()
    for(i in seq_along(x)){
      col <- flatten_frames(x[[i]])
      if(is.data.frame(col)){
        cols <- unclass(col)
        names(cols) <- paste(names(x)[i], names(col), sep = ".")
        out <- c(out, cols)
      } else {
        out[[names(x)[i]]] <- col
      }
    }
    return(structure(out, row.names = .set_row_names(nrow(x)), class = "data.frame"))
  }
  if(is.list(x))
    x[] <- lapply(x, flatten_frames)
  x
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
    title <- sprintf("<V8 engine %s>", version())
    cat(title, "\n")
    lapply(ns, function(fn){
      cat(format_function(x[[fn]], fn), sep = "\n")
    })
    invisible()
  }
}

#' @export
#' @rawNamespace if (getRversion() >= "4.3.0") S3method(base::`@`, V8)
`@.V8` <- function(object, name, ...){
  object[[name]]
}

### enable tab completion
#' @rawNamespace if (getRversion() >= "4.3.0" && !is.null(asNamespace("utils")$.AtNames)) S3method(utils::.AtNames,V8)
.AtNames.V8 <- function(x, pattern) {
  asNamespace("utils")$findMatches(pattern, ls(x))
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
    version = version(),
    numeric_version = v8_version_numeric()
  )
}

# normalizes e.g. 7.8.279.23-node.56
v8_version_numeric <- function(){
  numeric_version(sub('^([0-9.]+).*', '\\1', version()))
}

raw_to_js <- function(x){
  stopifnot(is.raw(x))
  paste0('new Uint8Array(', jsonlite::toJSON(as.integer(x)), ')')
}

generate_rd <- function(){
  out <- paste(utils::capture.output(print(v8())), collapse = "\n")
  paste("\\preformatted{", "## ctx <- v8()", out, "}\n", sep = "\n")
}
