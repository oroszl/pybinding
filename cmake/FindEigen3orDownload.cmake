# - Try to find Eigen3 or download it
# An exact version must be specified.
# The output variables are:
#
#  EIGEN3_INCLUDE_DIR - the eigen include directory
#  EIGEN3_VERSION - eigen version
#

if(Eigen3orDownload_FIND_VERSION)
    # try to find local copy first
    find_package(Eigen3 ${Eigen3orDownload_FIND_VERSION})
else()
    message(FATAL_ERROR "A version must be specified")
endif()

if (NOT EIGEN3_FOUND)
    # try to download it, if it isn't already
    set(dependencies_dir "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")
    set(EIGEN3_INCLUDE_DIR ${dependencies_dir}/eigen3)

    if(NOT EXISTS ${EIGEN3_INCLUDE_DIR})
        message(STATUS "Downloading Eigen3...")
        set(tmp_dir "${dependencies_dir}/tmp")
        set(tar_file "${tmp_dir}/eigen.tar.gz")
        file(DOWNLOAD "https://bitbucket.org/eigen/eigen/get/${Eigen3orDownload_FIND_VERSION}.tar.gz"
             ${tar_file})

        # extract downloaded tar file
        execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf ${tar_file}
                        WORKING_DIRECTORY ${tmp_dir})
        file(REMOVE ${tar_file})
        file(GLOB ex_dir ${tmp_dir}/eigen-*)

        if(NOT EXISTS ${ex_dir})
            set(EIGEN3_INCLUDE_DIR "EIGEN3_INCLUDE_DIR-NOTFOUND")
            if(Eigen3orDownload_FIND_REQUIRED)
                message(FATAL_ERROR "Could not find or download Eigen3 ${Eigen3orDownload_FIND_VERSION}")
            endif()
        else()
            set(EIGEN3_VERSION ${Eigen3orDownload_FIND_VERSION})

            # rename to a nicer dir
            file(RENAME ${ex_dir} ${EIGEN3_INCLUDE_DIR})
            # delete everything except the headers and signature file
            file(GLOB to_delete ${EIGEN3_INCLUDE_DIR}/*)
            list(REMOVE_ITEM to_delete ${EIGEN3_INCLUDE_DIR}/Eigen)
            list(REMOVE_ITEM to_delete ${EIGEN3_INCLUDE_DIR}/unsupported)
            list(REMOVE_ITEM to_delete ${EIGEN3_INCLUDE_DIR}/signature_of_eigen3_matrix_library)
            file(REMOVE_RECURSE ${to_delete})
            file(REMOVE_RECURSE ${tmp_dir})

            message(STATUS "Downloaded Eigen3: ${EIGEN3_VERSION} ${EIGEN3_INCLUDE_DIR}")
            mark_as_advanced(EIGEN3_INCLUDE_DIR)
        endif()
    endif()
endif()