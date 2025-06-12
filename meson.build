project('redes-1', 'c',
  version : '0.1',
  default_options : ['warning_level=2', 'c_std=gnu11'])

sds = subproject('sds')
sds_dep = sds.get_variable('sds_dep')
subdir('src')

shared_lib = static_library('shared', shared, dependencies: [sds_dep])
shared_dep = declare_dependency(link_with: shared_lib)
threads_dep = dependency('threads')


executable('simple', simple, dependencies: shared_dep)
executable('thread', thread, dependencies: [shared_dep, threads_dep])

