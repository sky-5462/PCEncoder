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

// 差分量化器		根据均值修正溢出，保存量化后的差分值
int diff_Quantizer(int diff_val, int avg_val, int quantizationBits) {
	// Quantization.
	diff_val = (diff_val & (-1 << quantizationBits)) + ((diff_val << 1) & (1 << quantizationBits));
	// Calibration.(in case of overflow) 
	int calibration = clipCalibration(diff_val + avg_val);

	return (diff_val + calibration);
}

void Slice::Construct_Octree_From_Slice() {
	// Create index for points vector.
	std::vector<int> sliceDataIndex(points.size());
	for (int i = 0; i < points.size(); ++i)
		sliceDataIndex[i] = i;

	// 使用颜色预测编码
	Vec3i32 sum = Vec3i32::zero();
	for (const auto& p : points) {
		sum += p.color;
	}
	avgColor = sum.divideAndRound(points.size());

	// Octree root node
	octree_root = std::make_unique<OctreeNode>();
	octree_root->origin = origin;
	octree_root->edgeLength = edgeLength;
	octree_root->level = _tzcnt_u32(edgeLength);
	octree_root->sliceDataIndex = std::move(sliceDataIndex);

	std::queue<OctreeNode*> q_octree;
	q_octree.push(octree_root.get());
	do {
		const auto& node_octree = q_octree.front();
		auto halfLength = node_octree->edgeLength / 2;
		auto center = node_octree->origin + halfLength;

		// Classify points into 8 types. Then put the index of each point into the coressponding subSlicesDataIndex.
		std::array<std::vector<int>, 8> subSlicesDataIndex;
		for (int index : node_octree->sliceDataIndex) {
			Vec3i32 diff = points[index].position - center;
			int subIndex = 0;
			subIndex += (diff.x < 0 ? 0 : 4);
			subIndex += (diff.y < 0 ? 0 : 2);
			subIndex += (diff.z < 0 ? 0 : 1);
			subSlicesDataIndex[subIndex].push_back(index);
		}

		for (int i = 0; i < 8; ++i) {
			// The i-th octant is none-empty. 
			if (!subSlicesDataIndex[i].empty()) {
				// For none-leaf nodes, create new nodes and then push them to the queue.
				node_octree->sub_node[i] = std::make_unique<OctreeNode>();
				node_octree->sub_node[i]->origin = Vec3i32((i & 4 ? center.x : node_octree->origin.x),
														   (i & 2 ? center.y : node_octree->origin.y),
														   (i & 1 ? center.z : node_octree->origin.z));
				node_octree->sub_node[i]->edgeLength = halfLength;
				node_octree->sub_node[i]->level = node_octree->level - 1;

				if (node_octree->level > 1) {
					node_octree->sub_node[i]->sliceDataIndex = std::move(subSlicesDataIndex[i]);
					q_octree.push(node_octree->sub_node[i].get());
				}
				else {
					node_octree->sub_node[i]->rawColor = points[subSlicesDataIndex[i][0]].color;
				}
			}
		}

		// 顺便进行色度采样
		if (isChromaSubsampling && node_octree->level == 1) {
			Vec3i32 sum = Vec3i32::zero();
			int num = 0;
			for (int i = 0; i < 8; ++i) {
				if (node_octree->sub_node[i]) {
					num++;
					sum += node_octree->sub_node[i]->rawColor;
				}
			}
			node_octree->rawColor = sum.divideAndRound(num);
		}
		node_octree->sliceDataIndex.clear();
		q_octree.pop();
	} while (!q_octree.empty());
}

void Slice::Octree_Compute_Attribute_Diff() {
	std::queue<OctreeNode*> q_octree;
	q_octree.push(octree_root.get());
	do {
		const auto& node_octree = q_octree.front();

		if (node_octree->level > 1) {
			for (int i = 0; i < 8; ++i) {
				if (node_octree->sub_node[i])
					q_octree.push(node_octree->sub_node[i].get());
			}
		}
		else {
			// 计算差分，量化，赋值
			for (int i = 0; i < 8; ++i) {
				if (node_octree->sub_node[i]) {
					int diff_x = node_octree->sub_node[i]->rawColor.x - avgColor.x;
					int diff_y = node_octree->sub_node[i]->rawColor.y - avgColor.y;
					int diff_z = node_octree->sub_node[i]->rawColor.z - avgColor.z;
					node_octree->sub_node[i]->predictedColor.x = diff_Quantizer(diff_x, avgColor.x, quantizationBits);
					node_octree->sub_node[i]->predictedColor.y = diff_Quantizer(diff_y, avgColor.y, quantizationBits);
					node_octree->sub_node[i]->predictedColor.z = diff_Quantizer(diff_z, avgColor.z, quantizationBits);
				}
			}
			if (isChromaSubsampling) {
				int diff_y = node_octree->rawColor.y - avgColor.y;
				int diff_z = node_octree->rawColor.z - avgColor.z;
				node_octree->predictedColor.y = diff_Quantizer(diff_y, avgColor.y, quantizationBits);
				node_octree->predictedColor.z = diff_Quantizer(diff_z, avgColor.z, quantizationBits);
			}
		}

		q_octree.pop();
	} while (!q_octree.empty());
}

