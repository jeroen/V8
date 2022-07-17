#include <libplatform/libplatform.h>
#include <v8.h>
using namespace v8;

int main(){
  std::unique_ptr<Platform> platform = platform::NewDefaultPlatform();
  V8::InitializePlatform(platform.get());
  V8::Initialize();
}
