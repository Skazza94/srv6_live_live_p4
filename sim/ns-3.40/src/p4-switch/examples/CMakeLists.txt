build_lib_example(
  NAME p4-switch-example
  SOURCE_FILES p4-switch-example.cc
  LIBRARIES_TO_LINK
    ${libcsma}
    ${libinternet}
    ${libapplications}
    ${libp4-switch}
)