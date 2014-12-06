#include <v8.h>

std::string jseval_string(std::vector< std::string > code);
bool jsvalidate_string(std::vector< std::string > code);
std::string eval_in_context(std::vector< std::string > code, v8::Persistent<v8::Context> context);
