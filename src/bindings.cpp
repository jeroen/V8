#include <libplatform/libplatform.h>
#include "V8_types.h"

/* use conditional apis below */
#define V8_VERSION_TOTAL (V8_MAJOR_VERSION * 100 + V8_MINOR_VERSION)

/* we dont assume <node/node_version.h> is installed */
#ifdef ISNODEJS
#if V8_MAJOR_VERSION == 10
#define NODEJS_LTS_API 18
#elif V8_MAJOR_VERSION == 9
#define NODEJS_LTS_API 16
#elif V8_MAJOR_VERSION == 8
#define NODEJS_LTS_API 14
#endif
#endif

#if !defined(ISNODEJS) || NODEJS_LTS_API > 16
#define FixedArrayParam ,v8::Local<v8::FixedArray> import_arributes
#else
#define FixedArrayParam
#endif

#if V8_VERSION_TOTAL < 803
#define PerformMicrotaskCheckpoint RunMicrotasks
#endif

/* __has_feature is a clang-ism, while __SANITIZE_ADDRESS__ is a gcc-ism */
#if defined(__clang__) && !defined(__SANITIZE_ADDRESS__)
#if defined(__has_feature) && __has_feature(address_sanitizer)
#define __SANITIZE_ADDRESS__ 1
#endif
#endif

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

void ctx_finalizer(ctx_type* context ){
  if(context)
    context->Reset();
  delete context;
}

static v8::Isolate* isolate = NULL;
static v8::Platform* platformptr = NULL;

static std::string read_text(std::string filename) {
  if(filename.empty() || (filename.at(0) != '.' && filename.at(0) != '/'))
    throw std::runtime_error("path should begin with . or /");
  std::ifstream t(filename);
  if(t.fail())
    throw std::runtime_error("Failed to open file: " + filename);
  std::stringstream buffer;
  buffer << t.rdbuf();
  return buffer.str();
}

// Extracts a C string from a V8 Utf8Value.
static const char* ToCString(const v8::String::Utf8Value& value) {
  return *value ? *value : "<string conversion failed>";
}

static v8::Local<v8::String> ToJSString(const char * str){
  v8::MaybeLocal<v8::String> out = v8::String::NewFromUtf8(isolate, str, v8::NewStringType::kNormal);
  return safe_to_local(out);
}

static void message_cb(v8::Local<v8::Message> message, v8::Local<v8::Value> data){
  v8::String::Utf8Value str(isolate, message->Get());
  REprintf("V8 MESSAGE (level %d): %s", message->ErrorLevel(), ToCString(str));
}

static void fatal_cb(const char* location, const char* message){
  REprintf("V8 FATAL ERROR in %s: %s", location, message);
}

static v8::Local<v8::Module> read_module(std::string filename, v8::Local<v8::Context> context);

static v8::MaybeLocal<v8::Module> ResolveModuleCallback(v8::Local<v8::Context> context, v8::Local<v8::String> specifier
                                                        FixedArrayParam, v8::Local<v8::Module> referrer) {
  v8::String::Utf8Value name(context->GetIsolate(), specifier);
  return read_module(*name, context);
}

static v8::MaybeLocal<v8::Promise> dynamic_module_loader(v8::Local<v8::Context> context, v8::Local<v8::String> specifier) {
  v8::Local<v8::Promise::Resolver> resolver = v8::Promise::Resolver::New(context).ToLocalChecked();
  v8::MaybeLocal<v8::Promise> promise(resolver->GetPromise());
  v8::String::Utf8Value name(context->GetIsolate(), specifier);
  try {
    v8::Local<v8::Module> module = read_module(*name, context);
    v8::Local<v8::Value> retValue;
    if (!module->Evaluate(context).ToLocal(&retValue))
      throw std::runtime_error("Failure loading module");
    resolver->Resolve(context, module->GetModuleNamespace()).FromMaybe(false);
  } catch(const std::exception& err) {
    std::string errmsg(std::string("problem loading module ") + *name + ": " + err.what());
    resolver->Reject(context, ToJSString(errmsg.c_str())).FromMaybe(false);
  } catch(...) {
    resolver->Reject(context, ToJSString("Unknown failure loading dynamic module")).FromMaybe(false);
  }
  return promise;
}

