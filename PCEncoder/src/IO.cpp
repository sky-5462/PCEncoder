#include <common.h>
#include <IO.h>
#include <tuple>



// 8-bit transform
static std::tuple<int, int, int> RGBToYUV(int r, int g, int b) {
	// 变换矩阵随便找的一个，能用就行
	int y = lroundf(0.299f * r + 0.587f * g + 0.114f * b);
	int u = lroundf(-0.169f * r - 0.331f * g + 0.499f * b) + 128;
	int v = lroundf(0.499f * r - 0.418f * g - 0.0813f * b) + 128;
	y = (y > 255 ? 255 : (y < 0 ? 0 : y));
	u = (u > 255 ? 255 : (u < 0 ? 0 : u));
	v = (v > 255 ? 255 : (v < 0 ? 0 : v));
	return std::make_tuple(y, u, v);
}

static std::tuple<int, int, int> YUVToRGB(int y, int u, int v) {
	int r = lroundf(y + 1.402f * (v - 128));
	int g = lroundf(y - 0.344f * (u - 128) - 0.714f * (v - 128));
	int b = lroundf(y + 1.772f * (u - 128));
	r = (r > 255 ? 255 : (r < 0 ? 0 : r));
	g = (g > 255 ? 255 : (g < 0 ? 0 : g));
	b = (b > 255 ? 255 : (b < 0 ? 0 : b));
	return std::make_tuple(r, g, b);
}

PointBuffer IO::readText(const std::string& path) const {
	/* 目前的文件头，暂时不考虑非法格式和自由解析
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
	std::ifstream in(path);
	int pointNum;
	std::string skip;
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

	PointBuffer buffer;
	for (int i = 0; i < pointNum; ++i) {
		// 如果要区分整数和浮点可能需要参数指定
		int x, y, z, r, g, b;
		in >> x >> y >> z >> r >> g >> b;

		if (cspInternal != cspExternal) {
			if (cspExternal == ColorSpace::RGB && cspInternal == ColorSpace::YUV) {
				auto [t1, t2, t3] = RGBToYUV(r, g, b);
				buffer.push(x, y, z, t1, t2, t3);
			}
			else
				throw std::logic_error("Invalid colorspace");
		}
		else {
			buffer.push(x, y, z, r, g, b);
		}
	}

	return buffer;
}

void IO::writeText(const std::string& path, const PointBuffer& buffer) const {
	std::ofstream out(path);
	out << "ply\n";
	out << "format ascii 1.0\n";
	out << "element vertex " << buffer.size() << '\n';
	out << "property float x\n";
	out << "property float y\n";
	out << "property float z\n";
	out << "property uchar red\n";
	out << "property uchar green\n";
	out << "property uchar blue\n";
	out << "end_header\n";
	
	for (const auto& p : buffer) {
		out << p.position.x << ' ' << p.position.y << ' ' << p.position.z << ' ';
		auto [y, u, v] = Vec3i32(p.color);
		if (cspInternal != cspExternal) {
			if (cspExternal == ColorSpace::RGB && cspInternal == ColorSpace::YUV) {
				auto [t1, t2, t3] = YUVToRGB(y, u, v);
				out << t1 << ' ' << t2 << ' ' << t3 << '\n';
			}
			else
				throw std::logic_error("Invalid colorspace");
		}
		else {
			out << y << ' ' << u << ' ' << v << '\n';
		}
	}
}

std::string IO::readBin(const std::string& path) const {
	std::ifstream in(path, std::ios::binary | std::ios::ate);
	int bytesNum = in.tellg();
	in.seekg(0);
	std::string stream;
	stream.resize(bytesNum);
	in.read(stream.data(), bytesNum);
	in.close();

	if (entropyType == EntropyEncodeType::HUFFMAN) {
		stream = huffman_decode_string(stream);
	}
	else if (entropyType == EntropyEncodeType::RLE) {
		stream = runLengthDecode(stream);
	}
	else if (entropyType == EntropyEncodeType::RLE_HUFFMAN) {
		stream = runLengthDecode(huffman_decode_string(stream));
	}
	else if (entropyType == EntropyEncodeType::ZLIB) {
		stream = zlib_decode_string(stream);
	}
	return stream;
}

void IO::writeBin(const std::string& path, const std::string& stream) const {
	std::ofstream out(path, ios::binary);
	if (entropyType == EntropyEncodeType::HUFFMAN) {
		auto encodedStream = huffman_encode_string(stream);
		out.write(encodedStream.c_str(), encodedStream.size());
	}
	else if (entropyType == EntropyEncodeType::RLE) {
		auto encodedStream = runLengthEncode(stream);
		out.write(encodedStream.c_str(), encodedStream.size());
	}
	else if (entropyType == EntropyEncodeType::RLE_HUFFMAN) {
		auto encodedStream = huffman_encode_string(runLengthEncode(stream));
		out.write(encodedStream.c_str(), encodedStream.size());
	}
	else if (entropyType == EntropyEncodeType::ZLIB) {
		auto encodedStream = zlib_encode_string(stream);
		out.write(encodedStream.c_str(), encodedStream.size());
	}
	else
		out.write(stream.c_str(), stream.size());
}