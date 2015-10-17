struct S { ~S(); };
void bar();
void foo() {
  S s;
  bar();
}
