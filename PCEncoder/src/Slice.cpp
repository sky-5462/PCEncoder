#include <Slice.h>
#include <queue>
#include <array>

static int clipCalibration(int val) {
	if (val > 255)
		return 255 - val;
	else if (val < 0)
		return 0 - val;
	else
		return 0;
}

// 这个split过于简单粗暴了
std::vector<Slice> Slice::split(const Slice& slice) {
	std::vector<Slice> result = { slice };
	if (slice.points.empty())
		return result;

	// 这里先实现个简单的算法，检测int8范围内能否表示颜色三通道的差值，若不能就对半分
	// 第一通道
	int size = result.size();
	for (int i = 0; i < size; ++i) {
		auto& slice = result[i];
		int min = INT_MAX, max = 0;
		for (const auto& p : slice.points) {
			if (p.color.x < min)
				min = p.color.x;
			if (p.color.x > max)
				max = p.color.x;
		}
		// 这里就用120当边界吧，也差不多
		if (max - min > 120) {
			int mid = (max + min) / 2;
			std::vector<Point> pointSet1, pointSet2;
			for (const auto& p : slice.points) {
				if (p.color.x < mid)
					pointSet1.push_back(p);
				else
					pointSet2.push_back(p);
			}
			slice.points = pointSet1;

			Slice slice2(slice.origin, slice.edgeLength, slice.clipDepth);
			slice2.isChromaSubsampling = slice.isChromaSubsampling;
			slice2.points = pointSet2;
			result.push_back(slice2);
		}
	}

	// 第二通道
	size = result.size();
	for (int i = 0; i < size; ++i) {
		auto& slice = result[i];
		int min = INT_MAX, max = 0;
		for (const auto& p : slice.points) {
			if (p.color.y < min)
				min = p.color.y;
			if (p.color.y > max)
				max = p.color.y;
		}
		// 这里就用120当边界吧，也差不多
		if (max - min > 120) {
			int mid = (max + min) / 2;
			std::vector<Point> pointSet1, pointSet2;
			for (const auto& p : slice.points) {
				if (p.color.x < mid)
					pointSet1.push_back(p);
				else
					pointSet2.push_back(p);
			}
			slice.points = pointSet1;

			Slice slice2(slice.origin, slice.edgeLength, slice.clipDepth);
			slice2.isChromaSubsampling = slice.isChromaSubsampling;
			slice2.points = pointSet2;
			result.push_back(slice2);
		}
	}

	// 第三通道
	size = result.size();
	for (int i = 0; i < size; ++i) {
		auto& slice = result[i];
		int min = INT_MAX, max = 0;
		for (const auto& p : slice.points) {
			if (p.color.z < min)
				min = p.color.z;
			if (p.color.z > max)
				max = p.color.z;
		}
		// 这里就用120当边界吧，也差不多
		if (max - min > 120) {
			int mid = (max + min) / 2;
			std::vector<Point> pointSet1, pointSet2;
			for (const auto& p : slice.points) {
				if (p.color.x < mid)
					pointSet1.push_back(p);
				else
					pointSet2.push_back(p);
			}
			slice.points = pointSet1;

			Slice slice2(slice.origin, slice.edgeLength, slice.clipDepth);
			slice2.isChromaSubsampling = slice.isChromaSubsampling;
			slice2.points = pointSet2;
			result.push_back(slice2);
		}
	}

	return result;
}

