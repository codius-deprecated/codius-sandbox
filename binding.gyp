{
  'variables': {
    'pkg-config': 'pkg-config'
  },
  'conditions': [
    ['OS=="linux"', {
    'targets': [
      {
        'target_name': 'codius-sandbox',
        'type': 'static_library',
        'sources': [
          'src/node-module.cpp',
        ],
        'include_dirs': [
          'include/',
          'src/'
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
      }
    ]}
  ]]
}
