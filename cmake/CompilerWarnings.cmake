# Enables compiler warnings on one of this project's targets. They're PRIVATE, so neither consumers
# nor third-party code (quantcup, Conan packages) inherit them. Warnings are errors only when this
# is the top-level project; opt out with `cmake --compile-no-warning-as-error`.
function(ts_enable_warnings target)
  target_compile_options(
    ${target}
    PRIVATE "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/W4;/permissive->"
            "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>")
  set_target_properties(${target} PROPERTIES COMPILE_WARNING_AS_ERROR ${PROJECT_IS_TOP_LEVEL})
endfunction()
