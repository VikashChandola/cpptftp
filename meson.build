project('cpptftp', 'cpp', default_options : ['cpp_std=c++17'])

r = run_command('cp', '-r', join_paths(meson.source_root(), 'test/bin/'), meson.build_root())

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
                         link_with : lib_tftpframe, dependencies : [dep_pthread],
                         version : '0.0.1', soversion : '0')

tftpclient_test_src = ['test/tftp_client.cpp']
tftpclient_test_exec = executable('tftp_client_test', sources :tftpclient_test_src, 
                            include_directories :tftp_inc_dir,
                            link_with :[lib_tftpclient, lib_tftpframe],
                            dependencies : [dep_pthread])
#----------------------------------------------------------------------------------------------------------------------

bin_tftpclient_inc_dir = include_directories('include')
bin_tftpclient_src = ['src/client.cpp']
bin_tftpclient = executable('client', sources : bin_tftpclient_src,
                                include_directories :bin_tftpclient_inc_dir,
                                link_with : [ lib_tftpclient],
                                dependencies : [dep_pthread])
#----------------------------------------------------------------------------------------------------------------------

simple_client_src = ['src/simple_client.cpp']
simple_client_bin = executable('simple_client', sources : simple_client_src,
                                include_directories :bin_tftpclient_inc_dir,
                                link_with : [ lib_tftpclient],
                                dependencies : [dep_pthread])
#----------------------------------------------------------------------------------------------------------------------

test('tftp frame library test ', tftpframe_test_exec)


