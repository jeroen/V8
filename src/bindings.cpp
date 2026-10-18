#include <libplatform/libplatform.h>
#include "V8_types.h"
#include <fstream>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "native_js.h"

/* use conditional apis below */
#define V8_VERSION_TOTAL (V8_MAJOR_VERSION * 100 + V8_MINOR_VERSION)

/* we dont assume <node/node_version.h> is installed */
#ifdef ISNODEJS
#if V8_MAJOR_VERSION == 10
#define NODEJS_LTS_API 18
#elif V8_MAJOR_VERSION == 9
#define NODEJS_LTS_API 16
#elif V8_MAJOR_VERSION < 9
#define NODEJS_LTS_API 14
#endif
#endif

#if V8_VERSION_TOTAL < 908 || NODEJS_LTS_API == 16
#define FixedArrayParam
#else
#define FixedArrayParam ,v8::Local<v8::FixedArray> import_arributes
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
    throw std::runtime_error("Invalid module: " + filename + " (paths should begin with . or /)");
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

static v8::Local<v8::Value> r_to_js(SEXP x, SEXP options, v8::Local<v8::Context> context);
static Rcpp::RObject js_to_r(v8::Local<v8::Value> value, v8::Local<v8::Value> options, v8::Local<v8::Context> context);

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
#if V8_VERSION_TOTAL >= 1402
  v8::String::Utf8Value name(v8::Isolate::GetCurrent(), specifier);
#else
  v8::String::Utf8Value name(context->GetIsolate(), specifier);
#endif
  try {
    return read_module(*name, context);
  }
  catch(const std::exception& err) {
    isolate->ThrowException(ToJSString(err.what()));
  }
  return v8::Local<v8::Module>();
}

