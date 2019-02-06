# V8

##### *Embedded JavaScript Engine for R*

[![Build Status](https://travis-ci.org/jeroen/V8.svg?branch=master)](https://travis-ci.org/jeroen/V8)
[![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/github/jeroen/V8?branch=master&svg=true)](https://ci.appveyor.com/project/jeroen/V8)
[![Coverage Status](https://codecov.io/github/jeroen/V8/coverage.svg?branch=master)](https://codecov.io/github/jeroen/V8?branch=master)
[![CRAN_Status_Badge](http://www.r-pkg.org/badges/version/V8)](http://cran.r-project.org/package=V8)
[![CRAN RStudio mirror downloads](http://cranlogs.r-pkg.org/badges/V8)](http://cran.r-project.org/web/packages/V8/index.html)

> An R interface to Google's open source JavaScript engine.
  V8 is written in C++ and implements ECMAScript as specified in ECMA-262,
  5th edition. In addition, this package implements typed arrays as
  specified in ECMA 6 used for high-performance computing and libraries
  compiled with 'emscripten'.

## Documentation

About the R package:

 - Vignette: [Introduction to V8 for R](https://cran.r-project.org/web/packages/V8/vignettes/v8_intro.html)
 - Vignette: [Using NPM packages in V8 with browserify](https://cran.r-project.org/web/packages/V8/vignettes/npm.html)

## Hello World

```r
# Create a new context
library(V8)
ctx <- v8()

# Evaluate some code
ctx$eval("var foo = 123")
ctx$eval("var bar = 456")
ctx$eval("foo+bar")

# Assign / get objects
ctx$assign("foo", JS("function(x){return x*x}"))
ctx$assign("bar", JS("foo(9)"))
ctx$get("bar")
```

Call functions from JavaScript libraries

```r
ctx <- V8::v8()
ctx$source("http://coffeescript.org/extras/coffee-script.js")
jscode <- ctx$call("CoffeeScript.compile", "square = (x) -> x * x", list(bare = TRUE))
ctx$eval(jscode)
ctx$call("square", 9)
```

## Installation

Binary packages for __OS-X__ or __Windows__ can be installed directly from CRAN:

```r
install.packages("V8")
```

Installation from source on Linux requires [`libv8`](https://developers.google.com/v8/intro) 3.14 or 3.15 (no newer!). On __Debian__ or __Ubuntu__ use [libv8-dev](https://packages.ubuntu.com/bionic/libv8-dev) OR [linode-dev](https://packages.debian.org/testing/libnode-dev):

```
# Most versions of Debian and Ubuntu
sudo apt-get install -y libv8-dev

# Alternatively for Debian Buster
sudo apt-get install libnode-dev
```

On __Fedora__ we need [v8-devel](https://apps.fedoraproject.org/packages/v8):

```
sudo yum install v8-devel
````

On __CentOS / RHEL__ we install [v8-devel](https://apps.fedoraproject.org/packages/v8-devel) via EPEL:

```
sudo yum install epel-release
sudo yum install v8-devel
```

On __OS-X__ use [v8](https://github.com/Homebrew/homebrew-core/blob/master/Formula/v8) from Homebrew:

```
brew install v8
```

On other systems you might need to install libv8 from source.

