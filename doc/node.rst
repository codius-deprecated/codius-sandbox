Node.js API
===========

The ``Sandbox`` class
+++++++++++++++++++++

.. js:class:: Sandbox()

  Construct a new sandbox

.. js:function:: Sandbox.mapFilename(filename)

  :param string filename: Filename to map

  Called when a filename used in open(), stat(), etc should be mapped from a
  real filename to one within the sandbox's environment.

  Should return a string, which is the new filename that will be passed to the
  underlying syscall.

.. js:function:: Sandbox.onIPC(api_name, method_name, arguments, cookie)

  :param string api_name: API being called
  :param string method_name: Method being called
  :param object arguments: Arguments for the API call
  :param object cookie: An opaque cookie that must be later passed to Sandbox.finishIPC()

  Do not touch the cookie or Very Bad Things could happen including, but not
  limited to: war, pestilance, spoilage of all the cheese in your home, a strong
  desire to port Emacs to Node.js.

.. js:function:: Sandbox.finishIPC(cookie, result)

  :param object cookie: The opaque cookie from Sandbox.onIPC() that was not
  touched.
  :param object result: API result that is passed to the sandbox

  Result should be a structure in the form of:

.. code-block:: js

  {
    'success': true,
    'result': {foo: {bar: 'baz'}}
  }

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

  :type Readable: stdio channel that maps to stderr

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
