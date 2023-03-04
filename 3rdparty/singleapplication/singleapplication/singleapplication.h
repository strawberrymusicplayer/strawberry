#ifndef SINGLEAPPLICATION_H
#define SINGLEAPPLICATION_H

#ifdef SINGLEAPPLICATION
#  error "SINGLEAPPLICATION already defined."
#endif

#define SINGLEAPPLICATION
#include "../singleapplication_t.h"
#undef SINGLEAPPLICATION_T_H
#undef SINGLEAPPLICATION

#endif  // SINGLEAPPLICATION_H
