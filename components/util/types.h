#pragma once

#include <cmath>
#include <cstdlib>
#include <optional>
#include <utility>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using ulong = unsigned long;

struct Vector3 {
  constexpr Vector3(float x, float y, float z)
      : x(x),
        y(y),
        z(z) {}

  constexpr Vector3(Vector3 const& other)
      : Vector3(other.x, other.y, other.z) {}

  constexpr Vector3(Vector3& other)
      : Vector3(other.x, other.y, other.z) {}

  constexpr Vector3(Vector3&& other)
      : Vector3(other.x, other.y, other.z) {}

  constexpr Vector3(float v)
      : Vector3(v, v, v) {}

  constexpr Vector3()
      : Vector3(0) {}

  constexpr Vector3 operator=(Vector3 const& other) {
    this->x = other.x;
    this->y = other.y;
    this->z = other.z;
    return *this;
  }

  constexpr Vector3 operator+(Vector3 const& other) {
    return Vector3(x + other.x, y + other.y, z + other.z);
  }
  constexpr Vector3 operator+=(Vector3 const& other) { return *this + other; }

  constexpr Vector3 operator-(Vector3 const& other) {
    return Vector3(x - other.x, y - other.y, z - other.z);
  }
  constexpr Vector3 operator-=(Vector3 const& other) { return *this - other; }

  constexpr Vector3 operator*(float s) { return Vector3(x * s, y * s, z * s); }
  constexpr Vector3 operator*=(float s) { return *this * s; }

  constexpr Vector3 operator/(float s) { return Vector3(x / s, y / s, z / s); }
  constexpr Vector3 operator/=(float s) { return *this / s; }

  float x;
  float y;
  float z;
};
