var Sandbox = require('./build/Release/obj.target/codius-sandbox.node').Sandbox;
var events = require('events');

inherits(Sandbox, events.EventEmitter);
exports.Sandbox = Sandbox;

Sandbox.prototype.mapFilename = function (filename) {
  return filename;
};

function inherits(target, source) {
  for (var k in source.prototype) {
    target.prototype[k] = source.prototype[k];
  }
}
