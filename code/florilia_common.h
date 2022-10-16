#ifndef FLORILIA_COMMON_H
#define FLORILIA_COMMON_H


#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#define internal static

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef int8_t  b8;
typedef int16_t b16;
typedef int32_t b32;
typedef int64_t b64;

typedef float  f32;
typedef double f64;

// Todo: rename to Kibibyte, Mebibyte, ...? 
#define Kilobytes(x) (1024*x)
#define Megabytes(x) (1024*Kilobytes(x))
#define Gigabytes(x) (1024*Megabytes(x))

#define ARRAY_COUNT(array) sizeof(array) / sizeof(array[0])


#endif // FLORILIA_COMMON_H
