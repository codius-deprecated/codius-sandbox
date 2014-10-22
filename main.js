var nativeModule = require('./build/Release/obj.target/codius-sandbox.node');
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
exports.Sandbox = Sandbox;

Sandbox.prototype.mapFilename = function (filename) {
  return filename;
};

Sandbox.prototype._init = function() {
  this.stdio = [
    new StdioWriter(),
    new StdioReader(),
    new StdioReader()
  ];

  this.stdout = this.stdio[1];
  this.stderr = this.stdio[2];
};

Sandbox.prototype.onData = function (fd, chunk) {
  this.stdio[fd].push(chunk);
};

function inherits(target, source) {
  for (var k in source.prototype) {
    target.prototype[k] = source.prototype[k];
  }
}
