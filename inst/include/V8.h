#include <v8.h>
#include <Rcpp.h>
void ctx_finalizer( v8::Persistent<v8::Context>* ctx );
typedef Rcpp::XPtr<v8::Persistent<v8::Context>, Rcpp::PreserveStorage, ctx_finalizer> ctxptr;
