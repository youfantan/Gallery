# Image Encode Unit | 图像编码单元
这个子模块用来对图片无损添加水印。
它的原理是使用libjpeg读取指定位置的数个DCT块，通过逆DCT变换把该区域的DCT块转换成原YCbCr数据，然后把水印以指定alpha叠加到原YCbCr数据，重新进行DCT变换后写回。  
在这种处理方式下，除水印区域以外的所有DCT块不受影响；传统的水印添加方法是对整个图像进行解码后叠加水印重新编码，这个过程会对其余DCT块进行二次DCT变换，对图像质量有一定影响。  
使用前需要生成一张128x64像素的白底黑字图片，黑色部分将被映射到水印区域。水印区域位于图像的正中央（由于DCT填充，可能会有些许偏移）。  
同时，子模块还会把原图像的EXIF信息复制到目标EXIF中。  
用法:`image_encode_unit [输入JPEG] [输出JPEG]`
构建:  
确保正确安装了CMake，Windows需要安装Vcpkg并配置环境变量VCPKG_ROOT，Linux需要执行`apt-get install libjpeg-turbo`以安装依赖包。
```
mkdir build
cd build
cmake ..
make -j4
```
即可。