static v8::MaybeLocal<v8::Promise> ResolveDynamicModuleCallback(
    v8::Local<v8::Context> context,
#if V8_VERSION_TOTAL >= 908
    v8::Local<v8::Data> host_defined_options,
    v8::Local<v8::Value> resource_name,
#elif V8_VERSION_TOTAL >= 603
    v8::Local<v8::ScriptOrModule> referrer,
#else
    v8::Local<v8::String> referrer,
#endif
    v8::Local<v8::String> specifier
    FixedArrayParam
) {
  return dynamic_module_loader(context, specifier);
}

static v8::ScriptOrigin make_origin(std::string filename){
#if defined(ISNODEJS) && NODEJS_LTS_API < 18
  return v8::ScriptOrigin(ToJSString( filename.c_str()), v8::Integer::New(isolate, 0),
                          v8::Integer::New(isolate, 0), v8::False(isolate), v8::Local<v8::Integer>(),
                          v8::Local<v8::Value>(), v8::False(isolate), v8::False(isolate), v8::True(isolate));
#elif V8_VERSION_TOTAL < 1201
  return v8::ScriptOrigin(isolate,ToJSString( filename.c_str()), 0, 0, false, -1,
                          v8::Local<v8::Value>(), false, false, true);
#else
  return v8::ScriptOrigin(ToJSString( filename.c_str()), 0, 0, false, -1,
                          v8::Local<v8::Value>(), false, false, true);
#endif
}

/* Helper fun that compiles JavaScript source code */
static v8::Local<v8::Module> read_module(std::string filename, v8::Local<v8::Context> context){
  v8::Local<v8::String> source_text = ToJSString(read_text(filename).c_str());
  if(source_text.IsEmpty())
    throw std::runtime_error("Failed to load JavaScript source. Check memory/stack limits.");
  v8::ScriptCompiler::Source source(source_text, make_origin(filename));
  v8::Local<v8::Module> module;
  if (!v8::ScriptCompiler::CompileModule(isolate, &source).ToLocal(&module))
    throw std::runtime_error("Failed to run CompileModule() source.");
  if(!module->InstantiateModule(context, ResolveModuleCallback).FromMaybe(false))
    throw std::runtime_error("Failed to run InstantiateModule().");
  return module;
}


// [[Rcpp::init]]
void start_v8_isolate(void *dll){
#ifdef V8_ICU_DATA_PATH
  // Needed if V8 is built with bundled ICU. Check CRAN package 'dagitty' to test.
  if( access( V8_ICU_DATA_PATH, F_OK ) != -1 ) {
    v8::V8::InitializeICUDefaultLocation(V8_ICU_DATA_PATH);
  }
#endif
#if V8_VERSION_TOTAL >= 704
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  platformptr = platform.get();
  platform.release(); //UBSAN complains if platform is destroyed when out of scope
#else
  platformptr = v8::platform::CreateDefaultPlatform();
  v8::V8::InitializePlatform(platformptr);
#endif
  v8::V8::Initialize();
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
    v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  isolate = v8::Isolate::New(create_params);
  if(!isolate)
    throw std::runtime_error("Failed to initiate V8 isolate");
  isolate->AddMessageListener(message_cb);
  isolate->SetFatalErrorHandler(fatal_cb);

#ifdef __SANITIZE_ADDRESS__
  /* Disable stack limit when using sanitizers (highest possible value, backwards) */
  isolate->SetStackLimit(1);
#else
  /* Workaround for packages hitting stack limit on Fedora, such as ggdag.
   * CurrentStackPosition trick copied from chromium. */
  static const int kWorkerMaxStackSize = 2000 * 1024;
  uintptr_t CurrentStackPosition = reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
  isolate->SetStackLimit(CurrentStackPosition - kWorkerMaxStackSize);
#endif
  isolate->SetHostImportModuleDynamicallyCallback(ResolveDynamicModuleCallback);
}

