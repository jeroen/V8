#ifdef __EMSCRIPTEN__

#include "V8_types.h"
#include <fstream>
#include <emscripten.h>

typedef struct {
  int len;
  char* data;
} js_result;

class js_data {
public:
  void operator()(js_result *result) {
    if (result) {
      free(result->data);
      free(result);
    }
  }
};

class js_char {
public:
  void operator()(char *result) { free(result); }
};

/*
 * Note: The webR Worker proxy supports non-standard blocking for a response using `async: false`.
 * We need sync behaviour here to work with webR's blocking communication channel.
 * This functionality is distinct to V8's `await` argument.
 */
EM_JS(js_result*, em_eval, (int ctx, const char* str, bool serialize, bool await), {
  const worker = globalThis._webr_v8_handles.get(ctx);
  if (!worker) {
    globalThis._webr_errmsg = "Invalid context";
    return 0;
  }
  const ret = worker.postMessage({cmd: 'eval', src: UTF8ToString(str), serialize, await} , { async: false });
  if (ret.error) {
    globalThis._webr_errmsg = ret.error;
    return 0;
  }

  // This is pretty nasty...
  // Use the first int to indicate if the result is a string or an ArrayBuffer,
  // then cast to a `js_result` struct. Perhaps with wasm64 we'll have enough
  // room for pointer tagging instead?
  const ptr = Module._malloc(8);
  if (ArrayBuffer.isView(ret.result)) {
    const len = ret.result.byteLength;
    const buf = Module._malloc(len);
    HEAP32.set([len], ptr / 4);
    HEAPU8.set(ret.result, buf);
    HEAPU32.set([buf], ptr / 4 + 1);
  } else {
    const len = new Int32Array([-1]);
    const buf = stringToNewUTF8(String(ret.result));
    HEAP32.set(len, ptr / 4);
    HEAPU32.set([buf], ptr / 4 + 1);
  }
  return ptr;
});

EM_JS(char*, em_get_errmsg, (), {
  return stringToNewUTF8(globalThis._webr_errmsg || "");
});

EM_JS(int, em_write_array_buffer, (int ctx, const char* key, unsigned char* data, int len), {
  const worker = globalThis._webr_v8_handles.get(ctx);
  if (!worker) {
    globalThis._webr_errmsg = "Invalid context";
    return 0;
  }

  const buf = HEAPU8.slice(data, data + len);
  const ret = worker.postMessage({ cmd: 'assign', key: UTF8ToString(key), value: buf } , { transfer: [buf.buffer], async: false });
  if (ret.error) {
    globalThis._webr_errmsg = ret.error;
    return -1;
  }
  return ret.result;
});

EM_JS(bool, em_validate, (int ctx, const char* str), {
  const worker = globalThis._webr_v8_handles.get(ctx);
  if (!worker) {
    globalThis._webr_errmsg = "Invalid context";
    return 0;
  }
  const ret = worker.postMessage({cmd: 'validate', src: UTF8ToString(str)} , { async: false });
  return !("error" in ret);
});

