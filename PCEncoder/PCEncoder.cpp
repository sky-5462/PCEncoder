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
#include <string_view>
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

enum class ColorSpace {
	RGB = 0,
	YUV = 1
};

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
	int level;
	vector<int> sliceDataIndex;
};

struct DecodeTreeNode {
	Vec3i32 origin;
	int edgeLength;
	int level;
};

class Slice {
public:
	Slice(const Vec3i32& origin, int edgeLength, int clipDepth) : origin(origin), edgeLength(edgeLength), clipDepth(clipDepth) {}

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
			return;

		EncodeTreeNode headNode;
		headNode.origin = origin;
		headNode.edgeLength = edgeLength;
		headNode.level = _tzcnt_u32(edgeLength) - clipDepth;  // 往下深入时level递减，归零时强制停止
		// 这一个分片在开头就被剪掉了
		if (headNode.level < 0) {
			points.clear();
			return;
		}

		headNode.sliceDataIndex.resize(points.size());
		for (int i = 0; i < points.size(); ++i) {
			headNode.sliceDataIndex[i] = i;
		}

		queue<EncodeTreeNode> q;
		q.push(headNode);
		do {
			const auto& node = q.front();
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
					if (node.level > 1) {
						EncodeTreeNode nextNode;
						nextNode.origin = Vec3i32((i & 4 ? center.x : node.origin.x),
												  (i & 2 ? center.y : node.origin.y),
												  (i & 1 ? center.z : node.origin.z));
						nextNode.edgeLength = halfLength;
						nextNode.level = node.level - 1;
						nextNode.sliceDataIndex = subSlicesDataIndex[i];
						q.push(nextNode);
					}
					else {
						// 倒数第二层已经得到叶子层结构，不需要写入输出流
						// 获得明度信息
						int sum = 0;
						int num = subSlicesDataIndex[i].size();
						for (int index : subSlicesDataIndex[i]) {
							sum += points[index].color.x;
						}
						encodedColor.push_back(lroundf((float)sum / num));
					}
				}
				else
					controlByte |= 0;
			}
			encodedTree.push_back(controlByte);

			// 倒数第二层，进行色度采样
			if (node.level == 1) {
				int sum1 = 0, sum2 = 0;
				int num = node.sliceDataIndex.size();
				for (int index : node.sliceDataIndex) {
					sum1 += points[index].color.y;
					sum2 += points[index].color.z;
				}
				encodedColor.push_back(lroundf((float)sum1 / num));
				encodedColor.push_back(lroundf((float)sum2 / num));

			}

			q.pop();
		} while (!q.empty());
	}

	// 返回解码的点数
	int decode() {
		int level = _tzcnt_u32(edgeLength) - clipDepth;
		queue<DecodeTreeNode> q;
		q.push({ origin, edgeLength, level });
		int treeIndex = 0;
		int colorIndex = 0;
		do {
			const auto& node = q.front();
			int controlByte = encodedTree[treeIndex];
			treeIndex++;
			// 倒数第二层读取颜色
			if (node.level == 1) {
				// 首先读取明度
				vector<Point> buffer;
				for (int i = 0; i < 8; ++i) {
					if (controlByte & 0x80) {
						int halfLength = node.edgeLength / 2;
						auto center = node.origin + halfLength;
						auto subNodeOrigin = Vec3i32((i & 4 ? center.x : node.origin.x),
													 (i & 2 ? center.y : node.origin.y),
													 (i & 1 ? center.z : node.origin.z));

						// 深度受限时应该在整个节点对应的块上显示而不是缩成一个点...
						uint8_t luma = encodedColor[colorIndex];
						colorIndex++;
						for (int x = 0; x < halfLength; ++x) {
							for (int y = 0; y < halfLength; ++y) {
								for (int z = 0; z < halfLength; ++z) {
									buffer.push_back({ subNodeOrigin + Vec3i32(x, y, z), Vec3u8(luma, 0, 0) });
								}
							}
						}
					}
					controlByte <<= 1;
				}

				// 然后读取色度
				uint8_t chroma1 = encodedColor[colorIndex];
				uint8_t chroma2 = encodedColor[colorIndex + 1];
				colorIndex += 2;
				for (auto& p : buffer) {
					p.color.y = chroma1;
					p.color.z = chroma2;
				}

				points.insert(points.end(), buffer.begin(), buffer.end());
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
						nextNode.level = node.level - 1;
						q.push(nextNode);
					}
					controlByte <<= 1;
				}
			}

			q.pop();
		} while (!q.empty());

		return points.size();
	}

	string serialize() const {
		string result;
		uint8_t tzNum = _tzcnt_u32(edgeLength);  // 保存边长对应的(1<<N)移位量，可以用8bit存下
		int treeNum = encodedTree.size();
		int colorsNum = encodedColor.size();
		result.insert(result.size(), (char*)&origin, 12);
		result.insert(result.size(), (char*)&treeNum, 4);
		result.insert(result.size(), (char*)&colorsNum, 4);
		result += tzNum;
		result += clipDepth;
		result.insert(result.size(), (char*)encodedTree.data(), treeNum);
		result.insert(result.size(), (char*)encodedColor.data(), colorsNum);
		return result;
	}
	static Slice parse(string_view view) {
		Vec3i32 origin;
		int treeNum, colorsNum;
		int index = 0;
		memcpy(&origin, &view[index], 12);
		index += 12;
		memcpy(&treeNum, &view[index], 4);
		index += 4;
		memcpy(&colorsNum, &view[index], 4);
		index += 4;
		uint8_t tzNum = view[index];
		int clipDepth = view[index + 1];
		index += 2;

		Slice slice(origin, 1 << tzNum, clipDepth);
		slice.encodedTree.resize(treeNum);
		slice.encodedColor.resize(colorsNum);
		memcpy(slice.encodedTree.data(), &view[index], treeNum);
		index += treeNum;
		memcpy(slice.encodedColor.data(), &view[index], colorsNum);

		return slice;
	}

