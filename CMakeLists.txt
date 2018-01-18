cmake_minimum_required(VERSION 2.8)

project(clark)

#file(GLOB SRC_ALL src/*.cc) # compile all files with *.cc suffix
#file(GLOB HDR *.hh) #add headers
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")

include_directories(${CMAKE_SOURCE_DIR}/src)

add_executable(getTargetsDef src/getTargetsDef.cc src/file.cc)
add_executable(getAccssnTaxID src/getAccssnTaxID.cc src/file.cc)
add_executable(getfilesToTaxNodes src/getfilesToTaxNodes.cc src/file.cc)
add_executable(getAbundance src/getAbundance.cc src/file.cc)
add_executable(getConfidenceDensity src/getConfidencedensity.cc src/file.cc)
add_executable(getGammaDensity src/getGammadensity.cc src/file.cc)
add_executable(makeSummaryTables src/file.cc src/makeSamplesSummaryTables.cc)
add_executable(converter src/main_spaced.cc src/kmersConversion.cc)
add_executable(exeSeq src/getSeqVirFiles.cc src/file.cc)

set(CLARK_SRC src/main.cc src/analyser.cc  src/file.cc  src/kmersConversion.cc src/FileHandler.cc src/FileHandlerA.cc src/FileHandlerQ.cc)
add_executable(CLARK ${CLARK_SRC})

add_executable(CLARK-l ${CLARK_SRC})
SET_TARGET_PROPERTIES(CLARK-l PROPERTIES COMPILE_FLAGS "-DCLARK_VERSION_TYPE=1")

add_executable(CLARK-S ${CLARK_SRC})
SET_TARGET_PROPERTIES(CLARK-S PROPERTIES COMPILE_FLAGS "-DCLARK_VERSION_TYPE=2")

#install(TARGETS CLARK CLARK-l CLARK-S DESTINATION exe)
