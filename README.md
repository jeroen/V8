# V8

> Embedded JavaScript Engine for R

[![Build Status](https://travis-ci.org/jeroen/V8.svg?branch=master)](https://travis-ci.org/jeroen/V8)
[![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/github/jeroen/V8?branch=master&svg=true)](https://ci.appveyor.com/project/jeroen/V8)
[![Coverage Status](https://codecov.io/github/jeroen/V8/coverage.svg?branch=master)](https://codecov.io/github/jeroen/V8?branch=master)
[![CRAN_Status_Badge](http://www.r-pkg.org/badges/version/V8)](http://cran.r-project.org/package=V8)
[![CRAN RStudio mirror downloads](http://cranlogs.r-pkg.org/badges/V8)](http://cran.r-project.org/web/packages/V8/index.html)

An R interface to Google's open source JavaScript engine. This 
package can now be compiled either with V8 version 6 (LTS) from nodejs
or with the legacy 3.14/3.15 version of V8.

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
ctx$source("https://cdnjs.cloudflare.com/ajax/libs/coffee-script/1.4.0/coffee-script.min.js")
jscode <- ctx$call("CoffeeScript.compile", "square = (x) -> x * x", list(bare = TRUE))
ctx$eval(jscode)
ctx$call("square", 9)
```

## Installation

Binary packages for __OS-X__ or __Windows__ can be installed directly from CRAN:

```r
install.packages("V8")
```

Installation from source on Linux requires [`libv8`](https://developers.google.com/v8/intro). On __Debian 10 (Buster)__ and up or __Ubuntu 19.04 (Disco)__ and up, use [linode-dev](https://packages.debian.org/testing/libnode-dev):

```sh
# Recent versions of Debian / Ubuntu
sudo apt-get install libnode-dev
```

On older versions of Debian/Ubuntu you need [libv8-dev](https://packages.ubuntu.com/bionic/libv8-dev):

```sh
# Older versions of Debian and Ubuntu
sudo apt-get install -y libv8-dev
```

On __Fedora__ we need [v8-devel](https://apps.fedoraproject.org/packages/v8):

```sh
sudo yum install v8-devel
````

On __CentOS / RHEL__ we install [v8-devel](https://apps.fedoraproject.org/packages/v8-devel) via EPEL:

```sh
sudo yum install epel-release
sudo yum install v8-devel
```

On __OS-X__ use [v8](https://github.com/Homebrew/homebrew-core/blob/master/Formula/v8) from Homebrew:

```sh
brew install v8
```

On other systems you might need to install libv8 from source.

