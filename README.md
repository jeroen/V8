Embedded JavaScript Engine
--------------------------

[![Build Status](https://travis-ci.org/jeroenooms/V8.svg?branch=master)](https://travis-ci.org/jeroenooms/V8)

> An R interface to Google's open source JavaScript engine.
  V8 is written in C++ and implements ECMAScript as specified in ECMA-262, 5th edition.
  In addition, this package implements typed arrays as specified in ECMA 6 in order to
  support high-performance computing and libraries compiled with 'emscripten'

Installation
------------

This package depends on libv8 around 3.14 or 3.16, which is the version included with most package managers:

 - Debian: [libv8-3.14-dev](https://packages.debian.org/sid/libv8-3.14-dev)
 - Fedora/EPEL: [v8-devel](https://apps.fedoraproject.org/packages/v8-devel)
 - Arch: [v8-3.14](https://aur.archlinux.org/packages/v8-3.14/)
 - OSX: [v8-315](https://github.com/Homebrew/homebrew-versions/blob/master/v8-315.rb) (`brew tap homebrew/versions; brewbrew install v8-315`)

Unfortunately the developers of libv8 do not care about backward compatibility and therefore recent branches of V8 (such as 3.22 or 4.xx) will not work. For this reason most distributions are unlikely to upgrade any time soon because it would break everything downstream (node, mongodb, etc).

The only distribution I am aware of that does not include a compatible version of v8 is OpenSUSE. So on this system you'll have to pull an older version from [rpmfind](http://www.rpmfind.net/linux/rpm2html/search.php?query=v8&system=opensuse) or [ruby gem](https://rubygems.org/gems/libv8/versions/3.16.14.7) or something.
