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

// [[Rcpp::export]]
ctxptr make_context(){
  /* setup console.log */
  HandleScope handle_scope;
  Handle<ObjectTemplate> global = ObjectTemplate::New();
  Handle<ObjectTemplate> console = ObjectTemplate::New();
  global->Set(String::NewSymbol("console"), console);
  console->Set(String::NewSymbol("log"), FunctionTemplate::New(ConsoleLog));
  console->Set(String::NewSymbol("warn"), FunctionTemplate::New(ConsoleWarn));
  console->Set(String::NewSymbol("error"), FunctionTemplate::New(ConsoleError));

  /* initialize the context */
  lstail->context = Context::New(NULL, global);
  ctxptr ptr(&(lstail->context));
  lstail->next = new node;
  lstail = lstail->next;
  return(ptr);
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
