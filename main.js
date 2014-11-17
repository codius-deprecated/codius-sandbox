var nativeModule = require('bindings')('node-codius-sandbox.node');
var ipc = require('bindings')('codius-api.node');
var Sandbox = nativeModule.Sandbox;
var events = require('events');
var stream = require('stream');
var util = require('util');

var Readable = require('stream').Readable;

function StdioWriter() {
};

function StdioReader(options) {
  Readable.call(this, options);
};

util.inherits (StdioReader, Readable);

StdioReader.prototype._read = function() {
};

inherits(Sandbox, events.EventEmitter);

/**
 * seccomp-enabled sandbox
 */

/**
 * Construct a new Sandbox
 * @memberof Sandbox
 * @constructor
 */
exports.Sandbox = Sandbox;

exports.ipc = ipc;

/**
 * Called when a filename used in open(), stat(), etc should be mapped from a
 * real filename to one within the sandbox's environment
 * @function mapFilename
 * @param {string} filename
 * @memberof Sandbox
 * @instance
 */
Sandbox.prototype.mapFilename = function (filename) {
  return filename;
};

/**
 * Internal function. Sets up stdio IPC channels upon construction
 * @memberof Sandbox
 * @function _init
 * @private
 * @instance
 */
Sandbox.prototype._init = function() {

  /**
   * stdio channels
   * @instance
   * @type Array
   */
  this.stdio = [
    new StdioWriter(),
    new StdioReader(),
    new StdioReader()
  ];

  /**
   * stdio channel that maps to stdout
   * @instance
   * @type Readable
   */
  this.stdout = this.stdio[1];

  /**
   * stdio channel that maps to stderr
   * @instance
   * @type Readable
   */
  this.stderr = this.stdio[2];
};

/**
 * Internal function. Called when data from within the sandbox is ready for
 * reading
 * @function onData
 * @memberof Sandbox
 * @instance
 * @private
 * @param {number} fd file descriptor inside the sandbox
 * @param {string} chunk Data read from the sandbox
 */
Sandbox.prototype.onData = function (fd, chunk) {
  this.stdio[fd].push(chunk);
};

/**
 * Spawn a binary inside the sandbox
 * @function spawn
 * @memberof Sandbox
 * @instance
 * @param {string} arg0 First argument
 * @param {string} [...] Second and beyond arguments
 */

/**
 * Kill the child process
 * @function kill
 * @memberof Sandbox
 * @instance
 */

/** 
 * Launch GDB when the child crashes
 * @member debuggerOnCrash
 * @memberof Sandbox
 * @instance
 * @type boolean
 */

//FIXME: For some reason these don't show up in jsdoc?
/**
 * Emitted when the sandboxed child has called bind() on a socket, which is now
 * mapped to a unix domain socket.
 * @event Sandbox#newSocket
 * @param {string} path Path to the unix socket
 */

/**
 * Emitted when the sandboxed child has exited
 * @event Sandbox#exit
 * @param {number} status Exit status
 */

/**
 * Emitted when the sandboxed child has received a signal
 * @event Sandbox#signal
 * @param {number} Signal received
 */

function inherits(target, source) {
  for (var k in source.prototype) {
    target.prototype[k] = source.prototype[k];
  }
}
