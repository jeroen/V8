#' Experimental WebAssembly
#'
#' Experimental wrapper to load a WebAssembly program. Returns a list of
#' exported functions. This will probably be moved into it's own package
#' once WebAssembly matures.
#'
#' @export
#' @param data either raw vector or file path with the binary wasm program
#' @examples # Load example wasm program
#' if(engine_info()$version > 6){
#' instance <- wasm(system.file('wasm/add.wasm', package = 'V8'))
#' instance$exports$add(12, 30)
#' }
wasm <- function(data){
  if(is.character(data))
    data <- readBin(normalizePath(data, mustWork = TRUE), raw(), file.info(data)$size)
  if(!is.raw(data))
    stop("Data must be file path or raw vector")
  ctx <- v8()
  ctx$assign('bytes', data)
  ctx$eval('let module = new WebAssembly.Module(bytes);')
  ctx$eval('let instance = new WebAssembly.Instance(module);')
  function_names <- ctx$get('Object.keys(instance.exports)')
  exports <- structure(lapply(function_names, function(f){
    body <- sprintf('call("instance.exports.%s", ...)', f)
    fun <- list(... = substitute(), parse(text = body)[[1]])
    as.function(fun, envir = environment(ctx$call))
  }), names = function_names)
  list(
    exports = exports
  )
}
