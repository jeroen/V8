IS_MUSL=$(ldd --version 2>&1 | grep musl)
if [ $? -eq 0 ] && [ "$IS_MUSL" ]; then
  URL="http://jeroen.github.io/V8/v8-8.3.110.13-musl.tar.gz"
else
  IS_GCC4=$($CXX --version | grep -P '^g++.*[^\d.]4(\.\d){2}')
  if [ $? -eq 0 ] && [ "$IS_GCC4" ]; then
    URL="http://jeroen.github.io/V8/v8-6.8.275.32-gcc48.tar.gz"
  else
    URL="http://jeroen.github.io/V8/v8-8.3.110.13-linux.tar.gz"
  fi
fi
${R_HOME}/bin/R -q -e "curl::curl_download('$URL','libv8.tar.gz',quiet=FALSE)"
tar xzf libv8.tar.gz
PKG_CFLAGS="-I${PWD}/v8/include"
PKG_LIBS="-L${PWD}/v8/lib -lv8_monolith"
