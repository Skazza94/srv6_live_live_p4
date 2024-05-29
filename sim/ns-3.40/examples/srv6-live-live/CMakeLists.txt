build_example(
  NAME live-live-experiment
  SOURCE_FILES live-live-experiment.cc
  LIBRARIES_TO_LINK
  ${libcsma}
  ${libinternet}
  ${libapplications}
  ${libp4-switch}
  ${libflow-monitor}
)

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

build_example(
  NAME live-live-seqn
  SOURCE_FILES live-live-seqn-experiment.cc
  LIBRARIES_TO_LINK
  ${libcsma}
  ${libinternet}
  ${libapplications}
  ${libp4-switch}
  ${libflow-monitor}
)
