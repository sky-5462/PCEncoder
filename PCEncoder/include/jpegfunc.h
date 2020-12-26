#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <jpeglib.h>


int Jpeg_Encode_From_Mem(std::vector<uint8_t>& encoded_data_dest, int quality, const std::vector<uint8_t>& input_image, int image_width, int image_height);

int Jpeg_Encode_From_Mem(std::vector<int8_t>& encoded_data_dest, int quality, const std::vector<uint8_t>& input_image, int image_width, int image_height);


int Jpeg_Decode_From_Mem(std::vector<uint8_t>& decoded_data_dest, std::vector<uint8_t>& jpeg_in_dat);

int Jpeg_Decode_From_Mem(std::vector<uint8_t>& decoded_data_dest, std::vector<int8_t>& jpeg_in_dat);

