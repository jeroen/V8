// [[Rcpp::depends(BH)]]
#include <v8.h>
#include <Rcpp.h>
#include <string>
#include <stdexcept>
#include <boost/algorithm/string/join.hpp>
using namespace v8;

/*
 V8 source parse:
 http://stackoverflow.com/questions/16613828/how-to-convert-stdstring-to-v8s-localstring

 Xptr examples:
 - https://github.com/RcppCore/Rcpp/blob/master/inst/unitTests/cpp/XPtr.cpp
 - http://romainfrancois.blog.free.fr/index.php?post/2010/01/08/External-pointers-with-Rcpp
*/

Handle<Script> compile_source( std::vector< std::string > code ){
  std::string src = boost::algorithm::join(code, "\n");
  Handle<String> source = String::New(src.c_str());
  Handle<Script> script = Script::Compile(source);
  return script;
}

std::string eval_in_context(std::vector< std::string > code, Persistent<Context> context) {

  // Create a scope
  HandleScope handle_scope;
  Context::Scope context_scope(context);

  // Compile source code
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

  // Convert the result to UTF8.
  String::Utf8Value utf8(result);
  return *utf8;
}

std::string jseval_string(std::vector< std::string > code) {
  // Evaluate in temporary context.
  Persistent<Context> context = Context::New();
  std::string out = eval_in_context(code, context);
  context.Dispose();
  return out;
}

bool jsvalidate_string(std::vector< std::string > code) {
  // Evaluate in temporary context.
  HandleScope handle_scope;
  Persistent<Context> context = Context::New();
  Context::Scope context_scope(context);

  // Try to compile, catching errors
  TryCatch trycatch;
  Handle<Script> script = compile_source(code);
  return !script.IsEmpty();
}
