#include <v8.h>
#include <string>
#include <stdexcept>
using namespace v8;

/*
 http://stackoverflow.com/questions/16613828/how-to-convert-stdstring-to-v8s-localstring
*/

Handle<Script> compile_source(std::string code){
  Handle<String> source = String::New(code.c_str());
  Handle<Script> script = Script::Compile(source);
  return script;
}

std::string eval_in_context(std::string code, Persistent<Context> context) {

  TryCatch trycatch;
  Handle<Script> script = compile_source(code);
  if(script.IsEmpty()) {
    Local<Value> exception = trycatch.Exception();
    String::AsciiValue exception_str(exception);
    throw std::invalid_argument(*exception_str);
  }

  // Run the script to get the result.
  Handle<Value> result = script->Run();
  if(result.IsEmpty()){
    Local<Value> exception = trycatch.Exception();
    String::AsciiValue exception_str(exception);
    throw std::runtime_error(*exception_str);
  }

  // Convert the result to an ASCII string and print it.
  String::Utf8Value utf8(result);
  return *utf8;
}

std::string jseval_string(std::string code) {
  // Evaluate in temporary context.
  HandleScope handle_scope;
  Persistent<Context> context = Context::New();
  Context::Scope context_scope(context);
  std::string out = eval_in_context(code, context);
  context.Dispose();
  return out;
}

bool jsvalidate_string(std::string code) {
  // Evaluate in temporary context.
  HandleScope handle_scope;
  Persistent<Context> context = Context::New();
  Context::Scope context_scope(context);

  // Try to compile, catching errors
  TryCatch trycatch;
  Handle<Script> script = compile_source(code);
  return !script.IsEmpty();
}
