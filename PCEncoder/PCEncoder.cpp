#include <iostream>
#include <array>
#include <vector>
#include <fstream>
#include <string>
#include <exception>
#include <memory>
#include <queue>
#include <immintrin.h>
using namespace std;

template <typename T>
struct Vec3 {
	T x;
	T y;
	T z;

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

// 每个点的数据，暂时只有坐标
using Point = Vec3f;
// 场景平移量
using ShiftValue = Vec3f;
// 场景大小
using Volumn = Vec3f;

struct PCData {
	// 场景的长宽高，平移场景使其坐标>=0
	Volumn volumn;
	ShiftValue shiftValue;

	// 将场景按照各边20等分（暂定20份）（或者划分基本单元平铺？）
	array<array<array<vector<Point>, 20>, 20>, 20> slices;
};

struct EncodeTreeNode {
	int level;
	Point min;
	Point max;
	vector<int> sliceDataIndex;
};

struct DecodeTreeNode {
	Point min;
	Point max;
};

struct EncodedSlice {
	bool hasPoints;
	vector<char> tree;
};

// 暂时只支持ply
// 使用堆空间，否则一启动就把栈爆破了
unique_ptr<PCData> readFile(const char* path) {
	ifstream in(path);
	// 暂时不考虑非法格式
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
	Point min({ FLT_MAX, FLT_MAX, FLT_MAX });
	Point max({ -FLT_MAX, -FLT_MAX, -FLT_MAX });
	vector<Point> pointsTemp(pointNum);
	for (auto& point : pointsTemp) {
		in >> point.x >> point.y >> point.z;
		min = Point::min(min, point);
		max = Point::max(max, point);
	}

	auto data = make_unique<PCData>();

	data->volumn = max - min;
	data->shiftValue = min;
	Volumn unit = data->volumn / 20.0f;

	for (const auto& point : pointsTemp) {
		Point shiftPoint = point - data->shiftValue;
		Vec3 index = shiftPoint.divideEach(unit);

		int xIdx = index.x;
		int yIdx = index.y;
		int zIdx = index.z;
		// 末端整除越界
		if (xIdx == 20)
			xIdx = 19;
		if (yIdx == 20)
			yIdx = 19;
		if (zIdx == 20)
			zIdx = 19;
		data->slices[xIdx][yIdx][zIdx].push_back(shiftPoint);
	}

	return data;
}

EncodedSlice encodeSlice(vector<Point>& sliceData, const Vec3f& sliceMin, const Vec3f& sliceMax) {
	EncodedSlice result;
	if (sliceData.empty()) {
		result.hasPoints = false;
		return result;
	}
	result.hasPoints = true;

	// 合并block内相同点（暂定使用浮点==）
	// 也可以用参数控制精度吧（控制单位步长）
	for (auto it = sliceData.begin(); it != sliceData.end(); ++it) {
		for (auto it2 = it + 1; it2 != sliceData.end(); ) {
			if (*it == *it2) {
				it2 = sliceData.erase(it2);
			}
			else {
				++it2;
			}
		}
	}

	EncodeTreeNode headNode;
	headNode.min = sliceMin;
	headNode.max = sliceMax;
	headNode.level = 0;
	for (int i = 0; i < sliceData.size(); ++i) {
		headNode.sliceDataIndex.push_back(i);
	}

	queue<EncodeTreeNode> q;
	q.push(headNode);
	do {
		const auto& node = q.front();
		// 暂定最高12层（刚好是12B，与3个浮点坐标所需空间相同），超出的截断
		// TODO: 有没有自适应的深度控制？
		if (node.level < 12) {
			// 划分内只包含一个点时，判断是否接近原点，若是则编码，否则继续分裂
			if (node.sliceDataIndex.size() == 1) {
				Vec3f diff = sliceData[node.sliceDataIndex[0]] - sliceMin;
				if (diff.x < 0.1f && diff.y < 0.1f && diff.z < 0.1f) {
					result.tree.push_back(0);
				}
				else
					goto split;
			}
			else {
			split:
				Point center = (node.min + node.max) / 2.0f;
				array<vector<int>, 8> subSlicesDataIndex;
				for (int index : node.sliceDataIndex) {
					const auto& onePoint = sliceData[index];
					Vec3f diff = onePoint - center;
					int subIndex;
					if (diff.x < 0.0f) {
						if (diff.y < 0.0f) {
							if (diff.z < 0.0f)
								subIndex = 0;
							else
								subIndex = 1;
						}
						else {
							if (diff.z < 0.0f)
								subIndex = 2;
							else
								subIndex = 3;
						}
					}
					else {
						if (diff.y < 0.0f) {
							if (diff.z < 0.0f)
								subIndex = 4;
							else
								subIndex = 5;
						}
						else {
							if (diff.z < 0.0f)
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
						nextNode.min.x = (i & 4 ? center.x : node.min.x);
						nextNode.max.x = (i & 4 ? node.max.x : center.x);
						nextNode.min.y = (i & 2 ? center.y : node.min.y);
						nextNode.max.y = (i & 2 ? node.max.y : center.y);
						nextNode.min.z = (i & 1 ? center.z : node.min.z);
						nextNode.max.z = (i & 1 ? node.max.z : center.z);
						nextNode.level = node.level + 1;
						nextNode.sliceDataIndex = subSlicesDataIndex[i];
						q.push(nextNode);
					}
					else
						controlByte |= 0;
				}
				result.tree.push_back(controlByte);
			}
		}
		else {
			result.tree.push_back(0);
		}

		q.pop();
	} while (!q.empty());

	return result;
}

void encode(const char* pathIn, const char* pathOut) {
	auto data = readFile(pathIn);

	ofstream out(pathOut, ios::binary);
	out.write((char*)&data->volumn, 12);  // 写入场景大小
	out.write((char*)&data->shiftValue, 12);  // 写入场景位移

	Volumn unit = data->volumn / 20.0f;
	for (int i = 0; i < 20; ++i) {
		for (int j = 0; j < 20; ++j) {
			for (int k = 0; k < 20; ++k) {
				Vec3f min({ unit.x * i, unit.y * j, unit.z * k });
				Vec3f max({ unit.x * (i + 1), unit.y * (j + 1), unit.z * (k + 1) });
				auto slice = encodeSlice(data->slices[i][j][k], min, max);
				bool hasPoints = slice.hasPoints;
				out.put(hasPoints);
				if (hasPoints) {
					out.write(slice.tree.data(), slice.tree.size());
				}
			}
		}
	}
}



vector<Point> decodeSlice(ifstream& in, const Vec3f& sliceMin, const Vec3f& sliceMax) {
	vector<Point> decodedPoints;
	bool hasPoints = in.get();
	if (!hasPoints)
		return decodedPoints;

	queue<DecodeTreeNode> q;
	q.push(DecodeTreeNode({ sliceMin, sliceMax }));
	do {
		const auto& node = q.front();
		int controlByte = in.get();
		if (controlByte == 0) {
			decodedPoints.push_back(node.min);
		}
		else {
			for (int i = 0; i < 8; ++i) {
				if (controlByte & 0x80) {
					Point center = (node.min + node.max) / 2.0f;
					DecodeTreeNode nextNode;
					nextNode.min.x = (i & 4 ? center.x : node.min.x);
					nextNode.max.x = (i & 4 ? node.max.x : center.x);
					nextNode.min.y = (i & 2 ? center.y : node.min.y);
					nextNode.max.y = (i & 2 ? node.max.y : center.y);
					nextNode.min.z = (i & 1 ? center.z : node.min.z);
					nextNode.max.z = (i & 1 ? node.max.z : center.z);
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

	auto data = make_unique<PCData>();
	in.read((char*)&data->volumn, 12);  // 读取场景大小
	in.read((char*)&data->shiftValue, 12);  // 读取场景位移

	Volumn unit = data->volumn / 20.0f;
	int pointNum = 0;
	for (int i = 0; i < 20; ++i) {
		for (int j = 0; j < 20; ++j) {
			for (int k = 0; k < 20; ++k) {
				Vec3f min({ unit.x * i, unit.y * j, unit.z * k });
				Vec3f max({ unit.x * (i + 1), unit.y * (j + 1), unit.z * (k + 1) });
				data->slices[i][j][k] = decodeSlice(in, min, max);
				pointNum += data->slices[i][j][k].size();
			}
		}
	}

	ofstream out(pathOut);
	out << "ply\n";
	out << "format ascii 1.0\n";
	out << "element vertex " << pointNum << '\n';
	out << "property float x\n";
	out << "property float y\n";
	out << "property float z\n";
	out << "end_header\n";
	for (int i = 0; i < 20; ++i) {
		for (int j = 0; j < 20; ++j) {
			for (int k = 0; k < 20; ++k) {
				for (const auto& point : data->slices[i][j][k]) {
					auto outPoint = point + data->shiftValue;
					out << outPoint.x << ' ' << outPoint.y << ' ' << outPoint.z << '\n';
				}
			}
		}
	}
}

int main() {
	encode("ricardo9_frame0017_ref.ply", "test.bin");
	decode("test.bin", "decode.ply");
}
