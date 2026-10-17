#include <libplatform/libplatform.h>
#include "V8_types.h"
#include <fstream>
#include <vector>

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

enum column_kind { COL_MISSING = 0, COL_INTEGER = 1, COL_DOUBLE = 2, COL_LOGICAL = 3, COL_STRING = 4, COL_FACTOR = 5 };

static const char* rows_to_columns_js = R"JS(
(function (rows) {
  "use strict";
  const n = rows.length;
  if (n === 0) return null;
  const keys = [];
  const seen = new Map();
  for (let i = 0; i < n; i++) {
    const row = rows[i];
    if (row === null || typeof row !== "object") return null;
    const proto = Object.getPrototypeOf(row);
    if (proto !== Object.prototype && proto !== null) return null;
    const rk = Object.keys(row);
    for (let j = 0; j < rk.length; j++) {
      if (!seen.has(rk[j])) {
        seen.set(rk[j], true);
        keys.push(rk[j]);
      }
    }
  }
  const p = keys.length;
  if (p === 0) return null;
  const scan = `"use strict";
    let kind = 0;
    for (let i = 0; i < n; i++) {
      const v = rows[i][KEY];
      if (v === undefined || v === null) continue;
      let vk;
      switch (typeof v) {
        case "number":
          vk = (Number.isInteger(v) && v > -2147483648 && v <= 2147483647) ? 1 : 2;
          break;
        case "boolean":
          vk = 3;
          break;
        case "string":
          vk = 4;
          break;
        default:
          return null;
      }
      if (kind === 0 || kind === vk) {
        kind = vk;
      } else if (kind <= 2 && vk <= 2) {
        kind = 2;
      } else {
        return null;
      }
    }
    if (kind === 0) return [kind, null, null];
    if (kind === 4) {
      const codes = new Int32Array(n);
      const levels = [];
      const index = new Map();
      for (let i = 0; i < n; i++) {
        const v = rows[i][KEY];
        if (typeof v !== "string") continue;
        let code = index.get(v);
        if (code === undefined) {
          levels.push(v);
          code = levels.length;
          index.set(v, code);
        }
        codes[i] = code;
      }
      return [kind, codes, levels];
    }
    const col = (kind === 2) ? new Float64Array(n) : new Int32Array(n);
    let mask = null;
    for (let i = 0; i < n; i++) {
      const v = rows[i][KEY];
      if (v === undefined || v === null) {
        if (mask === null) mask = new Uint8Array(n);
        mask[i] = 1;
      } else {
        col[i] = v;
      }
    }
    return [kind, col, mask];`;
  const kinds = new Int32Array(p);
  const cols = new Array(p);
  const masks = new Array(p);
  for (let j = 0; j < p; j++) {
    const res = (new Function("rows", "n", scan.split("KEY").join(JSON.stringify(keys[j]))))(rows, n);
    if (res === null) return null;
    kinds[j] = res[0];
    cols[j] = res[1];
    masks[j] = res[2];
  }
  return [keys, kinds, cols, masks];
})
)JS";

