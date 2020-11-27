#pragma once

#include <common.h>
#include <string>
#include <string_view>

struct EncodeTreeNode {
	Vec3i32 origin;
	int edgeLength;
	int level;
	std::vector<int> sliceDataIndex;
};

struct DecodeTreeNode {
	Vec3i32 origin;
	int edgeLength;
	int level;
};

class Slice {
public:
	Slice(const Vec3i32& origin, int edgeLength, int clipDepth, bool isChromaSubsampling) : 
		origin(origin), edgeLength(edgeLength), clipDepth(clipDepth), isChromaSubsampling(isChromaSubsampling) {}
	Slice(const Vec3i32& origin, int edgeLength, int clipDepth, bool isChromaSubsampling, const Vec3u8& avgColor) : 
		origin(origin), edgeLength(edgeLength), clipDepth(clipDepth), isChromaSubsampling(isChromaSubsampling), avgColor(avgColor) {}
	Slice(const Vec3i32& origin, int edgeLength, int clipDepth, bool isChromaSubsampling, const std::vector<Point>& points) :
		origin(origin), edgeLength(edgeLength), clipDepth(clipDepth), isChromaSubsampling(isChromaSubsampling), points(points) {}

	void addPoint(const Point& p) {
		points.push_back(p);
	}
	const std::vector<Point>& getPoints() const {
		return points;
	}

	bool empty() const {
		return points.empty();
	}

	static std::vector<Slice> split(const Slice& slice);

	void encode();
	std::vector<Point> decode();

	std::string serialize() const;
	static Slice parse(std::string_view view);

private:
	Vec3i32 origin;
	int edgeLength;
	int clipDepth;
	bool isChromaSubsampling;
	Vec3u8 avgColor;
	std::vector<Point> points;
	std::vector<char> encodedTree;
	std::vector<int8_t> encodedColor;
};
