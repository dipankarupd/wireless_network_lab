# set(COMPONENT_SRCS "global_variables.c")

# set(COMPONENT_ADD_INCLUDEDIRS ". include")

# register_component()
idf_component_register(
    SRCS "ttm4138_ecc.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES ecc
)
