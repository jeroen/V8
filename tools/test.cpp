#ifndef __EMSCRIPTEN__
#include <libplatform/libplatform.h>
#include <v8.h>
using namespace v8;

int main(){
#if (V8_MAJOR_VERSION * 100 + V8_MINOR_VERSION) >= 704
  std::unique_ptr<Platform> platform = platform::NewDefaultPlatform();
  V8::InitializePlatform(platform.get());
#endif
  V8::Initialize();
}
#endif // __EMSCRIPTEN__
