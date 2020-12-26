#ifndef ZLIBFUNC_H
#define ZLIBFUNC_H

//#pragma comment(lib, ".\\lib\\zlibstat.lib") 
#include <zlib.h>
#include <string>
using namespace std;

#define DEFAULT_MAX_DECODE_BUF 32*1024*1024

string zlib_encode_string(const string& byteStream);
string zlib_decode_string(const string& byteStream);


#endif // !ZLIBFUNC_H