#' Mark character strings as literal JavaScript code
#'
#' This function \code{JS()} marks character vectors with a special class, so
#' that it will be treated as literal JavaScript code. It was copied from the
#' htmlwidgets package, and does exactly the same thing.
#'
#' @param ... character vectors as the JavaScript source code (all arguments
#'   will be pasted into one character string)
#' @author Yihui Xie
#' @export
#' @examples ct <- v8()
#' ct$eval("1+1")
#' ct$eval(JS("1+1"))
#' ct$assign("test", JS("2+3"))
#' ct$get("test")
JS <- function(...) {
  x <- c(...)
  if (is.null(x)) return()
  if (!is.character(x))
    stop("The arguments for JS() must be a chraracter vector")
  x <- paste(x, collapse = '\n')
  structure(x, class = unique(c("JS_EVAL", oldClass(x))))
}
