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

void burn_tree(translation* node) {          // this function is used for deallocating translation tree
    if (node->zero)burn_tree(node->zero);
    if (node->one)burn_tree(node->one);
    free(node);
}

// process_n_bits_TO_STRING function reads n successive bits from the compressed file
// and stores it in a leaf of the translation tree
// after creating that leaf and sometimes nodes that are binding that leaf to the tree
void process_n_bits_TO_STRING(unsigned char* current_byte, int n, int* current_bit_count, FILE* fp_read, translation* node, unsigned char uChar) {
    for (int i = 0; i < n; i++) {
        if (*current_bit_count == 0) {
            fread(current_byte, 1, 1, fp_read);
            *current_bit_count = 8;
        }

        switch ((*current_byte) & check_decode) {
        case 0:
            //if (!(node->zero))node->zero = (translation*)malloc(sizeof(translation));
            if (!(node->zero))node->zero = new translation;
            node = node->zero;
            break;
        case 128:
            //if (!(node->one))node->one = (translation*)malloc(sizeof(translation));
            if (!(node->one))node->one = new translation;
            node = node->one;
            break;
        }
        (*current_byte) <<= 1;
        (*current_bit_count)--;
    }
    node->character = uChar;
}

// process_8_bits_NUMBER reads 8 successive bits from compressed file
// and returns it in unsigned char form
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
    //register FILE *original_fp=fopen(path_in,"rb"),*compressed_fp;
    errno_t err;
    register FILE* original_fp, * compressed_fp;
    err = fopen_s(&original_fp, path_in, "rb");
    if (NULL == original_fp) {
        cout << path_in << " file does not exist" << endl;
        return 0;
    }
    scompressed = path_in;
    scompressed += ".compressed";



    //--------------------1------------------------
    fseek(original_fp, 0, SEEK_END);
    long int size = ftell(original_fp);
    rewind(original_fp);
    // size information will later be written to compressed file
//---------------------------------------------



//--------------------2------------------------
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
    // This code block counts number of times that all of the unique bytes is used on the first block
    // and stores that info in 'number' array
    // after that it checks the 'number' array and writes the number of unique byte count to 'letter_count' variable
//---------------------------------------------



//--------------------3------------------------
//ersel array[letter_count * 2 - 1];
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
    // creating the base of translation array(and then sorting them by ascending numbers)
    //     this array of type 'ersel' will not be used after calculating transformed versions of every unique byte
    //     instead its info will be written in a new string array called str_arr 
//---------------------------------------------



//-------------------4-------------------------
    // min1 and min2 represents nodes that has minimum weights
    // isleaf is the pointer that traverses through leafs and
    // notleaf is the pointer that traverses through nodes that are not leafs
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
    // At every cycle, 2 of the least weighted nodes will be chosen to
    // create a new node that has weight equal to sum of their weights combined.
        // After we are done with these nodes they will become childrens of created nodes
        // and they will be passed so that they wont be used in this process again.
//---------------------------------------------

//-------------------5-------------------------
    for (e = array + letter_count * 2 - 2; e > array - 1; e--) {
        if (e->left) {
            e->left->bit = e->bit + e->left->bit;
        }
        if (e->right) {
            e->right->bit = e->bit + e->right->bit;
        }

    }
    // In this block we are adding the bytes from root to leafs
    // and after this is done every leaf will have a transformation string that corresponds to it
        // Note: It is actually a very neat process. Using 4th and 5th code blocks, we are making sure that
        // the most used character is using least number of bits.
            // Specific number of bits we re going to use for that character is determined by weight distribution
//---------------------------------------------











//compressed_fp=fopen(&scompressed[0],"wb");
    err = fopen_s(&compressed_fp, &scompressed[0], "wb");

    //-------------writes first---------------
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
    //This code block is writing byte count of the original file to compressed file's first 8 bytes
        //It is done like this to make sure that it can work on little, big or middle-endian systems
//----------------------------------------

//-----------writes second----------------
    fwrite(&letter_count, sizeof(int), 1, compressed_fp);
    total_bits += 8;
    //----------------------------------------


    //----writes third and forth---------------
    int check_password = 0;
    fwrite(&check_password, sizeof(int), 1, compressed_fp);
    total_bits += 8;
    //Above code block puts password to compressed file
//----------------------------------------

