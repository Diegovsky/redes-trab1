project('redes-1', 'c',
  version : '0.1',
  default_options : ['warning_level=2', 'c_std=gnu11'])

subdir('libs')
subdir('src')

executable('server', server, link_with: [md5_dep])
executable('client', client, c_args: ['-DCLIENT'], link_with: [md5_dep])

