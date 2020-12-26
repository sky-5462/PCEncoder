#include <jpegfunc.h>

/* input_image 存储的方式为：R1,G1,B1,R2,G2,B2……*/
int Jpeg_Encode_From_Mem(std::vector<uint8_t>& encoded_data_dest, int quality, const std::vector<uint8_t>& input_image, int image_width, int image_height) {

    struct jpeg_compress_struct cinfo;

    struct jpeg_error_mgr jerr;
    /* More stuff */
    JSAMPROW row_pointer[1];	/* pointer to JSAMPLE row[s] */
    int row_stride;		/* physical row width in image buffer */

    cinfo.err = jpeg_std_error(&jerr);
    /* Now we can initialize the JPEG compression object. */
    jpeg_create_compress(&cinfo);

    cinfo.image_width = image_width; 	/* image width and height, in pixels */
    cinfo.image_height = image_height;
    cinfo.input_components = 3;		/* # of color components per pixel */
    cinfo.in_color_space = JCS_RGB; 	/* colorspace of input image */

    jpeg_set_defaults(&cinfo);

    jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);

    unsigned char* out_buffer;
    size_t BufferSize = 0;

    jpeg_mem_dest(&cinfo, &out_buffer, &BufferSize); // data written to mem

    jpeg_start_compress(&cinfo, TRUE);

    row_stride = image_width * cinfo.input_components; /* JSAMPLEs per row in image_buffer */

    while (cinfo.next_scanline < cinfo.image_height) {
        /* jpeg_write_scanlines expects an array of pointers to scanlines.
         * Here the array is only one element long, but you could pass
         * more than one scanline at a time if that's more convenient.
         */
        row_pointer[0] = (unsigned char*)&input_image[cinfo.next_scanline * row_stride];
        (void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);

    encoded_data_dest.resize(BufferSize);
    std::copy((char*)out_buffer, (char*)(out_buffer + BufferSize), encoded_data_dest.data());
    free(out_buffer);

    jpeg_destroy_compress(&cinfo);

    return BufferSize;
}

int Jpeg_Encode_From_Mem(std::vector<int8_t>& encoded_data_dest, int quality, const std::vector<uint8_t>& input_image, int image_width, int image_height) {

    struct jpeg_compress_struct cinfo;

    struct jpeg_error_mgr jerr;
    /* More stuff */
    JSAMPROW row_pointer[1];	/* pointer to JSAMPLE row[s] */
    int row_stride;		/* physical row width in image buffer */

    cinfo.err = jpeg_std_error(&jerr);
    /* Now we can initialize the JPEG compression object. */
    jpeg_create_compress(&cinfo);

    cinfo.image_width = image_width; 	/* image width and height, in pixels */
    cinfo.image_height = image_height;
    cinfo.input_components = 3;		/* # of color components per pixel */
    cinfo.in_color_space = JCS_RGB; 	/* colorspace of input image */

    jpeg_set_defaults(&cinfo);

    jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);

    unsigned char* out_buffer;
    size_t BufferSize = 0;

    jpeg_mem_dest(&cinfo, &out_buffer, &BufferSize); // data written to mem

    jpeg_start_compress(&cinfo, TRUE);

    row_stride = image_width * cinfo.input_components; /* JSAMPLEs per row in image_buffer */

    while (cinfo.next_scanline < cinfo.image_height) {
        /* jpeg_write_scanlines expects an array of pointers to scanlines.
         * Here the array is only one element long, but you could pass
         * more than one scanline at a time if that's more convenient.
         */
        row_pointer[0] = (unsigned char*)&input_image[cinfo.next_scanline * row_stride];
        (void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);

    encoded_data_dest.resize(BufferSize);
    std::copy((char*)out_buffer, (char*)(out_buffer + BufferSize), encoded_data_dest.data());
    free(out_buffer);

    jpeg_destroy_compress(&cinfo);

    return BufferSize;
}


int Jpeg_Decode_From_Mem(std::vector<uint8_t>& decoded_data_dest, std::vector<uint8_t>& jpeg_in_dat) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPARRAY buffer;		/* Output row buffer */
    int row_stride;		/* physical row width in output buffer */


    cinfo.err = jpeg_std_error(&jerr);

    jpeg_create_decompress(&cinfo);

    /* Specify data source for decompression */
    if (jpeg_in_dat.size())
        jpeg_mem_src(&cinfo, (unsigned char*)jpeg_in_dat.data(), jpeg_in_dat.size());
    else
        return -1;

    /* Read file header, set default decompression parameters */
    (void)jpeg_read_header(&cinfo, TRUE);

    /* Start decompressor */
    (void)jpeg_start_decompress(&cinfo);

    /* JSAMPLEs per row in output buffer */
    row_stride = cinfo.output_width * cinfo.output_components;
    /* Make a one-row-high sample array that will go away when done with image */
    buffer = (*cinfo.mem->alloc_sarray)
        ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

    if (decoded_data_dest.size() != row_stride * cinfo.output_height)
        decoded_data_dest.resize(row_stride * cinfo.output_height);

    while (cinfo.output_scanline < cinfo.output_height)
    {
        // read a scanline and copy it to the output image
        (void)jpeg_read_scanlines(&cinfo, buffer, 1);
        std::copy(buffer[0], buffer[0] + row_stride, &decoded_data_dest[row_stride * (cinfo.output_scanline - 1)]);
    }

    (void)jpeg_finish_decompress(&cinfo);

    jpeg_destroy_decompress(&cinfo);

    return 0;
}

int Jpeg_Decode_From_Mem(std::vector<uint8_t>& decoded_data_dest, std::vector<int8_t>& jpeg_in_dat) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPARRAY buffer;		/* Output row buffer */
    int row_stride;		/* physical row width in output buffer */


    cinfo.err = jpeg_std_error(&jerr);

    jpeg_create_decompress(&cinfo);

    /* Specify data source for decompression */
    if (jpeg_in_dat.size())
        jpeg_mem_src(&cinfo, (unsigned char*)jpeg_in_dat.data(), jpeg_in_dat.size());
    else
        return -1;

    /* Read file header, set default decompression parameters */
    (void)jpeg_read_header(&cinfo, TRUE);

    /* Start decompressor */
    (void)jpeg_start_decompress(&cinfo);

    /* JSAMPLEs per row in output buffer */
    row_stride = cinfo.output_width * cinfo.output_components;
    /* Make a one-row-high sample array that will go away when done with image */
    buffer = (*cinfo.mem->alloc_sarray)
        ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

    if (decoded_data_dest.size() != row_stride * cinfo.output_height)
        decoded_data_dest.resize(row_stride * cinfo.output_height);

    while (cinfo.output_scanline < cinfo.output_height)
    {
        // read a scanline and copy it to the output image
        (void)jpeg_read_scanlines(&cinfo, buffer, 1);
        std::copy(buffer[0], buffer[0] + row_stride, &decoded_data_dest[row_stride * (cinfo.output_scanline - 1)]);
    }

    (void)jpeg_finish_decompress(&cinfo);

    jpeg_destroy_decompress(&cinfo);

    return 0;
}
