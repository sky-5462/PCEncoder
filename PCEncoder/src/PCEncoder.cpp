#include <common.h>
#include <IO.h>
#include <Slice.h>
#include <vector>
#include <string>
#include <string_view>
#include <immintrin.h>

class PCEncoder {
public:
	// 对公有参数赋值后调用encode
	std::string pathIn;
	std::string pathOut;
	int sliceMaxEdgeLength;   // 必须是2的整次幂（默认64）
	int clipDepth;            // 从底剪除的层数
	bool isChromasubsampling;
	int quantizationBits;     // 量化位数（暂时使用看位宽的暴力量化）
	EntropyEncodeType treeEntropyType;
	EntropyEncodeType colorEntropyType;
	IOParameters ioParameters;

	// 默认设置
	PCEncoder() :
		sliceMaxEdgeLength(64),
		clipDepth(0),
		ioParameters(),
		isChromasubsampling(true),
		quantizationBits(0),
		treeEntropyType(EntropyEncodeType::NONE),
		colorEntropyType(EntropyEncodeType::NONE) {
	}

	void encode() {
		// 参数检查

		IO io(ioParameters);
		buffer = io.readText(pathIn);

		// 这里应该有个平移场景去掉负数坐标的操作，得到PointU32

		Vec3i32 volumn = buffer.getVolumn();    // 场景大小
		Vec3i32 sliceNum = (volumn / sliceMaxEdgeLength) + 1;

		// 分配初始分片
		slices.reserve(sliceNum.x * sliceNum.y * sliceNum.z);
		for (int i = 0; i < sliceNum.x; ++i) {
			for (int j = 0; j < sliceNum.y; ++j) {
				for (int k = 0; k < sliceNum.z; ++k) {
					Slice slice(Vec3i32(i, j, k) * sliceMaxEdgeLength, sliceMaxEdgeLength, clipDepth);
					slice.setChromaSubsampling(isChromasubsampling);
					slice.setEntropyType(treeEntropyType, colorEntropyType);
					slices.push_back(slice);
				}
			}
		}
		// 分配点到各初始分片内
		for (const auto& p : buffer) {
			Vec3i32 sliceIndexVec = p.position / sliceMaxEdgeLength;
			int index = sliceIndexVec.x * sliceNum.y * sliceNum.z + sliceIndexVec.y * sliceNum.z + sliceIndexVec.z;
			slices[index].addPoint(p);
		}
		buffer.clear();

		// 这里可以插入分片分裂以及质量控制相关的操作
		std::vector<Slice> splitedSlices;
		for (const auto& slice : slices) {
			auto splited = Slice::split(slice);
			splitedSlices.insert(splitedSlices.end(), splited.begin(), splited.end());
		}
		slices = splitedSlices;

		// 压缩分片
		for (auto& slice : slices) {
			if (!slice.empty()) {
				slice.encode(quantizationBits);
			}
		}

		// 写入输出流
		std::string byteStream;
		std::vector<int> lengthTable;    // 分片长度表，用于快速寻址分片
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

		io.writeBin(pathOut, byteStream);
		slices.clear();
	}

	void decode() {
		// 参数检查

		IO io(ioParameters);
		auto byteStream = io.readBin(pathIn);

		int index = byteStream.size() - 4;
		int length;
		memcpy(&length, &byteStream[index], 4);
		std::vector<int> lengthTable(length);
		index -= length * 4;
		memcpy(lengthTable.data(), &byteStream[index], length * 4);

		index = 0;
		std::string_view view(byteStream);
		for (int len : lengthTable) {
			auto slice = Slice::parse(view.substr(index, len));
			index += len;
			buffer.insertPoints(slice.decode());
		}

		io.writeText(pathOut, buffer);
		buffer.clear();
	}

private:
	PointBuffer buffer;
	std::vector<Slice> slices;
};

int main() {
	PCEncoder encoder;
	encoder.pathIn = "ricardo9_frame0017.ply";
	encoder.pathOut = "test.bin";
	encoder.isChromasubsampling = true;
	encoder.treeEntropyType = EntropyEncodeType::HUFFMAN;
	encoder.colorEntropyType = EntropyEncodeType::HUFFMAN;
	encoder.quantizationBits = 0;
	encoder.encode();
	encoder.pathIn = "test.bin";
	encoder.pathOut = "decode.ply";
	encoder.decode();
}
