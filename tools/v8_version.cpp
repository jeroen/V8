// #include <v8.h>
#include <v8-version.h>
#include <iostream>

int main(){
  int v8_maj = 0, v8_min = 0, v8_bul = 0;

#if defined(V8_MAJOR_VERSION)
  v8_maj = V8_MAJOR_VERSION;
#endif

#if defined(V8_MINOR_VERSION)
  v8_min = V8_MINOR_VERSION;
#endif

#if defined(V8_BUILD_NUMBER)
  v8_bul = V8_BUILD_NUMBER;
#endif

  int v8ver = v8_maj * 100000 + v8_min * 1000 + v8_bul;

  std::cout<< v8ver << std::endl;

  return 0;
}
