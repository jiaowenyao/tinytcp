

function(my_add_excutable target_name srcs depends libs)
    add_executable(${target_name} ${srcs})
    add_dependencies(${target_name} ${depends})
    target_link_libraries(${target_name} ${libs})
endfunction()


