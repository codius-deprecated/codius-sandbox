{
  'variables': {
    'pkg-config': 'pkg-config',
    'coverage_cflags': '',
    'coverage_ldflags': '',
    'conditions': [
      ['"<!(echo $USE_GCOV)"', {
        'coverage_cflags': '-fprofile-arcs -ftest-coverage --coverage',
        'coverage_ldflags': '-fprofile-arcs -ftest-coverage'
      }]
    ]
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
      'cflags': ['-fPIC -Wall -Werror <(coverage_cflags)'],
      'ldflags': ['<(coverage_ldflags)'],
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
        '<!@(<(pkg-config) --cflags libseccomp) <(coverage_cflags) -fPIC --std=c++11 -g -Wall -Werror'
      ],
      'ldflags': [
        '<!@(<(pkg-config) --libs-only-L --libs-only-other libseccomp) <(coverage_ldflags)'
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
        'test/ipc.cpp',
        'test/vfs.cpp'
      ],
      'include_dirs': [
        'include',
      ],
      'dependencies': [
        'codius-sandbox',
        'codius-sandbox-rpc'
      ],
      'cflags': [
        '<!@(<(pkg-config) --cflags cppunit libuv libseccomp) <(coverage_cflags) -fPIC --std=c++11 -g -Wall -Werror -DBUILD_PATH=<(module_root_dir)'
      ],
      'cflags_cc!': [
        '-fno-rtti', 
        '-fno-exceptions'
      ],
      'ldflags': [
        '<!@(<(pkg-config) --libs-only-L --libs-only-other libuv libseccomp cppunit) <(coverage_ldflags)'
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
          '-fno-rtti'
        ],
        'cflags': [
          '<!@(<(pkg-config) --cflags libseccomp) -fPIC --std=c++11 -g -Wall -Werror <(coverage_cflags)'
        ],
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other libseccomp) <(coverage_ldflags)'
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l libseccomp) -ldl'
        ]
      }
    ]}
  ]]
}
