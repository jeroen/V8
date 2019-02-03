#include <libplatform/libplatform.h>
#include "V8_types.h"
using namespace v8;

void ctx_finalizer( Persistent<Context>* context ){
  if(context)
    context->Reset();
  delete context;
}

static v8::Isolate* isolate = NULL;

// Extracts a C string from a V8 Utf8Value.
static const char* ToCString(const v8::String::Utf8Value& value) {
  return *value ? *value : "<string conversion failed>";
}

static Local<String> ToJSString(const char * str){
  return String::NewFromUtf8(isolate, str, NewStringType::kNormal).ToLocalChecked();
}

// [[Rcpp::export]]
void start_v8_isolate(){
  static std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  V8::InitializePlatform(platform.get());
  V8::Initialize();
  //V8::InitializeICU();
  Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
    ArrayBuffer::Allocator::NewDefaultAllocator();
  isolate = Isolate::New(create_params);
  if(!isolate)
    throw std::runtime_error("Failed to initiate V8 isolate");
}

/* Helper fun that compiles JavaScript source code */
static Local<Script> compile_source(std::string src){
  Local<String> source = String::NewFromUtf8(isolate, src.c_str(), NewStringType::kNormal).ToLocalChecked();
  Local<Script> script = Script::Compile(source);
  return script;
}

/* console.log */
static void ConsoleLog(const v8::FunctionCallbackInfo<v8::Value>& args) {
  for (int i=0; i < args.Length(); i++) {
    v8::HandleScope handle_scope(args.GetIsolate());
    v8::String::Utf8Value str(args.GetIsolate(), args[i]);
    Rprintf("%s", ToCString(str));
  }
  Rprintf("\n");
  args.GetReturnValue().Set(v8::Undefined(args.GetIsolate()));
}

/* console.warn */
static void ConsoleWarn(const v8::FunctionCallbackInfo<v8::Value>& args) {
  for (int i=0; i < args.Length(); i++) {
    v8::HandleScope handle_scope(args.GetIsolate());
    v8::String::Utf8Value str(args.GetIsolate(), args[i]);
    Rf_warningcall_immediate(R_NilValue, ToCString(str));
  }
  Rprintf("\n");
  args.GetReturnValue().Set(v8::Undefined(args.GetIsolate()));
}

/* console.error */
static void ConsoleError(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if(args.Length()){
    args.GetIsolate()->ThrowException(args[0]);
  }
  args.GetReturnValue().Set(v8::Undefined(args.GetIsolate()));
}

void r_callback(std::string fun, const v8::FunctionCallbackInfo<v8::Value>& args) {
  try {
    Rcpp::Function r_call = Rcpp::Environment::namespace_env("V8")[fun];
    String::Utf8Value arg0(args[0]);
    Rcpp::String fun(*arg0);
    Rcpp::CharacterVector out;
    if(args[1]->IsUndefined()){
      out = r_call(fun);
    } else if(args[2]->IsUndefined()) {
      String::Utf8Value arg1(v8::JSON::Stringify(args.GetIsolate()->GetCurrentContext(), args[1]).ToLocalChecked());
      Rcpp::String json(ToCString(arg1));
      out = r_call(fun, json);
    } else {
      String::Utf8Value arg1(v8::JSON::Stringify(args.GetIsolate()->GetCurrentContext(), args[1]).ToLocalChecked());
      String::Utf8Value arg2(v8::JSON::Stringify(args.GetIsolate()->GetCurrentContext(), args[2]).ToLocalChecked());
      Rcpp::String val(ToCString(arg1));
      Rcpp::String json(ToCString(arg2));
      out = r_call(fun, val, json);
    }
    args.GetReturnValue().Set( v8::JSON::Parse(args.GetIsolate(), ToJSString(std::string(out[0]).c_str())).ToLocalChecked());
  } catch( const std::exception& e ) {
    args.GetIsolate()->ThrowException(ToJSString(e.what()));
  }
}

/* console.r.call() function */
static void console_r_call(const v8::FunctionCallbackInfo<v8::Value>& args) {
  r_callback("r_call", args);
}

