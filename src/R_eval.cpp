#include <Rcpp.h>
#include "jseval.h"
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
//' @examples jseval("JSON.stringify({x:Math.random()})")
//' jseval("(function(x){return x+1;})(123)")
//' jseval("foo = 123; bar = 456; foo + bar")
//'
//' # Cannot define anonymous function in global scope
//' jsvalidate("function(x){2*x}") #FALSE
//' jsvalidate("function foo(x){2*x}") #TRUE
//' jsvalidate("foo = function(x){2*x}") #TRUE
// [[Rcpp::export]]
std::string jseval( std::vector< std::string > code ) {
  return jseval_string(code[0]);
}

//' @rdname JavaScript
// [[Rcpp::export]]
bool jsvalidate(std::vector< std::string > code) {
  return jsvalidate_string(code[0]);
}
