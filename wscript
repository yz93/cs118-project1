# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

from waflib import Logs, Utils, Context
import os

VERSION = '0.1'
APPNAME = 'SimpleBT'

def options(opt):
    opt.load(['compiler_c', 'compiler_cxx', 'gnu_dirs'])
    opt.load(['boost', 'default-compiler-flags'], tooldir=['.waf-tools'])

    syncopt = opt.add_option_group ("SimpleBT Options")

    syncopt.add_option('--debug', action='store_true', default=False, dest='debug',
                       help='''debugging mode''')
    syncopt.add_option('--with-tests', action='store_true', default=False, dest='_tests',
                       help='''build unit tests''')

def configure(conf):
    conf.load(['compiler_c', 'compiler_cxx', 'gnu_dirs', 'boost', 'default-compiler-flags'])

    boost_libs = 'system iostreams'
    if conf.options._tests:
        conf.env['_TESTS'] = 1
        conf.define('_TESTS', 1);
        boost_libs += ' unit_test_framework'

    conf.check_boost(lib=boost_libs)

    conf.write_config_header('config.hpp')

def build(bld):
    libsbt = bld(
        target="SimpleBT",
        features=['cxx', 'cxxshlib'],
        source =  bld.path.ant_glob(['src/**/*.cpp']),
        use = 'BOOST',
        includes = ['src', '.'],
        export_includes=['src', '.'],
        )

    # Unit tests
    if bld.env["_TESTS"]:
        bld.recurse('tests')

    bld(
        target='simple-bt',
        features='cxx cxxprogram',
        source='src/main.cpp',
        use='SimpleBT',
        )
