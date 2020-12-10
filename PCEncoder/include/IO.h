#pragma once
#include <common.h>
#include <fstream>
#include <string>

enum class TextFormat {
	PLY
};

enum class ColorSpace {
	RGB,
	YUV
};

struct IOParameters {
	TextFormat textFormat;
	ColorSpace cspExternal;
	ColorSpace cspInternal;

	IOParameters() :
		textFormat(TextFormat::PLY),
		cspExternal(ColorSpace::RGB),
		cspInternal(ColorSpace::YUV) {
	}
};

class IO {
public:
	IO(const IOParameters& para) :
		textFormat(para.textFormat),
		cspExternal(para.cspExternal),
		cspInternal(para.cspInternal) {
	}

	PointBuffer readText(const std::string& path) const;
	void writeText(const std::string& path, const PointBuffer& buffer) const;

	std::string readBin(const std::string& path) const;
	void writeBin(const std::string& path, const std::string& stream) const;

private:
	TextFormat textFormat;
	ColorSpace cspExternal;
	ColorSpace cspInternal;
};