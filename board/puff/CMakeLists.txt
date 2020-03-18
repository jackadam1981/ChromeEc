cmake_minimum_required(VERSION 3.15)
project(puff)

function(find_src_files)
    set(options )
    set(one_value_args RET)
    set(multi_value_args DIRECTORIES GLOBS)
    cmake_parse_arguments(FIND_SRC_FILES "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})
    foreach (dir ${FIND_SRC_FILES_DIRECTORIES})
        foreach (glob ${FIND_SRC_FILES_GLOBS})
            list(APPEND globs ${dir}/${glob})
        endforeach()
        file(GLOB_RECURSE src ${globs})
        list(APPEND srcs ${src})
    endforeach()
    set(${FIND_SRC_FILES_RET} ${srcs} PARENT_SCOPE)
endfunction()

function(setup_ec)
    set(options )
    set(one_value_args )
    set(multi_value_args INCLUDES RELATIVE_INCLUDES BOARDS DEFINES EXTRA_SOURCE_DIRS)
    cmake_parse_arguments(SETUP_EC "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    set(ec_root ${PROJECT_SOURCE_DIR}/../../)
    set(project_name ec)
    set(project_dir ${CMAKE_SOURCE_DIR}/../../)
    set(extra_includes ${SETUP_EC_INCLUDES})
    set(extra_defines ${SETUP_EC_DEFINES})
    set(boards ${SETUP_EC_BOARDS})
    set(ec_default_globs
            *.h
            *.cc
            *.c
            *.tasklist
            *.inc
            )
    # TODO(tomhughes): treat .tasklist and .inc as C source
    #set_source_files_properties(foo.bar PROPERTIES LANGUAGE C)
    # TODO(tomhughes): Allow choosing chip
    find_src_files(
            DIRECTORIES ${project_dir}/chip/npcx
            GLOBS ${ec_default_globs}
            RET chip_srcs
    )
    # TODO(tomhughes): Allow choosing core
    find_src_files(
            DIRECTORIES ${project_dir}/core/cortex-m
            GLOBS ${ec_default_globs}
            RET core_srcs
    )
    foreach (extra ${SETUP_EC_EXTRA_SOURCE_DIRS})
        find_src_files(
                DIRECTORIES ${project_dir}/${extra}
                GLOBS ${ec_default_globs}
                RET extra_src
        )
        list(APPEND extra_srcs ${extra_src})
    endforeach()
    foreach (board ${boards})
        set(board_dir ${project_dir}/board/${board})
        list(APPEND board_includes ${board_dir})
        find_src_files(
                DIRECTORIES ${board_dir}
                GLOBS ${ec_default_globs}
                RET board_src
        )
        list(APPEND board_srcs ${board_src})
    endforeach()
    list(APPEND common_src_dirs
            common
            builtin
            driver
            docs
            extra
            fuzz
            include
            power
            test
            third_party
            util
    )
    foreach (common_src ${common_src_dirs})
        set(common_src_dir ${project_dir}/${common_src})
        find_src_files(
                DIRECTORIES ${common_src_dir}
                GLOBS ${ec_default_globs}
                RET common_src
        )
        list(APPEND common_srcs ${common_src})
    endforeach()
    add_executable(${project_name})
    target_sources(${project_name} PRIVATE ${board_srcs})
    target_sources(${project_name} PRIVATE ${chip_srcs})
    target_sources(${project_name} PRIVATE ${common_srcs})
    target_sources(${project_name} PRIVATE ${core_srcs})
    target_sources(${project_name} PRIVATE ${extra_srcs})
    target_include_directories(${project_name} PRIVATE ${project_dir})
    target_include_directories(${project_name} PRIVATE ${board_includes})
    target_include_directories(${project_name} PRIVATE ${relative_includes})
    target_include_directories(${project_name} PRIVATE ${extra_includes})
    target_compile_definitions(${project_name} PRIVATE ${extra_defines})
    # TODO(tomhughes): configure chip based on board defines
    target_include_directories(${project_name} PRIVATE ${project_dir}/chip/npcx)
    # TODO(tomhughes): configure core based on chip
    target_include_directories(${project_name} PRIVATE ${project_dir}/core/cortex-m)
    target_include_directories(${project_name} PRIVATE ${project_dir}/core/cortex-m/include)
    target_include_directories(${project_name} PRIVATE ${project_dir}/common)
    target_include_directories(${project_name} PRIVATE ${project_dir}/core)
    target_include_directories(${project_name} PRIVATE ${project_dir}/core/host)
    target_include_directories(${project_name} PRIVATE ${project_dir}/fuzz)
    target_include_directories(${project_name} PRIVATE ${project_dir}/include)
    target_include_directories(${project_name} PRIVATE ${project_dir}/third_party/boringssl/include)
    target_include_directories(${project_name} PRIVATE ${project_dir}/../../third_party/cryptoc/include)
    ## NOTE(tomhughes): doesn't seem to work so have to hardcode defines above
    ##target_compile_options(ec PRIVATE -imacros platform/ec/board/nocturne_fp/ec.tasklist)
endfunction()

setup_ec(
    BOARDS puff
    # TODO extract config?
    # DEFINES ...
    # EXTRA_SOURCE_DIRS ...
)