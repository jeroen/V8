#include <v8.h>
#include <Rcpp.h>
#include "jseval.h"
using namespace v8;
using namespace Rcpp;

//' Validate and evaluate JavaScript code
//'
//' The \code{jsvalidate} function tests if a string is valid JavaScript.
//' The \code{jseval} function evaluates a JavaScript string and returns
//' console output, equivalent to \code{eval()} in JavaScript. These
//' functions are stateless: their context is destroyed after the evaluation
//' has completed.
//'
//' @param code A string with JavaScript code
//' @rdname JavaScript
//' @name JavaScript
//' @return Console output.
//' @examples # Evaluate JavaScript code
//' jseval("JSON.stringify({x:Math.random()})")
//' jseval("(function(x){return x+1;})(123)")
//' jseval(c("foo = 123", "bar = 456", "foo + bar"))
//'
//' # Load a library (doesn't do anything yet)
//' underscore <- system.file("js/underscore.js", package="V8")
//' jseval(readLines(underscore))
//'
//' # Cannot define anonymous function in global scope
//' jsvalidate("function(x){2*x}") #FALSE
//' jsvalidate("function foo(x){2*x}") #TRUE
//' jsvalidate("foo = function(x){2*x}") #TRUE
// [[Rcpp::export]]
std::string jseval( std::vector< std::string > code ) {
  return jseval_string(code);
}

//' @rdname JavaScript
// [[Rcpp::export]]
bool jsvalidate(std::vector< std::string > code) {
  return jsvalidate_string(code);
}

// [[Rcpp::export]]
XPtr< v8::Persistent<v8::Context> > make_context(){
  Persistent<Context> context = Context::New();
  XPtr< Persistent<Context> > ptr(&(context));
  return(ptr);
}

// [[Rcpp::export]]
std::string context_eval(std::vector< std::string > code, XPtr< v8::Persistent<v8::Context> > ptr){
  v8::Persistent<v8::Context> context = *ptr;
  return eval_in_context(code, context);
}
