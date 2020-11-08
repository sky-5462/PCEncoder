#include <iostream>
#include <array>
#include <vector>
#include <fstream>
#include <string>
#include <exception>
#include <memory>
#include <queue>
#include <immintrin.h>
#include <cinttypes>
using namespace std;

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
	bool operator == (const Vec3& rhs) const {
		return x == rhs.x && y == rhs.y && z == rhs.z;
	}
	bool operator !=(const Vec3& rhs) const {
		return !(*this == rhs);
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
	static Vec3 min(const Vec3& lhs, const Vec3& rhs) {
		float x = (lhs.x < rhs.x ? lhs.x : rhs.x);
		float y = (lhs.y < rhs.y ? lhs.y : rhs.y);
		float z = (lhs.z < rhs.z ? lhs.z : rhs.z);
		return Vec3({ x, y, z });
	}
	static Vec3 max(const Vec3& lhs, const Vec3& rhs) {
		float x = (lhs.x > rhs.x ? lhs.x : rhs.x);
		float y = (lhs.y > rhs.y ? lhs.y : rhs.y);
		float z = (lhs.z > rhs.z ? lhs.z : rhs.z);
		return Vec3({ x, y, z });
	}
};

using Vec3f = Vec3<float>;
using Vec3i = Vec3<int>;
using Vec3u8 = Vec3<uint8_t>;

// 应付可能的浮点数输入
struct PointFloat {
	Vec3f position;
	Vec3u8 color;
};

// 内部用整数化存储点
struct PointInt {
	Vec3i position;
	Vec3u8 color;
};

class PCData {
public:
	Vec3i shiftValue;	       // 平移场景使其坐标>=0
	//Vec3f baseVec;             // 基向量长度（如果要加入采样预处理的化就需要这个，这里暂时没有）
	Vec3i sliceCount;          // 沿各坐标轴的分片数目
	int sliceEdgeLength;       // 分片立方体边长（这里暂时使用64）（必须是2的整次幂）

	// 编码时使用三维索引引用，外面套三层vector对内存不友好
	// 解码时作为一维的分片数组
	vector<vector<PointInt>> slices;

	vector<PointInt>& atSlice(int x, int y, int z) {
		int index = x * (sliceCount.y * sliceCount.z) + y * sliceCount.z + z;
		return slices[index];
	}
	vector<PointInt>& atSlice(const Vec3i& indexVec) {
		return this->atSlice(indexVec.x, indexVec.y, indexVec.z);
	}

};

struct EncodeTreeNode {
	//int level;
	Vec3i origin;
	int edgeLength;
	vector<int> sliceDataIndex;
};

struct DecodeTreeNode {
	Vec3i origin;
	int edgeLength;
};

struct EncodedSlice {
	bool hasPoints;
	vector<char> treeBytes;
	vector<Vec3u8> colors;
	// int treeDepth;       // 每个分片内树的最大深度，受到分片大小的制约，log(64)即末尾零的个数(tzcnt)
};

// 暂时只支持ply
PCData readFile(const char* path) {
	ifstream in(path);
	// 暂时不考虑非法格式和文件头解析
	/* 目前的文件头
		ply
		format ascii 1.0
		element vertex {number}
		property float x
		property float y
		property float z
		property uchar red
		property uchar green
		property uchar blue
		end_header
	*/
	int pointNum;
	string skip;
	in >> skip;  // ply
	in >> skip;
	in >> skip;
	in >> skip;  // format
	in >> skip;
	in >> skip;
	in >> pointNum;  // pointNum
	in >> skip;
	in >> skip;
	in >> skip;  // x
	in >> skip;
	in >> skip;
	in >> skip;  // y
	in >> skip;
	in >> skip;
	in >> skip;  // z
	in >> skip;
	in >> skip;
	in >> skip;  // r
	in >> skip;
	in >> skip;
	in >> skip;  // g
	in >> skip;
	in >> skip;
	in >> skip;  // b
	in >> skip;  // end_header

	// 求出各边最大最小值，进行平移和切分
	Vec3i min(INT_MAX, INT_MAX, INT_MAX);
	Vec3i max(INT_MIN, INT_MIN, INT_MIN);
	vector<PointInt> buffer(pointNum);
	for (auto& p : buffer) {
		int x, y, z, r, g, b;
		in >> x >> y >> z >> r >> g >> b;
		p.position = Vec3i(x, y, z);
		p.color = Vec3u8(r, g, b);

		min = Vec3f::min(min, p.position);
		max = Vec3f::max(max, p.position);
	}

	PCData data;
	data.shiftValue = min;
	data.sliceEdgeLength = 64;  // 暂时使用64x64x64的分片
	auto volumn = Vec3i(max - min);  // 场景大小，用来切片
	data.sliceCount = volumn / data.sliceEdgeLength + 1;	 // 各坐标轴分片数
	data.slices.resize(data.sliceCount.x * data.sliceCount.y * data.sliceCount.z);  // 分配空间

	for (const auto& p : buffer) {
		auto shiftPosition = p.position - data.shiftValue;
		auto indexVec = shiftPosition / data.sliceEdgeLength;       // 分片索引
		data.atSlice(indexVec).push_back({ Vec3i(shiftPosition), p.color });
	}

	return data;
}

