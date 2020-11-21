#include <zlibfunc.h>

#define CHECK_ERR(err, msg) { \
    if (err != Z_OK) { \
        fprintf(stderr, "%s error: %d\n", msg, err); \
        exit(1); \
    } \
}

string zlib_encode_string(const string& byteStream) {
	uLong srcLen = byteStream.size();
	uLong destLen = compressBound(srcLen);
	string dest;
	dest.resize(destLen);
	int err = compress((Bytef*)&dest[0], &destLen, (Bytef*)&byteStream[0], srcLen);
	dest.resize(destLen);
	CHECK_ERR(err, "zlib compress");
	return dest;
}

string zlib_decode_string(const string& byteStream) {
	uLong srcLen = byteStream.size();
	uLong destLen = DEFAULT_MAX_DECODE_BUF;
	string dest;
	dest.resize(destLen);
	int err = uncompress((Bytef*)&dest[0], &destLen, (Bytef*)&byteStream[0], srcLen);
	dest.resize(destLen);
	CHECK_ERR(err, "zlib uncompress");
	return dest;
}