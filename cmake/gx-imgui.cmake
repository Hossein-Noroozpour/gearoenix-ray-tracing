FUNCTION(GX_PREPARE_IMGUI IMGUI_PATH)
    SET(DES_FILE_PATH "${IMGUI_PATH}/imconfig.h")
    FILE(READ "${DES_FILE_PATH}" IMGUI_FILE)
    # Vulkan backend
    STRING(REPLACE "//#define IMGUI_IMPL_VULKAN_NO_PROTOTYPES" "#define IMGUI_IMPL_VULKAN_NO_PROTOTYPES" IMGUI_FILE "${IMGUI_FILE}")

    FILE(WRITE ${DES_FILE_PATH} "${IMGUI_FILE}")
ENDFUNCTION(GX_PREPARE_IMGUI)