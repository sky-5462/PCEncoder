#include "Huffman.h"

unsigned char check_decode = 0b10000000;

bool erselcompare0(ersel a, ersel b) {
    return a.number < b.number;
}

void write_from_uChar(unsigned char uChar, unsigned char* current_byte, int current_bit_count, FILE* fp_write) {
    (*current_byte) <<= 8 - current_bit_count;
    (*current_byte) |= (uChar >> current_bit_count);
    fwrite(current_byte, 1, 1, fp_write);
    *current_byte = uChar;
}

void burn_tree(translation* node) {          
    if (node->zero)burn_tree(node->zero);
    if (node->one)burn_tree(node->one);
    free(node);
}


void process_n_bits_TO_STRING(unsigned char* current_byte, int n, int* current_bit_count, FILE* fp_read, translation* node, unsigned char uChar) {
    for (int i = 0; i < n; i++) {
        if (*current_bit_count == 0) {
            fread(current_byte, 1, 1, fp_read);
            *current_bit_count = 8;
        }

        switch ((*current_byte) & check_decode) {
        case 0:
            if (!(node->zero))node->zero = new translation;
            node = node->zero;
            break;
        case 128:
            if (!(node->one))node->one = new translation;
            node = node->one;
            break;
        }
        (*current_byte) <<= 1;
        (*current_bit_count)--;
    }
    node->character = uChar;
}


unsigned char process_8_bits_NUMBER(unsigned char* current_byte, int current_bit_count, FILE* fp_read) {
    unsigned char val, temp_byte;
    fread(&temp_byte, 1, 1, fp_read);
    val = (*current_byte) | (temp_byte >> current_bit_count);
    *current_byte = temp_byte << 8 - current_bit_count;
    return val;
}

void str_without_compress(char* p) {
    char s[12] = "desserpmoc.";
    char* p1 = p, * p2 = s;
    if (strlen(p) < 12)return;
    for (; *p1; p1++);
    p1--;
    for (; *p2; p1--, p2++) {
        if (*p1 != *p2) {
            return;
        }
    }
    p1++;
    *p1 = 0;
}