void Slice::Octree_encode() {
	std::queue<OctreeNode*> q_octree;
	q_octree.push(octree_root.get());
	do {
		const auto& node_octree = q_octree.front();
		int controlByte = 0;
		for (int i = 0; i < 8; ++i) {
			controlByte <<= 1;
			if (node_octree->sub_node[i]) {
				controlByte |= 1;
				if (node_octree->level > 1) {
					q_octree.push(node_octree->sub_node[i].get());
				}
			}
			else
				controlByte |= 0;
		}
		treeStream.push_back(controlByte);

		if (node_octree->level == 1) {
			if (isChromaSubsampling) {
				for (int i = 0; i < 8; ++i) {
					if (node_octree->sub_node[i])
						colorStream.push_back(node_octree->sub_node[i]->predictedColor.x);
				}
				colorStream.push_back(node_octree->predictedColor.y);
				colorStream.push_back(node_octree->predictedColor.z);
			}
			else {
				for (int i = 0; i < 8; ++i) {
					if (node_octree->sub_node[i]) {
						colorStream.push_back(node_octree->sub_node[i]->predictedColor.x);
						colorStream.push_back(node_octree->sub_node[i]->predictedColor.y);
						colorStream.push_back(node_octree->sub_node[i]->predictedColor.z);
					}
				}
			}
		}

		q_octree.pop();
	} while (!q_octree.empty());
}


std::vector<Point> Slice::decode() {
	int level = _tzcnt_u32(edgeLength);
	std::queue<DecodeTreeNode> q;
	q.push({ origin, edgeLength, level });
	int treeIndex = 0;
	int colorIndex = 0;
	do {
		const auto& node = q.front();
		int controlByte = treeStream[treeIndex];
		treeIndex++;
		// 倒数第二层读取颜色
		if (node.level == 1) {
			// 首先读取明度
			std::vector<Point> buffer;
			for (int i = 0; i < 8; ++i) {
				if (controlByte & 0x80) {
					auto center = node.origin + Vec3i32(1, 1, 1);
					auto subNodeOrigin = Vec3i32((i & 4 ? center.x : node.origin.x),
												 (i & 2 ? center.y : node.origin.y),
												 (i & 1 ? center.z : node.origin.z));

					uint8_t luma = colorStream[colorIndex] + avgColor.x;
					colorIndex++;
					uint8_t chroma1 = 0;
					uint8_t chroma2 = 0;
					if (!isChromaSubsampling) {
						chroma1 = colorStream[colorIndex] + avgColor.y;
						chroma2 = colorStream[colorIndex + 1] + avgColor.z;
						colorIndex += 2;
					}
					buffer.push_back({ subNodeOrigin, Vec3u8(luma, chroma1, chroma2) });
				}
				controlByte <<= 1;
			}

			if (isChromaSubsampling) {
				uint8_t chroma1 = colorStream[colorIndex] + avgColor.y;
				uint8_t chroma2 = colorStream[colorIndex + 1] + avgColor.z;
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

std::string Slice::serialize() {
	string entropyTree, entropyColor;
	entropyTree.assign(treeStream.cbegin(), treeStream.cend());
	entropyColor.assign(colorStream.cbegin(), colorStream.cend());

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

	std::string result;
	uint8_t tzNum = _tzcnt_u32(edgeLength);  // 保存边长对应的(1<<N)移位量，可以用8bit存下
	int treeNum = entropyTree.size();
	int colorsNum = entropyColor.size();
	result.insert(result.size(), (char*)&origin, 12);
	result.insert(result.size(), (char*)&treeNum, 4);
	result.insert(result.size(), (char*)&colorsNum, 4);
	result += tzNum;
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
	bool isChromaSubsampling = view[index + 1];
	EntropyEncodeType treeEntropyType = (EntropyEncodeType)view[index + 2];
	EntropyEncodeType colorEntropyType = (EntropyEncodeType)view[index + 3];
	index += 4;
	Vec3u8 avgColor;
	memcpy(&avgColor, &view[index], 3);
	index += 3;

	Slice slice(origin, 1 << tzNum);
	slice.isChromaSubsampling = isChromaSubsampling;
	slice.avgColor = avgColor;
	slice.treeEntropyType = treeEntropyType;
	slice.colorEntropyType = colorEntropyType;

	string entropyTree, entropyColor;
	entropyTree.resize(treeNum);
	entropyColor.resize(colorsNum);
	memcpy(entropyTree.data(), &view[index], treeNum);
	index += treeNum;
	memcpy(entropyColor.data(), &view[index], colorsNum);

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

	slice.treeStream.assign(entropyTree.cbegin(), entropyTree.cend());
	slice.colorStream.assign(entropyColor.cbegin(), entropyColor.cend());

	return slice;
}

void getRawColorRecursion(std::array<std::vector<uint8_t>, 3>& out, const OctreeNode* node) {
	// 建树的时候附带了level，这里直接用
	if (node->level == 1) {
		for (const auto& subNode : node->sub_node) {
			if (subNode) {
				out[0].push_back(subNode->rawColor.x);
				out[1].push_back(subNode->rawColor.y);
				out[2].push_back(subNode->rawColor.z);
			}
		}
	}
	else {
		for (const auto& subNode : node->sub_node) {
			if (subNode) {
				getRawColorRecursion(out, subNode.get());
			}
		}
	}
}

std::array<std::vector<uint8_t>, 3> Slice::getRawColorWithDFS() const {
	std::array<std::vector<uint8_t>, 3> result;
	getRawColorRecursion(result, octree_root.get());
	return result;
}