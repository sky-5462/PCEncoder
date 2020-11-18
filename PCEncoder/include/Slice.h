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

	void addPoint(const Point& p) {
		points.push_back(p);
	}
	const std::vector<Point>& getPoints() const {
		return points;
	}

	bool empty() const {
		return points.empty();
	}

	void encode();
	std::vector<Point> decode();

	std::string serialize() const;
	static Slice parse(std::string_view view);

private:
	Vec3i32 origin;
	int edgeLength;
	int clipDepth;
	bool isChromaSubsampling;
	std::vector<Point> points;
	std::vector<char> encodedTree;
	std::vector<uint8_t> encodedColor;
};
