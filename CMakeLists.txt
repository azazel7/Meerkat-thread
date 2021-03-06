project (Meerkat)

set(PROCESSOR_COUNT 0)

set(cpuinfo_file "/proc/cpuinfo")
if(EXISTS "${cpuinfo_file}")
	file(STRINGS "${cpuinfo_file}" procs REGEX "^processor.: [0-9]+$")
	list(LENGTH procs PROCESSOR_COUNT)
endif()

message("Compile for ${PROCESSOR_COUNT} core(s)")
cmake_minimum_required(VERSION 2.8)

#Flag pour la compilation
set(CMAKE_C_FLAGS "-Wall -DNUMBER_OF_CORE=${PROCESSOR_COUNT}")

set(SRC_DIR "src")
set(TST_DIR "tst")
set(EXEMPLE_DIR "exemple")
set(EXECUTABLE_OUTPUT_PATH build)

file(GLOB_RECURSE Plankton_SOURCES "${SRC_DIR}/*.c")
file(GLOB_RECURSE Plankton_HEADERS "${SRC_DIR}/*.h")

file(GLOB_RECURSE test_source "${TST_DIR}/*.c")
file(GLOB_RECURSE exemple_source "${EXEMPLE_DIR}/*.c")

set(TST_SOURCE ${test_source} ${Plankton_SOURCES})

#Pour si on veut faire des sous-dossiers dans src
set(Plankton_INCLUDE_DIRS "")
foreach(_headerFile ${Plankton_HEADERS})
    get_filename_component(_dir ${_headerFile} PATH)
    list(APPEND Plankton_INCLUDE_DIRS ${_dir})
endforeach()
list(REMOVE_DUPLICATES Plankton_INCLUDE_DIRS)

#Ajout d'un define pour éliminer des informations de debug
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG -Wall")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wall -march=native -mtune=native -O3")

#Ajoute le repertoire qui contiendra libmeerkat.so
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/build)
#Librairie utilisé pour notre bibli
add_library(meerkat SHARED ${Plankton_SOURCES})
#Link pthread avec meerkat
target_link_libraries(meerkat pthread)

#Produit tous les exemples
foreach(exempleFile ${exemple_source})
	#Récupére le nom sans extension et sans chemin du fichier d'exemple
	get_filename_component(rd ${exempleFile} NAME_WE)
	#Ajoute le fichier d'exemple pour créer un nouvel exécutable
	add_executable (${rd} ${exempleFile})
	#Link meerkat dynamiquement au fichier d'exemple
	target_link_libraries(${rd} meerkat)
	#Ajoute le fichier d'exemple pour créer un nouvel exécutable via pthread
	add_executable (${rd}_pthread ${exempleFile})
	#Link meerkat dynamiquement au fichier d'exemple
	target_link_libraries(${rd}_pthread pthread)
	set_target_properties(${rd}_pthread PROPERTIES COMPILE_FLAGS "-DUSE_PTHREAD")
endforeach()
