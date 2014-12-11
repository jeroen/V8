/*
 R bindings to V8. Copyright 2014, Jeroen Ooms.

 Note: Rcpp completely ignores character encodings, so need to convert manually.

 V8 source parsing:
 http://stackoverflow.com/questions/16613828/how-to-convert-stdstring-to-v8s-localstring

 Xptr examples:
 - https://github.com/RcppCore/Rcpp/blob/master/inst/unitTests/cpp/XPtr.cpp
 - http://romainfrancois.blog.free.fr/index.php?post/2010/01/08/External-pointers-with-Rcpp
*/

#include <v8.h>
#include <Rcpp.h>
using namespace v8;

/* a linked list keeping track of running contexts */
struct node {
  Persistent<Context> context;
  node *next;
};

node ctxlist = *(new node);
node *lstail = &ctxlist;

void ctx_finalizer( Persistent<Context>* context ){
  if(context){
    context->Dispose();
  }
}

typedef Rcpp::XPtr<Persistent<Context>, Rcpp::PreserveStorage, ctx_finalizer> ctxptr;

/* Helper fun that compiles JavaScript source code */
Handle<Script> compile_source( std::string src ){
  Handle<String> source = String::New(src.c_str());
  Handle<Script> script = Script::Compile(source);
  return script;
}

// [[Rcpp::export]]
ctxptr make_context(){
  lstail->context = Context::New();
  ctxptr ptr(&(lstail->context));
  lstail->next = new node;
  lstail = lstail->next;
  return(ptr);
}

// [[Rcpp::export]]
std::string context_eval(SEXP src, Rcpp::XPtr< v8::Persistent<v8::Context> > ctx){
  // Test if context still exists
  if(!ctx)
    throw std::runtime_error("Context has been disposed.");

  // Create a scope
  HandleScope handle_scope;
  Context::Scope context_scope(*ctx);

  // Compile source code
  TryCatch trycatch;
  Handle<Script> script = compile_source(Rf_translateCharUTF8(Rf_asChar(src)));
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

  // Convert result to UTF8.
  String::Utf8Value utf8(result);
  return *utf8;
}

// [[Rcpp::export]]
bool context_validate(SEXP src, Rcpp::XPtr< v8::Persistent<v8::Context> > ctx) {

  // Test if context still exists
  if(!ctx)
    throw std::runtime_error("Context has been disposed.");

  // Create scope
  HandleScope handle_scope;
  Context::Scope context_scope(*ctx);

  // Try to compile, catch errors
  TryCatch trycatch;
  Handle<Script> script = compile_source(Rf_translateCharUTF8(Rf_asChar(src)));
  return !script.IsEmpty();
}

// [[Rcpp::export]]
bool context_null(Rcpp::XPtr< v8::Persistent<v8::Context> > ctx) {
  // Test if context still exists
  return(!ctx);
}
