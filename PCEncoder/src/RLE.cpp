#include <RLE.h>

std::string runLengthEncode(const std::string& stream) {
	std::string result;
	int count = 1;
	char prev = stream[0];
	for (int i = 1; i < stream.size(); ++i) {
		char ch = stream[i];
		if (count == 255) {
			// 说实话这种情况几乎不存在吧。。。
			result += 255;
			result += prev;
			prev = ch;
			count = 1;
		}
		else {
			if (prev == ch)
				count++;
			else {
				result += count;
				result += prev;
				prev = ch;
				count = 1;
			}
		}
	}
	result += count;
	result += prev;

	return result;
}

std::string runLengthDecode(const std::string& stream) {
	std::string result;
	int index = 0;
	while (index < stream.size()) {
		uint8_t count = stream[index];
		char ch = stream[index + 1];
		result.insert(result.size(), count, ch);
		index += 2;
	}
	return result;
}
