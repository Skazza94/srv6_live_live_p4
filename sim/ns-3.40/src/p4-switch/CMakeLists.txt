find_external_library(DEPENDENCY_NAME BMv2
                      LIBRARY_NAME bmall)

if(${BMv2_FOUND})
    build_lib(
        LIBNAME p4-switch
        SOURCE_FILES
            helper/p4-switch-helper.cc
            model/p4-switch-channel.cc
            model/p4-switch-net-device.cc
            model/p4-pipeline.cc
            model/primitives.cc
        HEADER_FILES
            helper/p4-switch-helper.h
            model/register_access.h
            model/p4-switch-channel.h
            model/p4-switch-net-device.h
            model/p4-pipeline.h
        LIBRARIES_TO_LINK
            ${libnetwork}
            ${libcore}
            ${BMv2_LIBRARIES}
    )
endif()