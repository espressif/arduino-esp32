function(add_arduino_libraries libs_dir)
    # Construct the absolute path for libs_dir relative to the CMake project root
    set(libs_dir_abs "${CMAKE_SOURCE_DIR}/${libs_dir}")
    
    # Verify if libs_dir_abs is a valid directory
    if(NOT IS_DIRECTORY "${libs_dir_abs}")
        message(FATAL_ERROR "The specified libs_dir (${libs_dir_abs}) is not a valid directory.")
        return()  # Stop processing if the directory is invalid
    endif()

    idf_component_get_property(arduino_esp32_lib arduino COMPONENT_LIB)
    
    # Loop through each folder in the libs_dir
    file(GLOB children RELATIVE ${libs_dir_abs} ${libs_dir_abs}/*)
    foreach(child ${children})
        if(IS_DIRECTORY "${libs_dir_abs}/${child}")
            # Check for library.properties and src directory
            if(EXISTS "${libs_dir_abs}/${child}/library.properties" AND IS_DIRECTORY "${libs_dir_abs}/${child}/src")
                message(STATUS "Processing ${child} in ${libs_dir_abs}")
                # Read the library.properties file to check for ldflags and precompiled
                file(STRINGS "${libs_dir_abs}/${child}/library.properties" properties)
                set(found_precompiled FALSE)
                foreach(property ${properties})
                    # Process LD_FLAGS
                    string(FIND "${property}" "ldflags=" ldflags_pos)
                    if(NOT ${ldflags_pos} EQUAL -1)
                        string(REPLACE "ldflags=" "" ldflags_value ${property})
                        set_property(DIRECTORY APPEND PROPERTY LINK_FLAGS "${ldflags_value}")
                        message(STATUS "Added linker flags from ${child}: ${ldflags_value}")
                    endif()
                    
                    # Process precompiled libraries
                    string(FIND "${property}" "precompiled=" precompiled_pos)
                    if(NOT ${precompiled_pos} EQUAL -1)
                        string(REPLACE "precompiled=" "" precompiled_value ${property})
                        set(found_precompiled TRUE)
                        if(precompiled_value STREQUAL "true" OR precompiled_value STREQUAL "full")
                            file(GLOB lib_files "${libs_dir_abs}/${child}/src/${IDF_TARGET}/lib*.a")
                            foreach(lib_file ${lib_files})
                                target_link_libraries(${arduino_esp32_lib} INTERFACE ${lib_file})
                                message(STATUS "Linked precompiled library for ${child}: ${lib_file}")
                            endforeach()
                        endif()
                        if(precompiled_value STREQUAL "true")
                            aux_source_directory("${libs_dir_abs}/${child}/src" src_files)
                            target_sources(${arduino_esp32_lib} INTERFACE ${src_files})
                            message(STATUS "Added source files for ${child}")
                        endif()
                    endif()
                endforeach()
                
                # If no precompiled property found, add all sources
                if(NOT found_precompiled)
                    aux_source_directory("${libs_dir_abs}/${child}/src" src_files)
                    target_sources(${arduino_esp32_lib} INTERFACE ${src_files})
                    message(STATUS "Added all source files from ${child} as no precompiled property was specified.")
                endif()

                target_include_directories(${arduino_esp32_lib} INTERFACE "${libs_dir_abs}/${child}/src")
                message(STATUS "Added include directory for ${child}")
            else()
                message(WARNING "Skipped ${child}: library.properties missing or src directory not found")
            endif()
        endif()
    endforeach()
endfunction()
