# cmake版本需求
CMAKE_MINIMUM_REQUIRED(VERSION 2.7)
# 工程名称
PROJECT(TineNetcoWEBServe_test)


include_directories (include)
aux_source_directory (src SRC_LIST)

ADD_EXECUTABLE(main main.cc ${SRC_LIST})