EncodedSlice encodeSlice(const vector<PointInt>& sliceData, const Vec3i& sliceOrigin, int sliceEdgeLength) {
	EncodedSlice result;
	if (sliceData.empty()) {
		result.hasPoints = false;
		return result;
	}
	result.hasPoints = true;

	EncodeTreeNode headNode;
	headNode.origin = sliceOrigin;
	headNode.edgeLength = sliceEdgeLength;
	//headNode.level = 0;
	headNode.sliceDataIndex.resize(sliceData.size());
	for (int i = 0; i < sliceData.size(); ++i) {
		headNode.sliceDataIndex[i] = i;
	}

	queue<EncodeTreeNode> q;
	q.push(headNode);
	do {
		const auto& node = q.front();
		// TODO: 自适应深度控制
		// TODO2: 如果分片大小能用16bit整数放下，表示点位置需要6B，对于大块节点的孤点来说可能更省流？
		// 整2次幂二分到最后必然只剩一个点在节点原点处（或空）
		if (node.sliceDataIndex.size() == 1 && sliceData[node.sliceDataIndex[0]].position == node.origin) {
			result.treeBytes.push_back(0);
			result.colors.push_back(sliceData[node.sliceDataIndex[0]].color);
		}
		else {
			auto halfLength = node.edgeLength / 2;
			auto center = node.origin + halfLength;
			array<vector<int>, 8> subSlicesDataIndex;
			for (int index : node.sliceDataIndex) {
				auto diff = sliceData[index].position - center;
				int subIndex;
				if (diff.x < 0) {
					if (diff.y < 0) {
						if (diff.z < 0)
							subIndex = 0;
						else
							subIndex = 1;
					}
					else {
						if (diff.z < 0)
							subIndex = 2;
						else
							subIndex = 3;
					}
				}
				else {
					if (diff.y < 0) {
						if (diff.z < 0)
							subIndex = 4;
						else
							subIndex = 5;
					}
					else {
						if (diff.z < 0)
							subIndex = 6;
						else
							subIndex = 7;
					}
				}
				subSlicesDataIndex[subIndex].push_back(index);
			}

			int controlByte = 0;
			for (int i = 0; i < 8; ++i) {
				controlByte <<= 1;
				if (!subSlicesDataIndex[i].empty()) {
					controlByte |= 1;
					EncodeTreeNode nextNode;
					nextNode.origin = Vec3f((i & 4 ? center.x : node.origin.x),
											(i & 2 ? center.y : node.origin.y),
											(i & 1 ? center.z : node.origin.z));
					nextNode.edgeLength = halfLength;
					//nextNode.level = node.level + 1;
					nextNode.sliceDataIndex = subSlicesDataIndex[i];
					q.push(nextNode);
				}
				else
					controlByte |= 0;
			}
			result.treeBytes.push_back(controlByte);
		}

		q.pop();
	} while (!q.empty());

	return result;
}