void Slice::encode(int quantizationBits) {
	if (points.empty())
		return;

	std::vector<int> sliceDataIndex(points.size());
	for (int i = 0; i < points.size(); ++i) {
		sliceDataIndex[i] = i;
	}

	// 削除单枝根节点
	while (true) {
		std::array<int, 8> count = { 0 };
		auto halfLength = edgeLength / 2;
		auto center = origin + halfLength;
		for (int index : sliceDataIndex) {
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
			count[subIndex]++;
		}
		int validCount = 0;
		int index = -1;
		for (int i = 0; i < 8; ++i) {
			if (count[i] != 0) {
				validCount++;
				index = i;
			}
		}
		if (validCount == 1) {
			origin = Vec3i32((index & 4 ? center.x : origin.x),
							 (index & 2 ? center.y : origin.y),
							 (index & 1 ? center.z : origin.z));
			edgeLength = halfLength;
		}
		else
			break;
	}

	int firstLevel = _tzcnt_u32(edgeLength) - clipDepth;  // 往下深入时level递减，归零时强制停止
	// 这一个分片在开头就被剪掉了
	if (firstLevel < 0) {
		points.clear();
		return;
	}

	EncodeTreeNode headNode;
	headNode.origin = origin;
	headNode.edgeLength = edgeLength;
	headNode.sliceDataIndex = sliceDataIndex;
	headNode.level = firstLevel;

	// 使用颜色预测编码
	Vec3i32 sum = Vec3i32::zero();
	for (const auto& p : points) {
		sum += p.color;
	}
	avgColor = sum / points.size();

	std::queue<EncodeTreeNode> q;
	q.push(headNode);
	do {
		const auto& node = q.front();
		auto halfLength = node.edgeLength / 2;
		auto center = node.origin + halfLength;
		std::array<std::vector<int>, 8> subSlicesDataIndex;
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
					int luma = lroundf((float)sum / num) - avgColor.x;
					// 量化
					luma = (luma & (-1 << quantizationBits)) + ((luma << 1) & (1 << quantizationBits));
					// 反量化，修正溢出
					int calibration = clipCalibration(luma + avgColor.x);
					encodedColor.push_back(luma + calibration);

					// 不进行色度采样时，与明度一同获取色度
					if (!isChromaSubsampling) {
						int sum1 = 0, sum2 = 0;
						int num = node.sliceDataIndex.size();
						for (int index : node.sliceDataIndex) {
							sum1 += points[index].color.y;
							sum2 += points[index].color.z;
						}
						int chroma1 = lroundf((float)sum1 / num) - avgColor.y;
						int chroma2 = lroundf((float)sum2 / num) - avgColor.z;
						chroma1 = (chroma1 & (-1 << quantizationBits)) + ((chroma1 << 1) & (1 << quantizationBits));
						chroma2 = (chroma2 & (-1 << quantizationBits)) + ((chroma2 << 1) & (1 << quantizationBits));
						int calibration1 = clipCalibration(chroma1 + avgColor.y);
						int calibration2 = clipCalibration(chroma2 + avgColor.z);
						encodedColor.push_back(chroma1 + calibration1);
						encodedColor.push_back(chroma2 + calibration2);
					}
				}
			}
			else
				controlByte |= 0;
		}
		encodedTree.push_back(controlByte);

		// 倒数第二层，进行色度采样
		if (isChromaSubsampling && node.level == 1) {
			int sum1 = 0, sum2 = 0;
			int num = node.sliceDataIndex.size();
			for (int index : node.sliceDataIndex) {
				sum1 += points[index].color.y;
				sum2 += points[index].color.z;
			}
			int chroma1 = lroundf((float)sum1 / num) - avgColor.y;
			int chroma2 = lroundf((float)sum2 / num) - avgColor.z;
			chroma1 = (chroma1 & (-1 << quantizationBits)) + ((chroma1 << 1) & (1 << quantizationBits));
			chroma2 = (chroma2 & (-1 << quantizationBits)) + ((chroma2 << 1) & (1 << quantizationBits));
			int calibration1 = clipCalibration(chroma1 + avgColor.y);
			int calibration2 = clipCalibration(chroma2 + avgColor.z);
			encodedColor.push_back(chroma1 + calibration1);
			encodedColor.push_back(chroma2 + calibration2);
		}

		q.pop();
	} while (!q.empty());


	entropyTree.assign(encodedTree.cbegin(), encodedTree.cend());
	entropyColor.assign(encodedColor.cbegin(), encodedColor.cend());

	if (treeEntropyType == EntropyEncodeType::HUFFMAN) {
		entropyTree = huffman_encode_string(entropyTree);
	}
	else if (treeEntropyType == EntropyEncodeType::RLE) {
		entropyTree = runLengthEncode(entropyTree);
	}
	else if (treeEntropyType == EntropyEncodeType::RLE_HUFFMAN) {
		entropyTree = huffman_encode_string(runLengthEncode(entropyTree));
	}
	else if (treeEntropyType == EntropyEncodeType::ZLIB) {
		entropyTree = zlib_encode_string(entropyTree);
	}

	if (colorEntropyType == EntropyEncodeType::HUFFMAN) {
		entropyColor = huffman_encode_string(entropyColor);
	}
	else if (colorEntropyType == EntropyEncodeType::RLE) {
		entropyColor = runLengthEncode(entropyColor);
	}
	else if (colorEntropyType == EntropyEncodeType::RLE_HUFFMAN) {
		entropyColor = huffman_encode_string(runLengthEncode(entropyColor));
	}
	else if (colorEntropyType == EntropyEncodeType::ZLIB) {
		entropyColor = zlib_encode_string(entropyColor);
	}
}

