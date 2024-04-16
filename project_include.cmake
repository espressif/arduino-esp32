function(convert_arduino_libraries_to_components)
    # Base directory for libraries relative to the project root
    set(libs_dir_abs "${CMAKE_SOURCE_DIR}/components")
    
    if(NOT IS_DIRECTORY "${libs_dir_abs}")
        message(FATAL_ERROR "The components dir (${libs_dir_abs}) is not a valid directory.")
        return()
    endif()

    # Get the name of the arduino component. It is the same as the name of the current directory.
    get_filename_component(arduino_component_name ${CMAKE_CURRENT_FUNCTION_LIST_DIR} NAME)
    message(STATUS "Arduino component name: ${arduino_component_name}")

    # Try to figure out the main component name or skip it
    if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/main")
        set(main_component_name "main")
    elseif(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/src")
        set(main_component_name "src")
    else()
        set(main_component_name "")
    endif()
    message(STATUS "Main component name: ${main_component_name}")

    file(GLOB children RELATIVE ${libs_dir_abs} ${libs_dir_abs}/*)
    foreach(child ${children})
        set(child_dir "${libs_dir_abs}/${child}")
        set(properties_file "${child_dir}/library.properties")
        set(src_dir "${child_dir}/src")
        set(utility_dir "${child_dir}/utility")
        set(component_cmakelist "${child_dir}/CMakeLists.txt")

        # Skip .DS_Store, files and folders that do not already contain CMakeLists.txt
        if(NOT "${child}" STREQUAL ".DS_Store" AND IS_DIRECTORY "${child_dir}" AND NOT EXISTS "${component_cmakelist}")
            # Require library.properties
            if(EXISTS "${properties_file}")
                # If we have src folder, then handle the new library structure
                if(IS_DIRECTORY "${src_dir}")
                    message(STATUS "Processing 1.5+ compatible library '${child}' in '${libs_dir_abs}'")
                    # Read properties
                    set(ldflags "")
                    set(precompiled "")
                    file(STRINGS "${properties_file}" properties)
                    foreach(property ${properties})
                        string(REGEX MATCH "ldflags=(.*)" _ ${property})
                        if(CMAKE_MATCH_COUNT GREATER 0)
                            set(ldflags "${CMAKE_MATCH_1}")
                        endif()
                        string(REGEX MATCH "precompiled=(.*)" _ ${property})
                        if(CMAKE_MATCH_COUNT GREATER 0)
                            set(precompiled "${CMAKE_MATCH_1}")
                        endif()
                    endforeach()

                    # Sources are not compiled only if precompiled=full and precompiled library for our target exists
                    file(WRITE ${component_cmakelist} "idf_component_register(SRCS ")
                    if(NOT precompiled STREQUAL "full" OR NOT EXISTS "${src_dir}/${IDF_TARGET}/lib${child}.a")
                        file(GLOB_RECURSE src_files "${src_dir}/*.c" "${src_dir}/*.cpp")
                        foreach(src_file ${src_files})
                            string(REPLACE "${child_dir}/" "" src_file "${src_file}")
                            file(APPEND ${component_cmakelist} "\"${src_file}\" ")
                        endforeach()
                    else()
                        file(APPEND ${component_cmakelist} "\"\" ")
                    endif()
                    file(APPEND ${component_cmakelist} "PRIV_REQUIRES ${arduino_component_name} ${main_component_name})\n")

                    # If this lib has precompiled libs and they match our target, get them
                    if((precompiled STREQUAL "true" OR precompiled STREQUAL "full") AND IS_DIRECTORY "${src_dir}/${IDF_TARGET}")
                        file(GLOB_RECURSE lib_files "${src_dir}/${IDF_TARGET}/lib*.a")
                        foreach(lib_file ${lib_files})
                            string(REPLACE "${child_dir}/" "" lib_file "${lib_file}")
                            file(APPEND ${component_cmakelist} "set_property(TARGET \${COMPONENT_TARGET} APPEND_STRING PROPERTY INTERFACE_LINK_LIBRARIES \"${lib_file}\")\n")
                        endforeach()
                    endif()

                    # Add src folder to includes
                    file(APPEND ${component_cmakelist} "target_include_directories(\${COMPONENT_TARGET} PUBLIC \"src\")\n")

                    # Add any custom LD_FLAGS, if defined
                    if(NOT "${ldflags}" STREQUAL "")
                        file(APPEND ${component_cmakelist} "target_link_libraries(\${COMPONENT_TARGET} INTERFACE \"${ldflags}\")\n")
                    endif()

                    message(STATUS "Created IDF component for ${child}")
                else()
                    message(STATUS "Processing 1.0 compatible library '${child}' in '${libs_dir_abs}'")
                    file(WRITE ${component_cmakelist} "idf_component_register(SRCS ")

                    # Add sources from the root folder
                    file(GLOB src_files "${child_dir}/*.c" "${child_dir}/*.cpp")
                    foreach(src_file ${src_files})
                        string(REPLACE "${child_dir}/" "" src_file "${src_file}")
                        file(APPEND ${component_cmakelist} "\"${src_file}\" ")
                    endforeach()

                    # Add sources from the utility folder if it exists
                    if(IS_DIRECTORY "${utility_dir}")
                        file(GLOB utility_files "${utility_dir}/*.c" "${utility_dir}/*.cpp")
                        foreach(utility_file ${utility_files})
                            string(REPLACE "${child_dir}/" "" utility_file "${utility_file}")
                            file(APPEND ${component_cmakelist} "\"${utility_file}\" ")
                        endforeach()
                    endif()

                    # Require arduino and main components to suceed in compilation
                    file(APPEND ${component_cmakelist} "PRIV_REQUIRES ${arduino_component_name} ${main_component_name})\n")

                    # Add root folder to includes
                    file(APPEND ${component_cmakelist} "target_include_directories(\${COMPONENT_TARGET} PUBLIC \".\"")

                    # Add the utility folder to includes if it exists
                    if(IS_DIRECTORY "${utility_dir}")
                        file(APPEND ${component_cmakelist} " \"utility\"")
                    endif()

                    file(APPEND ${component_cmakelist} ")\n")
                    message(STATUS "Created IDF component for ${child}")
                endif()
            else()
                message(STATUS "Skipped ${child}: Required 'library.properties' not found")
            endif()
        endif()
    endforeach()
endfunction()