private:
	Vec3i32 origin;
	int edgeLength;
	int clipDepth;
	vector<Point> points;
	vector<char> encodedTree;
	vector<uint8_t> encodedColor;
};

class EntropyEncoder {
public:
	string encode(const string& byteStream) {
		return byteStream;
	}
	string decode(const string& byteStream) {
		return byteStream;
	}
};

class PCEncoder {
public:
	// 对公有参数赋值后调用encode
	string pathIn;
	string pathOut;
	int sliceMaxEdgeLength;   // 必须是2的整次幂（默认64）
	int clipDepth;      // 从底剪除的层数
	ColorSpace cspIn;   // 输入颜色空间

	PCEncoder() : sliceMaxEdgeLength(64), clipDepth(0), cspIn(ColorSpace::RGB) {}

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
					slices.emplace_back(Vec3i32(i, j, k) * sliceMaxEdgeLength, sliceMaxEdgeLength, clipDepth);
				}
			}
		}
		// 分配点到各初始分片内
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
	EntropyEncoder entropy;

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

			if (cspIn == ColorSpace::RGB) {
				// RGB -> YCbCr, 变换矩阵随便找的一个，能用就行
				int c1 = lroundf(0.299f * r + 0.587f * g + 0.114f * b);
				int c2 = lroundf(-0.169f * r - 0.331f * g + 0.499f * b) + 128;
				int c3 = lroundf(0.499f * r - 0.418f * g - 0.0813f * b) + 128;
				c1 = (c1 > 255 ? 255 : (c1 < 0 ? 0 : c1));
				c2 = (c2 > 255 ? 255 : (c2 < 0 ? 0 : c2));
				c3 = (c3 > 255 ? 255 : (c3 < 0 ? 0 : c3));
				inputBuffer.push(x, y, z, c1, c2, c3);
			}
			else
				inputBuffer.push(x, y, z, r, g, b);
		}
	}

	void writeBin() {
		string byteStream;
		vector<int> lengthTable;    // 分片长度表，用于快速寻址分片
		for (const auto& slice : slices) {
			if (!slice.empty()) {
				const auto& temp = slice.serialize();
				lengthTable.push_back(temp.size());
				byteStream += temp;
			}
		}

		// 最后写入长度表和长度表偏移
		int length = lengthTable.size();
		byteStream.insert(byteStream.size(), (char*)lengthTable.data(), length * 4);
		byteStream.insert(byteStream.size(), (char*)&length, 4);
		// 写入CSP
		byteStream += (char)cspIn;

		byteStream = entropy.encode(byteStream);

		ofstream out(pathOut, ios::binary);
		out.write(byteStream.c_str(), byteStream.size());
	}

	void readBin() {
		ifstream in(pathIn, ios::binary | ios::ate);
		int bytesNum = in.tellg();
		in.seekg(0);
		string byteStream;
		byteStream.resize(bytesNum);
		in.read(byteStream.data(), bytesNum);
		in.close();

		byteStream = entropy.decode(byteStream);

		int index = byteStream.size() - 1;
		cspIn = (ColorSpace)byteStream[index];
		index -= 4;
		int length;
		memcpy(&length, &byteStream[index], 4);
		vector<int> lengthTable(length);
		index -= length * 4;
		memcpy(lengthTable.data(), &byteStream[index], length * 4);

		index = 0;
		string_view view(byteStream);
		slices.reserve(length);
		for (int len : lengthTable) {
			slices.push_back(Slice::parse(view.substr(index, len)));
			index += len;
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
				auto [y, u, v] = Vec3i32(p.color);
				if (cspIn == ColorSpace::RGB) {
					int r = lroundf(y + 1.402f * (v - 128));
					int g = lroundf(y - 0.344f * (u - 128) - 0.714f * (v - 128));
					int b = lroundf(y + 1.772f * (u - 128));
					r = (r > 255 ? 255 : (r < 0 ? 0 : r));
					g = (g > 255 ? 255 : (g < 0 ? 0 : g));
					b = (b > 255 ? 255 : (b < 0 ? 0 : b));
					out << r << ' ' << g << ' ' << b << '\n';
				}
				else
					out << y << ' ' << u << ' ' << v << '\n';
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
