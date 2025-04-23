find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
include(AddLLVM)

llvm_map_components_to_libnames(llvm_libs
  Core
  IRReader
  Support
  AsmPrinter
)

target_link_libraries(compiler PRIVATE ${llvm_libs})
