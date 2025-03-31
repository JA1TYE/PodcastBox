#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

ifdef CONFIG_AUDIO_BOARD_CUSTOM
COMPONENT_ADD_INCLUDEDIRS += ./nau8315
COMPONENT_SRCDIRS += ./nau8315

COMPONENT_ADD_INCLUDEDIRS += ./podcastbox
COMPONENT_SRCDIRS += ./podcastbox
endif