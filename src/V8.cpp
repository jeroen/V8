/*
 R bindings to V8. Copyright 2014, Jeroen Ooms.

 Notes:
 - Rcpp completely ignores character encodings, so need to convert manually.
 - Implementation of Typed Arrays taken from node v0.6.21

 V8 source parsing:
 - http://stackoverflow.com/questions/16613828/how-to-convert-stdstring-to-v8s-localstring

 Xptr examples:
 - https://github.com/RcppCore/Rcpp/blob/master/inst/unitTests/cpp/XPtr.cpp
 - http://romainfrancois.blog.free.fr/index.php?post/2010/01/08/External-pointers-with-Rcpp
*/

#include <v8.h>
#include <Rcpp.h>
#include "v8_typed_array.h"
#include "v8_json.h"
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

/* console.log */
static Handle<Value> ConsoleLog(const Arguments& args) {
  for (int i=0; i < args.Length(); i++) {
    String::AsciiValue str(args[i]->ToString());
    Rprintf("%s", *str);
  }
  Rprintf("\n");
  return v8::Undefined();
}

/* console.warn */
static Handle<Value> ConsoleWarn(const Arguments& args) {
  for (int i=0; i < args.Length(); i++) {
    String::AsciiValue str(args[i]->ToString());
    Rf_warningcall_immediate(R_NilValue, *str);
  }
  return v8::Undefined();
}

/* console.error */
static Handle<Value> ConsoleError(const Arguments& args) {
  if(args.Length()){
    return v8::ThrowException(args[0]);
  }
  return v8::Undefined();
}

static Handle<Value> r_callback(std::string fun, const Arguments& args) {
  try {
    Rcpp::Function r_call = Rcpp::Environment::namespace_env("V8")[fun];
    String::Utf8Value arg0(args[0]);
    Rcpp::String fun(*arg0);
    Rcpp::CharacterVector out;
    if(args[1]->IsUndefined()){
      out = r_call(fun);
    } else {
      String::Utf8Value arg1(json_stringify(args[1]));
      Rcpp::String json(*arg1);
      out = r_call(fun, json);
    }
    return json_parse(String::New(std::string(out[0]).c_str()));
  } catch( const std::exception& e ) {
    return v8::ThrowException(String::New(e.what()));
  }
}

/* console.r.call() function */
static Handle<Value> console_r_call(const Arguments& args) {
  return r_callback("r_call", args);
}

/* console.r.get() function */
static Handle<Value> console_r_get(const Arguments& args) {
  return r_callback("r_get", args);
}

/* console.r.eval() function */
static Handle<Value> console_r_eval(const Arguments& args) {
  return r_callback("r_eval", args);
}

// [[Rcpp::export]]
ctxptr make_context(bool set_console){
  /* setup console.log */
  HandleScope handle_scope;
  Handle<ObjectTemplate> global = ObjectTemplate::New();
  if(set_console){
    Handle<ObjectTemplate> console = ObjectTemplate::New();
    global->Set(String::NewSymbol("console"), console);
    console->Set(String::NewSymbol("log"), FunctionTemplate::New(ConsoleLog));
    console->Set(String::NewSymbol("warn"), FunctionTemplate::New(ConsoleWarn));
    console->Set(String::NewSymbol("error"), FunctionTemplate::New(ConsoleError));

    /* emscripted assumes a print function */
    global->Set(String::NewSymbol("print"), FunctionTemplate::New(ConsoleLog));

    /* R callback interface */
    Handle<ObjectTemplate> console_r = ObjectTemplate::New();
    console->Set(String::NewSymbol("r"), console_r);
    console_r->Set(String::NewSymbol("call"), FunctionTemplate::New(console_r_call));
    console_r->Set(String::NewSymbol("get"), FunctionTemplate::New(console_r_get));
    console_r->Set(String::NewSymbol("eval"), FunctionTemplate::New(console_r_eval));

  }
  /* initialize the context */
  lstail->context = Context::New(NULL, global);
  ctxptr ptr(&(lstail->context));
  lstail->next = new node;
  lstail = lstail->next;
  return(ptr);
}

