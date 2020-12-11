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

// Octree Struct
struct OctreeNode {
	Vec3i32 origin;
	int edgeLength;
	int level;
	std::vector<int> sliceDataIndex;
	OctreeNode* sub_node[8] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	Vec3u8* color = NULL;
};


class Slice {
public:
	Slice(const Vec3i32& origin, int edgeLength, int clipDepth) :
		origin(origin),
		edgeLength(edgeLength),
		clipDepth(clipDepth),
		isChromaSubsampling(false),
		quantizationBits(0),
		avgColor(Vec3u8::zero()),
		octree_root(NULL),
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

	// 构造八叉树
	void Construct_Octree_From_Slice();

	// 对八叉树的叶子节点计算属性（颜色）的差分 （含量化）
	void Octree_Compute_Attribute_Diff();

	// 对八叉树的叶子节点做色度抽样
	void Octree_Chroma_Subsample();

	// 八叉树压缩
	void Octree_encode();

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
	int quantizationBits;
	Vec3u8 avgColor;
	std::vector<Point> points;
	OctreeNode* octree_root;
	std::vector<char> encodedTree;
	std::vector<int8_t> encodedColor;

	EntropyEncodeType treeEntropyType;
	EntropyEncodeType colorEntropyType;
	string entropyTree;
	string entropyColor;
};
