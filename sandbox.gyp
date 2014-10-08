{
  'variables': {
    'pkg-config': 'pkg-config'
  },
  'conditions': [
    ['OS=="linux"', {
    'targets': [
      {
        'target_name': 'codius-sandbox',
        'type': '<(library)',
        'sources': [
          'src/inject.c'
        ],
        'cflags': [
          '<!@(<(pkg-config) --cflags libseccomp)'
        ],
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other libseccomp)'
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l libseccomp)'
        ]
      }
    ]}
  ]]
}