static v8::MaybeLocal<v8::Promise> dynamic_module_loader(v8::Local<v8::Context> context, v8::Local<v8::String> specifier) {
  v8::Local<v8::Promise::Resolver> resolver = v8::Promise::Resolver::New(context).ToLocalChecked();
  v8::MaybeLocal<v8::Promise> promise(resolver->GetPromise());
#if V8_VERSION_TOTAL >= 1402
  v8::String::Utf8Value name(v8::Isolate::GetCurrent(), specifier);
#else
  v8::String::Utf8Value name(context->GetIsolate(), specifier);
#endif
  try {
    v8::Local<v8::Module> module = read_module(*name, context);
    resolver->Resolve(context, module->GetModuleNamespace()).FromMaybe(false);
  } catch(const std::exception& err) {
    resolver->Reject(context, ToJSString(err.what())).FromMaybe(false);
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
#if V8_VERSION_TOTAL < 908 || NODEJS_LTS_API == 16
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

static std::string throw_js_err(v8::Local<v8::Value> Exception, std::string filename){
  std::string errmsg(std::string("Failed to import ES module '") + filename + "': " + *v8::String::Utf8Value(isolate, Exception));
  throw std::runtime_error(errmsg);
}

/* Helper fun that compiles JavaScript source code */
static v8::Local<v8::Module> read_module(std::string filename, v8::Local<v8::Context> context){
  v8::Local<v8::String> source_text = ToJSString(read_text(filename).c_str());
  if(source_text.IsEmpty())
    throw std::runtime_error("Failed to read module file (check memory/stack limits.");
  v8::TryCatch trycatch(isolate);
  v8::ScriptCompiler::Source source(source_text, make_origin(filename));
  v8::Local<v8::Module> module;
  if (!v8::ScriptCompiler::CompileModule(isolate, &source).ToLocal(&module)){
    if(trycatch.HasCaught())
      throw_js_err(trycatch.Exception(), filename);
    throw std::runtime_error("Failed to run CompileModule() source.");
  }
  if(!module->InstantiateModule(context, ResolveModuleCallback).FromMaybe(false)){
    if(trycatch.HasCaught())
      throw_js_err(trycatch.Exception(), filename);
    throw std::runtime_error("Failed to run InstantiateModule().");
  }
  v8::Local<v8::Value> retValue;
  if (!module->Evaluate(context).ToLocal(&retValue)){
    if(trycatch.HasCaught())
      throw_js_err(trycatch.Exception(), filename);
    throw std::runtime_error("Failure loading module");
  }
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
#if V8_VERSION_TOTAL >= 1401 && V8_VERSION_TOTAL < 1404
  v8::V8::SetFlagsFromString("--harmony-temporal");
#endif
#if V8_VERSION_TOTAL > 805 && V8_VERSION_TOTAL < 909
  /* Flag introduced: https://github.com/v8/v8/commit/ba688c6ec9c
   * Flag removed: https://github.com/v8/v8/commit/1771e4aaa */
  v8::V8::SetFlagsFromString("--experimental-wasm-reftypes");
#endif
  v8::V8::Initialize();
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
    v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  isolate = v8::Isolate::New(create_params);
  if(!isolate)
    throw std::runtime_error("Failed to initiate V8 isolate");

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);

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

static void r_callback_native(std::string cb, const v8::FunctionCallbackInfo<v8::Value>& args, v8::Local<v8::Context> context) {
  try {
    Rcpp::Environment ns = Rcpp::Environment::namespace_env("V8");
    Rcpp::Function r_fun(ns.get(cb + "_native"));
    v8::String::Utf8Value arg0(args.GetIsolate(), args[0]);
    Rcpp::String fun(ToCString(arg0), CE_UTF8);
    v8::Local<v8::Value> undefined = v8::Undefined(isolate);
    v8::Local<v8::Value> value = (args.Length() > 1) ? args[1] : undefined;
    v8::Local<v8::Value> options = (args.Length() > 2) ? args[2] : undefined;
    Rcpp::RObject arg2 = js_to_r(options, undefined, context);
    Rcpp::RObject arg1 = js_to_r(value, (cb == "r_assign") ? options : undefined, context);
    Rcpp::RObject out = r_fun(fun, arg1, arg2);
    if(out.inherits("cb_error")){
      Rcpp::CharacterVector msg(out);
      args.GetIsolate()->ThrowException(ToJSString(std::string(msg.at(0)).c_str()));
    } else if(out.inherits("V8_callback_value")){
      Rcpp::List res(out);
      args.GetReturnValue().Set(r_to_js(res[0], res[1], context));
    } else {
      args.GetReturnValue().SetNull();
    }
  } catch( const std::exception& e ) {
    args.GetIsolate()->ThrowException(ToJSString(e.what()));
  }
}

void r_callback(std::string cb, const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
  v8::Local<v8::Value> native;
  if(context->Global()->GetPrivate(context, v8::Private::ForApi(isolate, ToJSString("V8::native"))).ToLocal(&native) && native->IsTrue()){
    r_callback_native(cb, args, context);
    return;
  }
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
  if(value.IsEmpty() || value->IsUndefined()){
    return R_NilValue;
  } else if(value->IsNull()){
    return Rcpp::CharacterVector::create(Rcpp::String("null"));
  } else if(value->IsArrayBufferView()){
    v8::Local<v8::ArrayBufferView> view = value.As<v8::ArrayBufferView>();
    Rcpp::RawVector data(view->ByteLength());
    view->CopyContents(data.begin(), view->ByteLength());
    return data;
  } else if(value->IsArrayBuffer()){
    v8::Local<v8::ArrayBuffer> buffer = value.As<v8::ArrayBuffer>();
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

static v8::Local<v8::Function> compile_function(const char* code, v8::Local<v8::Context> context){
  v8::Local<v8::Script> script = compile_source(code, context);
  if(script.IsEmpty())
    return v8::Local<v8::Function>();
  v8::Local<v8::Value> fn = safe_to_local(script->Run(context));
  if(fn.IsEmpty() || !fn->IsFunction())
    return v8::Local<v8::Function>();
  return fn.As<v8::Function>();
}

static v8::Local<v8::ArrayBuffer> copy_to_array_buffer(const void* data, size_t bytes){
  v8::Local<v8::ArrayBuffer> buffer = v8::ArrayBuffer::New(isolate, bytes);
  if(bytes == 0)
    return buffer;
#if V8_VERSION_TOTAL >= 1005 || NODEJS_LTS_API == 18
  memcpy(buffer->Data(), data, bytes);
#elif V8_VERSION_TOTAL < 901 || NODEJS_LTS_API == 16
  memcpy(buffer->GetContents().Data(), data, bytes);
#else
  memcpy(buffer->GetBackingStore()->Data(), data, bytes);
#endif
  return buffer;
}

static v8::Local<v8::Array> make_array(v8::Local<v8::Value>* values, size_t n){
#if V8_VERSION_TOTAL >= 700
  return v8::Array::New(isolate, values, n);
#else
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Array> out = v8::Array::New(isolate, static_cast<int>(n));
  for(size_t i = 0; i < n; i++){
    if(out->Set(context, static_cast<uint32_t>(i), values[i]).IsNothing())
      throw std::runtime_error("Failed to create array.");
  }
  return out;
#endif
}

static v8::Local<v8::String> to_js_name(SEXP el){
  return ToJSString(el == NA_STRING ? "NA" : Rf_translateCharUTF8(el));
}

static v8::Local<v8::Object> options_to_js(SEXP options){
  v8::EscapableHandleScope scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> out = v8::Object::New(isolate);
  if(TYPEOF(options) != VECSXP)
    return scope.Escape(out);
  SEXP names = Rf_getAttrib(options, R_NamesSymbol);
  if(Rf_isNull(names))
    return scope.Escape(out);
  for(R_xlen_t i = 0; i < Rf_xlength(options); i++){
    SEXP val = VECTOR_ELT(options, i);
    if(Rf_xlength(val) < 1)
      continue;
    v8::Local<v8::Value> jsval;
    switch(TYPEOF(val)){
    case STRSXP:
      jsval = to_js_name(STRING_ELT(val, 0));
      break;
    case LGLSXP:
      if(LOGICAL(val)[0] == NA_LOGICAL)
        continue;
      jsval = v8::Boolean::New(isolate, LOGICAL(val)[0] != 0);
      break;
    case INTSXP:
      jsval = v8::Number::New(isolate, static_cast<double>(INTEGER(val)[0]));
      break;
    case REALSXP:
      jsval = v8::Number::New(isolate, REAL(val)[0]);
      break;
    default:
      continue;
    }
    if(out->Set(context, to_js_name(STRING_ELT(names, i)), jsval).IsNothing())
      throw std::runtime_error("Failed to set option.");
  }
  return scope.Escape(out);
}

static v8::Local<v8::Value> r_to_node(SEXP x, bool auto_unbox){
  v8::EscapableHandleScope scope(isolate);
  size_t n = static_cast<size_t>(Rf_xlength(x));
  switch(TYPEOF(x)){
  case NILSXP: {
    v8::Local<v8::Value> node[1] = {v8::Integer::New(isolate, 0)};
    return scope.Escape(make_array(node, 1));
  }
  case LGLSXP:
  case INTSXP:
  case REALSXP:
  case STRSXP: {
    if(TYPEOF(x) == STRSXP && n == 1 && Rf_inherits(x, "json")){
      v8::Local<v8::Value> node[2] = {
        v8::Integer::New(isolate, 9),
        ToJSString(Rf_translateCharUTF8(STRING_ELT(x, 0)))
      };
      return scope.Escape(make_array(node, 2));
    }
    SEXP dims = Rf_getAttrib(x, R_DimSymbol);
    bool has_dims = !Rf_isNull(dims) && Rf_xlength(dims) >= 2;
    bool scalar = auto_unbox && !has_dims && n == 1 && !Rf_inherits(x, "AsIs");
    v8::Local<v8::Value> node[4];
    node[3] = v8::Boolean::New(isolate, scalar);
    if(TYPEOF(x) == REALSXP){
      node[0] = v8::Integer::New(isolate, 3);
      node[1] = v8::Float64Array::New(copy_to_array_buffer(REAL(x), n * sizeof(double)), 0, n);
      node[2] = v8::Null(isolate);
      const double* v = REAL(x);
      for(size_t i = 0; i < n; i++){
        if(R_IsNA(v[i])){
          std::vector<uint8_t> mask(n, 0);
          for(size_t k = i; k < n; k++)
            mask[k] = R_IsNA(v[k]) ? 1 : 0;
          node[2] = v8::Uint8Array::New(copy_to_array_buffer(mask.data(), n), 0, n);
          break;
        }
      }
    } else if(TYPEOF(x) == STRSXP){
      std::unordered_map<SEXP, int32_t> index;
      std::vector<int32_t> codes(n);
      std::vector<v8::Local<v8::Value>> levels;
      for(size_t i = 0; i < n; i++){
        SEXP el = STRING_ELT(x, static_cast<R_xlen_t>(i));
        if(el == NA_STRING){
          codes[i] = NA_INTEGER;
          continue;
        }
        std::unordered_map<SEXP, int32_t>::iterator it = index.find(el);
        if(it == index.end()){
          levels.push_back(ToJSString(Rf_translateCharUTF8(el)));
          int32_t code = static_cast<int32_t>(levels.size());
          index[el] = code;
          codes[i] = code;
        } else {
          codes[i] = it->second;
        }
      }
      node[0] = v8::Integer::New(isolate, 4);
      node[1] = v8::Int32Array::New(copy_to_array_buffer(codes.data(), n * sizeof(int32_t)), 0, n);
      node[2] = make_array(levels.data(), levels.size());
    } else if(Rf_isFactor(x)){
      SEXP lev = Rf_getAttrib(x, R_LevelsSymbol);
      size_t nlev = static_cast<size_t>(Rf_xlength(lev));
      std::vector<v8::Local<v8::Value>> levels(nlev);
      for(size_t i = 0; i < nlev; i++)
        levels[i] = to_js_name(STRING_ELT(lev, static_cast<R_xlen_t>(i)));
      node[0] = v8::Integer::New(isolate, 4);
      node[1] = v8::Int32Array::New(copy_to_array_buffer(INTEGER(x), n * sizeof(int32_t)), 0, n);
      node[2] = make_array(levels.data(), nlev);
    } else {
      const int* v = (TYPEOF(x) == LGLSXP) ? LOGICAL(x) : INTEGER(x);
      node[0] = v8::Integer::New(isolate, TYPEOF(x) == LGLSXP ? 1 : 2);
      node[1] = v8::Int32Array::New(copy_to_array_buffer(v, n * sizeof(int32_t)), 0, n);
      node[2] = v8::Null(isolate);
    }
    v8::Local<v8::Array> vec = make_array(node, 4);
    if(!has_dims)
      return scope.Escape(vec);
    size_t ndims = static_cast<size_t>(Rf_xlength(dims));
    std::vector<v8::Local<v8::Value>> dimvals(ndims);
    for(size_t i = 0; i < ndims; i++)
      dimvals[i] = v8::Integer::New(isolate, INTEGER(dims)[i]);
    v8::Local<v8::Value> arr[3] = {
      v8::Integer::New(isolate, 8),
      vec,
      make_array(dimvals.data(), ndims)
    };
    return scope.Escape(make_array(arr, 3));
  }
  case RAWSXP: {
    v8::Local<v8::Value> node[2] = {
      v8::Integer::New(isolate, 5),
      v8::Uint8Array::New(copy_to_array_buffer(RAW(x), n), 0, n)
    };
    return scope.Escape(make_array(node, 2));
  }
  case VECSXP: {
    std::vector<v8::Local<v8::Value>> children(n);
    for(size_t i = 0; i < n; i++)
      children[i] = r_to_node(VECTOR_ELT(x, static_cast<R_xlen_t>(i)), auto_unbox);
    SEXP names = Rf_getAttrib(x, R_NamesSymbol);
    v8::Local<v8::Value> jsnames = v8::Null(isolate);
    if(!Rf_isNull(names)){
      std::vector<v8::Local<v8::Value>> nm(n);
      for(size_t i = 0; i < n; i++)
        nm[i] = to_js_name(STRING_ELT(names, static_cast<R_xlen_t>(i)));
      jsnames = make_array(nm.data(), n);
    }
    if(Rf_inherits(x, "data.frame")){
      R_xlen_t nrow = (n > 0) ? Rf_xlength(VECTOR_ELT(x, 0)) : Rf_xlength(Rf_getAttrib(x, R_RowNamesSymbol));
      if(Rf_isNull(names))
        throw std::invalid_argument("data frame columns must be named");
      v8::Local<v8::Value> node[4] = {
        v8::Integer::New(isolate, 7),
        make_array(children.data(), n),
        jsnames,
        v8::Number::New(isolate, static_cast<double>(nrow))
      };
      return scope.Escape(make_array(node, 4));
    }
    v8::Local<v8::Value> node[3] = {
      v8::Integer::New(isolate, 6),
      make_array(children.data(), n),
      jsnames
    };
    return scope.Escape(make_array(node, 3));
  }
  default:
    throw std::invalid_argument(std::string("No native conversion for R type: ") + Rf_type2char(static_cast<SEXPTYPE>(TYPEOF(x))));
  }
}

static v8::Local<v8::Value> r_to_js(SEXP x, SEXP options, v8::Local<v8::Context> context){
  v8::EscapableHandleScope scope(isolate);
  v8::TryCatch trycatch(isolate);
  bool auto_unbox = true;
  if(TYPEOF(options) == VECSXP){
    SEXP names = Rf_getAttrib(options, R_NamesSymbol);
    for(R_xlen_t i = 0; !Rf_isNull(names) && i < Rf_xlength(options); i++){
      SEXP val = VECTOR_ELT(options, i);
      if(std::string(CHAR(STRING_ELT(names, i))) == "auto_unbox" && TYPEOF(val) == LGLSXP && Rf_xlength(val) == 1)
        auto_unbox = LOGICAL(val)[0] == 1;
    }
  }
  v8::Local<v8::Value> node = r_to_node(x, auto_unbox);
  v8::Local<v8::Function> fn = compile_function(native_decode_js, context);
  if(fn.IsEmpty())
    throw std::runtime_error("Failed to compile native decoder.");
  v8::Local<v8::Value> argv[2] = {node, options_to_js(options)};
  v8::Local<v8::Value> out = safe_to_local(fn->Call(context, context->Global(), 2, argv));
  if(out.IsEmpty()){
    v8::String::Utf8Value exception(isolate, trycatch.Exception());
    throw std::runtime_error(ToCString(exception));
  }
  return scope.Escape(out);
}

static Rcpp::RObject node_to_r(v8::Local<v8::Value> value, v8::Local<v8::Context> context){
  v8::HandleScope scope(isolate);
  if(value.IsEmpty() || !value->IsArray())
    throw std::runtime_error("Invalid node in native conversion.");
  v8::Local<v8::Array> node = value.As<v8::Array>();
  int tag = safe_to_local(node->Get(context, 0))->Int32Value(context).FromMaybe(-1);
  switch(tag){
  case 0:
    return R_NilValue;
  case 1:
  case 2:
  case 3: {
    v8::Local<v8::ArrayBufferView> data = safe_to_local(node->Get(context, 1)).As<v8::ArrayBufferView>();
    v8::Local<v8::Value> maskval = safe_to_local(node->Get(context, 2));
    size_t n = data->ByteLength() / (tag == 3 ? sizeof(double) : sizeof(int32_t));
    R_xlen_t nr = static_cast<R_xlen_t>(n);
    std::vector<uint8_t> mask;
    if(maskval->IsUint8Array()){
      mask.resize(n);
      maskval.As<v8::Uint8Array>()->CopyContents(mask.data(), n);
    }
    if(tag == 3){
      Rcpp::NumericVector x(nr);
      data->CopyContents(x.begin(), n * sizeof(double));
      for(size_t i = 0; i < mask.size(); i++)
        if(mask[i]) x[static_cast<R_xlen_t>(i)] = NA_REAL;
      return x;
    }
    if(tag == 2){
      Rcpp::IntegerVector x(nr);
      data->CopyContents(x.begin(), n * sizeof(int32_t));
      for(size_t i = 0; i < mask.size(); i++)
        if(mask[i]) x[static_cast<R_xlen_t>(i)] = NA_INTEGER;
      return x;
    }
    Rcpp::LogicalVector x(nr);
    data->CopyContents(x.begin(), n * sizeof(int32_t));
    for(size_t i = 0; i < mask.size(); i++)
      if(mask[i]) x[static_cast<R_xlen_t>(i)] = NA_LOGICAL;
    return x;
  }
  case 4: {
    v8::Local<v8::Int32Array> codes = safe_to_local(node->Get(context, 1)).As<v8::Int32Array>();
    v8::Local<v8::Array> levels = safe_to_local(node->Get(context, 2)).As<v8::Array>();
    uint32_t nlev = levels->Length();
    Rcpp::CharacterVector lev(static_cast<R_xlen_t>(nlev));
    for(uint32_t i = 0; i < nlev; i++){
      v8::String::Utf8Value str(isolate, safe_to_local(levels->Get(context, i)));
      lev[i] = Rf_mkCharCE(*str, CE_UTF8);
    }
    size_t n = codes->Length();
    std::vector<int32_t> code(n);
    codes->CopyContents(code.data(), n * sizeof(int32_t));
    Rcpp::CharacterVector x(static_cast<R_xlen_t>(n));
    for(size_t i = 0; i < n; i++)
      x[static_cast<R_xlen_t>(i)] = (code[i] == 0) ? NA_STRING : STRING_ELT(lev, code[i] - 1);
    return x;
  }
  case 5: {
    v8::Local<v8::ArrayBufferView> view = safe_to_local(node->Get(context, 1)).As<v8::ArrayBufferView>();
    Rcpp::RawVector x(static_cast<R_xlen_t>(view->ByteLength()));
    view->CopyContents(x.begin(), view->ByteLength());
    return x;
  }
  case 6:
  case 7: {
    v8::Local<v8::Array> children = safe_to_local(node->Get(context, 1)).As<v8::Array>();
    v8::Local<v8::Value> names = safe_to_local(node->Get(context, 2));
    uint32_t n = children->Length();
    Rcpp::List x(static_cast<R_xlen_t>(n));
    for(uint32_t i = 0; i < n; i++)
      x[i] = node_to_r(safe_to_local(children->Get(context, i)), context);
    Rcpp::CharacterVector nm(static_cast<R_xlen_t>(names->IsArray() ? n : 0));
    if(names->IsArray()){
      v8::Local<v8::Array> namearr = names.As<v8::Array>();
      for(uint32_t i = 0; i < n; i++){
        v8::String::Utf8Value str(isolate, safe_to_local(namearr->Get(context, i)));
        nm[i] = Rf_mkCharCE(*str, CE_UTF8);
      }
      x.attr("names") = nm;
    }
    if(tag == 6)
      return x;
    int nrow = static_cast<int>(safe_to_local(node->Get(context, 3))->NumberValue(context).FromMaybe(0));
    Rcpp::RObject rownames = Rcpp::IntegerVector::create(NA_INTEGER, -nrow);
    for(uint32_t i = 0; i < n; i++){
      if(std::string(nm[i]) != "_row")
        continue;
      SEXP rn = x[i];
      bool valid = TYPEOF(rn) == STRSXP && !Rf_any_duplicated(rn, FALSE);
      for(R_xlen_t k = 0; valid && k < Rf_xlength(rn); k++)
        valid = STRING_ELT(rn, k) != NA_STRING;
      if(valid){
        rownames = rn;
        Rcpp::List keep(static_cast<R_xlen_t>(n - 1));
        Rcpp::CharacterVector keepnames(static_cast<R_xlen_t>(n - 1));
        for(uint32_t k = 0, m = 0; k < n; k++){
          if(k == i)
            continue;
          keep[m] = x[k];
          keepnames[m] = nm[k];
          m++;
        }
        keep.attr("names") = keepnames;
        x = keep;
      }
      break;
    }
    x.attr("row.names") = rownames;
    x.attr("class") = "data.frame";
    return x;
  }
  case 8: {
    Rcpp::RObject x = node_to_r(safe_to_local(node->Get(context, 1)), context);
    v8::Local<v8::Array> dims = safe_to_local(node->Get(context, 2)).As<v8::Array>();
    uint32_t nd = dims->Length();
    Rcpp::IntegerVector dim(static_cast<R_xlen_t>(nd));
    for(uint32_t i = 0; i < nd; i++)
      dim[i] = safe_to_local(dims->Get(context, i))->Int32Value(context).FromMaybe(0);
    x.attr("dim") = dim;
    return x;
  }
  }
  throw std::runtime_error("Invalid node in native conversion.");
}

static Rcpp::RObject js_to_r(v8::Local<v8::Value> value, v8::Local<v8::Value> options, v8::Local<v8::Context> context){
  v8::HandleScope scope(isolate);
  v8::TryCatch trycatch(isolate);
  v8::Local<v8::Function> fn = compile_function(native_encode_js, context);
  if(fn.IsEmpty())
    throw std::runtime_error("Failed to compile native encoder.");
  v8::Local<v8::Value> undefined = v8::Undefined(isolate);
  v8::Local<v8::Value> argv[2] = {
    value.IsEmpty() ? undefined : value,
    options.IsEmpty() ? undefined : options
  };
  v8::Local<v8::Value> node = safe_to_local(fn->Call(context, context->Global(), 2, argv));
  if(node.IsEmpty()){
    v8::String::Utf8Value exception(isolate, trycatch.Exception());
    throw std::runtime_error(ToCString(exception));
  }
  return node_to_r(node, context);
}

// [[Rcpp::export]]
bool write_native(Rcpp::String key, SEXP value, SEXP options, ctxptr ctx){
  if(!ctx)
    throw std::runtime_error("v8::Context has been disposed.");

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = ctx.checked_get()->Get(isolate);
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Value> out = r_to_js(value, options, context);
  v8::Local<v8::String> name = ToJSString(key.get_cstring());
  v8::Local<v8::Object> global = context->Global();
  if(!global->Has(context, name).FromMaybe(true) || !global->Delete(context, name).IsNothing())
    return !global->Set(context, name, out).IsNothing();
  return false;
}

// [[Rcpp::export]]
Rcpp::RObject context_eval(Rcpp::String src, ctxptr ctx, bool serialize = false, bool await = false, bool native = false, SEXP options = R_NilValue){
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
  if(serialize == true){
    if(native)
      return js_to_r(result, options_to_js(options), context);
    return convert_object(result);
  }

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
ctxptr make_context(bool set_console, bool native = true){
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
  if(context->Global()->SetPrivate(context, v8::Private::ForApi(isolate, ToJSString("V8::native")), v8::Boolean::New(isolate, native)).IsNothing())
    Rcpp::warning("Could not set backend.");
  ctx_type *ptr = new ctx_type(isolate, context);
  return ctxptr(ptr);
}
