#include <v8.h>
#include <Rcpp.h>

#if (V8_MAJOR_VERSION * 100 + V8_MINOR_VERSION) >= 1001
typedef v8::Global<v8::Context> ctx_type;
#else
typedef v8::Persistent<v8::Context> ctx_type;
#endif

// typedef Rcpp::XPtr< ctx_type > v8_xptr;
void ctx_finalizer(ctx_type* ctx);
typedef Rcpp::XPtr< ctx_type, Rcpp::PreserveStorage, ctx_finalizer> ctxptr;
