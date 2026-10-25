#pragma once

#include "klib/StringView.h" /* IWYU pragma: keep */

#define K_NAME VecChar
#define K_TYPE char
#include "klib/VecGen-inc.h"

#define K_NAME VecPChar
#define K_TYPE char*
#include "klib/VecGen-inc.h"

#define K_NAME VecSV
#define K_TYPE k_StringView
#include "klib/VecGen-inc.h"

#define K_NAME VecU16
#define K_TYPE uint16_t
#include "klib/VecGen-inc.h"

#define K_NAME VecWChar
#define K_TYPE wchar_t
#include "klib/VecGen-inc.h"
