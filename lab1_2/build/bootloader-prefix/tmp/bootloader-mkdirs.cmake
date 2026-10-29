# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/huang/esp/esp-idf/components/bootloader/subproject"
  "/home/huang/esp/lab1_2/build/bootloader"
  "/home/huang/esp/lab1_2/build/bootloader-prefix"
  "/home/huang/esp/lab1_2/build/bootloader-prefix/tmp"
  "/home/huang/esp/lab1_2/build/bootloader-prefix/src/bootloader-stamp"
  "/home/huang/esp/lab1_2/build/bootloader-prefix/src"
  "/home/huang/esp/lab1_2/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/huang/esp/lab1_2/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/huang/esp/lab1_2/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
