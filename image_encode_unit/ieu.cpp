#include <algorithm>
#include <jpeglib.h>
#include <cmath>
#include <format>
#include <iostream>
#include <tiff.h>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define BLOCK_SIZE 8

struct YCbCr {
    uint8_t Y;
    uint8_t Cb;
    uint8_t Cr;
};

inline double alpha(int u) { return (u == 0) ? (1.0 / sqrt(2.0)) : 1.0; }
inline void ConvertRGBToYCbCr(unsigned char R, unsigned char G, unsigned char B,
               unsigned char &Y, unsigned char &Cb, unsigned char &Cr) {
    double y  =  0.299    * R + 0.587    * G + 0.114    * B;
    double cb = -0.168736 * R - 0.331264 * G + 0.5      * B + 128;
    double cr =  0.5      * R - 0.418688 * G - 0.081312 * B + 128;

    Y  = static_cast<unsigned char>(std::max(0.0, std::min(255.0, y)));
    Cb = static_cast<unsigned char>(std::max(0.0, std::min(255.0, cb)));
    Cr = static_cast<unsigned char>(std::max(0.0, std::min(255.0, cr)));
}

void IDCT_8x8(const JCOEF* F, double f[BLOCK_SIZE][BLOCK_SIZE]) {
    for (int i = 0; i < BLOCK_SIZE; ++i) {
        for (int j = 0; j < BLOCK_SIZE; ++j) {
            double sum = 0.0;
            for (int u = 0; u < BLOCK_SIZE; ++u) {
                for (int v = 0; v < BLOCK_SIZE; ++v) {
                    sum += alpha(u) * alpha(v) *
                           F[u*BLOCK_SIZE + v] *
                           cos((2*i+1) * u * M_PI / 16.0) *
                           cos((2*j+1) * v * M_PI / 16.0);
                }
            }
            f[i][j] = 0.25 * sum;
        }
    }
}

void DCT_8x8(const double (&f)[BLOCK_SIZE][BLOCK_SIZE], JCOEF* F) {
    for (int i = 0; i < BLOCK_SIZE; ++i) {
        for (int j = 0; j < BLOCK_SIZE; ++j) {
            double sum = 0.0;
            for (int x = 0; x < BLOCK_SIZE; ++x) {
                for (int y = 0; y < BLOCK_SIZE; ++y) {
                    sum += f[x][y] *
                           cos((2*x+1) * i * M_PI / 16.0) *
                           cos((2*y+1) * j * M_PI / 16.0);
                }
            }
            F[i*BLOCK_SIZE + j] = (JCOEF)(0.25 * alpha(i)*alpha(j) * sum);
        }
    }
}

void Blend8x8(double (&Y)[BLOCK_SIZE][BLOCK_SIZE], double (&Cb)[BLOCK_SIZE][BLOCK_SIZE], double (&Cr)[BLOCK_SIZE][BLOCK_SIZE], YCbCr (&tem8x8)[BLOCK_SIZE][BLOCK_SIZE], uint8_t alpha) {
    float opacity = static_cast<float>(alpha) / 255.0f;
    for (int i = 0; i < BLOCK_SIZE; i++) {
        for (int j = 0; j < BLOCK_SIZE; j++) {
            if (tem8x8[i][j].Y < 64) {
                Y[i][j] = ((static_cast<int>(Y[i][j] * (1 + opacity))) > 255) ? 255 : static_cast<int>(Y[i][j] * (1 + opacity));
            }



        }
    }
}

