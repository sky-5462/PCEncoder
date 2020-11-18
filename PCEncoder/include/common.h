#pragma once

#include <vector>
#include <cinttypes>
#include <exception>

// 这个可以调库
template <typename T>
class Vec3 {
public:
	T x;
	T y;
	T z;

	Vec3() {}
	Vec3(T val1, T val2, T val3) : x(val1), y(val2), z(val3) {}

	// 类型转换
	// 与下面的符合重载联用会导致可能导致隐式的错误类型转换，这样子设计可能不太合理
	template<typename T2>
	Vec3(T2&& rhs) {
		x = rhs.x;
		y = rhs.y;
		z = rhs.z;
	}

	Vec3 operator +(const Vec3& rhs) const {
		return Vec3({ x + rhs.x, y + rhs.y, z + rhs.z });
	}
	Vec3 operator -(const Vec3& rhs) const {
		return Vec3({ x - rhs.x, y - rhs.y, z - rhs.z });
	}
	Vec3 operator +(T val) const {
		return Vec3({ x + val, y + val, z + val });
	}
	Vec3 operator -(T val) const {
		return Vec3({ x - val, y - val, z - val });
	}
	Vec3 operator *(T val) const {
		return Vec3({ x * val, y * val, z * val });
	}
	Vec3 operator /(T val) const {
		return Vec3({ x / val, y / val, z / val });
	}
	Vec3 operator %(T val) const {
		return Vec3({ x % val, y % val, z % val });
	}
	bool operator == (const Vec3& rhs) const {
		return x == rhs.x && y == rhs.y && z == rhs.z;
	}
	bool operator !=(const Vec3& rhs) const {
		return !(*this == rhs);
	}
	void operator +=(const Vec3& rhs) {
		x += rhs.x;
		y += rhs.y;
		z += rhs.z;
	}
	void operator -=(const Vec3& rhs) {
		x -= rhs.x;
		y -= rhs.y;
		z -= rhs.z;
	}
	Vec3 multiplyEach(const Vec3& rhs) const {
		return Vec3({ x * rhs.x, y * rhs.y, z * rhs.z });
	}
	Vec3 divideEach(const Vec3& rhs) const {
		return Vec3({ x / rhs.x, y / rhs.y, z / rhs.z });
	}
	Vec3 multiplyEach(T val) const {
		return *this * val;
	}
	Vec3 divideEach(T val) const {
		return *this / val;
	}
	void minAtPlace(const Vec3& rhs) {
		x = (x < rhs.x ? x : rhs.x);
		y = (y < rhs.y ? y : rhs.y);
		z = (z < rhs.z ? z : rhs.z);
	}
	void maxAtPlace(const Vec3& rhs) {
		x = (x > rhs.x ? x : rhs.x);
		y = (y > rhs.y ? y : rhs.y);
		z = (z > rhs.z ? z : rhs.z);
	}

	static Vec3 zero() {
		return Vec3(0, 0, 0);
	}
};

using Vec3u8 = Vec3<uint8_t>;
using Vec3i32 = Vec3<int>;


struct Point {
	Vec3i32 position;
	Vec3u8 color;
};

class PointBuffer {
public:
	PointBuffer() : max(INT_MIN, INT_MIN, INT_MIN), min(INT_MAX, INT_MAX, INT_MAX) {}
	void push(int x, int y, int z, int r, int g, int b) {
		Vec3i32 position(x, y, z);
		points.push_back({ position, Vec3u8(r, g, b) });
		max.maxAtPlace(position);
		min.minAtPlace(position);
	}
	auto getVolumn() const {
		return max;
	}
	auto begin() const {
		return points.begin();
	}
	auto end() const {
		return points.end();
	}
	void clear() {
		points.clear();
	}
	auto size() const {
		return points.size();
	}
	void insertPoints(const std::vector<Point>& pointSet) {
		points.insert(points.end(), pointSet.begin(), pointSet.end());
	}

private:
	std::vector<Point> points;
	Vec3i32 max;
	Vec3i32 min;
};
