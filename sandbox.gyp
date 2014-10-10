{
  'variables': {
    'pkg-config': 'pkg-config'
  },
  'targets': [
    { 'target_name': 'codius-sandbox-host',
      'type': 'executable',
      'dependencies': [
        'codius-sandbox-rpc'
      ],
      'sources': [
        'src/host.cpp',
        'src/sandbox.cpp'
      ],
      'actions': [
        { 'action_name': 'produce_syscall_names',
          'inputs': ['/usr/include/asm/unistd_64.h'],
          'outputs': ['src/names.cpp'],
          'action': ['python', 'build_names.py', '<@(_inputs)', '<@(_outputs)']
        }
      ],
      'include_dirs': [
        'include',
        'src/'
      ],
      'cflags': [
        '<!@(<(pkg-config) --cflags libseccomp) -fPIC --std=c++11 -g'
      ],
      'ldflags': [
        '<!@(<(pkg-config) --libs-only-L --libs-only-other libseccomp)'
      ],
      'libraries': [
        '<!@(<(pkg-config) --libs-only-l libseccomp) -ldl'
      ]
    },
    { 'target_name': 'codius-sandbox-rpc',
      'type': '<(library)',
      'sources': [
        'src/json.c',
        'src/jsmn.c',
        'src/codius-util.c'
      ],
      'include_dirs': [
        'include',
        'src/'
      ],
      'cflags': ['-fPIC -g'],
      'direct_dependent_settings': {
        'include_dirs': ['include']
      }
    }
  ],
  'conditions': [
    ['OS=="linux"', {
    'targets': [
      {
        'target_name': 'codius-sandbox',
        'type': '<(library)',
        'sources': [
          'src/inject.c',
        ],
        'include_dirs': [
          'include',
          'src/'
        ],
        'cflags': [
          '<!@(<(pkg-config) --cflags libseccomp) -fPIC'
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
