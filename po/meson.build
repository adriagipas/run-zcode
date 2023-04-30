i18n = import('i18n')
# define GETTEXT_PACKAGE
add_project_arguments('-DGETTEXT_PACKAGE="@0@"'.format(meson.project_name()),
                      language:'c')
prefix= get_option('prefix')
localedir= join_paths(prefix, get_option('localedir'))
add_project_arguments('-DGETTEXT_LOCALEDIR="@0@"'.format(localedir),
                      language:'c')
i18n.gettext(meson.project_name())