std::vector<Point> Slice::decode() {
	if (treeEntropyType == EntropyEncodeType::HUFFMAN) {
		entropyTree = huffman_decode_string(entropyTree);
	}
	else if (treeEntropyType == EntropyEncodeType::RLE) {
		entropyTree = runLengthDecode(entropyTree);
	}
	else if (treeEntropyType == EntropyEncodeType::RLE_HUFFMAN) {
		entropyTree = runLengthDecode(huffman_decode_string(entropyTree));
	}
	else if (treeEntropyType == EntropyEncodeType::ZLIB) {
		entropyTree = zlib_decode_string(entropyTree);
	}

	if (colorEntropyType == EntropyEncodeType::HUFFMAN) {
		entropyColor = huffman_decode_string(entropyColor);
	}
	else if (colorEntropyType == EntropyEncodeType::RLE) {
		entropyColor = runLengthDecode(entropyColor);
	}
	else if (colorEntropyType == EntropyEncodeType::RLE_HUFFMAN) {
		entropyColor = runLengthDecode(huffman_decode_string(entropyColor));
	}
	else if (colorEntropyType == EntropyEncodeType::ZLIB) {
		entropyColor = zlib_decode_string(entropyColor);
	}

	encodedTree.assign(entropyTree.cbegin(), entropyTree.cend());
	encodedColor.assign(entropyColor.cbegin(), entropyColor.cend());

	int level = _tzcnt_u32(edgeLength) - clipDepth;
	std::queue<DecodeTreeNode> q;
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
			std::vector<Point> buffer;
			for (int i = 0; i < 8; ++i) {
				if (controlByte & 0x80) {
					int halfLength = node.edgeLength / 2;
					auto center = node.origin + halfLength;
					auto subNodeOrigin = Vec3i32((i & 4 ? center.x : node.origin.x),
												 (i & 2 ? center.y : node.origin.y),
												 (i & 1 ? center.z : node.origin.z));

					// 深度受限时应该在整个节点对应的块上显示而不是缩成一个点...
					uint8_t luma = encodedColor[colorIndex] + avgColor.x;
					colorIndex++;
					uint8_t chroma1 = 0;
					uint8_t chroma2 = 0;
					if (!isChromaSubsampling) {
						chroma1 = encodedColor[colorIndex] + avgColor.y;
						chroma2 = encodedColor[colorIndex + 1] + avgColor.z;
						colorIndex += 2;
					}
					for (int x = 0; x < halfLength; ++x) {
						for (int y = 0; y < halfLength; ++y) {
							for (int z = 0; z < halfLength; ++z) {
								buffer.push_back({ subNodeOrigin + Vec3i32(x, y, z), Vec3u8(luma, chroma1, chroma2) });
							}
						}
					}
				}
				controlByte <<= 1;
			}

			if (isChromaSubsampling) {
				uint8_t chroma1 = encodedColor[colorIndex] + avgColor.y;
				uint8_t chroma2 = encodedColor[colorIndex + 1] + avgColor.z;
				colorIndex += 2;
				for (auto& p : buffer) {
					p.color.y = chroma1;
					p.color.z = chroma2;
				}
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

	return points;
}

std::string Slice::serialize() const {
	std::string result;
	uint8_t tzNum = _tzcnt_u32(edgeLength);  // 保存边长对应的(1<<N)移位量，可以用8bit存下
	int treeNum = entropyTree.size();
	int colorsNum = entropyColor.size();
	result.insert(result.size(), (char*)&origin, 12);
	result.insert(result.size(), (char*)&treeNum, 4);
	result.insert(result.size(), (char*)&colorsNum, 4);
	result += tzNum;
	result += clipDepth;
	result += isChromaSubsampling;
	result += (char)treeEntropyType;
	result += (char)colorEntropyType;
	result.insert(result.size(), (char*)&avgColor, 3);
	result.insert(result.size(), (char*)entropyTree.data(), treeNum);
	result.insert(result.size(), (char*)entropyColor.data(), colorsNum);
	return result;
}

Slice Slice::parse(std::string_view view) {
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
	bool isChromaSubsampling = view[index + 2];
	EntropyEncodeType treeEntropyType = (EntropyEncodeType)view[index + 3];
	EntropyEncodeType colorEntropyType = (EntropyEncodeType)view[index + 4];
	index += 5;
	Vec3u8 avgColor;
	memcpy(&avgColor, &view[index], 3);
	index += 3;

	Slice slice(origin, 1 << tzNum, clipDepth);
	slice.isChromaSubsampling = isChromaSubsampling;
	slice.avgColor = avgColor;
	slice.treeEntropyType = treeEntropyType;
	slice.colorEntropyType = colorEntropyType;
	slice.entropyTree.resize(treeNum);
	slice.entropyColor.resize(colorsNum);
	memcpy(slice.entropyTree.data(), &view[index], treeNum);
	index += treeNum;
	memcpy(slice.entropyColor.data(), &view[index], colorsNum);

	return slice;
}
