#pragma once

#include "pw_preprocessor/compiler.h"

// Deklariere die Funktion, damit der Compiler sie kennt
#ifdef __cplusplus
extern "C" {
#endif
void pw_assert_basic_HandleFailure(const char* message);
#ifdef __cplusplus
}
#endif

// Das Makro, das Pigweed intern aufruft
//#define PW_ASSERT_HANDLE_FAILURE(message) pw_assert_basic_HandleFailure(message)