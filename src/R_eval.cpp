#include <Rcpp.h>
#include "jseval.h"
using namespace Rcpp;

// [[Rcpp::export]]
std::string jseval( std::vector< std::string > code ) {
  return jseval_string(code[0]);
}

// [[Rcpp::export]]
bool jsvalidate(std::vector< std::string > code) {
  return jsvalidate_string(code[0]);
}
