#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <iostream>
#include <cstdio>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <fstream>
using namespace std;

struct ersel {
    ersel* left, * right;
    long int number;
    unsigned char character;
    string bit;
};

bool erselcompare0(ersel a, ersel b);

struct translation {
    translation* zero = NULL, * one = NULL;
    unsigned char character;
};

void write_from_uChar(unsigned char, unsigned char*, int, FILE*);

void str_without_compress(char*);

unsigned char process_8_bits_NUMBER(unsigned char*, int, FILE*);

void process_n_bits_TO_STRING(unsigned char*, int, int*, FILE*, translation*, unsigned char);

void burn_tree(translation*);

int huffman_encode_file(char* path_in);

int huffman_decode_file(char* path_in);

string huffman_encode_string(const string& byteStream);

string huffman_decode_string(const string& byteStream);

#endif