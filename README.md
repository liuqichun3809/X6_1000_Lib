# X6_1000_Lib
generate python api form C++ driver

1. 环境需求：
  1）Innovative Integration的Malibu库；
  2）Visual Studio (2010以上)；
  3）python；
  4）swig；
2. 命令窗口运行：$swig -threads -c++ -python x6api.i，该命令将生成x6api_wrap.cxx和x6api.py文件；
3. VS打开x6api.sln工程，添加需求的Malibu、VS、pyhton库，根据需求设置版本，运行将生成_x6api.dll和_x6api.lib文件；
4. 将_x6api.dll重命名为_x6api.pyd；
5. 将_x6api.pyd、_x6api.lib和x6api.py放到同一文件目录下，即可通过python调用x6api.py里的class和函数。
