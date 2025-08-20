#include <cstdio>
#include <iostream>
#include <jpeglib.h>
#include <vector>


struct pt {
    uint32_t x;
    uint32_t y;
};

struct RGB {
    uint8_t R;
    uint8_t G;
    uint8_t B;
};

uint8_t CrossSample(uint8_t (&sample)[4]) {
    return (sample[0] + sample[1] + sample[2] + sample[3]) / 4;
}

uint32_t MatrixAt(uint32_t x, uint32_t y, uint32_t w) {
    return y * w + x;
}
uint32_t MatrixAt(pt p, uint32_t w) {
    return p.y * w + p.x;
}

void ScaleDown(std::vector<RGB>& origin, std::vector<RGB>& output, float zoom_factor, uint32_t width, uint32_t height) {
    uint32_t dwidth = static_cast<uint32_t>(static_cast<float>(width) / zoom_factor);
    uint32_t dheight = static_cast<uint32_t>(static_cast<float>(height) / zoom_factor);
    for (int y = 0; y < dheight; ++y) {
        for (int x = 0; x < dwidth; ++x) {
            uint32_t ox = x * zoom_factor;
            uint32_t oy = y * zoom_factor;
            pt s0, s1, s2, s3;
            s0 = { ox, oy - 1 };
            s1 = { ox, oy + 1 };
            s2 = { ox - 1, oy };
            s3 = { ox + 1, oy };
            if (ox == 0) {
                s2 = { ox,  oy};
            } else if (ox == width - 1) {
                s3 = {ox, oy};
            }
            if (oy == 0) {
                s0 = { ox, oy };
            } else if (oy == width - 1) {
                s1 = {ox, oy };
            }
            uint8_t Rs[4] = {
                origin[MatrixAt(s0, width)].R,
                origin[MatrixAt(s1, width)].R,
                origin[MatrixAt(s2, width)].R,
                origin[MatrixAt(s3, width)].R
            };
            uint8_t Gs[4] = {
                origin[MatrixAt(s0, width)].G,
                origin[MatrixAt(s1, width)].G,
                origin[MatrixAt(s2, width)].G,
                origin[MatrixAt(s3, width)].G
            };
            uint8_t Bs[4] = {
                origin[MatrixAt(s0, width)].B,
                origin[MatrixAt(s1, width)].B,
                origin[MatrixAt(s2, width)].B,
                origin[MatrixAt(s3, width)].B
            };
            output[MatrixAt(x, y, dwidth)].R = CrossSample(Rs);
            output[MatrixAt(x, y, dwidth)].G = CrossSample(Gs);
            output[MatrixAt(x, y, dwidth)].B = CrossSample(Bs);
        }
    }
}

RGB* Cutting(std::vector<RGB>& origin, std::vector<RGB>& output, uint32_t dheight, uint32_t TARGET_WIDTH, uint32_t TARGET_HEIGHT) {
    uint32_t beginY = dheight - TARGET_HEIGHT;
    if (!beginY) return output.data();
    beginY /= 2;
    return output.data() + (beginY * static_cast<uint32_t>(TARGET_WIDTH));
}

int main(int argc, char** argv) {
    if (argc < 5) {
        std::cout << "Wrong arguments" << std::endl;
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
    float TARGET_WIDTH = std::atof(argv[3]);
    float TARGET_HEIGHT = std::atof(argv[4]);
    if (TARGET_WIDTH == 0.0f) {
        std::cout << "Taret width is not a float" << std::endl;
        return 1;
    }
    if (TARGET_HEIGHT == 0.0f) {
        std::cout << "Target height is not a float" << std::endl;
        return 1;
    }
    jpeg_decompress_struct cinfo{};
    jpeg_error_mgr jerr{};
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, fp);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);
    uint32_t width = cinfo.image_width;
    uint32_t height = cinfo.image_height;
    if ((cinfo.num_components != 3) || (cinfo.out_color_space != JCS_RGB)) {
        std::cout << "Input image color scheme is not RGB888" << std::endl;
        return 1;
    }
    if (width < TARGET_WIDTH) {
        std::cout << "Input image width less than " << TARGET_WIDTH << " px" << std::endl;
        return 1;
    }
    if (height < TARGET_HEIGHT) {
        std::cout << "Input image height less than " << TARGET_HEIGHT << " px" << std::endl;
        return 1;
    }
    float zoom_factor = static_cast<float>(width) / TARGET_WIDTH;
    uint32_t twidth = static_cast<uint32_t>(static_cast<float>(width) / zoom_factor);
    uint32_t theight = static_cast<uint32_t>(static_cast<float>(height) / zoom_factor);
    std::vector<RGB> pixels(width  * height);
    std::vector<RGB> outPixels(twidth  * theight);
    auto dataPtr = pixels.data();
    while (cinfo.output_scanline < height) {
        jpeg_read_scanlines(&cinfo, reinterpret_cast<uint8_t**>(&dataPtr), 1);
        dataPtr += width;
    }
    ScaleDown(pixels, outPixels, zoom_factor, width, height);
    auto outDataPtr = Cutting(pixels, outPixels, theight, TARGET_WIDTH, TARGET_HEIGHT);
    jpeg_compress_struct cinfo_out;
    jpeg_error_mgr jerr_out;
    cinfo_out.err = jpeg_std_error(&jerr_out);
    jpeg_create_compress(&cinfo_out);
    jpeg_stdio_dest(&cinfo_out, fp_out);

    cinfo_out.image_width = TARGET_WIDTH;
    cinfo_out.image_height = TARGET_HEIGHT;
    cinfo_out.input_components = 3;
    cinfo_out.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo_out);
    jpeg_set_quality(&cinfo_out, 50, TRUE);

    jpeg_start_compress(&cinfo_out, TRUE);

    while (cinfo_out.next_scanline < cinfo_out.image_height) {
        jpeg_write_scanlines(&cinfo_out, reinterpret_cast<uint8_t**>(&outDataPtr), 1);
        outDataPtr += twidth;
    }

    jpeg_finish_compress(&cinfo_out);
    jpeg_destroy_compress(&cinfo_out);

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(fp_out);

}
