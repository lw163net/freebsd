// RUN: clang-cc -fsyntax-only -verify %s

struct S {};
S E0;

namespace {
  enum {
    E0 = 1,
    E1 = E0 + 1
  };
}


