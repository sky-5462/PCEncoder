#pragma once

#include <common.h>
#include <huffman.h>
#include <RLE.h>
#include <fstream>
#include <string>

enum class TextFormat {
	PLY
};

enum class ColorSpace {
	RGB,
	YUV
};

enum class EntropyEncodeType {
	NONE,
	RLE,
	HUFFMAN,
	RLE_HUFFMAN
};

struct IOParameters {
	TextFormat textFormat;
	ColorSpace cspExternal;
	ColorSpace cspInternal;
	EntropyEncodeType entropyType;

	IOParameters() :
		textFormat(TextFormat::PLY),
		cspExternal(ColorSpace::RGB),
		cspInternal(ColorSpace::YUV),
		entropyType(EntropyEncodeType::HUFFMAN) {
	}
};

class IO {
public:
	IO(const IOParameters& para) :
		textFormat(para.textFormat),
		cspExternal(para.cspExternal),
		cspInternal(para.cspInternal),
		entropyType(para.entropyType) {
	}

	PointBuffer readText(const std::string& path) const;
	void writeText(const std::string& path, const PointBuffer& buffer) const;

	std::string readBin(const std::string& path) const;
	void writeBin(const std::string& path, const std::string& stream) const;

private:
	TextFormat textFormat;
	ColorSpace cspExternal;
	ColorSpace cspInternal;
	EntropyEncodeType entropyType;
};