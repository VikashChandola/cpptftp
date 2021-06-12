project('cpptftp', 'cpp', default_options : ['cpp_std=c++17', 'b_sanitize=address', 'b_lundef=false', 'warning_level=3'])

r = run_command('cp', '-r', join_paths(meson.source_root(), 'test/bin/'), meson.build_root())
r = run_command('chmod', '+x', join_paths(meson.build_root(), 'bin/setup_tests.sh'))
r = run_command('bash', join_paths(meson.build_root(), 'bin/setup_tests.sh'),
                meson.build_root())

cpp = meson.get_compiler('cpp')
dep_pthread = cpp.find_library('pthread', required : true)


tftp_inc_dir = include_directories('include')

#----------------------------------------------------------------------------------------------------------------------

lib_tftpframe_src = ['src/tftp_frame.cpp']
lib_tftpframe = library('tftpframe', sources :lib_tftpframe_src,
                        include_directories :tftp_inc_dir,
                        version : '0.0.1', soversion :'0')

tftpframe_test_src = ['test/tftp_frame_test.cpp']
tftpframe_test_exec = executable('tftp_frame_test', sources :tftpframe_test_src, 
                            include_directories :tftp_inc_dir,
                            link_with :lib_tftpframe)
#----------------------------------------------------------------------------------------------------------------------

lib_tftpclient_src = ['src/tftp_client.cpp']
lib_tftpclient = library('tftpclient', sources : lib_tftpclient_src,
                         include_directories :tftp_inc_dir,
                         link_with : lib_tftpframe,
                         dependencies : [dep_pthread],
                         version : '0.0.1', soversion : '0')

tftpclient_test_src = ['test/tftp_client.cpp']
tftpclient_test_exec = executable('tftp_client_test', sources :tftpclient_test_src, 
                            include_directories :tftp_inc_dir,
                            link_with :[lib_tftpclient],
                            dependencies : [dep_pthread])
#----------------------------------------------------------------------------------------------------------------------

lib_tftpserver_src = ['src/tftp_server.cpp']
lib_tftpserver = library('tftpserver', sources : lib_tftpserver_src,
                         include_directories :tftp_inc_dir,
                         link_with : lib_tftpframe,
                         dependencies : [dep_pthread],
                         version : '0.0.1', soversion : '0')
#----------------------------------------------------------------------------------------------------------------------

bin_tftpclient_src = ['src/client.cpp','src/tftp_client.cpp', 'src/tftp_frame.cpp']
bin_tftpclient = executable('tftp_client', sources : bin_tftpclient_src,
                                include_directories :tftp_inc_dir,
                                dependencies : [dep_pthread])
#----------------------------------------------------------------------------------------------------------------------

simple_client_src = ['src/simple_client.cpp','src/tftp_client.cpp', 'src/tftp_frame.cpp']
simple_client_bin = executable('simple_client', sources : simple_client_src,
                                include_directories :tftp_inc_dir,
                                link_with : [ lib_tftpclient],
                                dependencies : [dep_pthread])
#----------------------------------------------------------------------------------------------------------------------

simple_server_src = ['src/simple_server.cpp', 'src/tftp_server.cpp', 'src/tftp_frame.cpp']
simple_server_bin = executable('simple_server', sources : simple_server_src,
                                include_directories :tftp_inc_dir,
                                dependencies : [dep_pthread])
#----------------------------------------------------------------------------------------------------------------------

