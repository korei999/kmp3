#pragma once

#include "common.h"

typedef uint8_t K_MAP_RESULT_STATUS;
static const uint8_t K_MAP_RESULT_STATUS_NOT_FOUND = 0;
static const uint8_t K_MAP_RESULT_STATUS_FOUND = 1;
static const uint8_t K_MAP_RESULT_STATUS_INSERTED = 2;
static const uint8_t K_MAP_RESULT_STATUS_FAILED = 3;
static const uint8_t K_MAP_RESULT_STATUS_REMOVED = 4;

typedef uint8_t K_MAP_BUCKET_FLAG;
static const uint8_t K_MAP_BUCKET_FLAG_NONE = 0;
static const uint8_t K_MAP_BUCKET_FLAG_OCCUPIED = 1;
static const uint8_t K_MAP_BUCKET_FLAG_DELETED = 2;

static const float K_MAP_LOAD_FACTOR = 0.5f;
