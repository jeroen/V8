/*
 R bindings to V8. Copyright 2014, Jeroen Ooms.

 Notes:
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

void ctx_finalizer( Persistent<Context>* context ){
  if(context)
    context->Dispose();
  delete context;
}

typedef Rcpp::XPtr<Persistent<Context>, Rcpp::PreserveStorage, ctx_finalizer> ctxptr;

/* Helper fun that compiles JavaScript source code */
static Handle<Script> compile_source( std::string src ){
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
    } else if(args[2]->IsUndefined()) {
      String::Utf8Value arg1(json_stringify(args[1]));
      Rcpp::String json(*arg1);
      out = r_call(fun, json);
    } else {
      String::Utf8Value arg1(json_stringify(args[1]));
      String::Utf8Value arg2(json_stringify(args[2]));
      Rcpp::String val(*arg1);
      Rcpp::String json(*arg2);
      out = r_call(fun, val, json);
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
  r_callback("r_eval", args);
  return v8::Undefined();
}

/* console.r.eval() function */
static Handle<Value> console_r_assign(const Arguments& args) {
  r_callback("r_assign", args);
  return v8::Undefined();
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
    console_r->Set(String::NewSymbol("assign"), FunctionTemplate::New(console_r_assign));

  }
  /* initialize the context */
  Persistent<Context> *ctptr = new Persistent<Context>(Context::New(NULL, global));
  return ctxptr(ctptr);
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

static Rcpp::RObject convert_object(v8::Local<v8::Value> value){
  if(value->IsUndefined() || value->IsNull()){
    return R_NilValue;
  }
  Local<v8::Object> obj = value->ToObject();
  if(obj->HasIndexedPropertiesInExternalArrayData()){
    int size = v8_typed_array::SizeOfArrayElementForType(obj->GetIndexedPropertiesExternalArrayDataType());
    size_t len = obj->GetIndexedPropertiesExternalArrayDataLength();
    Rcpp::RawVector buf(len * size);
    memcpy(buf.begin(), obj->GetIndexedPropertiesExternalArrayData(), len * size);
    return buf;
  } else {
    //convert to string without jsonify
    //v8::String::Utf8Value utf8(isolate, value);
    String::Utf8Value utf8(json_stringify(value));
    if(!utf8.length())
      return R_NilValue;
    return Rcpp::CharacterVector::create(Rcpp::String(*utf8));
  }
}

// [[Rcpp::export]]
Rcpp::RObject context_eval(Rcpp::String src, Rcpp::XPtr< v8::Persistent<v8::Context> > ctx, bool serialize = false){
  // Test if context still exists
  if(!ctx)
    throw std::runtime_error("Context has been disposed.");

  //converts input to UTF8 if needed
  src.set_encoding(CE_UTF8);

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
  Local<Value> result = script->Run();
  if(result.IsEmpty()){
    Local<Value> exception = trycatch.Exception();
    String::AsciiValue exception_str(exception);
    throw std::runtime_error(*exception_str);
  }

  // Serialize to JSON or Raw
  if(serialize == true)
    return convert_object(result);

  // Convert result to UTF8.
  v8::String::Utf8Value utf8(result);
  Rcpp::String str(*utf8, CE_UTF8);
  Rcpp::CharacterVector out(1);
  out.at(0) = str;
  return out;
}

// [[Rcpp::export]]
bool context_validate(Rcpp::String src, Rcpp::XPtr< v8::Persistent<v8::Context> > ctx) {

  // Test if context still exists
  if(!ctx)
    throw std::runtime_error("Context has been disposed.");

  //converts input to UTF8 if needed
  src.set_encoding(CE_UTF8);

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

// [[Rcpp::export]]
bool write_array_buffer(Rcpp::String key, Rcpp::RawVector data, Rcpp::XPtr< v8::Persistent<v8::Context> > ctx){
  if(!ctx)
    throw std::runtime_error("Context has been disposed.");

  // Create scope
  HandleScope handle_scope;
  Context::Scope context_scope(*ctx);
  v8::Handle<v8::String> name = String::NewSymbol(key.get_cstring());
  v8::Handle<v8::Object> global = (*ctx)->Global();
  v8::Local<v8::Object> obj = v8_typed_array::new_array(data.length());
  memcpy(obj->GetIndexedPropertiesExternalArrayData(), data.begin(), data.length());
  global->Set(name, obj);
  return true;
}
