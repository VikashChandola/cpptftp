project('cpptftp', 'cpp',
        default_options:['cpp_std=c++17',
                         'b_sanitize=address',
                         'b_lundef=false',
                         'warning_level=3'])

conf_data = configuration_data()
if get_option('EXTENSIVE_DEBUG_LOG').enabled()
  conf_data.set('CONF_EXTENSIVE_DEBUG_LOG', true)
endif
conf_data.set('CONF_CONSTANT_DELAY_DURATION', get_option('CONSTANT_DELAY_DURATION'))
conf_data.set('CONF_NETWORK_TIMEOUT', get_option('NETWORK_TIMEOUT'))
conf_data.set('CONF_MAX_RETRY_COUNT', get_option('MAX_RETRY_COUNT'))
configure_file(input: 'project_config.hpp.in', output: 'project_config.hpp', configuration: conf_data)

run_command('cp', '-r', join_paths(meson.source_root(), 'test/bin/'), meson.build_root())
run_command('chmod', '+x', join_paths(meson.build_root(), 'bin/setup_tests.sh'))
run_command('bash', join_paths(meson.build_root(), 'bin/setup_tests.sh'), meson.build_root(), join_paths(meson.build_root(),'bin/'))

cpp = meson.get_compiler('cpp')
dep_pthread = cpp.find_library('pthread', required: true)

tftp_inc_dir = include_directories('include')

#----------------------------------------------------------------------------------------------------------------------

test_tftp_frame_src = ['test/tftp_frame_test.cpp',
                       'src/tftp_frame.cpp']
executable('test_tftp_frame', sources: test_tftp_frame_src,include_directories: tftp_inc_dir)

test_duration_generator_src = ['test/duration_generator_test.cpp']
executable('test_duration_generator', sources: test_duration_generator_src, include_directories: tftp_inc_dir)

test_file_io_src = ['test/file_io_test.cpp']
executable('test_file_io', sources: test_file_io_src, include_directories: tftp_inc_dir)

#----------------------------------------------------------------------------------------------------------------------

simple_client_src = ['src/simple_client.cpp',
                     'src/tftp_client.cpp',
                     'src/tftp_frame.cpp']
executable('simple_client', sources: simple_client_src, include_directories: tftp_inc_dir, dependencies: [dep_pthread])
#----------------------------------------------------------------------------------------------------------------------

simple_server_src =['src/simple_server.cpp',
                    'src/tftp_server.cpp',
                    'src/tftp_frame.cpp']
executable('simple_server', sources: simple_server_src, include_directories: tftp_inc_dir, dependencies: [dep_pthread])
#----------------------------------------------------------------------------------------------------------------------
