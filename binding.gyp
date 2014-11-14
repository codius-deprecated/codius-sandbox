{
  'variables': {
    'pkg-config': 'pkg-config'
  },
  'targets': [
    { 'target_name': 'codius-sandbox-rpc',
      'type': 'static_library',
      'sources': [
        'src/json.c',
        'src/codius-util.c'
      ],
      'include_dirs': [
        'include',
        'src/'
      ],
      'cflags': ['-fPIC -Wall -Werror'],
      'direct_dependent_settings': {
        'include_dirs': ['include']
      }
    },
    { 'target_name': 'syscall-tester',
      'type': 'executable',
      'sources': [
        'test/syscall-tester.c'
      ]
    },
    { 'target_name': 'node-codius-sandbox',
      'sources': [
        'src/sandbox-node-module.cpp',
      ],
      'include_dirs': [
        'include'
      ],
      'cflags': [
        '--std=c++11'
      ],
      'dependencies': [
        'codius-sandbox'
      ],
      'cflags': [
        '<!@(<(pkg-config) --cflags libseccomp) -fPIC --std=c++11 -g -Wall -Werror'
      ],
      'ldflags': [
        '<!@(<(pkg-config) --libs-only-L --libs-only-other libseccomp)'
      ],
      'libraries': [
        '<!@(<(pkg-config) --libs-only-l libseccomp) -ldl'
      ]
    },
    { 'target_name': 'codius-unittests',
      'type': 'executable',
      'sources': [
        'test/main.cpp',
        'test/syscalls.cpp',
        'test/sandbox.cpp',
        'test/ipc.cpp'
      ],
      'include_dirs': [
        'include',
      ],
      'dependencies': [
        'codius-sandbox',
        'codius-sandbox-rpc'
      ],
      'cflags': [
        '<!@(<(pkg-config) --cflags cppunit libuv libseccomp) -fPIC --std=c++11 -g -Wall -Werror -DBUILD_PATH=<(module_root_dir)'
      ],
      'cflags_cc!': [
        '-fno-rtti'
      ],
      'ldflags': [
        '<!@(<(pkg-config) --libs-only-L --libs-only-other libuv libseccomp cppunit)'
      ],
      'libraries': [
        '<!@(<(pkg-config) --libs-only-l cppunit libuv libseccomp) -ldl'
      ]
    }
  ],
  'conditions': [
    ['OS=="linux"', {
    'targets': [
      {
        'target_name': 'codius-sandbox',
        'type': 'static_library',
        'sources': [
          'src/sandbox.cpp',
          'src/sandbox-ipc.cpp',
          'src/vfs.cpp',
          'src/dirent-builder.cpp',
          'src/native-filesystem.cpp',
          'src/exec-sandbox.cpp',
          'src/thread-sandbox.cpp'
        ],
        'include_dirs': [
          'include',
          'src/'
        ],
        'dependencies': [
          'codius-sandbox-rpc'
        ],
        'cflags_cc!': [
          '-fno-rtti', '-fno-exceptions'
        ],
        'cflags': [
          '<!@(<(pkg-config) --cflags libseccomp)',
          '-fPIC',
          '--std=c++11',
          '-g',
          '-Wall',
          '-Werror',
          '-fexceptions'
        ],
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other libseccomp)'
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l libseccomp) -ldl'
        ]
      }
    ]}
  ]]
}
