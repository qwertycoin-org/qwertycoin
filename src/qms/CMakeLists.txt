set(qms_sources
  protocol.cpp)

monero_find_all_headers(qms_private_headers "${CMAKE_CURRENT_SOURCE_DIR}")
monero_private_headers(qms ${qms_private_headers})
monero_add_library(qms ${qms_sources} ${qms_private_headers})
target_link_libraries(qms
  PUBLIC cryptonote_basic common
  PRIVATE ${SODIUM_LIBRARY} ${EXTRA_LIBRARIES})

