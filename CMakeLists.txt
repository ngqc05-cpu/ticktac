# Phiên bản CMake tối thiểu (ESP-IDF v5.x thường yêu cầu 3.16 trở lên)
cmake_minimum_required(VERSION 3.16)

# Nạp các cấu hình mặc định của ESP-IDF
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Đặt tên cho dự án của bạn (bạn có thể đổi tên tùy ý, viết liền không dấu)
project(ticktac)