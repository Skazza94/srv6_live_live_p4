build_lib_example(
  NAME p4-switch-example
  SOURCE_FILES p4-switch-example.cc
  LIBRARIES_TO_LINK
    ${libcsma}
    ${libinternet}
    ${libapplications}
    ${libp4-switch}
)

build_lib_example(
  NAME live-live-example
  SOURCE_FILES live-live-example.cc
  LIBRARIES_TO_LINK
    ${libcsma}
    ${libinternet}
    ${libapplications}
    ${libp4-switch}
)