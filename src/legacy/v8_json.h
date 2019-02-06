/* Jeroen: old versions do not have the v8::JSON C++ api
 * Hence we just wrap the actual JavaScript functions
 */

#include <v8.h>

v8::Handle<v8::Value> json_parse (v8::Handle<v8::Value> jsonString) {
  v8::HandleScope scope;
  v8::Handle<v8::Context> context = v8::Context::GetCurrent();
  v8::Handle<v8::Object> global = context->Global();
  v8::Handle<v8::Value> jsonValue = global->Get(v8::String::New("JSON"));

  if (! jsonValue->IsObject()) {
    return scope.Close(v8::Undefined());
  }

  v8::Handle<v8::Object> json = jsonValue->ToObject();
  v8::Handle<v8::Value> parse = json->Get(v8::String::New("parse"));

  if (parse.IsEmpty() ||
      ! parse->IsFunction()) {
      return scope.Close(v8::Undefined());
  }

  // cast into a function and call
  v8::Handle<v8::Function> parseFunction = v8::Handle<v8::Function>::Cast(parse);
  return scope.Close(parseFunction->Call(json, 1, &jsonString));
}

v8::Handle<v8::Value> json_stringify (v8::Handle<v8::Value> obj) {
  v8::HandleScope scope;
  v8::Handle<v8::Context> context = v8::Context::GetCurrent();
  v8::Handle<v8::Object> global = context->Global();
  v8::Handle<v8::Value> jsonValue = global->Get(v8::String::New("JSON"));

  if (! jsonValue->IsObject()) {
    return scope.Close(v8::Undefined());
  }

  v8::Handle<v8::Object> json = jsonValue->ToObject();
  v8::Handle<v8::Value> stringify = json->Get(v8::String::New("stringify"));

  if (stringify.IsEmpty() ||
      ! stringify->IsFunction()) {
      return scope.Close(v8::Undefined());
  }

  // cast into a function and call
  v8::Handle<v8::Function> stringifyFunction = v8::Handle<v8::Function>::Cast(stringify);
  return scope.Close(stringifyFunction->Call(json, 1, &obj));
}
