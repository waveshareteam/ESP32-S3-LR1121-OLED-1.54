file(GLOB_RECURSE SRCS  src/*.c)

set(INCLUDE_DIRS  include )

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS ${INCLUDE_DIRS}
)
