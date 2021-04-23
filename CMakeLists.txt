cmake_minimum_required(VERSION 3.0)
project(light)

include_directories(
  include
  include/light
)

add_executable(client
  src/client.cpp
  src/tcp/tcp_connection.cpp
  src/tcp/tcp_client.cpp
)
target_link_libraries(client
  -lpthread
  glog
  -lboost_system
)

add_executable(test_publisher
  src/test_publish.cpp
  src/tcp/tcp_connection.cpp
  src/tcp/tcp_client.cpp
)
target_link_libraries(test_publisher
  -lpthread
  glog
  -lboost_system
)

add_executable(test_subscriber
  src/test_subscribe.cpp
  src/tcp/tcp_connection.cpp
  src/tcp/tcp_client.cpp
)
target_link_libraries(test_subscriber
  -lpthread
  glog
  -lboost_system
)

add_executable(test_service
  src/test_service.cpp
  src/tcp/tcp_connection.cpp
  src/tcp/tcp_client.cpp
)
target_link_libraries(test_service
  -lpthread
  glog
  -lboost_system
)

add_executable(test_call
  src/test_call.cpp
  src/tcp/tcp_connection.cpp
  src/tcp/tcp_client.cpp
)
target_link_libraries(test_call
  -lpthread
  glog
  -lboost_system
)

add_executable(pssc_core
  src/pssc/core.cpp
  src/tcp/tcp_connection.cpp
  src/tcp/tcp_service.cpp
)
target_link_libraries(pssc_core
  -lpthread
  glog
  -lboost_system
)