# CMakeLists.txt for assembling NASM test files
# Creates flat binaries from .asm files for emulator testing

find_program(NASM_EXECUTABLE nasm)

if(NOT NASM_EXECUTABLE)
    message(WARNING "NASM not found - test binaries will not be regenerated")
    return()
endif()

# Get all .asm files in this directory
file(GLOB ASM_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.asm")

foreach(asm_file ${ASM_FILES})
    get_filename_component(test_name ${asm_file} NAME_WE)
    set(BIN_OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${test_name}.bin)

    add_custom_command(
        OUTPUT ${BIN_OUTPUT}
        COMMAND ${NASM_EXECUTABLE} -f bin -o ${BIN_OUTPUT} ${asm_file}
        DEPENDS ${asm_file}
        COMMENT "Assembling ${test_name}.asm"
    )

    list(APPEND ASM_BIN_OUTPUTS ${BIN_OUTPUT})
endforeach()

add_custom_target(assemble_tests ALL DEPENDS ${ASM_BIN_OUTPUTS})
