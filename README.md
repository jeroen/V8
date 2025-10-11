# V8

> Embedded JavaScript and WebAssembly Engine for R

[![CRAN_Status_Badge](https://www.r-pkg.org/badges/version/V8)](https://cran.r-project.org/package=V8)
[![CRAN RStudio mirror downloads](https://cranlogs.r-pkg.org/badges/V8)](https://cran.r-project.org/web/packages/V8/index.html)

An R interface to V8: Google's open source JavaScript and WebAssembly 
engine. This package can be compiled either with V8 version 6 and up or NodeJS
when built as a shared library.

## Getting started

About the R package:

 - Vignette: [Introduction to V8 for R](https://cran.r-project.org/web/packages/V8/vignettes/v8_intro.html)
 - Vignette: [Using NPM packages in V8 with browserify](https://cran.r-project.org/web/packages/V8/vignettes/npm.html)
 
To see some quick examples in R run:

```r
library(V8)
example(v8)
```

## Installation
 
Binary packages for __OS-X__ or __Windows__ can be installed directly from CRAN:

```r
install.packages("V8")
```

On Linux you need a suitable libv8 installation, see below.

### Linux: Static libv8

On amd64/arm64 Linux/MacOS platforms it is possible (and recommended) to automatically download a suitable static build of libv8 during package installation. This is enabled by default on Ubuntu, RHEL, OpenSuse, Alpine, and MacOS. For other systems you can opt-in by setting an environment variable `DOWNLOAD_STATIC_LIBV8` during installation, for example:

```r
Sys.setenv(DOWNLOAD_STATIC_LIBV8 = 1)
install.packages("V8")
```

This way, you can install the R package on any x64 Linux system, without external system requirements. Alternatively, it is also still possible to install libv8 from your distribution as described below. This may be needed for servers running other architectures, or when building the R package without internet access.

### Debian / Ubuntu

Installation from source on Linux requires [`libv8`](https://v8.dev/). On Ubuntu / Debian you need to install either [libv8-dev](https://packages.ubuntu.com/bionic/libv8-dev), or [libnode-dev](https://packages.ubuntu.com/eoan/libnode-dev). On the latest systems, `libv8-dev` is actually an alias for `libnode-dev` so they are the same:

```sh
# Debian and Ubuntu
sudo apt-get install -y libv8-dev
```

### Fedora / Redhat

On __Fedora__ we need v8-devel (which Fedora provides as part of [nodejs](https://src.fedoraproject.org/rpms/nodejs)):

```sh
sudo yum install v8-devel
````

On __CentOS 7 / RHEL 7__ we install first need to enable EPEL:

```sh
sudo yum install epel-release
sudo yum install v8-devel
```

On __RockyLinux 8 / RHEL 8__, `v8-devel` can be installed from the `nodejs:16-epel` module repository:

```sh
yum --refresh --enablerepo=epel-testing-modular install @nodejs:16-epel/minimal v8-devel
```

### Arch Linux

Arch users are advised to install the [`v8-r`](https://aur.archlinux.org/packages/v8-r/) package, which has been configured to work well with R. Installation can done through your preferred AUR helper such as yay, Trizen, etc. However, since V8 contains a large codebase and (re-)compilation takes a while, users may prefer to build and update it manually. For example,

```sh
## Arch
cd /tmp
yay -G v8-r   
cd v8-r
makepkg -si
```


### Homebrew

On __OS-X__ use [v8](https://github.com/Homebrew/homebrew-core/blob/master/Formula/v8) from Homebrew:

```sh
brew install v8
```

But it is much easier to set `DOWNLOAD_STATIC_LIBV8` instead.


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

