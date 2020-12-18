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
	// No points in the slice.
	if (points.empty()) return;

	// Create index for points vector.
	std::vector<int> sliceDataIndex(points.size());
	for (int i = 0; i < points.size(); ++i)
		sliceDataIndex[i] = i;

	// 使用颜色预测编码
	Vec3i32 sum = Vec3i32::zero();
	for (const auto& p : points) {
		sum += p.color;
	}
	avgColor = sum / points.size();

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
				node_octree->sub_node[i]->sliceDataIndex = std::move(subSlicesDataIndex[i]);

				if (node_octree->level > 1) {
					q_octree.push(node_octree->sub_node[i].get());
				}
				else {
					// 倒数第二层已经得到叶子层结构，不需要写入输出流

					// leaf-nodes attributes
					node_octree->sub_node[i]->color = Vec3u8(0, 0, 0);

					// 初始构造八叉树时，不计算差分，不进行色度采样，获得明度信息
					// subSlicesDataIndex已经移动到node_octree里面了，不能再用前面的
					const auto& subIndics = node_octree->sub_node[i]->sliceDataIndex;
					int sum = 0, sum1 = 0, sum2 = 0;
					int num = subIndics.size();
					for (int index : subIndics) {
						sum += points[index].color.x;
						sum1 += points[index].color.y;
						sum2 += points[index].color.z;
					}

					int luma = lroundf((float)sum / num);
					int chroma1 = lroundf((float)sum1 / num);
					int chroma2 = lroundf((float)sum2 / num);

					node_octree->sub_node[i]->color.x = luma;
					node_octree->sub_node[i]->color.y = chroma1;
					node_octree->sub_node[i]->color.z = chroma2;
				}
			}
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
					int diff_x = node_octree->sub_node[i]->color.x - avgColor.x;
					int diff_y = node_octree->sub_node[i]->color.y - avgColor.y;
					int diff_z = node_octree->sub_node[i]->color.z - avgColor.z;
					node_octree->sub_node[i]->color.x = diff_Quantizer(diff_x, avgColor.x, quantizationBits);
					node_octree->sub_node[i]->color.y = diff_Quantizer(diff_y, avgColor.y, quantizationBits);
					node_octree->sub_node[i]->color.z = diff_Quantizer(diff_z, avgColor.z, quantizationBits);
				}
			}
			if (isChromaSubsampling) {
				int diff_y = node_octree->color.y - avgColor.y;
				int diff_z = node_octree->color.z - avgColor.z;
				node_octree->color.y = diff_Quantizer(diff_y, avgColor.y, quantizationBits);
				node_octree->color.z = diff_Quantizer(diff_z, avgColor.z, quantizationBits);
			}
		}

		q_octree.pop();
	} while (!q_octree.empty());
}

void Slice::Octree_Chroma_Subsample() {
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
			node_octree->color = Vec3u8(0, 0, 0);
			// 统计色度chroma
			int sum1 = 0, sum2 = 0;
			int num = 0;
			for (int i = 0; i < 8; ++i) {
				if (node_octree->sub_node[i]) {
					num++;
					sum1 += node_octree->sub_node[i]->color.y;
					sum2 += node_octree->sub_node[i]->color.z;
				}
			}
			// 计算均值作为抽样结果，赋值
			int chroma1 = lroundf((float)sum1 / num);
			int chroma2 = lroundf((float)sum2 / num);
			node_octree->color.y = chroma1;
			node_octree->color.z = chroma2;
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
		encodedTree.push_back(controlByte);

		if (node_octree->level == 1) {
			if (isChromaSubsampling) {
				for (int i = 0; i < 8; ++i) {
					if (node_octree->sub_node[i])
						encodedColor.push_back(node_octree->sub_node[i]->color.x);
				}
				encodedColor.push_back(node_octree->color.y);
				encodedColor.push_back(node_octree->color.z);
			}
			else {
				for (int i = 0; i < 8; ++i) {
					if (node_octree->sub_node[i]) {
						encodedColor.push_back(node_octree->sub_node[i]->color.x);
						encodedColor.push_back(node_octree->sub_node[i]->color.y);
						encodedColor.push_back(node_octree->sub_node[i]->color.z);
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

std::string Slice::serialize() {
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
	slice.entropyTree.resize(treeNum);
	slice.entropyColor.resize(colorsNum);
	memcpy(slice.entropyTree.data(), &view[index], treeNum);
	index += treeNum;
	memcpy(slice.entropyColor.data(), &view[index], colorsNum);

	if (treeEntropyType == EntropyEncodeType::HUFFMAN) {
		slice.entropyTree = huffman_decode_string(slice.entropyTree);
	}
	else if (treeEntropyType == EntropyEncodeType::RLE) {
		slice.entropyTree = runLengthDecode(slice.entropyTree);
	}
	else if (treeEntropyType == EntropyEncodeType::RLE_HUFFMAN) {
		slice.entropyTree = runLengthDecode(huffman_decode_string(slice.entropyTree));
	}
	else if (treeEntropyType == EntropyEncodeType::ZLIB) {
		slice.entropyTree = zlib_decode_string(slice.entropyTree);
	}

	if (colorEntropyType == EntropyEncodeType::HUFFMAN) {
		slice.entropyColor = huffman_decode_string(slice.entropyColor);
	}
	else if (colorEntropyType == EntropyEncodeType::RLE) {
		slice.entropyColor = runLengthDecode(slice.entropyColor);
	}
	else if (colorEntropyType == EntropyEncodeType::RLE_HUFFMAN) {
		slice.entropyColor = runLengthDecode(huffman_decode_string(slice.entropyColor));
	}
	else if (colorEntropyType == EntropyEncodeType::ZLIB) {
		slice.entropyColor = zlib_decode_string(slice.entropyColor);
	}

	slice.encodedTree.assign(slice.entropyTree.cbegin(), slice.entropyTree.cend());
	slice.encodedColor.assign(slice.entropyColor.cbegin(), slice.entropyColor.cend());

	return slice;
}