int huffman_encode_file(char* path_in)
{

    long int number[256];
    long int bits = 0, total_bits = 0;
    int letter_count = 0;
    for (long int* i = number; i < number + 256; i++) {
        *i = 0;
    }

    string scompressed;
    errno_t err;
    register FILE* original_fp, * compressed_fp;
    err = fopen_s(&original_fp, path_in, "rb");
    if (NULL == original_fp) {
        cout << path_in << " file does not exist" << endl;
        return 0;
    }
    scompressed = path_in;
    scompressed += ".compressed";




    fseek(original_fp, 0, SEEK_END);
    long int size = ftell(original_fp);
    rewind(original_fp);

    register unsigned char x;
    fread(&x, 1, 1, original_fp);
    for (long int i = 0; i < size; i++) {
        number[x]++;
        fread(&x, 1, 1, original_fp);
    }
    rewind(original_fp);

    for (long int* i = number; i < number + 256; i++) {
        if (*i) {
            letter_count++;
        }
    }

    ersel* array = new ersel[letter_count * 2 - 1];
    register ersel* e = array;
    for (long int* i = number; i < number + 256; i++) {
        if (*i) {
            e->right = NULL;
            e->left = NULL;
            e->number = *i;
            e->character = i - number;
            e++;
        }
    }
    sort(array, array + letter_count, erselcompare0);

    ersel* min1 = array, * min2 = array + 1, * current = array + letter_count, * notleaf = array + letter_count, * isleaf = array + 2;
    for (int i = 0; i < letter_count - 1; i++) {
        current->number = min1->number + min2->number;
        current->left = min1;
        current->right = min2;
        min1->bit = "1";
        min2->bit = "0";
        current++;

        if (isleaf >= array + letter_count) {
            min1 = notleaf;
            notleaf++;
        }
        else {
            if (isleaf->number < notleaf->number) {
                min1 = isleaf;
                isleaf++;
            }
            else {
                min1 = notleaf;
                notleaf++;
            }
        }

        if (isleaf >= array + letter_count) {
            min2 = notleaf;
            notleaf++;
        }
        else if (notleaf >= current) {
            min2 = isleaf;
            isleaf++;
        }
        else {
            if (isleaf->number < notleaf->number) {
                min2 = isleaf;
                isleaf++;
            }
            else {
                min2 = notleaf;
                notleaf++;
            }
        }

    }

    for (e = array + letter_count * 2 - 2; e > array - 1; e--) {
        if (e->left) {
            e->left->bit = e->bit + e->left->bit;
        }
        if (e->right) {
            e->right->bit = e->bit + e->right->bit;
        }

    }

    err = fopen_s(&compressed_fp, &scompressed[0], "wb");

    {
        long int temp_size = size;
        unsigned char temp;
        for (int i = 0; i < 8; i++) {
            temp = temp_size % 256;
            fwrite(&temp, 1, 1, compressed_fp);
            temp_size /= 256;
        }
        total_bits += 64;
    }

    fwrite(&letter_count, sizeof(int), 1, compressed_fp);
    total_bits += 8;

    int check_password = 0;
    fwrite(&check_password, sizeof(int), 1, compressed_fp);
    total_bits += 8;

    char* str_pointer;
    unsigned char current_byte, len, current_character;
    int current_bit_count = 0;
    string str_arr[256];
    for (e = array; e < array + letter_count; e++) {
        str_arr[(e->character)] = e->bit;
        len = e->bit.length();
        current_character = e->character;

        write_from_uChar(current_character, &current_byte, current_bit_count, compressed_fp);
        write_from_uChar(len, &current_byte, current_bit_count, compressed_fp);
        total_bits += len + 16;


        str_pointer = &e->bit[0];
        while (*str_pointer) {
            if (current_bit_count == 8) {
                fwrite(&current_byte, 1, 1, compressed_fp);
                current_bit_count = 0;
            }
            switch (*str_pointer) {
            case '1':current_byte <<= 1; current_byte |= 1; current_bit_count++; break;
            case '0':current_byte <<= 1; current_bit_count++; break;
            default:cout << "An error has occurred" << endl << "Compression process aborted" << endl;
                fclose(compressed_fp);
                fclose(original_fp);
                remove(&scompressed[0]);
                return 1;
            }
            str_pointer++;
        }

        bits += len * e->number;
    }
    total_bits += bits;
    unsigned char bits_in_last_byte = total_bits % 8;
    if (bits_in_last_byte) {
        total_bits = (total_bits / 8 + 1) * 8;

    }



    cout << "[Huffman Encoder] The size of the ORIGINAL file is: " << size << " bytes" << endl;
    cout << "[Huffman Encoder] The size of the COMPRESSED file will be: " << total_bits / 8 << " bytes" << endl;
    cout << "[Huffman Encoder] Compressed file's size will be [%" << 100 * ((float)total_bits / 8 / size) << "] of the original file" << endl;
    if (total_bits / 8 > size) {
        cout << endl << "[Huffman Encoder] COMPRESSED FILE'S SIZE WILL BE HIGHER THAN THE ORIGINAL" << endl << endl;
    }



    fread(&x, 1, 1, original_fp);
    for (long int i = 0; i < bits;) {
        str_pointer = &str_arr[x][0];
        while (*str_pointer) {
            if (current_bit_count == 8) {
                fwrite(&current_byte, 1, 1, compressed_fp);
                current_bit_count = 0;
            }
            switch (*str_pointer) {
            case '1':i++; current_byte <<= 1; current_byte |= 1; current_bit_count++; break;
            case '0':i++; current_byte <<= 1; current_bit_count++; break;
            default:cout << "An error has occurred" << endl << "Process has been aborted";
                fclose(compressed_fp);
                fclose(original_fp);
                remove(&scompressed[0]);
                return 2;
            }
            str_pointer++;
        }
        fread(&x, 1, 1, original_fp);
    }


    if (current_bit_count == 8) {
        fwrite(&current_byte, 1, 1, compressed_fp);
    }
    else {
        current_byte <<= 8 - current_bit_count;
        fwrite(&current_byte, 1, 1, compressed_fp);
    }




    fclose(compressed_fp);
    fclose(original_fp);

    cout << "[Huffman Encoder] Compression completed." << endl;

    return 0;
}

