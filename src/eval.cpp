#include <v8.h>
#include <string>
#include <stdexcept>
using namespace std;
using namespace v8;

/*
 http://stackoverflow.com/questions/16613828/how-to-convert-stdstring-to-v8s-localstring
*/

std::string jseval_string(std::string code) {
  // Create a stack-allocated handle scope.
  HandleScope handle_scope;

  // Create a new context.
  Persistent<Context> context = Context::New();

  // Enter the created context for compiling and
  // running the hello world script.
  Context::Scope context_scope(context);

  // Create a string containing the JavaScript source code.
  Handle<String> source = String::New(code.c_str());

  // Compile the source code.
  TryCatch trycatch;
  Handle<Script> script = Script::Compile(source);
  if(script.IsEmpty()) {
    Local<Value> exception = trycatch.Exception();
    String::AsciiValue exception_str(exception);
    context.Dispose();
    throw std::invalid_argument(*exception_str);
  }

  // Run the script to get the result.
  Handle<Value> result = script->Run();
  if(result.IsEmpty()){
    Local<Value> exception = trycatch.Exception();
    String::AsciiValue exception_str(exception);
    context.Dispose();
    throw std::runtime_error(*exception_str);
  }

  // Dispose the persistent context.
  context.Dispose();

  // Convert the result to an ASCII string and print it.
  String::Utf8Value utf8(result);
  return *utf8;
}

bool jsvalidate_string(std::string code) {
  // Create a stack-allocated handle scope.
  HandleScope handle_scope;

  // Create a new context.
  Persistent<Context> context = Context::New();

  // Enter the created context for compiling and
  // running the hello world script.
  Context::Scope context_scope(context);

  // Create a string containing the JavaScript source code.
  Handle<String> source = String::New(code.c_str());

  // Compile the source code.
  TryCatch trycatch;
  Handle<Script> script = Script::Compile(source);
  context.Dispose();

  //return success
  return !script.IsEmpty();
}
