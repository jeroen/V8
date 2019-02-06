#include <libplatform/libplatform.h>
#include "V8_types.h"

/* used for setting icu data below */
#ifdef __APPLE__
#define V8_ICU_DATA_PATH "/usr/local/opt/v8/libexec/icudtl.dat"
#include <unistd.h>
#endif

/* Note: Tov8::LocalChecked() aborts if x is empty */
template <typename T>
v8::Local<T> safe_to_local(v8::MaybeLocal<T> x){
  return x.IsEmpty() ? v8::Local<T>() : x.ToLocalChecked();
}

void ctx_finalizer( v8::Persistent<v8::Context>* context ){
  if(context)
    context->Reset();
  delete context;
}

static v8::Isolate* isolate = NULL;

// Extracts a C string from a V8 Utf8Value.
static const char* ToCString(const v8::String::Utf8Value& value) {
  return *value ? *value : "<string conversion failed>";
}

static v8::Local<v8::String> ToJSString(const char * str){
  v8::MaybeLocal<v8::String> out = v8::String::NewFromUtf8(isolate, str, v8::NewStringType::kNormal);
  return safe_to_local(out);
}

// [[Rcpp::init]]
void start_v8_isolate(void *dll){
#ifdef V8_ICU_DATA_PATH
  // Needed if V8 is built with bundled ICU. Check CRAN package 'dagitty' to test.
  if( access( V8_ICU_DATA_PATH, F_OK ) != -1 ) {
    v8::V8::InitializeICUDefaultLocation(V8_ICU_DATA_PATH);
  }
#endif
  v8::V8::InitializePlatform(v8::platform::CreateDefaultPlatform());
  v8::V8::Initialize();
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
    v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  isolate = v8::Isolate::New(create_params);
  if(!isolate)
    throw std::runtime_error("Failed to initiate V8 isolate");
}

/* Helper fun that compiles JavaScript source code */
static v8::Local<v8::Script> compile_source(std::string src, v8::Local<v8::Context> context){
  v8::Local<v8::String> source = ToJSString(src.c_str());
  v8::MaybeLocal<v8::Script> script = v8::Script::Compile(context, source);
  return safe_to_local(script);
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
    v8::String::Utf8Value arg0(args.GetIsolate(), args[0]);
    Rcpp::String fun(*arg0);
    Rcpp::CharacterVector out;
    if(args[1]->IsUndefined()){
      out = r_call(fun);
    } else if(args[2]->IsUndefined()) {
      v8::Local<v8::Object> obj1 = v8::Local<v8::Object>::Cast(args[1]);
      v8::String::Utf8Value arg1(args.GetIsolate(), v8::JSON::Stringify(args.GetIsolate()->GetCurrentContext(), obj1).ToLocalChecked());
      Rcpp::String json(ToCString(arg1));
      out = r_call(fun, json);
    } else {
      v8::Local<v8::Object> obj1 = v8::Local<v8::Object>::Cast(args[1]);
      v8::Local<v8::Object> obj2 = v8::Local<v8::Object>::Cast(args[2]);
      v8::String::Utf8Value arg1(args.GetIsolate(), v8::JSON::Stringify(args.GetIsolate()->GetCurrentContext(), obj1).ToLocalChecked());
      v8::String::Utf8Value arg2(args.GetIsolate(), v8::JSON::Stringify(args.GetIsolate()->GetCurrentContext(), obj2).ToLocalChecked());
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
    throw std::runtime_error("v8::Context has been disposed.");

  //converts input to UTF8 if needed
  src.set_encoding(CE_UTF8);

  // Create a scope
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(ctx.checked_get()->Get(isolate));

  // Compile source code
  v8::TryCatch trycatch(isolate);
  v8::Handle<v8::Script> script = compile_source(src, ctx.checked_get()->Get(isolate));
  if(script.IsEmpty()) {
    v8::String::Utf8Value exception(isolate, trycatch.Exception());
    throw std::invalid_argument(ToCString(exception));
  }

  // Run the script to get the result.
  v8::MaybeLocal<v8::Value> res = script->Run(ctx.checked_get()->Get(isolate));
  v8::Handle<v8::Value> result = safe_to_local(res);
  if(result.IsEmpty()){
    v8::String::Utf8Value exception(isolate, trycatch.Exception());
    throw std::runtime_error(ToCString(exception));
  }

  // Convert result to UTF8.
  v8::String::Utf8Value utf8(isolate, result);
  Rcpp::String out(*utf8);
  out.set_encoding(CE_UTF8);
  return out;
}

// [[Rcpp::export]]
bool context_validate(Rcpp::String src, Rcpp::XPtr< v8::Persistent<v8::Context> > ctx) {

  // Test if context still exists
  if(!ctx)
    throw std::runtime_error("v8::Context has been disposed.");

  //converts input to UTF8 if needed
  src.set_encoding(CE_UTF8);

  // Create a scope
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(ctx.checked_get()->Get(isolate));

  // Try to compile, catch errors
  v8::TryCatch trycatch(isolate);
  v8::Handle<v8::Script> script = compile_source(src, ctx.checked_get()->Get(isolate));
  return !script.IsEmpty();
}

// [[Rcpp::export]]
bool context_null(Rcpp::XPtr< v8::Persistent<v8::Context>> ctx) {
  // Test if context still exists
  return(!ctx);
}

v8::Local<v8::Object> console_template(){
  v8::Local<v8::ObjectTemplate> console = v8::ObjectTemplate::New(isolate);
  console->Set(ToJSString("log"), v8::FunctionTemplate::New(isolate, ConsoleLog));
  console->Set(ToJSString("warn"), v8::FunctionTemplate::New(isolate, ConsoleWarn));
  console->Set(ToJSString("error"), v8::FunctionTemplate::New(isolate, ConsoleError));

  // R callback interface
  v8::Local<v8::ObjectTemplate> console_r = v8::ObjectTemplate::New(isolate);
  console->Set(ToJSString("r"), console_r);
  console_r->Set(ToJSString("call"), v8::FunctionTemplate::New(isolate, console_r_call));
  console_r->Set(ToJSString("get"), v8::FunctionTemplate::New(isolate, console_r_get));
  console_r->Set(ToJSString("eval"), v8::FunctionTemplate::New(isolate, console_r_eval));
  console_r->Set(ToJSString("assign"), v8::FunctionTemplate::New(isolate, console_r_assign));
  return console->NewInstance();
}

// [[Rcpp::export]]
ctxptr make_context(bool set_console){
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);

  // emscripted requires a print function
  global->Set(ToJSString("print"), v8::FunctionTemplate::New(isolate, ConsoleLog));
  v8::Local<v8::Context> context = v8::Context::New(isolate, NULL, global);
  v8::Context::Scope context_scope(context);

  // need to unset global.console, or it will crash in some V8 versions (e.g. Fedora)
  // See: https://stackoverflow.com/questions/49620965/v8-cannot-set-objecttemplate-with-name-console
  if(set_console){
    if(context->Global()->Has(ToJSString("console"))){
      context->Global()->Delete(ToJSString("console"));
    }
    context->Global()->Set(ToJSString("console"), console_template());

  }
  v8::Persistent<v8::Context> *ptr = new v8::Persistent<v8::Context>(isolate, context);
  return ctxptr(ptr);
}

// [[Rcpp::export]]
bool context_enable_typed_arrays( Rcpp::XPtr< v8::Persistent<v8::Context> > ctx ){
  return true;
}