void encode(const char* pathIn, const char* pathOut) {
	auto data = readFile(pathIn);

	/*  输出格式：
		移位量
		分片边长
		[原点，树结构字节数，颜色字节数，树结构，颜色]...
		（就不用像原来一样一条依赖链从头到尾）
	*/

	ofstream out(pathOut, ios::binary);
	out.write((char*)&data.shiftValue, 12);
	out.write((char*)&data.sliceEdgeLength, 4);

	// 可能做变宽分片？
	for (int i = 0; i < data.sliceCount.x; ++i) {
		for (int j = 0; j < data.sliceCount.y; ++j) {
			for (int k = 0; k < data.sliceCount.z; ++k) {
				const auto& slice = data.atSlice(i, j, k);
				// 忽略空分片
				if (!slice.empty()) {
					auto origin = Vec3i(i, j, k) * data.sliceEdgeLength;
					auto encoded = encodeSlice(slice, origin, data.sliceEdgeLength);
					int treeBytesNum = encoded.treeBytes.size();
					int colorsNum = encoded.colors.size();
					out.write((char*)&origin, 12);
					out.write((char*)&treeBytesNum, 4);
					out.write((char*)&colorsNum, 4);
					out.write((char*)encoded.treeBytes.data(), treeBytesNum);
					out.write((char*)encoded.colors.data(), colorsNum * 3);
				}
			}
		}
	}
}



vector<PointInt> decodeSlice(const vector<char>& treeBytes, const vector<Vec3u8>& colors, const Vec3i& origin, int sliceEdgeLength) {
	vector<PointInt> decodedPoints;
	queue<DecodeTreeNode> q;
	q.push({ origin, sliceEdgeLength });
	int treeIndex = 0;
	int colorIndex = 0;
	do {
		const auto& node = q.front();
		int controlByte = treeBytes[treeIndex];
		treeIndex++;
		if (controlByte == 0) {
			const auto& color = colors[colorIndex];
			colorIndex++;
			decodedPoints.push_back({ node.origin, color });
		}
		else {
			for (int i = 0; i < 8; ++i) {
				if (controlByte & 0x80) {
					int halfLength = node.edgeLength / 2;
					auto center = node.origin + halfLength;
					DecodeTreeNode nextNode;
					nextNode.origin = Vec3f((i & 4 ? center.x : node.origin.x),
											(i & 2 ? center.y : node.origin.y),
											(i & 1 ? center.z : node.origin.z));
					nextNode.edgeLength = halfLength;
					q.push(nextNode);
				}
				controlByte <<= 1;
			}
		}
		q.pop();
	} while (!q.empty());

	return decodedPoints;
}

void decode(const char* pathIn, const char* pathOut) {
	ifstream in(pathIn, ios::binary);

	PCData data;
	in.read((char*)&data.shiftValue, 12);
	in.read((char*)&data.sliceEdgeLength, 4);

	int pointNum = 0;
	Vec3i origin;
	while (in.read((char*)&origin, 12)) {
		int treeBytesNum, colorsNum;
		in.read((char*)&treeBytesNum, 4);
		in.read((char*)&colorsNum, 4);
		vector<char> treeBytes(treeBytesNum);
		vector<Vec3u8> colors(colorsNum);
		in.read(treeBytes.data(), treeBytesNum);
		in.read((char*)colors.data(), colorsNum * 3);

		auto slice = decodeSlice(treeBytes, colors, origin, data.sliceEdgeLength);
		data.slices.push_back(slice);
		pointNum += slice.size();
	}

	ofstream out(pathOut);
	out << "ply\n";
	out << "format ascii 1.0\n";
	out << "element vertex " << pointNum << '\n';
	out << "property float x\n";
	out << "property float y\n";
	out << "property float z\n";
	out << "property uchar red\n";
	out << "property uchar green\n";
	out << "property uchar blue\n";
	out << "end_header\n";
	for (const auto& slice : data.slices) {
		for (const auto& p : slice) {
			auto shiftPos = p.position + data.shiftValue;
			out << shiftPos.x << ' ' << shiftPos.y << ' ' << shiftPos.z << ' ';
			auto [r, g, b] = Vec3i(p.color);
			out << r << ' ' << g << ' ' << b << '\n';
		}
	}
}

int main() {
	encode("ricardo9_frame0017.ply", "test.bin");
	decode("test.bin", "decode.ply");
}
