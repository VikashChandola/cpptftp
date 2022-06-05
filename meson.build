project('cpptftp', 'cpp',
        default_options:['cpp_std=c++20',
                         'b_lundef=false',
                         'warning_level=3'])
cpp = meson.get_compiler('cpp')
dep_pthread = cpp.find_library('pthread', required: true)
dep_boost_coroutine = cpp.find_library('boost_coroutine', required: true)

inc_dir = include_directories('include')

#------------------------------------------------------------------------------
test_packet_src = ['test/packet.cpp']
test_packet = executable('test_frame', sources: test_packet_src,
                         include_directories: inc_dir)
test('tftp packet', test_packet)
#------------------------------------------------------------------------------
tftp_src = ['src/tftp.cpp']
tftpt = executable('cpptftp', sources: tftp_src,
                   include_directories: inc_dir,
                   dependencies: [dep_pthread, dep_boost_coroutine])
#------------------------------------------------------------------------------
