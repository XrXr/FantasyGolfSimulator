cmake_minimum_required (VERSION 3.5.2)

execute_process(
    COMMAND ${EXE_COMMAND} ${ARG_ONE} ${ARG_TWO}
    OUTPUT_FILE ${OUTPUT_PATH}
)