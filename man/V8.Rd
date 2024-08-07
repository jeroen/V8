% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/V8.R
\name{V8}
\alias{V8}
\alias{v8}
\alias{new_context}
\alias{engine_info}
\title{Run JavaScript in a V8 context}
\usage{
v8(global = "global", console = TRUE, ...)

engine_info()
}
\arguments{
\item{global}{character vector indicating name(s) of the global environment. Use NULL for no name.}

\item{console}{expose \code{console} API (\code{console.log}, \code{console.warn}, \code{console.error}).}

\item{...}{ignored parameters for past/future versions.}
}
\description{
The \code{\link[=v8]{v8()}} function (formerly called \code{new_context}) creates a
new V8 \emph{context}. A context provides an execution environment that allows
separate, unrelated, JavaScript code to run in a single instance of V8, like a
tab in a browser.
}
\details{
A V8 context cannot be saved or duplicated, but creating a new context and sourcing
code is very cheap. You can run as many parallel v8 contexts as you want. R packages
that use V8 can use a separate V8 context for each object or function call.

The name of the global object (i.e. \code{global} in node and \code{window}
in browsers) can be set with the global argument. A context always have a global
scope, even when no name is set. When a context is initiated with \code{global = NULL},
the global environment can be reached by evaluating \code{this} in the global scope,
for example: \code{ct$eval("Object.keys(this)")}.
}
\section{V8 Context Methods}{

\Sexpr[results=rd, stage=build, echo=FALSE]{V8:::generate_rd()}

The \code{ct$eval} method evaluates a string of JavaScript code in the same way
as \code{eval} in JavaScript. By default \code{eval()} returns a string with
console output; but when the \code{serialize} parameter is set to \code{TRUE} it
serializes the JavaScript return object to a JSON string or a raw buffer.

The \code{ct$get}, \code{ct$assign} and \code{ct$call} functions automatically
convert arguments and return value between R and JavaScript (using JSON). To pass
literal JavaScript arguments that should not be converted to JSON, wrap them in
\code{JS()}, see examples.

If a call to \code{ct$eval()},\code{ct$get()}, or \code{ct$call()} returns a JavaScript promise,
you can set \code{await = TRUE} to wait for the promise to be resolved. It will then
return the result of the promise, or an error in case the promise is rejected.

The \code{ct$validate} function is used to test
if a piece of code is valid JavaScript syntax within the context, and always
returns TRUE or FALSE.

In an interactive R session you can use \code{ct$console()} to switch to an
interactive JavaScript console. Here you can use \code{console.log} to print
objects, and there is some support for JS tab-completion. This is mostly for
testing and debugging, it may not work perfectly in every IDE or R-frontend.
}

\section{Data Interchange}{

JSON is used for data interchange between R and JavaScript. Therefore you can
(and should) only exchange data types that have a sensible JSON representation.
One exception is raw vectors which are converted to/from Uint8Array buffers, see
below. All other arguments and objects are automatically converted according to the mapping
described in \href{https://arxiv.org/abs/1403.2805}{Ooms (2014)}, and implemented
by the jsonlite package in \code{\link[jsonlite:fromJSON]{jsonlite::fromJSON()}} and \code{\link[jsonlite:fromJSON]{jsonlite::toJSON()}}.

As for version 3.0 of this R package, Raw vectors are converted to \code{Uint8Array}
typed arrays, and vice versa. This makes it possible to efficiently copy large chunks
binary data between R and JavaScript, which is useful for running \link{wasm}
or emscripten.
}

\section{Note about Linux and Legacy V8 engines}{

This R package can be compiled against modern (V8 version 6+) libv8 API, or the legacy
libv8 API (V8 version 3.15 and below). You can check \code{V8::engine_info()} to see the version
that is running. The legacy version does not support modern JS (ES6) or WASM, but it is
still the default on older versions of Ubuntu and CentOS. The latest versions of all major
Linux distributions now provide a modern version of V8. For Ubuntu 16.04 and 18.04
we provide backports of libv8 (via libnode-dev), see the
\href{https://github.com/jeroen/v8#backports-for-xenial-and-bionic}{readme} for details.
}

\examples{
# Create a new context
ctx <- v8();

# Evaluate some code
ctx$eval("var foo = 123")
ctx$eval("var bar = 456")
ctx$eval("foo+bar")

# Functions and closures
ctx$eval("JSON.stringify({x:Math.random()})")
ctx$eval("(function(x){return x+1;})(123)")

# Objects (via JSON only)
ctx$assign("mydata", mtcars)
ctx$get("mydata")
outlist <- ctx$get("mydata", simplifyVector = FALSE)
outlist[1]

# Assign JavaScript
ctx$assign("foo", JS("function(x){return x*x}"))
ctx$assign("bar", JS("foo(9)"))
ctx$get("bar")

# Validate script without evaluating
ctx$validate("function foo(x){2*x}") #TRUE
ctx$validate("foo = function(x){2*x}") #TRUE
ctx$validate("function(x){2*x}") #FALSE

# Use a JavaScript library
ctx$source("https://cdnjs.cloudflare.com/ajax/libs/underscore.js/1.13.6/underscore-min.js")
ctx$call("_.filter", mtcars, JS("function(x){return x.mpg < 15}"))

# Example from underscore manual
ctx$eval("_.templateSettings = {interpolate: /\\\\{\\\\{(.+?)\\\\}\\\\}/g}")
ctx$eval("var template = _.template('Hello {{ name }}!')")
ctx$call("template", list(name = "Mustache"))

# Call anonymous function
ctx$call("function(x, y){return x * y}", 123, 3)

\dontrun{
#CoffeeScript
ct2 <- v8()
ct2$source("http://coffeescript.org/v1/browser-compiler/coffee-script.js")
jscode <- ct2$call("CoffeeScript.compile", "square = (x) -> x * x", list(bare = TRUE))
ct2$eval(jscode)
ct2$call("square", 9)

# Interactive console
ct3 <- v8()
ct3$console()
# //this is JavaScript
# var test = [1,2,3]
# JSON.stringify(test)
# exit
}

}
\references{
A Mapping Between JSON Data and R Objects (Ooms, 2014): \url{https://arxiv.org/abs/1403.2805}
}