static const char* columns_to_rows_js = R"JS(
(function (names, kinds, cols, extra, has_na, n) {
  "use strict";
  const p = names.length;
  const values = new Array(p);
  const checks = new Array(p);
  let any_na = false;
  for (let j = 0; j < p; j++) {
    const c = "c" + j + "[i]";
    switch (kinds[j]) {
      case 1: values[j] = c; checks[j] = c + " !== NA"; break;
      case 2: values[j] = c; checks[j] = "x" + j + "[i] === 0"; break;
      case 3: values[j] = "(" + c + " !== 0)"; checks[j] = c + " !== NA"; break;
      case 4: values[j] = c; checks[j] = c + " !== null"; break;
      case 5: values[j] = "x" + j + "[" + c + " - 1]"; checks[j] = c + " !== NA"; break;
    }
    if (has_na[j]) any_na = true;
  }
  let src = "\"use strict\";\nconst NA = -2147483648;\n";
  for (let j = 0; j < p; j++)
    src += "const c" + j + " = cols[" + j + "], x" + j + " = extra[" + j + "];\n";
  src += "const rows = new Array(n);\nfor (let i = 0; i < n; i++) {\n";
  if (any_na) {
    src += "const row = {};\n";
    for (let j = 0; j < p; j++) {
      const key = JSON.stringify(names[j]);
      const store = (names[j] === "__proto__") ?
        "Object.defineProperty(row, " + key + ", {value: " + values[j] + ", enumerable: true, writable: true, configurable: true});" :
        "row[" + key + "] = " + values[j] + ";";
      src += has_na[j] ? "if (" + checks[j] + ") " + store + "\n" : store + "\n";
    }
    src += "rows[i] = row;\n";
  } else {
    const fields = new Array(p);
    for (let j = 0; j < p; j++)
      fields[j] = "[" + JSON.stringify(names[j]) + "]: " + values[j];
    src += "rows[i] = {" + fields.join(", ") + "};\n";
  }
  src += "}\nreturn rows;\n";
  return (new Function("cols", "extra", "n", src))(cols, extra, n);
})
)JS";

static v8::Local<v8::Function> compile_function(const char* code, v8::Local<v8::Context> context){
  v8::Local<v8::Script> script = compile_source(code, context);
  if(script.IsEmpty())
    return v8::Local<v8::Function>();
  v8::Local<v8::Value> fn = safe_to_local(script->Run(context));
  if(fn.IsEmpty() || !fn->IsFunction())
    return v8::Local<v8::Function>();
  return fn.As<v8::Function>();
}

