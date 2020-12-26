#pragma once
#pragma comment(lib, ".\\lib\\zlibstat.lib") 
#include <zlib.h>
#include <huffman.h>
#include <zlibfunc.h>
#include <RLE.h>

#include <common.h>
#include <string>
#include <string_view>
#include <memory>
#include <array>

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

// Octree Struct
struct OctreeNode {
	Vec3i32 origin;
	int edgeLength;
	int level;
	Vec3u8 rawColor;
	Vec3u8 predictedColor;
	std::vector<int> sliceDataIndex;
	std::array<std::unique_ptr<OctreeNode>, 8> sub_node;
};

class Slice {
public:
	Slice(const Vec3i32& origin, int edgeLength) :
		origin(origin),
		edgeLength(edgeLength),
		isChromaSubsampling(false),
		quantizationBits(0),
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
	void setquantizationBits(int quantizationBits) {
		this->quantizationBits = quantizationBits;
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

	// 构造八叉树并进行色度采样
	void Construct_Octree_From_Slice();

	// 对八叉树的叶子节点计算属性（颜色）的差分 （含量化）
	void Octree_Compute_Attribute_Diff();

	// 八叉树压缩
	void Octree_encode();

	std::vector<Point> decode();

	std::string serialize();
	static Slice parse(std::string_view view);

	// 用DFS遍历得到各个通道的原始颜色
	std::array<std::vector<uint8_t>, 3> getRawColorWithDFS() const;

private:
	Vec3i32 origin;
	int edgeLength;
	bool isChromaSubsampling;
	int quantizationBits;
	Vec3u8 avgColor;
	std::vector<Point> points;
	std::unique_ptr<OctreeNode> octree_root;

	std::vector<char> treeStream;
	std::vector<int8_t> colorStream;

	EntropyEncodeType treeEntropyType;
	EntropyEncodeType colorEntropyType;
};
