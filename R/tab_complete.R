#' @importFrom utils rc.options
tab_complete <- function(ct, buf){
  # Check if this is a completable expression
  if(!ct$validate(paste0(buf, "xx"))){
    return(character())
  }

  # Buffer ends with a dot
  if(substr(buf, nchar(buf), nchar(buf)) == "."){
    base <- substr(buf, 1, nchar(buf)-1)
    token <- ""
  } else {
    base <- head(strsplit(buf, ".", fixed=T)[[1]], -1)
    token <- tail(strsplit(buf, ".", fixed=T)[[1]], 1)
  }
  object <- paste(c("this", base), collapse=".")
  comps <- tryCatch({
    fields <- ct$call("Object.getOwnPropertyNames", JS(object))
    suggestions <- grep(paste0("^", token, "."), fields, value = TRUE)
    if(length(base) && length(suggestions)) {
      paste(paste(base, collapse="."), suggestions, sep = ".")
    } else {
      suggestions
    }
  }, error = function(e) character());
  sort(comps)
}
