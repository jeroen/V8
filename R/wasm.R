#' Experimental WebAssembly
#'
#' Experimental wrapper to load a WebAssembly program. Returns a list of
#' exported functions. This will probably be moved into it's own package
#' once WebAssembly matures.
#'
#' The `wasm_features()` function uses the [wasm-feature-detect](https://github.com/GoogleChromeLabs/wasm-feature-detect)
#' JavaScript library to test which WASM capabilities are supported in the
#' current version of libv8.
#'
#' @export
#' @rdname wasm
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

#' @export
#' @rdname wasm
#' @examples wasm_features()
wasm_features <- function(){
  ctx <- v8()
  ctx$source(system.file('js/wasm-feature-detect.js', package = 'V8'))
  wrapper <- "async function test_wasm_features() {
  let out = {};
  keys = Object.keys(wasmFeatureDetect);
  for (var i = 0; i < keys.length; i++) {
    var key = keys[i];
    out[key] = await wasmFeatureDetect[key]()
  }
  return out;
}"
  ctx$eval(wrapper)
  ctx$call('test_wasm_features', await = TRUE)
}
