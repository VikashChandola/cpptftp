project('cpptftp', 'cpp',
        default_options:['cpp_std=c++17',
                         'b_lundef=false',
                         'warning_level=3'])
cpp = meson.get_compiler('cpp')
dep_pthread = cpp.find_library('pthread', required: true)

inc_dir = include_directories('include')

#------------------------------------------------------------------------------
test_packet_src = ['test/test_packet.cpp']
test_packet = executable('test_frame', sources: test_packet_src,
                         include_directories: inc_dir)
test('tftp packet', test_packet)
#------------------------------------------------------------------------------
