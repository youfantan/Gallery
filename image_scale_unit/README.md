# Image Scale Unit | 图像缩放单元
这个子模块用来对图片进行缩放
通过给出目标图像大小，该模块将会首先计算达到给定宽度的缩放倍数，然后进行十字采样，结果映射到一个比例不变的缩放图像中；
紧接着，它将会选取图像中央的部分，使这部分同时满足给定宽度和高度，进行裁剪后输出缩放图像，此时图像的大小严格对应输入尺寸。
用法:`image_scale_unit [输入JPEG] [输出JPEG] [目标宽度] [目标高度]`
构建:  
确保正确安装了CMake，Windows需要安装Vcpkg并配置环境变量VCPKG_ROOT，Linux需要执行`apt-get install libjpeg-turbo`以安装依赖包。
```
mkdir build
cd build
cmake ..
make -j4
```
即可。