/* console.r.get() function */
static void console_r_get(const v8::FunctionCallbackInfo<v8::Value>& args) {
  r_callback("r_get", args);
}

/* console.r.eval() function */
static void console_r_eval(const v8::FunctionCallbackInfo<v8::Value>& args) {
  r_callback("r_eval", args);
}

/* console.r.eval() function */
static void console_r_assign(const v8::FunctionCallbackInfo<v8::Value>& args) {
  r_callback("r_assign", args);
}

// [[Rcpp::export]]
std::string version(){
  return v8::V8::GetVersion();
}

// [[Rcpp::export]]
Rcpp::String context_eval(Rcpp::String src, Rcpp::XPtr< v8::Persistent<v8::Context> > ctx){
  // Test if context still exists
  if(!ctx)
    throw std::runtime_error("Context has been disposed.");

  //converts input to UTF8 if needed
  src.set_encoding(CE_UTF8);

  // Create a scope
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(ctx.checked_get()->Get(isolate));

  // Compile source code
  TryCatch trycatch(isolate);
  Handle<Script> script = compile_source(src);
  if(script.IsEmpty()) {
    v8::String::Utf8Value exception(isolate, trycatch.Exception());
    throw std::invalid_argument(ToCString(exception));
  }

  // Run the script to get the result.
  Handle<Value> result = script->Run();
  if(result.IsEmpty()){
    v8::String::Utf8Value exception(isolate, trycatch.Exception());
    throw std::runtime_error(ToCString(exception));
  }

  // Convert result to UTF8.
  String::Utf8Value utf8(result);
  Rcpp::String out(*utf8);
  out.set_encoding(CE_UTF8);
  return out;
}

// [[Rcpp::export]]
bool context_validate(Rcpp::String src, Rcpp::XPtr< v8::Persistent<v8::Context> > ctx) {

  // Test if context still exists
  if(!ctx)
    throw std::runtime_error("Context has been disposed.");

  //converts input to UTF8 if needed
  src.set_encoding(CE_UTF8);

  // Create a scope
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(ctx.checked_get()->Get(isolate));

  // Try to compile, catch errors
  TryCatch trycatch(isolate);
  Handle<Script> script = compile_source(src);
  return !script.IsEmpty();
}

// [[Rcpp::export]]
bool context_null(Rcpp::XPtr< v8::Persistent<v8::Context> > ctx) {
  // Test if context still exists
  return(!ctx);
}

// [[Rcpp::export]]
ctxptr make_context(bool set_console){
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  Local<ObjectTemplate> global = ObjectTemplate::New(isolate);

  if(set_console){
    Local<ObjectTemplate> console = ObjectTemplate::New(isolate);
    global->Set(ToJSString("console"), console);
    console->Set(ToJSString("log"), FunctionTemplate::New(isolate, ConsoleLog));
    console->Set(ToJSString("warn"), FunctionTemplate::New(isolate, ConsoleWarn));
    console->Set(ToJSString("error"), FunctionTemplate::New(isolate, ConsoleError));

    // emscripted assumes a print function
    global->Set(ToJSString("print"), FunctionTemplate::New(isolate, ConsoleLog));

    // R callback interface
    Local<ObjectTemplate> console_r = ObjectTemplate::New(isolate);
    console->Set(ToJSString("r"), console_r);
    console_r->Set(ToJSString("call"), FunctionTemplate::New(isolate, console_r_call));
    console_r->Set(ToJSString("get"), FunctionTemplate::New(isolate, console_r_get));
    console_r->Set(ToJSString("eval"), FunctionTemplate::New(isolate, console_r_eval));
    console_r->Set(ToJSString("assign"), FunctionTemplate::New(isolate, console_r_assign));

  }
  // initialize the context
  Persistent<Context> *ptr = new Persistent<Context>(isolate, Context::New(isolate, NULL, global));
  return ctxptr(ptr);
}

// [[Rcpp::export]]
bool context_enable_typed_arrays( Rcpp::XPtr< v8::Persistent<v8::Context> > ctx ){
  return true;
}
