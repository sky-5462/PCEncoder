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
	bool isChromasubsampling;
	bool using_JPEG;
	int quantizationBits;     // 量化位数（暂时使用看位宽的暴力量化）
	EntropyEncodeType treeEntropyType;
	EntropyEncodeType colorEntropyType;
	IOParameters ioParameters;

	// 默认设置
	PCEncoder() :
		sliceMaxEdgeLength(64),
		ioParameters(),
		isChromasubsampling(true),
		using_JPEG(true),
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
					Slice slice(Vec3i32(i, j, k) * sliceMaxEdgeLength, sliceMaxEdgeLength);
					slice.setChromaSubsampling(isChromasubsampling);
					slice.setquantizationBits(quantizationBits);
					slice.setEntropyType(treeEntropyType, colorEntropyType);
					slices.push_back(std::move(slice));
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

		// 对分片建树
		for (auto& slice : slices) {
			if (!slice.empty()) {
				slice.Construct_Octree_From_Slice();
				slice.Octree_Compute_Attribute_Diff();
				// 压缩分片
				slice.Octree_encode();
			}
		}		

		// 写入输出流
		std::string byteStream;
		std::vector<int> lengthTable;    // 分片长度表，用于快速寻址分片
		for (auto& slice : slices) {
			if (!slice.empty()) {
				auto temp = slice.serialize();
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

	// 与 encode() 的区别仅在于增加了 using_JPEG 的条件分支
	void encode_using_JPEG() {
		// 参数检查

		if (using_JPEG)
			ioParameters.cspInternal = ColorSpace::RGB;


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
					Slice slice(Vec3i32(i, j, k) * sliceMaxEdgeLength, sliceMaxEdgeLength);
					slice.setChromaSubsampling(isChromasubsampling);
					slice.setquantizationBits(quantizationBits);
					slice.setEntropyType(treeEntropyType, colorEntropyType);
					slices.push_back(std::move(slice));
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

		if (using_JPEG) {
			for (auto& slice : slices) {
				if (!slice.empty()) {
					slice.Construct_Octree_From_Slice();
					slice.Octree_encode_using_JPEG();
				}
			}
		}
		else {
			for (auto& slice : slices) {
				if (!slice.empty()) {
					slice.Construct_Octree_From_Slice();
					slice.Octree_Compute_Attribute_Diff();
					slice.Octree_encode();
				}
			}
		}

		

		// 写入输出流
		std::string byteStream;
		std::vector<int> lengthTable;    // 分片长度表，用于快速寻址分片
		for (auto& slice : slices) {
			if (!slice.empty()) {
				auto temp = using_JPEG ? slice.serialize_using_JPEG() : slice.serialize();
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

	// 与 decode() 的区别仅在于增加了 using_JPEG 的条件分支
	void decode_using_JPEG() {
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
			auto slice = using_JPEG ? Slice::parse_using_JPEG(view.substr(index, len)) : Slice::parse(view.substr(index, len));
			index += len;
			buffer.insertPoints(using_JPEG ? slice.Octree_decode_using_JPEG() : slice.decode());
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
	encoder.isChromasubsampling = false;
	encoder.using_JPEG = true;
	encoder.treeEntropyType = EntropyEncodeType::HUFFMAN;
	encoder.colorEntropyType = EntropyEncodeType::HUFFMAN;
	encoder.quantizationBits = 0;
	encoder.encode_using_JPEG();
	encoder.pathIn = "test.bin";
	encoder.pathOut = "decode.ply";
	encoder.decode_using_JPEG();
}
