#' Run WebAssembly
#'
#' Load a WebAssembly program. Returns a list of exported functions.
#'
#' @export
#' @param data either raw vector or file path with the binary wasm program
#' @examples # Load example wasm program
#' if(engine_info()$version > 6){
#' exports <- wasm(system.file('wasm/add.wasm', package = 'V8'))
#' exports$add(12, 30)
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
  structure(lapply(function_names, function(f){
    body <- sprintf('call("instance.exports.%s", ...)', f)
    fun <- list(... = substitute(), parse(text = body)[[1]])
    as.function(fun, envir = environment(ctx$call))
  }), names = function_names)
}