EM_JS(int, em_make_context, (), {
  if (!globalThis._webr_v8_handles) {
    globalThis._webr_v8_ctx_count = 0;
    globalThis._webr_v8_gid = 0;
    globalThis._webr_v8_handles = new Map();
  }
  const url = URL.createObjectURL(new Blob([`
    self.console.r = {
      call: (rfn, args = []) => {
        const id = self._webr_v8_gid++;
        self.postMessage({ id, cmd: 'rcall', rfn, args });
        // TODO: Return values requires sync request
        return null;
      },
      get: (name, opts = []) => {
        // TODO: Return values requires sync request
        return null;
      },
      assign: (name, obj, opts = {}) => {
        const id = self._webr_v8_gid++;
        self.postMessage({ id, cmd: 'rassign', name, obj, opts });
        return null;
      },
    };
    self.convert = (data, serialize) => {
      if (serialize) {
        return ArrayBuffer.isView(data) ?  data : JSON.stringify(data)
      }
      return String(data);
    };
    self.onmessage = (ev) => {
      const { uuid, data } = ev.data;
      try {
        switch (data.cmd) {
          case 'eval': {
            const result = self.eval(data.src);
            if (data.await) {
              Promise.resolve(result).then(resolved => {
                self.postMessage({ uuid, result: convert(resolved, data.serialize) });
              }).catch(err => {
                self.postMessage({ uuid, error: err.message });
              });
            } else {
              const tmp = convert(result, data.serialize);
              self.postMessage({ uuid, result: convert(result, data.serialize) }); 
            }
            break;
          }
          case 'assign': {
            self[data.key] = data.value;
            self.postMessage({ uuid, result: true });
            break;
          }
          case 'validate': {
            try {
              new Function(data.src);
              self.postMessage({ uuid, result: true });
            } catch (err) {
              self.postMessage({ uuid, error: err.message });
            }
            break;
          }
        }
      } catch (e) {
        console.error(e);
        self.postMessage({ uuid, error: e.message });
      }
    };
  `]));
  const worker = new Worker(url);
  worker.onmessage = (ev) => {
    const data = ev.data;
    if ('cmd' in data) {
      switch(data.cmd) {
        case 'rcall': {
          const { rfn, args } = data;
          try {
            self.Module.webr.evalR(`do.call(${rfn}, jsonlite::fromJSON(r"-[${JSON.stringify(args)}]-", simplifyVector = FALSE))`);
          } catch (e) {
            console.error("Error in rcall:", e.message);
          }
          break;
        }
        case 'rassign': {
          const { name, obj, opts } = data;
          try {
            self.Module.webr.evalR(`
              ${name} <- jsonlite::fromJSON(
                r"-[${JSON.stringify(obj)}]-",
                jsonlite::fromJSON(r"-[${JSON.stringify(opts)}]-")
              )`);
          } catch (e) {
            console.error("Error in rassign:", e.message);
          }
          break;
        }
      }
    }
  };
  globalThis._webr_v8_handles.set(++globalThis._webr_v8_ctx_count, worker);
  return globalThis._webr_v8_ctx_count;
});

void ctx_finalizer(ctx_type* context ){
  delete context;
}

// [[Rcpp::init]]
void start_v8_isolate(void *dll){
}

// [[Rcpp::export]]
std::string version(){
  return "9999";
}

// [[Rcpp::export]]
Rcpp::RObject context_eval(Rcpp::String src, ctxptr ctx, bool serialize = false, bool await = false){
  if(!ctx)
    throw std::runtime_error("v8::Context has been disposed.");

  std::unique_ptr<js_result, js_data> res(em_eval(*ctx, src.get_cstring(), serialize, await));

  if (!res.get()) {
    std::unique_ptr<char, js_char> err(em_get_errmsg());
    Rcpp::stop(err.get());
  }

  // See above, :/
  js_result* result = res.get(); 
  if (result->len < 0) {
    Rcpp::String str(result->data, CE_UTF8);
    Rcpp::CharacterVector out(1);
    out.at(0) = str;
    return out;
  } else {
    uint32_t length = result->len;
    Rcpp::RawVector out(length);
    memcpy(out.begin(), result->data, out.size());
    return out;
  }
}

// [[Rcpp::export]]
bool write_array_buffer(Rcpp::String key, Rcpp::RawVector data, ctxptr ctx){
  if(!ctx)
    throw std::runtime_error("v8::Context has been disposed.");
  int res = em_write_array_buffer(*ctx, key.get_cstring(), data.begin(), data.size());
  if (res < 0) {
    std::unique_ptr<char, js_char> err(em_get_errmsg());
    Rcpp::stop(err.get());
  }
  return true;
}

// [[Rcpp::export]]
bool context_validate(Rcpp::String src, ctxptr ctx) {
  if(!ctx)
    throw std::runtime_error("v8::Context has been disposed.");
  return em_validate(*ctx, src.get_cstring());
}

// [[Rcpp::export]]
bool context_null(ctxptr ctx) {
  // Test if context still exists
  return(!ctx);
}

// [[Rcpp::export]]
ctxptr make_context(bool set_console){
  int ctx = em_make_context();
  ctx_type *ptr = new ctx_type(ctx);
  return ctxptr(ptr);
}

#endif // __EMSCRIPTEN__
