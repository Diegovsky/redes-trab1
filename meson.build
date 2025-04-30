project('redes-1', 'c',
  version : '0.1',
  default_options : ['warning_level=3', 'c_std=c23', 'warning_level=3'])

subdir('libs')
subdir('src')

exe1 = executable('server', server, link_with: [md5_dep])
exe2 = executable('client', client, link_with: [md5_dep])