void ReadWatermark(YCbCr(&data)[128 * 64]) {
    FILE* fp = fopen("watermark.jpg", "rb");
    if (!fp) {
        std::cout << "watermark.jpg not found" << std::endl;
        exit(EXIT_FAILURE);
    }

    jpeg_decompress_struct cinfo;
    jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, fp);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    uint32_t width  = cinfo.output_width;
    uint32_t height = cinfo.output_height;
    uint32_t channels = cinfo.output_components;

    std::vector<unsigned char> buffer(width * channels);
    while (cinfo.output_scanline < cinfo.output_height) {
        unsigned char* rowptr = buffer.data();
        jpeg_read_scanlines(&cinfo, &rowptr, 1);
        int row = cinfo.output_scanline - 1;
        for (int col = 0; col < width; col++) {
            unsigned char R = buffer[col * 3 + 0];
            unsigned char G = buffer[col * 3 + 1];
            unsigned char B = buffer[col * 3 + 2];
            unsigned char Y, Cb, Cr;
            ConvertRGBToYCbCr(R, G, B, Y, Cb, Cr);
            int idx = row * width + col;
            data[idx].Y = Y;
            data[idx].Cb = Cb;
            data[idx].Cr = Cr;
        }
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(fp);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        perror("Wrong arguments.");
        return 1;
    }

    FILE* fp = fopen(argv[1], "rb");
    if (!fp) {
        std::cout << "Cannot open specified file:" << argv[1];
        return 1;
    }
    FILE* fp_out = fopen(argv[2], "wb");
    if (!fp_out) {
        std::cout << "Cannot open specified file:" << argv[2];
        return 1;
    }
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, fp);
    jpeg_save_markers(&cinfo, JPEG_APP0 + 1, 0xFFFF);
    jpeg_read_header(&cinfo, TRUE);

    jvirt_barray_ptr* coef_arrays = jpeg_read_coefficients(&cinfo);

    YCbCr watermark[64][128]{};
    ReadWatermark(reinterpret_cast<YCbCr(&)[64 * 128]>(watermark));
    int offbx = (cinfo.image_width / 8) / 2 - 8;
    int offby = (cinfo.image_height / 8) / 2 - 4;
    for (int by = 0; by < 8; by++) {
        for (int bx = 0; bx < 16; bx++) {

            JBLOCKARRAY buffer_Y  = cinfo.mem->access_virt_barray(
                (j_common_ptr)&cinfo, coef_arrays[0], offby + by, 1, TRUE);
            JBLOCKARRAY buffer_Cb = cinfo.mem->access_virt_barray(
                (j_common_ptr)&cinfo, coef_arrays[1], offby + by, 1, TRUE);
            JBLOCKARRAY buffer_Cr = cinfo.mem->access_virt_barray(
                (j_common_ptr)&cinfo, coef_arrays[2], offby + by, 1, TRUE);

            JBLOCK* block_Y  = &buffer_Y[0][offbx + bx];
            JBLOCK* block_Cb = &buffer_Cb[0][offbx + bx];
            JBLOCK* block_Cr = &buffer_Cr[0][offbx + bx];

            double fY[BLOCK_SIZE][BLOCK_SIZE];
            double fCb[BLOCK_SIZE][BLOCK_SIZE];
            double fCr[BLOCK_SIZE][BLOCK_SIZE];

            IDCT_8x8(*block_Y, fY);
            IDCT_8x8(*block_Cb, fCb);
            IDCT_8x8(*block_Cr, fCr);

            YCbCr tem[BLOCK_SIZE][BLOCK_SIZE]{};
            for (int y = 0; y < BLOCK_SIZE; y++) {
                for (int x = 0; x < BLOCK_SIZE; x++) {
                    tem[y][x] = watermark[by * 8 + y][bx * 8 + x];
                }
            }
            Blend8x8(fY, fCb, fCr, tem, 255);

            DCT_8x8(fY, *block_Y);
            DCT_8x8(fCb, *block_Cb);
            DCT_8x8(fCr, *block_Cr);
        }
    }

    jpeg_compress_struct cinfo_out;
    jpeg_error_mgr jerr_out;
    cinfo_out.err = jpeg_std_error(&jerr_out);
    jpeg_create_compress(&cinfo_out);
    jpeg_stdio_dest(&cinfo_out, fp_out);
    jpeg_copy_critical_parameters(&cinfo, &cinfo_out);
    jpeg_write_coefficients(&cinfo_out, coef_arrays);
    jpeg_saved_marker_ptr marker;
    for (marker = cinfo.marker_list; marker != nullptr; marker = marker->next) {
        if (marker->marker == JPEG_APP0 + 1) {
            jpeg_write_marker(&cinfo_out, marker->marker,
                              marker->data, marker->data_length);
        }
    }
    jpeg_finish_compress(&cinfo_out);
    jpeg_destroy_compress(&cinfo_out);

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    fclose(fp);
    fclose(fp_out);
    return 0;
}

