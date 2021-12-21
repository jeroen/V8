IS_MUSL=$(ldd --version 2>&1 | grep musl)
if [ $? -eq 0 ] && [ "$IS_MUSL" ]; then
  URL="https://github.com/jeroen/V8/releases/download/v3.6.0/v8-9.6.180.12-alpine.tar.gz"
elif [ "$(uname -m | grep 'ar[mc]')" ]; then
  URL="https://github.com/jeroen/V8/releases/download/v3.6.0/v8-9.6.180.12-arm64.tar.gz"
else
  IS_GCC4=$($CXX --version | grep -P '^g++.*[^\d.]4(\.\d){2}')
  if [ $? -eq 0 ] && [ "$IS_GCC4" ]; then
    URL="https://github.com/jeroen/V8/releases/download/v3.6.0/v8-6.8.275.32-gcc-4.8.tar.gz"
  else
    URL="https://github.com/jeroen/V8/releases/download/v3.6.0/v8-9.6.180.12-amd64.tar.gz"
  fi
fi
if [ ! -f ".deps/lib/libv8_monolith.a" ]; then
  ${R_HOME}/bin/R -q -e "curl::curl_download('$URL','libv8.tar.gz',quiet=FALSE)"
  tar xzf libv8.tar.gz
  rm -f libv8.tar.gz
  mv v8 .deps
fi
PKG_CFLAGS="-I${PWD}/.deps/include"
PKG_LIBS="-L${PWD}/.deps/lib -lv8_monolith"
