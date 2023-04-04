cmake_minimum_required(VERSION 3.8)
project(rosfs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FILE_OFFSET_BITS=64")
add_compile_definitions(_FILE_OFFSET_BITS=64)

set(rosfs_MAJOR_VERSION 0)
set(rosfs_MINOR_VERSION 0)
set(rosfs_PATCH_VERSION 0)
set(rosfs_VERSION
  ${rosfs_MAJOR_VERSION}.${rosfs_MINOR_VERSION}.${rosfs_PATCH_VERSION})

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# uncomment the following section in order to fill in
# further dependencies manually.
find_package(FUSE REQUIRED)
find_package(ament_cmake_ros REQUIRED)
find_package(rcl REQUIRED)
find_package(rclc REQUIRED)
find_package(example_interfaces REQUIRED)
find_package(rclc_parameter REQUIRED)

find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)

include_directories(include)
include_directories(${FUSE_INCLUDE_DIR})
link_libraries(pubsubfs ${FUSE_LIBRARIES})

add_executable(main src/main.c src/circ_buff.c src/type.c)
ament_target_dependencies(main rcl rclc std_msgs geometry_msgs)

install(TARGETS
  main
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()

# colcon build --cmake-args '-DCMAKE_BUILD_TYPE=Debug' --cmake-clean-first