{
  'variables': {
    'pkg-config': 'pkg-config'
  },
  'targets': [
    { 'target_name': 'codius-sandbox-rpc',
      'type': 'shared_library',
      'sources': [
        'src/json.c',
        'src/codius-util.c'
      ],
      'include_dirs': [
        'include',
        'src/'
      ],
      'cflags': ['-fPIC'],
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
        'sources': [
          'src/sandbox.cpp',
          'src/sandbox-node-module.cpp'
        ],
        'include_dirs': [
          'include',
          'src/'
        ],
        'dependencies': [
          'codius-sandbox-rpc'
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
      }
    ]}
  ]]
}