/* Helper fun that compiles JavaScript source code */
static v8::Local<v8::Script> compile_source(std::string src, v8::Local<v8::Context> context){
  v8::Local<v8::String> source = ToJSString(src.c_str());
  if(source.IsEmpty()){
    throw std::runtime_error("Failed to load JavaScript source. Check memory/stack limits.");
  }
  v8::MaybeLocal<v8::Script> script = v8::Script::Compile(context, source);
  return safe_to_local(script);
}

static void pump_promises(){
  v8::platform::PumpMessageLoop(platformptr, isolate, v8::platform::MessageLoopBehavior::kDoNotWait);
  isolate->PerformMicrotaskCheckpoint();
  Rcpp::checkUserInterrupt();
}

/* Try to resolve pending promises */
static void ConsolePump(const v8::FunctionCallbackInfo<v8::Value>& args) {
  pump_promises();
  //args.GetReturnValue().Set(v8::Undefined(args.GetIsolate()));
}


/* console.log */
static void ConsoleLog(const v8::FunctionCallbackInfo<v8::Value>& args) {
  for (int i=0; i < args.Length(); i++) {
    v8::HandleScope handle_scope(args.GetIsolate());
    v8::String::Utf8Value str(args.GetIsolate(), args[i]);
    Rprintf("%s", ToCString(str));
  }
  Rprintf("\n");
  //args.GetReturnValue().Set(v8::Undefined(args.GetIsolate()));
}

/* console.warn */
static void ConsoleWarn(const v8::FunctionCallbackInfo<v8::Value>& args) {
  for (int i=0; i < args.Length(); i++) {
    v8::HandleScope handle_scope(args.GetIsolate());
    v8::String::Utf8Value str(args.GetIsolate(), args[i]);
    Rf_warningcall_immediate(R_NilValue, "%s", ToCString(str));
  }
  //args.GetReturnValue().Set(v8::Undefined(args.GetIsolate()));
}

/* console.error */
static void ConsoleError(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if(args.Length()){
    args.GetIsolate()->ThrowException(args[0]);
  }
  //args.GetReturnValue().Set(v8::Undefined(args.GetIsolate()));
}