static Rcpp::RObject rows_to_data_frame(v8::Local<v8::Value> value, v8::Local<v8::Context> context){
  if(value.IsEmpty() || !value->IsArray())
    return R_NilValue;

  v8::TryCatch trycatch(isolate);
  v8::Local<v8::Function> fn = compile_function(rows_to_columns_js, context);
  if(fn.IsEmpty())
    return R_NilValue;
  v8::Local<v8::Value> argv[1] = {value};
  v8::Local<v8::Value> out = safe_to_local(fn->Call(context, context->Global(), 1, argv));
  if(out.IsEmpty() || !out->IsArray())
    return R_NilValue;

  v8::Local<v8::Array> parts = out.As<v8::Array>();
  v8::Local<v8::Array> keys = safe_to_local(parts->Get(context, 0)).As<v8::Array>();
  v8::Local<v8::Int32Array> kinds = safe_to_local(parts->Get(context, 1)).As<v8::Int32Array>();
  v8::Local<v8::Array> cols = safe_to_local(parts->Get(context, 2)).As<v8::Array>();
  v8::Local<v8::Array> masks = safe_to_local(parts->Get(context, 3)).As<v8::Array>();

  uint32_t n = value.As<v8::Array>()->Length();
  uint32_t p = keys->Length();
  R_xlen_t nr = static_cast<R_xlen_t>(n);

  std::vector<int32_t> kind(p);
  kinds->CopyContents(kind.data(), p * sizeof(int32_t));

  Rcpp::List df(static_cast<R_xlen_t>(p));
  Rcpp::CharacterVector names(static_cast<R_xlen_t>(p));
  std::vector<uint8_t> mask(n);

  for(uint32_t j = 0; j < p; j++){
    v8::HandleScope column_scope(isolate);
    v8::String::Utf8Value key(isolate, safe_to_local(keys->Get(context, j)));
    names[j] = Rcpp::String(*key, CE_UTF8);

    v8::Local<v8::Value> col = safe_to_local(cols->Get(context, j));
    v8::Local<v8::Value> colmask = safe_to_local(masks->Get(context, j));
    bool has_mask = !colmask.IsEmpty() && colmask->IsUint8Array();
    if(has_mask)
      colmask.As<v8::Uint8Array>()->CopyContents(mask.data(), n);

    switch(kind[j]){
    case COL_INTEGER: {
      Rcpp::IntegerVector x(nr);
      col.As<v8::Int32Array>()->CopyContents(x.begin(), n * sizeof(int32_t));
      if(has_mask){
        for(uint32_t i = 0; i < n; i++)
          if(mask[i]) x[i] = NA_INTEGER;
      }
      df[j] = x;
      break;
    }
    case COL_DOUBLE: {
      Rcpp::NumericVector x(nr);
      col.As<v8::Float64Array>()->CopyContents(x.begin(), n * sizeof(double));
      if(has_mask){
        for(uint32_t i = 0; i < n; i++)
          if(mask[i]) x[i] = NA_REAL;
      }
      df[j] = x;
      break;
    }
    case COL_LOGICAL: {
      Rcpp::LogicalVector x(nr);
      col.As<v8::Int32Array>()->CopyContents(x.begin(), n * sizeof(int32_t));
      if(has_mask){
        for(uint32_t i = 0; i < n; i++)
          if(mask[i]) x[i] = NA_LOGICAL;
      }
      df[j] = x;
      break;
    }
    case COL_STRING: {
      v8::Local<v8::Array> levels = colmask.As<v8::Array>();
      uint32_t nlev = levels->Length();
      Rcpp::CharacterVector lev(static_cast<R_xlen_t>(nlev));
      for(uint32_t i = 0; i < nlev; i++){
        v8::String::Utf8Value str(isolate, safe_to_local(levels->Get(context, i)));
        lev[i] = Rf_mkCharCE(*str, CE_UTF8);
      }
      std::vector<int32_t> codes(n);
      col.As<v8::Int32Array>()->CopyContents(codes.data(), n * sizeof(int32_t));
      Rcpp::CharacterVector x(nr);
      for(uint32_t i = 0; i < n; i++)
        x[i] = (codes[i] == 0) ? NA_STRING : STRING_ELT(lev, codes[i] - 1);
      df[j] = x;
      break;
    }
    default:
      df[j] = Rcpp::LogicalVector(nr, NA_LOGICAL);
    }
  }

  if(trycatch.HasCaught())
    return R_NilValue;

  df.attr("names") = names;
  df.attr("row.names") = Rcpp::IntegerVector::create(NA_INTEGER, -static_cast<int>(n));
  df.attr("class") = "data.frame";
  return df;
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

static v8::Local<v8::Array> to_js_strings(SEXP x){
  R_xlen_t len = Rf_xlength(x);
  std::vector<v8::Local<v8::Value>> out(static_cast<size_t>(len));
  for(R_xlen_t i = 0; i < len; i++){
    SEXP el = STRING_ELT(x, i);
    if(el == NA_STRING){
      out[static_cast<size_t>(i)] = v8::Null(isolate);
    } else {
      out[static_cast<size_t>(i)] = ToJSString(Rf_translateCharUTF8(el));
    }
  }
  return v8::Array::New(isolate, out.data(), out.size());
}

// [[Rcpp::export]]
bool write_data_frame(Rcpp::String key, Rcpp::List cols, int nrow, ctxptr ctx){
  if(!ctx)
    throw std::runtime_error("v8::Context has been disposed.");

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = ctx.checked_get()->Get(isolate);
  v8::Context::Scope context_scope(context);
  v8::TryCatch trycatch(isolate);

  if(nrow < 0)
    throw std::invalid_argument("nrow must be non-negative");
  size_t n = static_cast<size_t>(nrow);
  size_t p = static_cast<size_t>(cols.size());
  SEXP colnames = Rf_getAttrib(cols, R_NamesSymbol);
  if(Rf_xlength(colnames) != cols.size())
    throw std::invalid_argument("all columns must be named");

  std::vector<v8::Local<v8::Value>> names(p);
  std::vector<v8::Local<v8::Value>> kinds(p);
  std::vector<v8::Local<v8::Value>> jscols(p);
  std::vector<v8::Local<v8::Value>> extra(p);
  std::vector<v8::Local<v8::Value>> has_na(p);

  for(size_t j = 0; j < p; j++){
    R_xlen_t rj = static_cast<R_xlen_t>(j);
    SEXP col = VECTOR_ELT(cols, rj);
    if(static_cast<size_t>(Rf_xlength(col)) != n)
      throw std::invalid_argument("all columns must have nrow elements");
    names[j] = ToJSString(Rf_translateCharUTF8(STRING_ELT(colnames, rj)));
    extra[j] = v8::Null(isolate);
    bool col_na = false;
    int kind;
    switch(TYPEOF(col)){
    case REALSXP: {
      kind = COL_DOUBLE;
      jscols[j] = v8::Float64Array::New(copy_to_array_buffer(REAL(col), n * sizeof(double)), 0, n);
      const double* x = REAL(col);
      size_t first_na = n;
      for(size_t i = 0; i < n; i++){
        if(R_IsNA(x[i])){
          first_na = i;
          break;
        }
      }
      if(first_na < n){
        std::vector<uint8_t> mask(n, 0);
        for(size_t i = first_na; i < n; i++)
          mask[i] = R_IsNA(x[i]) ? 1 : 0;
        extra[j] = v8::Uint8Array::New(copy_to_array_buffer(mask.data(), n), 0, n);
        col_na = true;
      }
      break;
    }
    case INTSXP:
    case LGLSXP: {
      kind = (TYPEOF(col) == LGLSXP) ? COL_LOGICAL : (Rf_isFactor(col) ? COL_FACTOR : COL_INTEGER);
      const int* x = (TYPEOF(col) == LGLSXP) ? LOGICAL(col) : INTEGER(col);
      jscols[j] = v8::Int32Array::New(copy_to_array_buffer(x, n * sizeof(int)), 0, n);
      for(size_t i = 0; i < n && !col_na; i++)
        col_na = (x[i] == NA_INTEGER);
      if(kind == COL_FACTOR)
        extra[j] = to_js_strings(Rf_getAttrib(col, R_LevelsSymbol));
      break;
    }
    case STRSXP:
      kind = COL_STRING;
      jscols[j] = to_js_strings(col);
      for(R_xlen_t i = 0; i < Rf_xlength(col) && !col_na; i++)
        col_na = (STRING_ELT(col, i) == NA_STRING);
      break;
    default:
      throw std::invalid_argument("unsupported column type");
    }
    kinds[j] = v8::Integer::New(isolate, kind);
    has_na[j] = v8::Boolean::New(isolate, col_na);
  }

  v8::Local<v8::Function> fn = compile_function(columns_to_rows_js, context);
  if(fn.IsEmpty())
    throw std::runtime_error("Failed to compile data frame converter.");

  v8::Local<v8::Value> argv[6] = {
    v8::Array::New(isolate, names.data(), p),
    v8::Array::New(isolate, kinds.data(), p),
    v8::Array::New(isolate, jscols.data(), p),
    v8::Array::New(isolate, extra.data(), p),
    v8::Array::New(isolate, has_na.data(), p),
    v8::Number::New(isolate, static_cast<double>(n))
  };
  v8::Local<v8::Value> rows = safe_to_local(fn->Call(context, context->Global(), 6, argv));
  if(rows.IsEmpty()){
    v8::String::Utf8Value exception(isolate, trycatch.Exception());
    throw std::runtime_error(ToCString(exception));
  }

  v8::Local<v8::String> name = ToJSString(key.get_cstring());
  v8::Local<v8::Object> global = context->Global();
  if(!global->Has(context, name).FromMaybe(true) || !global->Delete(context, name).IsNothing())
    return !global->Set(context, name, rows).IsNothing();
  return false;
}

// [[Rcpp::export]]
Rcpp::RObject context_eval(Rcpp::String src, ctxptr ctx, bool serialize = false, bool await = false, bool native = false){
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
    if(native){
      Rcpp::RObject df = rows_to_data_frame(result, context);
      if(!df.isNULL())
        return df;
    }
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