// [[Rcpp::export]]
bool context_enable_typed_arrays( Rcpp::XPtr< v8::Persistent<v8::Context> > ctx ){
  HandleScope handle_scope;
  Context::Scope context_scope(*ctx);
  v8_typed_array::AttachBindings((*ctx)->Global());
  return true;
}

// [[Rcpp::export]]
std::string version(){
  return v8::V8::GetVersion();
}

// [[Rcpp::export]]
std::string context_eval(std::string src, Rcpp::XPtr< v8::Persistent<v8::Context> > ctx){
  // Test if context still exists
  if(!ctx)
    throw std::runtime_error("Context has been disposed.");

  // Create a scope
  HandleScope handle_scope;
  Context::Scope context_scope(*ctx);

  // Compile source code
  TryCatch trycatch;
  Handle<Script> script = compile_source(src);
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
bool context_validate(std::string src, Rcpp::XPtr< v8::Persistent<v8::Context> > ctx) {

  // Test if context still exists
  if(!ctx)
    throw std::runtime_error("Context has been disposed.");

  // Create scope
  HandleScope handle_scope;
  Context::Scope context_scope(*ctx);

  // Try to compile, catch errors
  TryCatch trycatch;
  Handle<Script> script = compile_source(src);
  return !script.IsEmpty();
}

// [[Rcpp::export]]
bool context_null(Rcpp::XPtr< v8::Persistent<v8::Context> > ctx) {
  // Test if context still exists
  return(!ctx);
}

/*
Rcpp does not deal well with UTF8 on windows.
Workaround below (hopefully temporary)
*/

// [[Rcpp::export]]
SEXP context_eval_safe(SEXP src, Rcpp::XPtr< v8::Persistent<v8::Context> > ctx){
  std::string str(Rf_translateCharUTF8(Rf_asChar(src)));
  std::string out = context_eval(str, ctx);
  SEXP res = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_STRING_ELT(res, 0, Rf_mkCharCE(out.c_str(), CE_UTF8));
  UNPROTECT(1);
  return res;
}

// [[Rcpp::export]]
bool context_validate_safe(SEXP src, Rcpp::XPtr< v8::Persistent<v8::Context> > ctx){
  std::string str(Rf_translateCharUTF8(Rf_asChar(src)));
  return context_validate(str, ctx);
}

/*
Method below does not work because null bytes get lost when converting to strings.
Should use ArrayBuffer types instead, e.g. Uint8Array.
*/

// [[Rcpp::export]]
bool context_assign_bin(std::string name, Rcpp::RawVector data, Rcpp::XPtr< v8::Persistent<v8::Context> > ctx) {

  // Test if context still exists
  if(!ctx)
    throw std::runtime_error("Context has been disposed.");

  // Create scope
  HandleScope handle_scope;
  Context::Scope context_scope(*ctx);
  v8::Handle<v8::Object> global = (*ctx)->Global();

  // Currently converts raw vectors to strings. Better would be ArrayBuffer (Uint8Array specifically)
  Local<v8::String> mystring = v8::String::New((const char*) RAW(data), data.length());
  global->Set(String::NewSymbol(name.c_str()), mystring);
  return true;
}

// [[Rcpp::export]]
Rcpp::RawVector context_get_bin(std::string name, Rcpp::XPtr< v8::Persistent<v8::Context> > ctx) {
  // Test if context still exists
  if(!ctx)
    throw std::runtime_error("Context has been disposed.");

  // Create scope
  HandleScope handle_scope;
  Context::Scope context_scope(*ctx);
  v8::Handle<v8::Object> global = (*ctx)->Global();

  //find the string
  Local<v8::String> mystring = global->Get(String::NewSymbol(name.c_str()))->ToString();
  Rcpp::RawVector res(mystring->Length());
  mystring->WriteAscii((char*) res.begin());
  return res;
}
