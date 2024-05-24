build_example(
  NAME live-live-sdwan
  SOURCE_FILES live-live-sdwan.cc
  LIBRARIES_TO_LINK
  ${libcsma}
  ${libinternet}
  ${libapplications}
  ${libp4-switch}
  ${libflow-monitor}
)