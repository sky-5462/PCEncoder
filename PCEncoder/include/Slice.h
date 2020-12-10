#pragma once
#pragma comment(lib, ".\\lib\\zlibstat.lib") 
#include <zlib.h>
#include <huffman.h>
#include <zlibfunc.h>
#include <RLE.h>

#include <common.h>
#include <string>
#include <string_view>

enum class EntropyEncodeType {
	NONE,
	RLE,
	HUFFMAN,
	RLE_HUFFMAN,
	ZLIB
};

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
	Slice(const Vec3i32& origin, int edgeLength, int clipDepth) :
		origin(origin),
		edgeLength(edgeLength),
		clipDepth(clipDepth),
		isChromaSubsampling(false),
		avgColor(Vec3u8::zero()),
		treeEntropyType(EntropyEncodeType::NONE),
		colorEntropyType(EntropyEncodeType::NONE) {
	}

	void addPoint(const Point& p) {
		points.push_back(p);
	}
	const std::vector<Point>& getPoints() const {
		return points;
	}
	bool empty() const {
		return points.empty();
	}

	void setChromaSubsampling(bool isChromaSubsampling) {
		this->isChromaSubsampling = isChromaSubsampling;
	}
	void setAvgColor(const Vec3u8& avgColor) {
		this->avgColor = avgColor;
	}
	void setPoints(const std::vector<Point>& points) {
		this->points = points;
	}
	void setEntropyType(EntropyEncodeType treeEntropyType, EntropyEncodeType colorEntropyType) {
		this->treeEntropyType = treeEntropyType;
		this->colorEntropyType = colorEntropyType;
	}


	static std::vector<Slice> split(const Slice& slice);

	void encode(int quantizationBits);
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

	EntropyEncodeType treeEntropyType;
	EntropyEncodeType colorEntropyType;
	string entropyTree;
	string entropyColor;
};
