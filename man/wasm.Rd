% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/wasm.R
\name{wasm}
\alias{wasm}
\title{Experimental WebAssembly}
\usage{
wasm(data)
}
\arguments{
\item{data}{either raw vector or file path with the binary wasm program}
}
\description{
Experimental wrapper to load a WebAssembly program. Returns a list of
exported functions. This will probably be moved into it's own package
once WebAssembly matures.
}
\examples{
# Load example wasm program
if(engine_info()$version > 6){
instance <- wasm(system.file('wasm/add.wasm', package = 'V8'))
instance$exports$add(12, 30)
}
}
