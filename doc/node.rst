Node.js API
===========

The ``Sandbox`` class
+++++++++++++++++++++

.. js:class:: Sandbox()

  Construct a new sandbox


.. js:function:: Sandbox.mapFilename(filename)

  :param string filename: Filename to map

  Called when a filename used in open(), stat(), etc should be mapped from a
  real filename to one within the sandbox's environment

.. js:function:: Sandbox._init()

  Internal function. Sets up stdio IPC channels upon construction

.. js:function:: Sandbox.onData(fd, chunk)

  :param number fd: File descriptor inside the sandbox
  :param string chunk: Data read from the sandbox

  Internal function. Called when data from within the sandbox is ready for
  reading.

.. js:function:: Sandbox.spawn(arg0, [...,] [options])

  :param arg0: First argument
  :param ...: Further arguments
  :param options: A structure of options

  Spawns a binary inside the sandbox

.. js:function:: Sandbox.kill()

  Kills the child process

Attributes
----------

.. js:attribute:: Sandbox.stdout
  
  :type Readable: stdio channel that maps to stdout

.. js:attribute:: Sandbox.stderr

  :type Readable: stdio channel that maps to stdin

.. js:attribute:: Sandbox.stdio

  :type Array: stdio channels

.. js:attribute:: Sandbox.debuggerOnCrash

  :type boolean: Launch GDB when the child crashes

Events
------

.. js:function:: Sandbox.newSocket

  :param string path: Path to the unix socket

  Emitted when the sandboxed child has called bind() on a socket, which is now
  mapped to a unix domain socket.

.. js:function:: Sandbox.exit

  :param number status: Exit status

  Emitted when the sandboxed child has exited

.. js:function:: Sandbox.signal

  :param number signal: Signal received

  Emitted when the sandboxed child has received a signal