//------------writes fifth----------------
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
        // above lines will write the byte and the number of bits
        // we re going to need to represent this specific byte's transformated version
        // after here we are going to write the transformed version of the number bit by bit.

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
        // from this point on total bits doesnt represent total bits but
        // instead it represents 8*number_of_bytes we are gonna use on our compressed file
    }
    /* Above loop of the code is doing 2 in this order
    1- It determines number of bits that we are gonna write to the compressed file
        (this number only represents number of bytes thats going to be translated it doesn't include translation script
    2-It writes the translation script into [name of the original].compressed file and the str_arr array */
    //----------------------------------------


    cout << "The size of the ORIGINAL file is: " << size << " bytes" << endl;
    cout << "The size of the COMPRESSED file will be: " << total_bits / 8 << " bytes" << endl;
    cout << "Compressed file's size will be [%" << 100 * ((float)total_bits / 8 / size) << "] of the original file" << endl;
    if (total_bits / 8 > size) {
        cout << endl << "COMPRESSED FILE'S SIZE WILL BE HIGHER THAN THE ORIGINAL" << endl << endl;
    }


    //------------writes sixth----------------
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
    // Above code writes bytes that are translated from original file to the compressed file.

    if (current_bit_count == 8) {
        fwrite(&current_byte, 1, 1, compressed_fp);
    }
    else {
        current_byte <<= 8 - current_bit_count;
        fwrite(&current_byte, 1, 1, compressed_fp);
    }
    //----------------------------------------



    fclose(compressed_fp);
    fclose(original_fp);

    cout << "Compression is complete" << endl;

    return 0;
}

int huffman_decode_file(char* path_in)
{
    int letter_count, password_length;
    register FILE* fp_compressed, * fp_new;
    errno_t err;
    char newfile[260] = "New-";
    //fp_compressed=fopen(path_in,"rb");
    err = fopen_s(&fp_compressed, path_in, "rb");
    if (!fp_compressed) {
        cout << path_in << " does not exist" << endl;
        return 0;
    }



    //---------reads .first-----------
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
    // Size was written to the compressed file from least significiant byte 
    // to the most significiant byte to make sure system's endianness
    // does not affect the process and that is why we are processing size information like this
//-------------------------------



    str_without_compress(path_in);
    //strcpy(newfile+4,path_in);
    strcpy_s(newfile + 4, strlen(path_in) + 1, path_in);
    //---------reads .second-----------
    fread(&letter_count, sizeof(int), 1, fp_compressed);
    if (letter_count == 0)letter_count = 256;
    //-------------------------------



    //------------reads .third and .fourth--------------------
    fread(&password_length, sizeof(int), 1, fp_compressed);
    if (password_length) {
        //char real_password[password_length+1],password_input[257];
        char* real_password = new char[password_length + 1];
        char password_input[257];
        fread(real_password, 1, password_length, fp_compressed);
        real_password[password_length] = 0;
        cout << "Enter password:";
        cin >> password_input;
        char* rp = real_password, * pi = password_input;
        for (; *rp && *pi; rp++, pi++) {
            if (*rp != *pi) {
                cout << "Wrong password" << endl;
                fclose(fp_compressed);
                return 0;
            }
        }
        if (*rp != *pi) {
            cout << "Wrong password" << endl;
            fclose(fp_compressed);
            return 0;
        }
        cout << "Correct Password" << endl;
    }
    // this code block reads and checks the password
//----------------------------------------------------



//----------------reads .fifth----------------------
    // and stores transformation info into translation tree for later use
    unsigned char current_byte = 0, current_character;
    int current_bit_count = 0, len;
    //translation* root = (translation*)malloc(sizeof(translation));
    translation* root = new translation;

    for (int i = 0; i < letter_count; i++) {
        current_character = process_8_bits_NUMBER(&current_byte, current_bit_count, fp_compressed);
        len = process_8_bits_NUMBER(&current_byte, current_bit_count, fp_compressed);
        if (len == 0)len = 256;
        process_n_bits_TO_STRING(&current_byte, len, &current_bit_count, fp_compressed, root, current_character);
    }
    //--------------------------------------------------



    //----------------reads .sixth----------------------
        // Translates .sixth from info that is stored in translation tree
        // than writes it to new file
    //fp_new=fopen(newfile,"wb");
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
    //--------------------------------------------------

    burn_tree(root);
    fclose(fp_new);
    cout << "Decompression is complete" << endl;
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
    return outStream;
}