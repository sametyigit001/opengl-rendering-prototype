foreach(runtime_dll IN LISTS RUNTIME_DLLS)
    get_filename_component(name "${runtime_dll}" NAME)
    file(COPY_FILE "${runtime_dll}" "${DESTINATION}/${name}" ONLY_IF_DIFFERENT)
endforeach()
