./get_reg base_bin diff_bin_path
example:
./get_reg reg_info_2333.bin ./
以reg_info_2333.bin为默认配置将路径下./所有的bin文件的差异信息补充到默认配置后输出reg_info.bin
./下的bin文件命名要求需要在文件名前有对应GPIO的状态数值（GPIO6 GPIO5 GPIO4对应数值范围0-7，GPIO4对应低位，默认配置bin文件可不按此命名要求）
example:
7reg_info_2333.bin -> GPIO6 GPIO5 GPIO4 为 111（7）时使用此配置文件

