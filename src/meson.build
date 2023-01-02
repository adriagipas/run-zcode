ROOT_H= include_directories('.')

subdir('utils')
subdir('core')

SRC_FILES= files('main.c')
RUNZCODE= executable('run-zcode',
                     SRC_FILES,
                     dependencies : [GLIB2],
                     include_directories : [],
                     link_with : [CORE,UTILS],
                     install : true)
