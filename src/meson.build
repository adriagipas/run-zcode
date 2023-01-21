ROOT_H= include_directories('.')

subdir('utils')
subdir('core')
subdir('debug')
subdir('frontend')

SRC_FILES= files('main.c')
RUNZCODE= executable('run-zcode',
                     SRC_FILES,
                     dependencies : [GLIB2,FONTCONFIG,SDL2TTF],
                     include_directories : [],
                     link_with : [CORE,UTILS,DEBUG,FRONTEND],
                     install : true)