void r_callback(std::string cb, const v8::FunctionCallbackInfo<v8::Value>& args) {
  try {
    Rcpp::Function r_call = Rcpp::Environment::namespace_env("V8")[cb];
    v8::String::Utf8Value arg0(args.GetIsolate(), args[0]);
    Rcpp::String fun(*arg0);
    Rcpp::CharacterVector out;
    if(args.Length() == 1 || args[1]->IsUndefined()){
      out = r_call(fun);
    } else if(args.Length() == 2 || args[2]->IsUndefined()) {
      v8::Local<v8::Object> obj1 = args[1]->ToObject(args.GetIsolate()->GetCurrentContext()).ToLocalChecked();
      v8::String::Utf8Value arg1(args.GetIsolate(), v8::JSON::Stringify(args.GetIsolate()->GetCurrentContext(), obj1).ToLocalChecked());
      Rcpp::String json(ToCString(arg1));
      out = r_call(fun, json);
    } else {
      v8::Local<v8::Object> obj1 = args[1]->ToObject(args.GetIsolate()->GetCurrentContext()).ToLocalChecked();
      v8::Local<v8::Object> obj2 = args[2]->ToObject(args.GetIsolate()->GetCurrentContext()).ToLocalChecked();
      v8::String::Utf8Value arg1(args.GetIsolate(), v8::JSON::Stringify(args.GetIsolate()->GetCurrentContext(), obj1).ToLocalChecked());
      v8::String::Utf8Value arg2(args.GetIsolate(), v8::JSON::Stringify(args.GetIsolate()->GetCurrentContext(), obj2).ToLocalChecked());
      Rcpp::String val(ToCString(arg1));
      Rcpp::String json(ToCString(arg2));
      out = r_call(fun, val, json);
    }
    v8::Local<v8::String> outstr(ToJSString(std::string(out.at(0)).c_str()));
    if(out.inherits("cb_error")){
      args.GetIsolate()->ThrowException(outstr);
    } else {
      args.GetReturnValue().Set( v8::JSON::Parse(args.GetIsolate()->GetCurrentContext(), outstr).ToLocalChecked());
    }
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

static Rcpp::RObject convert_object(v8::Local<v8::Value> value){
  if(value.IsEmpty() || value->IsNullOrUndefined()){
    return R_NilValue;
  } else if(value->IsArrayBuffer() || value->IsArrayBufferView()){
    v8::Local<v8::ArrayBuffer> buffer = value->IsArrayBufferView() ?
    value.As<v8::ArrayBufferView>()->Buffer() : value.As<v8::ArrayBuffer>();
    Rcpp::RawVector data(buffer->ByteLength());
#if V8_VERSION_TOTAL >= 1005 || NODEJS_LTS_API == 18
    memcpy(data.begin(), buffer->Data(), data.size());
#elif V8_VERSION_TOTAL < 901 || NODEJS_LTS_API == 16
    memcpy(data.begin(), buffer->GetContents().Data(), data.size());
#else
    /* Try to avoid this API: github.com/jeroen/V8/issues/152 */
    memcpy(data.begin(), buffer->GetBackingStore()->Data(), data.size());
#endif
    return data;
  } else {
    //convert to string without jsonify
    //v8::String::Utf8Value utf8(isolate, value);
    v8::Local<v8::Object> obj1 = value->ToObject(isolate->GetCurrentContext()).ToLocalChecked();
    v8::String::Utf8Value utf8(isolate, v8::JSON::Stringify(isolate->GetCurrentContext(), obj1).ToLocalChecked());
    return Rcpp::CharacterVector::create(Rcpp::String(*utf8));
  }
}

// [[Rcpp::export]]
Rcpp::RObject context_eval(Rcpp::String src, ctxptr ctx, bool serialize = false, bool await = false){
  // Test if context still exists
  if(!ctx)
    throw std::runtime_error("v8::Context has been disposed.");

  //converts input to UTF8 if needed
  src.set_encoding(CE_UTF8);

  // Create a scope
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = ctx.checked_get()->Get(isolate);
  v8::Context::Scope context_scope(context);

  // Compile source code
  v8::TryCatch trycatch(isolate);
  v8::Local<v8::Script> script = compile_source(src, context);
  if(script.IsEmpty()) {
    v8::String::Utf8Value exception(isolate, trycatch.Exception());
    if(*exception){
      throw std::invalid_argument(ToCString(exception));
    } else {
      throw std::runtime_error("Failed to interpret script. Check memory/stack limits.");
    }
  }

  // Run the script to get the result.
  v8::MaybeLocal<v8::Value> res = script->Run(context);
  v8::Local<v8::Value> result = safe_to_local(res);
  if(result.IsEmpty()){
    v8::String::Utf8Value exception(isolate, trycatch.Exception());
    throw std::runtime_error(ToCString(exception));
  }

  /* PumpMessageLoop is needed to load wasm from the background threads
   After this we still need to call PerformMicrotaskCheckpoint to resolve outstanding promises
   This may be better, but HasPendingBackgroundTasks() requires v8 8.3, see also
   https://docs.google.com/document/d/18vaABH1mR35PQr8XPHZySuQYgSjJbWFyAW63LW2m8-w
  */

  // while (v8::platform::PumpMessageLoop(platformptr, isolate, isolate->HasPendingBackgroundTasks() ?
  //   v8::platform::MessageLoopBehavior::kWaitForWork : v8::platform::MessageLoopBehavior::kDoNotWait)){
  // }


  // See https://groups.google.com/g/v8-users/c/r8nn6m6Lsj4/m/WrjLpk1PBAAJ
  if (await && result->IsPromise()) {
    v8::Local<v8::Promise> promise = result.As<v8::Promise>();
    while (promise->State() == v8::Promise::kPending)
      pump_promises();
    if (promise->State() == v8::Promise::kRejected) {
      v8::String::Utf8Value rejectmsg(isolate, promise->Result());
      throw std::runtime_error(ToCString(rejectmsg));
    } else {
      result = promise->Result();
    }
  }

  // Serialize to JSON or Raw
  if(serialize == true)
    return convert_object(result);

  // Convert result to string
  v8::String::Utf8Value utf8(isolate, result);
  Rcpp::String str(*utf8);
  str.set_encoding(CE_UTF8);
  Rcpp::CharacterVector out(1);
  out.at(0) = str;
  return out;
}

// [[Rcpp::export]]
bool write_array_buffer(Rcpp::String key, Rcpp::RawVector data, ctxptr ctx){
  // Test if context still exists
  if(!ctx)
    throw std::runtime_error("v8::Context has been disposed.");

  // Create a scope
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = ctx.checked_get()->Get(isolate);
  v8::Context::Scope context_scope(context);
  v8::TryCatch trycatch(isolate);

  // Initiate ArrayBuffer and ArrayBufferView (uint8 typed array)
  v8::Local<v8::ArrayBuffer> buffer = v8::ArrayBuffer::New(isolate, data.size());
  v8::Local<v8::Uint8Array> typed_array = v8::Uint8Array::New(buffer, 0, data.size());

#if V8_VERSION_TOTAL >= 1005 || NODEJS_LTS_API == 18
  memcpy(buffer->Data(), data.begin(), data.size());
#elif V8_VERSION_TOTAL < 901 || NODEJS_LTS_API == 16
  memcpy(buffer->GetContents().Data(), data.begin(), data.size());
#else
  memcpy(buffer->GetBackingStore()->Data(), data.begin(), data.size());
#endif

  // Assign to object (delete first if exists)
  v8::Local<v8::String> name = ToJSString(key.get_cstring());
  v8::Local<v8::Object> global = context->Global();
  if(!global->Has(context, name).FromMaybe(true) || !global->Delete(context, name).IsNothing())
    return !global->Set(context, name, typed_array).IsNothing();
  return false;
}

// [[Rcpp::export]]
bool context_validate(Rcpp::String src, ctxptr ctx) {

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
  v8::Local<v8::Script> script = compile_source(src, ctx.checked_get()->Get(isolate));
  return !script.IsEmpty();
}

// [[Rcpp::export]]
bool context_null(ctxptr ctx) {
  // Test if context still exists
  return(!ctx);
}

v8::Local<v8::Object> console_template(){
  v8::Local<v8::ObjectTemplate> console = v8::ObjectTemplate::New(isolate);
  console->Set(ToJSString("log"), v8::FunctionTemplate::New(isolate, ConsoleLog));
  console->Set(ToJSString("warn"), v8::FunctionTemplate::New(isolate, ConsoleWarn));
  console->Set(ToJSString("error"), v8::FunctionTemplate::New(isolate, ConsoleError));
  console->Set(ToJSString("pump"), v8::FunctionTemplate::New(isolate, ConsolePump));

  // R callback interface
  v8::Local<v8::ObjectTemplate> console_r = v8::ObjectTemplate::New(isolate);
  console->Set(ToJSString("r"), console_r);
  console_r->Set(ToJSString("call"), v8::FunctionTemplate::New(isolate, console_r_call));
  console_r->Set(ToJSString("get"), v8::FunctionTemplate::New(isolate, console_r_get));
  console_r->Set(ToJSString("eval"), v8::FunctionTemplate::New(isolate, console_r_eval));
  console_r->Set(ToJSString("assign"), v8::FunctionTemplate::New(isolate, console_r_assign));
  return console->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
}

// [[Rcpp::export]]
ctxptr make_context(bool set_console){
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);

  // emscripted requires a print function
  global->Set(ToJSString("print"), v8::FunctionTemplate::New(isolate, ConsoleLog));
  v8::Local<v8::Context> context = v8::Context::New(isolate, NULL, global);
  if(*context == NULL)
    throw std::runtime_error("Failed to create new context. Check memory stack limits.");
  v8::Context::Scope context_scope(context);

  v8::Local<v8::String> console = ToJSString("console");
  // need to unset global.console, or it will crash in some V8 versions (e.g. Fedora)
  // See: https://stackoverflow.com/questions/49620965/v8-cannot-set-objecttemplate-with-name-console
  if(set_console){
    if(context->Global()->Has(context, console).FromMaybe(true)){
       if(context->Global()->Delete(context, console).IsNothing())
         Rcpp::warning("Could not delete console.");
    }
    if(context->Global()->Set(context, console, console_template()).IsNothing())
      Rcpp::warning("Could not set console.");
  }
  ctx_type *ptr = new ctx_type(isolate, context);
  return ctxptr(ptr);
}
