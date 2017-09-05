function Event(sender) {
	this._sender = sender;
	this._listeners = [];
}

Event.prototype = {
	attach : function (listener) {
		this._listeners.push(listener);
	},
	notify : function (args) {
		for (var index = 0; index < this._listeners.length; index += 1) {
			this._listeners[index](this._sender, args);
		}
	}
};

var IEFixes = function() {
    if (document.all && !window.setTimeout.isPolyfill) {
        var __nativeST__ = window.setTimeout;
        window.setTimeout = function(vCallback, nDelay /*, argumentToPass1, argumentToPass2, etc. */) {
            var aArgs = Array.prototype.slice.call(arguments, 2);
            return __nativeST__(vCallback instanceof Function ? function() {
                vCallback.apply(null, aArgs);
            } : vCallback, nDelay);
        };
        window.setTimeout.isPolyfill = true;
    }

    if (document.all && !window.setInterval.isPolyfill) {
        var __nativeSI__ = window.setInterval;
        window.setInterval = function(vCallback, nDelay /*, argumentToPass1, argumentToPass2, etc. */) {
            var aArgs = Array.prototype.slice.call(arguments, 2);
            return __nativeSI__(vCallback instanceof Function ? function() {
                vCallback.apply(null, aArgs);
            } : vCallback, nDelay);
        };
        window.setInterval.isPolyfill = true;
    }
    
    var method;
    var noop = function () {};
    var methods = [
        'assert', 'clear', 'count', 'debug', 'dir', 'dirxml', 'error',
        'exception', 'group', 'groupCollapsed', 'groupEnd', 'info', 'log',
        'markTimeline', 'profile', 'profileEnd', 'table', 'time', 'timeEnd',
        'timeStamp', 'trace', 'warn'
    ];
    var length = methods.length;
    var console = (window.console = window.console || {});

    while (length--) {
        method = methods[length];
        // Only stub undefined methods.
        if (!console[method]) {
            console[method] = noop;
        }
    }
};

var commonInit = function() {
    if (navigator.appVersion.indexOf("MSIE") !== -1) {
        IEFixes();
    }
};