int huffman_decode_file(char* path_in)
{
    int letter_count, password_length;
    register FILE* fp_compressed, * fp_new;
    errno_t err;
    char newfile[260] = "New-";
    err = fopen_s(&fp_compressed, path_in, "rb");
    if (!fp_compressed) {
        cout << path_in << " does not exist" << endl;
        return 0;
    }



    long int size = 0;
    {
        long int multiplier = 1;
        unsigned char temp;
        for (int i = 0; i < 8; i++) {
            fread(&temp, 1, 1, fp_compressed);
            size += temp * multiplier;
            multiplier *= 256;
        }
    }


    str_without_compress(path_in);
    strcpy_s(newfile + 4, strlen(path_in) + 1, path_in);
    fread(&letter_count, sizeof(int), 1, fp_compressed);
    if (letter_count == 0)letter_count = 256;

    fread(&password_length, sizeof(int), 1, fp_compressed);


    unsigned char current_byte = 0, current_character;
    int current_bit_count = 0, len;
    translation* root = new translation;

    for (int i = 0; i < letter_count; i++) {
        current_character = process_8_bits_NUMBER(&current_byte, current_bit_count, fp_compressed);
        len = process_8_bits_NUMBER(&current_byte, current_bit_count, fp_compressed);
        if (len == 0)len = 256;
        process_n_bits_TO_STRING(&current_byte, len, &current_bit_count, fp_compressed, root, current_character);
    }





    err = fopen_s(&fp_new, newfile, "wb");
    translation* node;

    for (long int i = 0; i < size; i++) {
        node = root;
        while (node->zero || node->one) {
            if (current_bit_count == 0) {
                fread(&current_byte, 1, 1, fp_compressed);
                current_bit_count = 8;
            }
            if (current_byte & check_decode) {
                node = node->one;
            }
            else {
                node = node->zero;
            }
            current_byte <<= 1;
            current_bit_count--;
        }
        fwrite(&(node->character), 1, 1, fp_new);
    }


    burn_tree(root);
    fclose(fp_new);
    fclose(fp_compressed);
    cout << "[Huffman Decoder] Decompression completed." << endl;
    return 0;
}

string huffman_encode_string(const string& byteStream) {
    std::string outStream;
    std::ofstream os("tmp1", std::ios::binary);
    os.write(byteStream.c_str(), byteStream.size());
    os.close();

    char path[] = "tmp1";
    huffman_encode_file(path);

    std::ifstream is("tmp1.compressed", std::ios::binary | std::ios::ate);
    int bytesNum = is.tellg();
    is.seekg(0);
    outStream.resize(bytesNum);
    is.read(outStream.data(), bytesNum);
    is.close();
    int err;
    err = remove("tmp1"); 
    if (err != 0) cout << "[Huffman Encoder] Delete temp failed." << endl;
    err = remove("tmp1.compressed");
    if (err != 0) cout << "[Huffman Encoder] Delete temp failed." << endl;
    return outStream;
}

string huffman_decode_string(const string& byteStream) {
    std::string outStream;
    std::ofstream os("tmp2", std::ios::binary);
    os.write(byteStream.c_str(), byteStream.size());
    os.close();

    char path[] = "tmp2";
    huffman_decode_file(path);

    std::ifstream is("New-tmp2", std::ios::binary | std::ios::ate);
    int bytesNum = is.tellg();
    is.seekg(0);
    outStream.resize(bytesNum);
    is.read(outStream.data(), bytesNum);
    is.close();
    int err;
    err = remove("tmp2");
    if (err != 0) cout << "[Huffman Decoder] Delete temp failed." << endl;
    err = remove("New-tmp2");
    if (err != 0) cout << "[Huffman Decoder] Delete temp failed." << endl;
    return outStream;
}