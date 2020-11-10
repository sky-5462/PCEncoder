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
	Vec3 operator %(T val) const {
		return Vec3({ x % val, y % val, z % val });
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

private:
	vector<Point> points;
	Vec3i32 max;
	Vec3i32 min;
};

struct EncodeTreeNode {
	Vec3i32 origin;
	int edgeLength;
	vector<int> sliceDataIndex;
};

struct DecodeTreeNode {
	Vec3i32 origin;
	int edgeLength;
};

class Slice {
public:
	Slice(const Vec3i32& origin, uint32_t edgeLength) : origin(origin), edgeLength(edgeLength) {}

	void addPoint(const Point& p) {
		points.push_back(p);
	}
	const vector<Point>& getPoints() const {
		return points;
	}

	bool empty() const {
		return points.empty();
	}

	void encode() {
		if (points.empty())
			throw logic_error("No point to encode");

		EncodeTreeNode headNode;
		headNode.origin = origin;
		headNode.edgeLength = edgeLength;
		headNode.sliceDataIndex.resize(points.size());
		for (int i = 0; i < points.size(); ++i) {
			headNode.sliceDataIndex[i] = i;
		}

		queue<EncodeTreeNode> q;
		q.push(headNode);
		do {
			const auto& node = q.front();
			// 整2次幂二分到最后必然只剩一个点在节点原点处（或空）
			if (node.sliceDataIndex.size() == 1 && points[node.sliceDataIndex[0]].position == node.origin) {
				encodedTree.push_back(0);
				encodedColor.push_back(points[node.sliceDataIndex[0]].color);
			}
			else {
				auto halfLength = node.edgeLength / 2;
				auto center = node.origin + halfLength;
				array<vector<int>, 8> subSlicesDataIndex;
				for (int index : node.sliceDataIndex) {
					Vec3i32 diff = points[index].position - center;
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
						nextNode.origin = Vec3i32((i & 4 ? center.x : node.origin.x),
												  (i & 2 ? center.y : node.origin.y),
												  (i & 1 ? center.z : node.origin.z));
						nextNode.edgeLength = halfLength;
						nextNode.sliceDataIndex = subSlicesDataIndex[i];
						q.push(nextNode);
					}
					else
						controlByte |= 0;
				}
				encodedTree.push_back(controlByte);
			}

			q.pop();
		} while (!q.empty());
	}

	// 返回解码的点数
	int decode() {
		queue<DecodeTreeNode> q;
		q.push({ origin, edgeLength });
		int treeIndex = 0;
		int colorIndex = 0;
		do {
			const auto& node = q.front();
			int controlByte = encodedTree[treeIndex];
			treeIndex++;
			if (controlByte == 0) {
				const auto& color = encodedColor[colorIndex];
				colorIndex++;
				points.push_back({ node.origin, color });
			}
			else {
				for (int i = 0; i < 8; ++i) {
					if (controlByte & 0x80) {
						int halfLength = node.edgeLength / 2;
						auto center = node.origin + halfLength;
						DecodeTreeNode nextNode;
						nextNode.origin = Vec3i32((i & 4 ? center.x : node.origin.x),
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

		return points.size();
	}

	// 写入文件，返回写入字节数（也要确认非空）
	int writeToBin(ostream& out) const {
		if (points.empty())
			throw logic_error("No point to write");

		uint8_t tzNum = _tzcnt_u32(edgeLength);  // 保存边长对应的(1<<N)移位量，可以用8bit存下
		int treeNum = encodedTree.size();
		int colorsNum = encodedColor.size();
		out.write((char*)&origin, 12);
		out.write((char*)&treeNum, 4);
		out.write((char*)&colorsNum, 4);
		out.write((char*)&tzNum, 1);
		out.write((char*)encodedTree.data(), treeNum);
		out.write((char*)encodedColor.data(), colorsNum * 3);
		return 12 + 4 + 4 + 1 + treeNum + colorsNum * 3;
	}
	static Slice readFromBin(ifstream& in) {
		Vec3i32 origin;
		int treeNum, colorsNum;
		uint8_t tzNum;
		in.read((char*)&origin, 12);
		in.read((char*)&treeNum, 4);
		in.read((char*)&colorsNum, 4);
		in.read((char*)&tzNum, 1);

		Slice slice(origin, 1 << tzNum);
		slice.encodedTree.resize(treeNum);
		slice.encodedColor.resize(colorsNum * 3);
		in.read((char*)slice.encodedTree.data(), treeNum);
		in.read((char*)slice.encodedColor.data(), colorsNum * 3);

		return slice;
	}

private:
	Vec3i32 origin;
	int edgeLength;
	vector<Point> points;
	vector<char> encodedTree;
	vector<Vec3u8> encodedColor;
};

class PCEncoder {
public:
	// 对公有参数赋值后调用encode
	string pathIn;
	string pathOut;
	int sliceMaxEdgeLength;   // 必须是2的整次幂（默认64）

	PCEncoder() : sliceMaxEdgeLength(64) {}

	void encode() {
		// 参数检查

		readPly();

		// 这里应该有个平移场景去掉负数坐标的操作，得到PointU32

		Vec3i32 volumn = inputBuffer.getVolumn();    // 场景大小
		Vec3i32 sliceNum = (volumn / sliceMaxEdgeLength) + 1;

		// 分配初始分片
		slices.reserve(sliceNum.x * sliceNum.y * sliceNum.z);
		for (int i = 0; i < sliceNum.x; ++i) {
			for (int j = 0; j < sliceNum.y; ++j) {
				for (int k = 0; k < sliceNum.z; ++k) {
					slices.emplace_back(Vec3i32(i, j, k) * sliceMaxEdgeLength, sliceMaxEdgeLength);
				}
			}
		}
		// 分配点到各初始分片内，变换为切片局部坐标
		for (const auto& p : inputBuffer) {
			Vec3i32 sliceIndexVec = p.position / sliceMaxEdgeLength;
			int index = sliceIndexVec.x * sliceNum.y * sliceNum.z + sliceIndexVec.y * sliceNum.z + sliceIndexVec.z;
			slices[index].addPoint(p);
		}
		inputBuffer.clear();

		// 这里可以插入分片分裂以及质量控制相关的操作

		// 压缩分片
		for (auto& slice : slices) {
			if (!slice.empty()) {
				slice.encode();
			}
		}

		writeBin();
		slices.clear();
	}

	void decode() {
		// 参数检查

		readBin();

		int sum = 0;
		for (auto& slice : slices) {
			int num = slice.decode();
			sum += num;
		}

		writePly(sum);
		slices.clear();
	}

private:
	PointBuffer inputBuffer;
	vector<Slice> slices;

	void readPly() {
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
		ifstream in(pathIn);
		int pointNum;
		string skip;
		in >> skip;  // ply
		in >> skip >> skip >> skip;  // format
		in >> skip >> skip >> pointNum;  // pointNum
		in >> skip >> skip >> skip;  // x
		in >> skip >> skip >> skip;  // y
		in >> skip >> skip >> skip;  // z
		in >> skip >> skip >> skip;  // r
		in >> skip >> skip >> skip;  // g
		in >> skip >> skip >> skip;  // b
		in >> skip;  // end_header

		for (int i = 0; i < pointNum; ++i) {
			// 如果要区分整数和浮点可能需要参数指定
			int x, y, z, r, g, b;
			in >> x >> y >> z >> r >> g >> b;
			inputBuffer.push(x, y, z, r, g, b);
		}
	}

	void writeBin() {
		// 写入分片数据
		ofstream out(pathOut, ios::binary);    // 分片长度表，用于快速寻址分片
		vector<int> lengthTable;
		for (const auto& slice : slices) {
			if (!slice.empty()) {
				int length = slice.writeToBin(out);
				lengthTable.push_back(length);
			}
		}

		// 最后写入长度表和长度表偏移
		int length = lengthTable.size();
		out.write((char*)lengthTable.data(), length * 4);
		out.write((char*)&length, 4);
	}

	void readBin() {
		ifstream in(pathIn, ios::binary);
		// 读取长度表偏移
		in.seekg(-4, SEEK_END);
		int length;
		in.read((char*)&length, 4);
		// 读取长度表
		in.seekg(-(length * 4), SEEK_CUR);
		vector<int> lengthTable(length);
		in.read((char*)lengthTable.data(), length * 4);

		// 读取length个切片（顺序读的话倒是不用seek）
		in.seekg(0);
		slices.reserve(length);
		for (int i = 0; i < length; ++i) {
			slices.push_back(Slice::readFromBin(in));
		}
	}

	void writePly(int pointNum) {
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
		for (const auto& slice : slices) {
			const auto& points = slice.getPoints();
			for (const auto& p : points) {
				out << p.position.x << ' ' << p.position.y << ' ' << p.position.z << ' ';
				auto [r, g, b] = Vec3i32(p.color);
				out << r << ' ' << g << ' ' << b << '\n';
			}
		}
	}
};

int main() {
	PCEncoder encoder;
	encoder.pathIn = "ricardo9_frame0017.ply";
	encoder.pathOut = "test.bin";
	encoder.encode();
	encoder.pathIn = "test.bin";
	encoder.pathOut = "decode.ply";
	encoder.decode();
}
