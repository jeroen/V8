# V8

> Embedded JavaScript Engine for R

[![Build Status](https://travis-ci.org/jeroen/V8.svg?branch=master)](https://travis-ci.org/jeroen/V8)
[![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/github/jeroen/V8?branch=master&svg=true)](https://ci.appveyor.com/project/jeroen/V8)
[![CRAN_Status_Badge](http://www.r-pkg.org/badges/version/V8)](http://cran.r-project.org/package=V8)
[![CRAN RStudio mirror downloads](http://cranlogs.r-pkg.org/badges/V8)](http://cran.r-project.org/web/packages/V8/index.html)

An R interface to Google's open source JavaScript engine. This 
package can now be compiled either with V8 version 6+ (LTS) from nodejs
or with the legacy 3.14/3.15 version of V8.

## Getting started

About the R package:

 - Vignette: [Introduction to V8 for R](https://cran.r-project.org/web/packages/V8/vignettes/v8_intro.html)
 - Vignette: [Using NPM packages in V8 with browserify](https://cran.r-project.org/web/packages/V8/vignettes/npm.html)
 
Binary packages for __OS-X__ or __Windows__ can be installed directly from CRAN:

```r
install.packages("V8")
```

On Linux you need a suitable libv8 installation.


### Linux: Static libv8

__NEW:__ As of V8 3.4 there is a new option for Linux users to automatically download a suitable static build of libv8 during package installation. To use this in R simply set an environment variable `DOWNLOAD_STATIC_LIBV8=1` for example:

```r
Sys.setenv(DOWNLOAD_STATIC_LIBV8 = 1)
install.packages("V8")
```

This way, you can install the V8 package on any Linux system, without separate system requirements. Alternatively, you can still install libv8 from your distribution as described below.

### Arch

Arch users are advised to install the [`v8-r`](https://aur.archlinux.org/packages/v8-r/) package, which has been configured to work well with R. Installation can done through your preferred AUR helper such as yay, Trizen, etc. However, since V8 contains a large codebase and (re-)compilation takes a while, users may prefer to build and update it manually. For example,

```sh
## Arch
cd /tmp
yay -G v8-r   
cd v8-r
makepkg -si
```


### Debian / Ubuntu 

Installation from source on Linux requires [`libv8`](https://developers.google.com/v8/intro). On Ubuntu / Debian you need to install either [libv8-dev](https://packages.ubuntu.com/bionic/libv8-dev), or [libnode-dev](https://packages.ubuntu.com/eoan/libnode-dev). On the latest systems, `libv8-dev` is actually an alias for `libnode-dev` so they are the same:

```sh
# Debian and Ubuntu
sudo apt-get install -y libv8-dev
```

### Backports for Xenial and Bionic

Ubuntu versions before 19.04 ship with a rather old V8 engine in libv8-dev. The R package can be compiled against this, but the engine only supports ES5, so some "modern" JavaScript syntax may not work. A lot of JS libraries these days require this.

A recent version of the V8 engine is available in `libnode-dev` from our the [cran/v8](https://launchpad.net/~cran/+archive/ubuntu/v8) PPA:

```sh
# Ubuntu Xenial (16.04) and Bionic (18.04) only
sudo add-apt-repository ppa:cran/v8
sudo apt-get update
sudo apt-get install libnode-dev
```

After installing `libnode-dev` you need to reinstall the R package, and you should be good to go.

### Travis CI

The above PPA is enabled by default in Travis for R 3.5 and up. You can use the following configuration to check R packges that depend on V8:

```yaml
dist: xenial
addons:
  apt:
    packages: libnode-dev
```

You should delete the travis repository package cache if you switch from `libv8-dev` to `libnode-dev`.

The [.travis.yml](https://github.com/jeroen/V8/blob/master/.travis.yml) in the V8 repository shows other possible configurations.

### Fedora / Redhat

On __Fedora__ we need [v8-devel](https://apps.fedoraproject.org/packages/v8):

```sh
sudo yum install v8-devel
````

On __CentOS / RHEL__ we install [v8-devel](https://apps.fedoraproject.org/packages/v8-devel) via EPEL:

```sh
sudo yum install epel-release
sudo yum install v8-devel
```

Not that on __CentOS / RHEL 8__, you first need to enable the `node:13` module repository:

```sh
# Needed on EPEL 8 only
yum install epel-release 
yum module enable nodejs:13
yum install v8-devel
```

### Homebrew

On __OS-X__ use [v8](https://github.com/Homebrew/homebrew-core/blob/master/Formula/v8) from Homebrew:

```sh
brew install v8
```

On other systems you might need to install libv8 from source.


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
