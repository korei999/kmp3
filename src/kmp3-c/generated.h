#pragma once

#include "klib/String.h" /* IWYU pragma: keep */

#define K_NAME VecChar
#define K_TYPE char
#include "klib/VecGen-inc.h"

#define K_NAME VecPChar
#define K_TYPE char*
#define K_SIZE_T int32_t
#include "klib/VecGen-inc.h"

#define K_NAME VecSV
#define K_TYPE k_StringView
#define K_SIZE_T int32_t
#include "klib/VecGen-inc.h"

#define K_NAME VecU16
#define K_TYPE uint16_t
#define K_SIZE_T int32_t
#include "klib/VecGen-inc.h"

#define K_NAME VecWChar
#define K_TYPE wchar_t
#define K_SIZE_T int32_t
#include "klib/VecGen-inc.h"

#define K_NAME VecString
#define K_TYPE k_String
#define K_SIZE_T int32_t
#include "klib/VecGen-inc.h"
