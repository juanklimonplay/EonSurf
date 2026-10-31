/* EonSurf: API web (DOM, eventos, red, almacenamiento) sobre QuickJS-ng.
 * Se ejecuta una vez por pagina antes de que exista el documento.
 * Todo se apoya en el objeto nativo oculto __ns. Sin dependencias.
 */
(function () {
'use strict';

var G = globalThis;
var ns = G.__ns;

/* ---------------------------------------------------------------- utilidades */

var hasOwn = Object.prototype.hasOwnProperty;
var ELEMENT_NODE = 1, TEXT_NODE = 3, COMMENT_NODE = 8, DOCUMENT_NODE = 9,
    DOCUMENT_TYPE_NODE = 10, DOCUMENT_FRAGMENT_NODE = 11;

var nodeSet = new WeakSet();

function isNode(v) {
  return v !== null && typeof v === 'object' && nodeSet.has(v);
}
function ntype(n) { return ns.type(n); }
function errText(e) {
  /* en QuickJS el stack no incluye el mensaje: van los dos */
  try {
    if (e && typeof e === 'object') {
      var head = (e.name ? e.name + ': ' : '') + (e.message !== undefined ? e.message : '');
      return e.stack ? head + ' ' + String(e.stack).split('\n').slice(0, 3).join(' | ') : head || String(e);
    }
    return String(e);
  } catch (_) { return 'error'; }
}
function log(msg) { try { ns.log(String(msg)); } catch (_) {} }
function nowMs() {
  try { return Date.now(); } catch (_) { return 0; }
}
function defineTag(proto, name) {
  try {
    Object.defineProperty(proto, Symbol.toStringTag,
      { value: name, configurable: true });
  } catch (_) {}
}
function setHidden(obj, name, value) {
  try {
    Object.defineProperty(obj, name,
      { value: value, writable: true, configurable: true, enumerable: false });
  } catch (_) { try { obj[name] = value; } catch (_) {} }
}

/* ---------------------------------------------------------------- documento */

var docCache = null;
function getDocument() {
  if (docCache !== null) {
    try { if (ns.type(docCache) === DOCUMENT_NODE) return docCache; }
    catch (_) { docCache = null; }
  }
  var d = null;
  try { d = ns.document(); } catch (_) { d = null; }
  if (d) docCache = d;
  return d;
}

/* ---------------------------------------------------------------- eventos */

var listenerMap = new WeakMap();
var handlerMap = new WeakMap();
var handlerWrappers = new WeakMap();

var CAPTURING_PHASE = 1, AT_TARGET = 2, BUBBLING_PHASE = 3;

function listenersOf(target, create) {
  var m = listenerMap.get(target);
  if (!m && create) { m = new Map(); listenerMap.set(target, m); }
  return m;
}

function addListener(target, type, fn, options) {
  if (fn === null || fn === undefined) return;
  type = String(type);
  var opts = options || {};
  var capture = false, once = false, passive = false, signal = null;
  if (typeof opts === 'boolean') capture = opts;
  else {
    capture = !!opts.capture;
    once = !!opts.once;
    passive = !!opts.passive;
    signal = opts.signal || null;
  }
  if (signal && signal.aborted) return;
  var map = listenersOf(target, true);
  var arr = map.get(type);
  if (!arr) { arr = []; map.set(type, arr); }
  for (var i = 0; i < arr.length; i++) {
    if (arr[i].fn === fn && arr[i].capture === capture) return;
  }
  var rec = { fn: fn, capture: capture, once: once, passive: passive,
              signal: signal, removed: false };
  arr.push(rec);
  if (isNode(target) && arr.length === 1) {
    try { ns.listen(type); } catch (_) {}
  }
  if (signal && typeof signal.addEventListener === 'function') {
    try {
      signal.addEventListener('abort', function () { removeRecord(target, type, rec); });
    } catch (_) {}
  }
}

function removeRecord(target, type, rec) {
  rec.removed = true;
  var map = listenerMap.get(target);
  if (!map) return;
  var arr = map.get(type);
  if (!arr) return;
  var i = arr.indexOf(rec);
  if (i >= 0) arr.splice(i, 1);
}

function removeListener(target, type, fn, options) {
  type = String(type);
  var capture = typeof options === 'boolean' ? options : !!(options && options.capture);
  var map = listenerMap.get(target);
  if (!map) return;
  var arr = map.get(type);
  if (!arr) return;
  for (var i = arr.length - 1; i >= 0; i--) {
    if (arr[i].fn === fn && arr[i].capture === capture) {
      arr[i].removed = true;
      arr.splice(i, 1);
      return;
    }
  }
}

function eventPath(target) {
  var path = [target];
  if (target === G || !isNode(target)) return path;
  var n = target, p = null;
  try { p = ns.parent(n); } catch (_) { p = null; }
  while (p) {
    path.push(p);
    n = p;
    try { p = ns.parent(n); } catch (_) { p = null; }
  }
  var root = path[path.length - 1];
  if (root && isNode(root)) {
    try { if (ns.type(root) === DOCUMENT_NODE) path.push(G); } catch (_) {}
  }
  return path;
}

function invokeListeners(target, ev, phase, filter) {
  var map = listenerMap.get(target);
  ev.currentTarget = target;
  ev.eventPhase = phase;
  if (!map) return;
  var arr = map.get(ev.type);
  if (!arr || arr.length === 0) return;
  var copy = arr.slice();
  for (var i = 0; i < copy.length; i++) {
    var rec = copy[i];
    if (rec.removed) continue;
    if (filter === 'capture' && !rec.capture) continue;
    if (filter === 'bubble' && rec.capture) continue;
    if (rec.once) removeRecord(target, ev.type, rec);
    try {
      if (typeof rec.fn === 'function') rec.fn.call(target, ev);
      else if (rec.fn && typeof rec.fn.handleEvent === 'function') rec.fn.handleEvent(ev);
    } catch (e) {
      log('listener ' + ev.type + ': ' + errText(e));
    }
    if (ev._immediateStop) break;
  }
}

function dispatchEventOn(target, ev) {
  if (!(ev instanceof Event)) throw new TypeError('dispatchEvent: not an Event');
  if (ev._dispatching) return true;
  ev._dispatching = true;
  ev._stop = false;
  ev._immediateStop = false;
  ev.target = target;
  ev.srcElement = target;
  if (!ev.timeStamp) ev.timeStamp = nowMs();
  var path = eventPath(target);
  ev._path = path;
  var i;
  for (i = path.length - 1; i > 0; i--) {
    invokeListeners(path[i], ev, CAPTURING_PHASE, 'capture');
    if (ev._stop) break;
  }
  if (!ev._stop) {
    invokeListeners(path[0], ev, AT_TARGET, 'capture');
    if (!ev._immediateStop) invokeListeners(path[0], ev, AT_TARGET, 'bubble');
    if (ev.bubbles && !ev._stop) {
      for (i = 1; i < path.length; i++) {
        invokeListeners(path[i], ev, BUBBLING_PHASE, 'bubble');
        if (ev._stop) break;
      }
    }
  }
  ev.currentTarget = null;
  ev.eventPhase = 0;
  ev._dispatching = false;
  return !ev.defaultPrevented;
}

/* manejadores on* */
var EVENT_HANDLER_NAMES = [
  'click', 'dblclick', 'mousedown', 'mouseup', 'mousemove', 'mouseover',
  'mouseout', 'mouseenter', 'mouseleave', 'contextmenu', 'wheel',
  'keydown', 'keyup', 'keypress', 'focus', 'blur', 'focusin', 'focusout',
  'input', 'change', 'submit', 'reset', 'invalid', 'select',
  'load', 'unload', 'error', 'abort', 'beforeunload', 'DOMContentLoaded',
  'readystatechange', 'pagehide', 'pageshow', 'hashchange', 'popstate',
  'resize', 'scroll', 'scrollend', 'message', 'online', 'offline',
  'loadstart', 'progress', 'loadend', 'loadedmetadata', 'loadeddata',
  'canplay', 'canplaythrough', 'playing', 'pause', 'ended', 'timeupdate',
  'volumechange', 'waiting', 'seeking', 'seeked', 'durationchange',
  'animationstart', 'animationend', 'animationiteration',
  'transitionstart', 'transitionend', 'touchstart', 'touchend', 'touchmove',
  'touchcancel', 'drag', 'dragstart', 'dragend', 'dragenter', 'dragleave',
  'dragover', 'drop', 'copy', 'cut', 'paste', 'storage', 'pointerdown',
  'pointerup', 'pointermove', 'pointerover', 'pointerout', 'pointerenter',
  'pointerleave', 'pointercancel', 'gotpointercapture', 'lostpointercapture'
];

function handlerFor(target, type) {
  var m = handlerMap.get(target);
  return m ? (m.get(type) || null) : null;
}

function setHandler(target, type, fn) {
  var m = handlerMap.get(target);
  if (!m) { m = new Map(); handlerMap.set(target, m); }
  if (fn === null || fn === undefined) { m.set(type, null); return; }
  if (typeof fn !== 'function') return;
  m.set(type, fn);
  var wm = handlerWrappers.get(target);
  if (!wm) { wm = new Map(); handlerWrappers.set(target, wm); }
  if (wm.has(type)) return;
  var wrapper = function (ev) {
    var cur = handlerFor(target, type);
    if (typeof cur !== 'function') return;
    var r;
    try { r = cur.call(target, ev); }
    catch (e) { log('handler ' + type + ': ' + errText(e)); return; }
    if (r === false && ev && typeof ev.preventDefault === 'function') ev.preventDefault();
  };
  wm.set(type, wrapper);
  addListener(target, type, wrapper, false);
}

function defineHandlerProps(proto) {
  for (var i = 0; i < EVENT_HANDLER_NAMES.length; i++) {
    (function (name) {
      try {
        Object.defineProperty(proto, 'on' + name, {
          configurable: true, enumerable: true,
          get: function () { return handlerFor(this, name); },
          set: function (fn) { setHandler(this, name, fn); }
        });
      } catch (_) {}
    })(EVENT_HANDLER_NAMES[i]);
  }
}

/* ---------------------------------------------------------------- EventTarget */

class EventTarget {
  addEventListener(type, fn, options) { addListener(this, type, fn, options); }
  removeEventListener(type, fn, options) { removeListener(this, type, fn, options); }
  dispatchEvent(ev) { return dispatchEventOn(this, ev); }
}
defineTag(EventTarget.prototype, 'EventTarget');
defineHandlerProps(EventTarget.prototype);

/* ---------------------------------------------------------------- Eventos */

class Event {
  constructor(type, init) {
    if (arguments.length === 0) type = '';
    init = init || {};
    this.type = String(type);
    this.bubbles = !!init.bubbles;
    this.cancelable = !!init.cancelable;
    this.composed = !!init.composed;
    this.defaultPrevented = false;
    this.target = null;
    this.currentTarget = null;
    this.srcElement = null;
    this.eventPhase = 0;
    this.isTrusted = !!init.isTrusted;
    this.timeStamp = nowMs();
    this.cancelBubble = false;
    this.returnValue = true;
    this._stop = false;
    this._immediateStop = false;
    this._dispatching = false;
    this._path = [];
  }
  preventDefault() {
    if (this.cancelable) { this.defaultPrevented = true; this.returnValue = false; }
  }
  stopPropagation() { this.cancelBubble = true; this._stop = true; }
  stopImmediatePropagation() {
    this.cancelBubble = true; this._stop = true; this._immediateStop = true;
  }
  composedPath() { return this._path ? this._path.slice() : []; }
  initEvent(type, bubbles, cancelable) {
    this.type = String(type);
    this.bubbles = !!bubbles;
    this.cancelable = !!cancelable;
  }
}
Event.NONE = 0; Event.CAPTURING_PHASE = 1; Event.AT_TARGET = 2; Event.BUBBLING_PHASE = 3;
Event.prototype.NONE = 0;
Event.prototype.CAPTURING_PHASE = 1;
Event.prototype.AT_TARGET = 2;
Event.prototype.BUBBLING_PHASE = 3;
defineTag(Event.prototype, 'Event');

class CustomEvent extends Event {
  constructor(type, init) {
    super(type, init);
    init = init || {};
    this.detail = init.detail === undefined ? null : init.detail;
  }
  initCustomEvent(type, bubbles, cancelable, detail) {
    this.initEvent(type, bubbles, cancelable);
    this.detail = detail === undefined ? null : detail;
  }
}
defineTag(CustomEvent.prototype, 'CustomEvent');

class UIEvent extends Event {
  constructor(type, init) {
    super(type, init);
    init = init || {};
    this.view = init.view === undefined ? G : init.view;
    this.detail = init.detail === undefined ? 0 : init.detail;
    this.which = init.which === undefined ? 0 : init.which;
  }
}
defineTag(UIEvent.prototype, 'UIEvent');

class MouseEvent extends UIEvent {
  constructor(type, init) {
    super(type, init);
    init = init || {};
    this.screenX = init.screenX === undefined ? 0 : init.screenX;
    this.screenY = init.screenY === undefined ? 0 : init.screenY;
    this.clientX = init.clientX === undefined ? 0 : init.clientX;
    this.clientY = init.clientY === undefined ? 0 : init.clientY;
    this.pageX = init.pageX === undefined ? this.clientX : init.pageX;
    this.pageY = init.pageY === undefined ? this.clientY : init.pageY;
    this.x = this.clientX; this.y = this.clientY;
    this.offsetX = init.offsetX === undefined ? 0 : init.offsetX;
    this.offsetY = init.offsetY === undefined ? 0 : init.offsetY;
    this.movementX = init.movementX === undefined ? 0 : init.movementX;
    this.movementY = init.movementY === undefined ? 0 : init.movementY;
    this.button = init.button === undefined ? 0 : init.button;
    this.buttons = init.buttons === undefined ? 0 : init.buttons;
    this.ctrlKey = !!init.ctrlKey;
    this.shiftKey = !!init.shiftKey;
    this.altKey = !!init.altKey;
    this.metaKey = !!init.metaKey;
    this.relatedTarget = init.relatedTarget === undefined ? null : init.relatedTarget;
  }
  getModifierState(key) {
    switch (String(key)) {
      case 'Control': return this.ctrlKey;
      case 'Shift': return this.shiftKey;
      case 'Alt': return this.altKey;
      case 'Meta': return this.metaKey;
      default: return false;
    }
  }
}
defineTag(MouseEvent.prototype, 'MouseEvent');

class KeyboardEvent extends UIEvent {
  constructor(type, init) {
    super(type, init);
    init = init || {};
    this.key = init.key === undefined ? '' : String(init.key);
    this.code = init.code === undefined ? '' : String(init.code);
    this.location = init.location === undefined ? 0 : init.location;
    this.repeat = !!init.repeat;
    this.isComposing = !!init.isComposing;
    this.ctrlKey = !!init.ctrlKey;
    this.shiftKey = !!init.shiftKey;
    this.altKey = !!init.altKey;
    this.metaKey = !!init.metaKey;
    this.keyCode = init.keyCode === undefined ? 0 : init.keyCode;
    this.which = init.which === undefined ? this.keyCode : init.which;
    this.charCode = init.charCode === undefined ? 0 : init.charCode;
  }
  getModifierState(key) {
    switch (String(key)) {
      case 'Control': return this.ctrlKey;
      case 'Shift': return this.shiftKey;
      case 'Alt': return this.altKey;
      case 'Meta': return this.metaKey;
      default: return false;
    }
  }
}
defineTag(KeyboardEvent.prototype, 'KeyboardEvent');

class FocusEvent extends UIEvent {
  constructor(type, init) {
    super(type, init);
    init = init || {};
    this.relatedTarget = init.relatedTarget === undefined ? null : init.relatedTarget;
  }
}
defineTag(FocusEvent.prototype, 'FocusEvent');

class InputEvent extends UIEvent {
  constructor(type, init) {
    super(type, init);
    init = init || {};
    this.data = init.data === undefined ? null : init.data;
    this.inputType = init.inputType === undefined ? '' : String(init.inputType);
    this.isComposing = !!init.isComposing;
    this.dataTransfer = init.dataTransfer === undefined ? null : init.dataTransfer;
  }
}
defineTag(InputEvent.prototype, 'InputEvent');

/* ---------------------------------------------------------------- MutationObserver */

var observers = [];

function isDescendantOf(node, ancestor) {
  var n = node;
  while (n) {
    if (n === ancestor) return true;
    try { n = ns.parent(n); } catch (_) { n = null; }
  }
  return false;
}

function notifyMutation(node, rec) {
  if (observers.length === 0) return;
  for (var i = 0; i < observers.length; i++) {
    var ob = observers[i];
    if (ob._disconnected || !ob._target) continue;
    var match = ob._target === node;
    if (!match && ob._options && ob._options.subtree) {
      match = isDescendantOf(node, ob._target);
    }
    if (!match) continue;
    var opts = ob._options || {};
    if (rec.type === 'childList' && !opts.childList) continue;
    if (rec.type === 'attributes' && !opts.attributes) continue;
    if (rec.type === 'characterData' && !opts.characterData) continue;
    var r = {
      type: rec.type,
      target: node,
      addedNodes: rec.addedNodes || [],
      removedNodes: rec.removedNodes || [],
      previousSibling: rec.previousSibling === undefined ? null : rec.previousSibling,
      nextSibling: rec.nextSibling === undefined ? null : rec.nextSibling,
      attributeName: rec.attributeName === undefined ? null : rec.attributeName,
      attributeNamespace: null,
      oldValue: rec.oldValue === undefined ? null : rec.oldValue
    };
    ob._records.push(r);
    scheduleObserver(ob);
  }
}

function scheduleObserver(ob) {
  if (ob._scheduled) return;
  ob._scheduled = true;
  queueMicrotask(function () {
    ob._scheduled = false;
    if (ob._disconnected) return;
    var recs = ob.takeRecords();
    if (recs.length === 0) return;
    try { ob._cb(recs, ob); }
    catch (e) { log('MutationObserver: ' + errText(e)); }
  });
}

class MutationObserver {
  constructor(cb) {
    if (typeof cb !== 'function') throw new TypeError('MutationObserver: callback required');
    this._cb = cb;
    this._records = [];
    this._target = null;
    this._options = null;
    this._scheduled = false;
    this._disconnected = true;
  }
  observe(target, options) {
    if (!isNode(target)) throw new TypeError('MutationObserver.observe: not a Node');
    this._target = target;
    this._options = options || {};
    this._disconnected = false;
    if (observers.indexOf(this) < 0) observers.push(this);
  }
  disconnect() {
    this._disconnected = true;
    this._records = [];
    this._target = null;
    var i = observers.indexOf(this);
    if (i >= 0) observers.splice(i, 1);
  }
  takeRecords() {
    var r = this._records;
    this._records = [];
    return r;
  }
}
defineTag(MutationObserver.prototype, 'MutationObserver');

/* ---------------------------------------------------------------- listas */

var nodeListHandler = {
  get: function (t, p, r) {
    if (typeof p === 'string' && /^\d+$/.test(p)) {
      var a = t._items(); var i = Number(p);
      return i < a.length ? a[i] : undefined;
    }
    if (p === 'length') return t._items().length;
    return Reflect.get(t, p, r);
  },
  has: function (t, p) {
    if (typeof p === 'string' && /^\d+$/.test(p)) return Number(p) < t._items().length;
    return Reflect.has(t, p);
  },
  ownKeys: function (t) {
    var a = t._items(), ks = [];
    for (var i = 0; i < a.length; i++) ks.push(String(i));
    ks.push('length');
    return ks;
  },
  getOwnPropertyDescriptor: function (t, p) {
    if (typeof p === 'string' && /^\d+$/.test(p)) {
      var a = t._items(), i = Number(p);
      if (i < a.length) {
        return { value: a[i], enumerable: true, configurable: true, writable: false };
      }
      return undefined;
    }
    if (p === 'length') {
      return { value: t._items().length, enumerable: false, configurable: true };
    }
    return undefined;
  }
};

class NodeList {
  constructor(src) { this._src = src; }
  _items() {
    var s = this._src;
    return typeof s === 'function' ? s() : s;
  }
  get length() { return this._items().length; }
  item(i) { return this._items()[i] || null; }
  forEach(cb, thisArg) {
    var a = this._items();
    for (var i = 0; i < a.length; i++) cb.call(thisArg, a[i], i, this);
  }
  entries() {
    var a = this._items(), i = 0;
    return { next: function () { return i < a.length ? { value: [i, a[i++]], done: false } : { value: undefined, done: true }; },
             [Symbol.iterator]: function () { return this; } };
  }
  keys() {
    var a = this._items(), i = 0;
    return { next: function () { return i < a.length ? { value: i++, done: false } : { value: undefined, done: true }; },
             [Symbol.iterator]: function () { return this; } };
  }
  values() {
    var a = this._items(), i = 0;
    return { next: function () { return i < a.length ? { value: a[i++], done: false } : { value: undefined, done: true }; },
             [Symbol.iterator]: function () { return this; } };
  }
  [Symbol.iterator]() { return this.values(); }
}
defineTag(NodeList.prototype, 'NodeList');

class HTMLCollection extends NodeList {
  namedItem(name) {
    name = String(name);
    var a = this._items();
    for (var i = 0; i < a.length; i++) {
      var el = a[i];
      if (!el || el.nodeType !== ELEMENT_NODE) continue;
      if (ns.getAttr(el, 'id') === name) return el;
      if (ns.getAttr(el, 'name') === name) return el;
    }
    return null;
  }
}
defineTag(HTMLCollection.prototype, 'HTMLCollection');

function makeList(arr) { return new Proxy(new NodeList(arr.slice()), nodeListHandler); }
function makeCollection(arr) { return new Proxy(new HTMLCollection(arr.slice()), nodeListHandler); }
function liveNodeList(getter) { return new Proxy(new NodeList(getter), nodeListHandler); }
function liveCollection(getter) { return new Proxy(new HTMLCollection(getter), nodeListHandler); }

/* ---------------------------------------------------------------- helpers DOM */

function toNode(v) {
  if (isNode(v)) return v;
  var d = getDocument();
  if (!d) throw new TypeError('no document');
  return ns.createText(String(v));
}
function elementsOf(node) {
  var kids = ns.children(node), out = [];
  for (var i = 0; i < kids.length; i++) {
    if (ns.type(kids[i]) === ELEMENT_NODE) out.push(kids[i]);
  }
  return out;
}
function firstElementChild(node) {
  var kids = ns.children(node);
  for (var i = 0; i < kids.length; i++) {
    if (ns.type(kids[i]) === ELEMENT_NODE) return kids[i];
  }
  return null;
}
function previousElement(node) {
  var p = ns.prev(node);
  while (p) {
    if (ns.type(p) === ELEMENT_NODE) return p;
    p = ns.prev(p);
  }
  return null;
}
function nextElement(node) {
  var p = ns.next(node);
  while (p) {
    if (ns.type(p) === ELEMENT_NODE) return p;
    p = ns.next(p);
  }
  return null;
}
function setAttrNotify(el, name, value) {
  var old = null;
  try { old = ns.getAttr(el, name); } catch (_) {}
  ns.setAttr(el, name, String(value));
  notifyMutation(el, { type: 'attributes', attributeName: String(name), oldValue: old });
  var low = String(name).toLowerCase();
  if (low.length > 2 && low.slice(0, 2) === 'on') {
    var fn = null;
    try { fn = new Function('event', String(value)); }
    catch (e) { log('bad handler ' + name + ': ' + errText(e)); }
    setHandler(el, low.slice(2), fn);
  }
}
function removeAttrNotify(el, name) {
  var old = null;
  try { old = ns.getAttr(el, name); } catch (_) {}
  ns.removeAttr(el, name);
  notifyMutation(el, { type: 'attributes', attributeName: String(name), oldValue: old });
  var low = String(name).toLowerCase();
  if (low.length > 2 && low.slice(0, 2) === 'on') setHandler(el, low.slice(2), null);
}

/* ---------------------------------------------------------------- Node */

class Node extends EventTarget {
  constructor() { throw new TypeError('Illegal constructor'); }

  get nodeType() { return ns.type(this); }
  get nodeName() { return ns.name(this); }
  get nodeValue() { return ns.value(this); }
  set nodeValue(v) {
    var s = v === null || v === undefined ? '' : String(v);
    ns.setValue(this, s);
  }
  get textContent() {
    var t = ns.text(this);
    return t === null || t === undefined ? '' : String(t);
  }
  set textContent(v) {
    var s = v === null || v === undefined ? '' : String(v);
    var t = ns.type(this);
    if (t === TEXT_NODE || t === COMMENT_NODE) {
      var old = ns.value(this);
      ns.setValue(this, s);
      notifyMutation(this, { type: 'characterData', oldValue: old });
    } else {
      var before = ns.children(this);
      ns.setText(this, s);
      notifyMutation(this, { type: 'childList', removedNodes: before });
    }
  }
  get parentNode() { return ns.parent(this); }
  get parentElement() {
    var p = ns.parent(this);
    return p && ns.type(p) === ELEMENT_NODE ? p : null;
  }
  get childNodes() { return liveNodeList(function () { return ns.children(this); }.bind(this)); }
  get firstChild() { return ns.first(this); }
  get lastChild() { return ns.last(this); }
  get nextSibling() { return ns.next(this); }
  get previousSibling() { return ns.prev(this); }
  get ownerDocument() {
    return ns.type(this) === DOCUMENT_NODE ? null : getDocument();
  }
  get isConnected() {
    var n = this;
    while (n) {
      if (ns.type(n) === DOCUMENT_NODE) return true;
      try { n = ns.parent(n); } catch (_) { n = null; }
    }
    return false;
  }
  appendChild(child) {
    if (!isNode(child)) throw new TypeError('appendChild: not a Node');
    if (ns.type(child) === DOCUMENT_FRAGMENT_NODE) {
      var kids = ns.children(child);
      for (var i = 0; i < kids.length; i++) this.appendChild(kids[i]);
      return child;
    }
    var prev = ns.last(this);
    var r = ns.append(this, child);
    notifyMutation(this, { type: 'childList', addedNodes: [child], previousSibling: prev });
    return isNode(r) ? r : child;
  }
  insertBefore(child, ref) {
    if (!isNode(child)) throw new TypeError('insertBefore: not a Node');
    if (ref === null || ref === undefined) return this.appendChild(child);
    if (!isNode(ref)) throw new TypeError('insertBefore: ref is not a Node');
    if (ns.type(child) === DOCUMENT_FRAGMENT_NODE) {
      var kids = ns.children(child);
      for (var i = 0; i < kids.length; i++) this.insertBefore(kids[i], ref);
      return child;
    }
    var prev = ns.prev(ref);
    var r = ns.insert(this, child, ref);
    notifyMutation(this, { type: 'childList', addedNodes: [child],
      previousSibling: prev, nextSibling: ref });
    return isNode(r) ? r : child;
  }
  removeChild(child) {
    if (!isNode(child)) throw new TypeError('removeChild: not a Node');
    var prev = ns.prev(child), next = ns.next(child);
    var r = ns.remove(this, child);
    notifyMutation(this, { type: 'childList', removedNodes: [child],
      previousSibling: prev, nextSibling: next });
    return isNode(r) ? r : child;
  }
  replaceChild(newChild, oldChild) {
    if (!isNode(newChild) || !isNode(oldChild)) throw new TypeError('replaceChild: not a Node');
    if (ns.type(newChild) === DOCUMENT_FRAGMENT_NODE) {
      var parent = this;
      var ref = oldChild;
      var kids = ns.children(newChild);
      for (var i = 0; i < kids.length; i++) parent.insertBefore(kids[i], ref);
      parent.removeChild(oldChild);
      return oldChild;
    }
    var r = ns.replace(this, newChild, oldChild);
    notifyMutation(this, { type: 'childList', addedNodes: [newChild], removedNodes: [oldChild] });
    return isNode(r) ? r : oldChild;
  }
  cloneNode(deep) {
    var c = ns.clone(this, !!deep);
    if (c) scanHandlers(c);
    return c;
  }
  contains(other) {
    if (!isNode(other)) return false;
    var n = other;
    while (n) {
      if (n === this) return true;
      try { n = ns.parent(n); } catch (_) { n = null; }
    }
    return false;
  }
  hasChildNodes() { return ns.first(this) !== null; }
  getRootNode() {
    var n = this;
    while (true) {
      var p = null;
      try { p = ns.parent(n); } catch (_) { p = null; }
      if (!p) return n;
      n = p;
    }
  }
  isSameNode(other) { return this === other; }
  isEqualNode(other) {
    if (!isNode(other)) return false;
    return equalNodes(this, other);
  }
  compareDocumentPosition(other) {
    if (!isNode(other)) return 1;
    if (this === other) return 0;
    if (this.contains(other)) return 20;
    if (other.contains(this)) return 10;
    var ra = this.getRootNode(), rb = other.getRootNode();
    if (ra !== rb) return 33;
    var ia = treeIndex(this, ra), ib = treeIndex(other, rb);
    if (ia < 0 || ib < 0) return 33;
    return ia < ib ? 4 : 2;
  }
  normalize() {}
}

Node.ELEMENT_NODE = 1;
Node.ATTRIBUTE_NODE = 2;
Node.TEXT_NODE = 3;
Node.CDATA_SECTION_NODE = 4;
Node.ENTITY_REFERENCE_NODE = 5;
Node.ENTITY_NODE = 6;
Node.PROCESSING_INSTRUCTION_NODE = 7;
Node.COMMENT_NODE = 8;
Node.DOCUMENT_NODE = 9;
Node.DOCUMENT_TYPE_NODE = 10;
Node.DOCUMENT_FRAGMENT_NODE = 11;
Node.DOCUMENT_POSITION_DISCONNECTED = 1;
Node.DOCUMENT_POSITION_PRECEDING = 2;
Node.DOCUMENT_POSITION_FOLLOWING = 4;
Node.DOCUMENT_POSITION_CONTAINS = 8;
Node.DOCUMENT_POSITION_CONTAINED_BY = 16;
Node.DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC = 32;
(function () {
  var names = ['ELEMENT_NODE', 'ATTRIBUTE_NODE', 'TEXT_NODE', 'CDATA_SECTION_NODE',
    'ENTITY_REFERENCE_NODE', 'ENTITY_NODE', 'PROCESSING_INSTRUCTION_NODE',
    'COMMENT_NODE', 'DOCUMENT_NODE', 'DOCUMENT_TYPE_NODE', 'DOCUMENT_FRAGMENT_NODE',
    'DOCUMENT_POSITION_DISCONNECTED', 'DOCUMENT_POSITION_PRECEDING',
    'DOCUMENT_POSITION_FOLLOWING', 'DOCUMENT_POSITION_CONTAINS',
    'DOCUMENT_POSITION_CONTAINED_BY', 'DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC'];
  for (var i = 0; i < names.length; i++) {
    try {
      Object.defineProperty(Node.prototype, names[i],
        { value: Node[names[i]], enumerable: false, configurable: true });
    } catch (_) {}
  }
})();
defineTag(Node.prototype, 'Node');

function equalNodes(a, b) {
  if (ns.type(a) !== ns.type(b)) return false;
  if (ns.name(a) !== ns.name(b)) return false;
  if ((ns.value(a) || '') !== (ns.value(b) || '')) return false;
  if (ns.type(a) === ELEMENT_NODE) {
    var an = ns.attrNames(a) || [], bn = ns.attrNames(b) || [];
    if (an.length !== bn.length) return false;
    for (var i = 0; i < an.length; i++) {
      if (ns.getAttr(a, an[i]) !== ns.getAttr(b, an[i])) return false;
    }
  }
  var ac = ns.children(a), bc = ns.children(b);
  if (ac.length !== bc.length) return false;
  for (var j = 0; j < ac.length; j++) {
    if (!equalNodes(ac[j], bc[j])) return false;
  }
  return true;
}

function treeIndex(node, root) {
  var i = 0, found = -1;
  function walk(n) {
    if (found >= 0) return;
    if (n === node) { found = i; return; }
    i++;
    var kids = ns.children(n);
    for (var k = 0; k < kids.length; k++) { walk(kids[k]); if (found >= 0) return; }
  }
  walk(root);
  return found;
}

/* ---------------------------------------------------------------- CharacterData */

class CharacterData extends Node {
  constructor() { throw new TypeError('Illegal constructor'); }
  get data() { return ns.value(this) || ''; }
  set data(v) {
    var old = ns.value(this);
    ns.setValue(this, v === null || v === undefined ? '' : String(v));
    notifyMutation(this, { type: 'characterData', oldValue: old });
  }
  get length() { return (ns.value(this) || '').length; }
  get nextElementSibling() { return nextElement(this); }
  get previousElementSibling() { return previousElement(this); }
  appendData(s) { this.data = this.data + String(s); }
  deleteData(off, n) {
    off = Number(off) || 0; n = Number(n) || 0;
    this.data = this.data.slice(0, off) + this.data.slice(off + n);
  }
  insertData(off, s) {
    off = Number(off) || 0;
    this.data = this.data.slice(0, off) + String(s) + this.data.slice(off);
  }
  replaceData(off, n, s) {
    off = Number(off) || 0; n = Number(n) || 0;
    this.data = this.data.slice(0, off) + String(s) + this.data.slice(off + n);
  }
  substringData(off, n) {
    off = Number(off) || 0; n = Number(n) || 0;
    return this.data.substr(off, n);
  }
  before() { insertNodesBefore(this, arguments); }
  after() { insertNodesAfter(this, arguments); }
  replaceWith() { replaceNodeWith(this, arguments); }
  remove() { if (this.parentNode) this.parentNode.removeChild(this); }
}
defineTag(CharacterData.prototype, 'CharacterData');

class Text extends CharacterData {
  constructor(data) {
    var d = getDocument();
    if (!d) throw new TypeError('no document');
    return ns.createText(data === undefined ? '' : String(data));
  }
  get wholeText() { return this.data; }
  get assignedSlot() { return null; }
  splitText(offset) {
    offset = Number(offset) || 0;
    var s = this.data;
    var tail = s.slice(offset);
    this.data = s.slice(0, offset);
    var d = getDocument();
    var t = ns.createText(tail);
    if (this.parentNode) this.parentNode.insertBefore(t, this.nextSibling);
    return t;
  }
}
defineTag(Text.prototype, 'Text');

class Comment extends CharacterData {
  constructor(data) {
    var d = getDocument();
    if (!d) throw new TypeError('no document');
    return ns.createComment(data === undefined ? '' : String(data));
  }
}
defineTag(Comment.prototype, 'Comment');

/* ---------------------------------------------------------------- DocumentFragment */

class DocumentFragment extends Node {
  constructor() {
    var d = getDocument();
    if (!d) throw new TypeError('no document');
    return ns.createFragment();
  }
}
defineTag(DocumentFragment.prototype, 'DocumentFragment');

/* ---------------------------------------------------------------- serializacion */

var VOID_ELEMENTS = {
  area: 1, base: 1, br: 1, col: 1, embed: 1, hr: 1, img: 1, input: 1,
  link: 1, meta: 1, source: 1, track: 1, wbr: 1
};

function escapeText(s) {
  return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}
function escapeAttr(s) {
  return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;')
    .replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

function serializeChildren(node) {
  var kids = ns.children(node), out = '';
  for (var i = 0; i < kids.length; i++) out += serializeNode(kids[i]);
  return out;
}

function serializeNode(n) {
  var t = ns.type(n);
  if (t === TEXT_NODE) {
    var p = ns.parent(n);
    var pn = p ? ns.name(p).toLowerCase() : '';
    var v = ns.value(n) || '';
    return (pn === 'script' || pn === 'style') ? v : escapeText(v);
  }
  if (t === COMMENT_NODE) return '<!--' + (ns.value(n) || '') + '-->';
  if (t === DOCUMENT_NODE || t === DOCUMENT_FRAGMENT_NODE) return serializeChildren(n);
  if (t === ELEMENT_NODE) {
    var tag = ns.name(n).toLowerCase();
    var s = '<' + tag;
    var names = ns.attrNames(n) || [];
    for (var i = 0; i < names.length; i++) {
      var name = String(names[i]).toLowerCase();
      var av = ns.getAttr(n, names[i]);
      s += ' ' + name + '="' + escapeAttr(av === null || av === undefined ? '' : av) + '"';
    }
    s += '>';
    if (VOID_ELEMENTS[tag]) return s;
    s += serializeChildren(n);
    s += '</' + tag + '>';
    return s;
  }
  return '';
}

/* ---------------------------------------------------------------- ParentNode / ChildNode */

function insertNodesBefore(node, args) {
  var parent = node.parentNode;
  if (!parent) return;
  for (var i = 0; i < args.length; i++) parent.insertBefore(toNode(args[i]), node);
}
function insertNodesAfter(node, args) {
  var parent = node.parentNode;
  if (!parent) return;
  var ref = node.nextSibling;
  for (var i = 0; i < args.length; i++) parent.insertBefore(toNode(args[i]), ref);
}
function replaceNodeWith(node, args) {
  var parent = node.parentNode;
  if (!parent) return;
  var ref = node.nextSibling;
  for (var i = 0; i < args.length; i++) parent.insertBefore(toNode(args[i]), node);
  parent.removeChild(node);
}
function parentAppend(args) {
  for (var i = 0; i < args.length; i++) this.appendChild(toNode(args[i]));
}
function parentPrepend(args) {
  var ref = this.firstChild;
  for (var i = 0; i < args.length; i++) this.insertBefore(toNode(args[i]), ref);
}
function parentReplaceChildren(args) {
  var kids = ns.children(this);
  for (var i = 0; i < kids.length; i++) this.removeChild(kids[i]);
  for (var j = 0; j < args.length; j++) this.appendChild(toNode(args[j]));
}

var PARENT_METHODS = {
  get children() { return liveCollection(function () { return elementsOf(this); }.bind(this)); },
  get firstElementChild() { return firstElementChild(this); },
  get lastElementChild() {
    var kids = elementsOf(this);
    return kids.length ? kids[kids.length - 1] : null;
  },
  get childElementCount() { return elementsOf(this).length; },
  append: function () { parentAppend.call(this, arguments); },
  prepend: function () { parentPrepend.call(this, arguments); },
  replaceChildren: function () { parentReplaceChildren.call(this, arguments); },
  querySelector: function (sel) { return qsa(this, sel, true); },
  querySelectorAll: function (sel) { return makeList(qsa(this, sel, false)); },
  getElementsByTagName: function (tag) {
    tag = String(tag);
    var root = this;
    return liveCollection(function () { return byTagAll(root, tag); });
  },
  getElementsByClassName: function (names) {
    names = String(names).split(/\s+/).filter(function (x) { return x; });
    var root = this;
    return liveCollection(function () {
      var all = byTagAll(root, '*'), out = [];
      for (var i = 0; i < all.length; i++) {
        var cls = ' ' + (ns.getAttr(all[i], 'class') || '').replace(/\s+/g, ' ') + ' ';
        var ok = true;
        for (var j = 0; j < names.length; j++) {
          if (cls.indexOf(' ' + names[j] + ' ') < 0) { ok = false; break; }
        }
        if (ok) out.push(all[i]);
      }
      return out;
    });
  }
};

var CHILD_METHODS = {
  get nextElementSibling() { return nextElement(this); },
  get previousElementSibling() { return previousElement(this); },
  before: function () { insertNodesBefore(this, arguments); },
  after: function () { insertNodesAfter(this, arguments); },
  replaceWith: function () { replaceNodeWith(this, arguments); },
  remove: function () { if (this.parentNode) this.parentNode.removeChild(this); }
};

function installMixin(proto, mixin) {
  var names = Object.getOwnPropertyNames(mixin);
  for (var i = 0; i < names.length; i++) {
    var d = Object.getOwnPropertyDescriptor(mixin, names[i]);
    try { Object.defineProperty(proto, names[i], d); } catch (_) {}
  }
}

/* ---------------------------------------------------------------- estado de UI */

var uiState = {
  readyState: 'loading',
  activeElement: null,
  focused: null
};
var scrollState = new WeakMap();
function scrollOf(el) {
  var s = scrollState.get(el);
  if (!s) { s = { top: 0, left: 0 }; scrollState.set(el, s); }
  return s;
}

/* ---------------------------------------------------------------- Element */

var classListCache = new WeakMap();
var datasetCache = new WeakMap();
var styleCache = new WeakMap();

class Element extends Node {
  constructor() { throw new TypeError('Illegal constructor'); }

  get tagName() { return ns.name(this); }
  get localName() { return ns.name(this).toLowerCase(); }
  get nodeName() { return ns.name(this); }
  get namespaceURI() { return 'http://www.w3.org/1999/xhtml'; }
  get prefix() { return null; }
  get id() { return ns.getAttr(this, 'id') || ''; }
  set id(v) { setAttrNotify(this, 'id', v === null ? '' : String(v)); }
  get className() { return ns.getAttr(this, 'class') || ''; }
  set className(v) { setAttrNotify(this, 'class', v === null ? '' : String(v)); }
  get classList() {
    var c = classListCache.get(this);
    if (!c) { c = new DOMTokenList(this, 'class'); classListCache.set(this, c); }
    return c;
  }
  get attributes() {
    var names = ns.attrNames(this) || [], arr = [];
    for (var i = 0; i < names.length; i++) {
      var v = ns.getAttr(this, names[i]);
      arr.push({
        name: String(names[i]), localName: String(names[i]).toLowerCase(),
        value: v === null ? '' : String(v), nodeName: String(names[i]),
        nodeValue: v === null ? '' : String(v), specified: true,
        namespaceURI: null, prefix: null, nodeType: 2
      });
    }
    arr.item = function (i) { return this[i] || null; };
    arr.getNamedItem = function (n) {
      n = String(n).toLowerCase();
      for (var k = 0; k < this.length; k++) if (this[k].name.toLowerCase() === n) return this[k];
      return null;
    };
    return arr;
  }
  get dataset() {
    var d = datasetCache.get(this);
    if (!d) { d = makeDataset(this); datasetCache.set(this, d); }
    return d;
  }
  get innerHTML() { return serializeChildren(this); }
  set innerHTML(v) {
    var before = ns.children(this);
    ns.setInnerHTML(this, v === null || v === undefined ? '' : String(v));
    notifyMutation(this, { type: 'childList', removedNodes: before });
    scanHandlers(this);
  }
  get outerHTML() { return serializeNode(this); }
  set outerHTML(v) {
    var parent = this.parentNode;
    if (!parent) return;
    var temp = createTempElement();
    ns.setInnerHTML(temp, v === null || v === undefined ? '' : String(v));
    var kids = ns.children(temp);
    for (var i = 0; i < kids.length; i++) scanHandlers(kids[i]);
    for (var j = 0; j < kids.length; j++) parent.insertBefore(kids[j], this);
    parent.removeChild(this);
  }

  getAttribute(name) {
    var v = ns.getAttr(this, String(name));
    return v === null ? null : String(v);
  }
  setAttribute(name, value) {
    if (value === undefined) value = '';
    setAttrNotify(this, String(name), value);
  }
  removeAttribute(name) { removeAttrNotify(this, String(name)); }
  hasAttribute(name) { return ns.getAttr(this, String(name)) !== null; }
  toggleAttribute(name, force) {
    name = String(name);
    var has = this.hasAttribute(name);
    if (force === undefined) {
      if (has) { this.removeAttribute(name); return false; }
      this.setAttribute(name, ''); return true;
    }
    if (force) { if (!has) this.setAttribute(name, ''); return true; }
    if (has) this.removeAttribute(name);
    return false;
  }
  getAttributeNames() {
    var names = ns.attrNames(this) || [], out = [];
    for (var i = 0; i < names.length; i++) out.push(String(names[i]).toLowerCase());
    return out;
  }
  getAttributeNode(name) {
    var v = this.getAttribute(name);
    if (v === null) return null;
    return { name: String(name), value: v, specified: true, nodeType: 2,
             nodeName: String(name), nodeValue: v };
  }
  setAttributeNode(attr) {
    if (attr && attr.name !== undefined) this.setAttribute(attr.name, attr.value);
    return null;
  }
  removeAttributeNode(attr) {
    if (attr && attr.name !== undefined) this.removeAttribute(attr.name);
    return attr || null;
  }

  insertAdjacentHTML(position, html) {
    position = String(position).toLowerCase();
    var temp = createTempElement();
    ns.setInnerHTML(temp, html === null || html === undefined ? '' : String(html));
    var kids = ns.children(temp);
    var parent = this.parentNode;
    var i;
    for (i = 0; i < kids.length; i++) scanHandlers(kids[i]);
    if (position === 'beforebegin') {
      if (!parent) return;
      for (i = 0; i < kids.length; i++) parent.insertBefore(kids[i], this);
    } else if (position === 'afterbegin') {
      var ref = this.firstChild;
      for (i = 0; i < kids.length; i++) this.insertBefore(kids[i], ref);
    } else if (position === 'beforeend') {
      for (i = 0; i < kids.length; i++) this.appendChild(kids[i]);
    } else if (position === 'afterend') {
      if (!parent) return;
      var next = this.nextSibling;
      for (i = 0; i < kids.length; i++) parent.insertBefore(kids[i], next);
    }
  }
  insertAdjacentElement(position, el) {
    position = String(position).toLowerCase();
    if (!isNode(el)) throw new TypeError('not a Node');
    if (position === 'beforebegin') { if (this.parentNode) this.parentNode.insertBefore(el, this); }
    else if (position === 'afterbegin') this.insertBefore(el, this.firstChild);
    else if (position === 'beforeend') this.appendChild(el);
    else if (position === 'afterend') { if (this.parentNode) this.parentNode.insertBefore(el, this.nextSibling); }
    return el;
  }
  insertAdjacentText(position, text) {
    this.insertAdjacentElement(position, toNode(String(text)));
  }

  matches(sel) { return matchesSelector(this, sel); }
  webkitMatchesSelector(sel) { return matchesSelector(this, sel); }
  msMatchesSelector(sel) { return matchesSelector(this, sel); }
  closest(sel) {
    var n = this;
    while (n && ns.type(n) === ELEMENT_NODE) {
      if (matchesSelector(n, sel)) return n;
      n = ns.parent(n);
    }
    return null;
  }

  getBoundingClientRect() {
    var r = null;
    try { r = ns.rect(this); } catch (_) { r = null; }
    if (!r) {
      return { x: 0, y: 0, left: 0, top: 0, right: 0, bottom: 0,
               width: 0, height: 0, toJSON: function () { return this; } };
    }
    var x = Number(r[0]) || 0, y = Number(r[1]) || 0;
    var w = Number(r[2]) || 0, h = Number(r[3]) || 0;
    return { x: x, y: y, left: x, top: y, right: x + w, bottom: y + h,
             width: w, height: h, toJSON: function () { return this; } };
  }
  getClientRects() {
    var r = this.getBoundingClientRect();
    if (!r.width && !r.height) return [];
    return [r];
  }
  get offsetWidth() { return this.getBoundingClientRect().width; }
  get offsetHeight() { return this.getBoundingClientRect().height; }
  get offsetTop() { return this.getBoundingClientRect().top; }
  get offsetLeft() { return this.getBoundingClientRect().left; }
  get offsetParent() { return this.parentElement; }
  get clientWidth() { return this.getBoundingClientRect().width; }
  get clientHeight() { return this.getBoundingClientRect().height; }
  get clientTop() { return 0; }
  get clientLeft() { return 0; }
  get scrollWidth() { return this.getBoundingClientRect().width; }
  get scrollHeight() { return this.getBoundingClientRect().height; }
  get scrollTop() { return scrollOf(this).top; }
  set scrollTop(v) { scrollOf(this).top = Number(v) || 0; }
  get scrollLeft() { return scrollOf(this).left; }
  set scrollLeft(v) { scrollOf(this).left = Number(v) || 0; }
  scrollIntoView() {}
  scrollTo() {}
  scrollBy() {}
  focus() {
    uiState.activeElement = this;
    uiState.focused = this;
    var ev = new FocusEvent('focus', { bubbles: false, cancelable: false });
    ev.isTrusted = false;
    dispatchEventOn(this, ev);
  }
  blur() {
    if (uiState.activeElement === this) uiState.activeElement = null;
    if (uiState.focused === this) uiState.focused = null;
    var ev = new FocusEvent('blur', { bubbles: false, cancelable: false });
    ev.isTrusted = false;
    dispatchEventOn(this, ev);
  }
  click() {
    var ev = new MouseEvent('click', { bubbles: true, cancelable: true, button: 0 });
    ev.isTrusted = false;
    var ok = dispatchEventOn(this, ev);
    if (!ok) return;
    var tag = ns.name(this).toUpperCase();
    var type = (ns.getAttr(this, 'type') || '').toLowerCase();
    if (tag === 'A') {
      var href = ns.getAttr(this, 'href');
      if (href !== null && href !== undefined) {
        try { navigateTo(href); } catch (_) {}
      }
      return;
    }
    if (tag === 'INPUT' && (type === 'checkbox' || type === 'radio')) {
      if (type === 'radio' && !this.checked) {
        var name = ns.getAttr(this, 'name');
        var form = this.form;
        if (name && form) {
          var inputs = form.elements;
          for (var i = 0; i < inputs.length; i++) {
            var inp = inputs[i];
            if (inp && inp.nodeType === ELEMENT_NODE &&
                ns.name(inp).toUpperCase() === 'INPUT' &&
                (ns.getAttr(inp, 'type') || '').toLowerCase() === 'radio' &&
                ns.getAttr(inp, 'name') === name) {
              ns.formSet(inp, 'checked', false);
            }
          }
        }
      }
      if (type === 'radio') {
        ns.formSet(this, 'checked', true);
      } else {
        ns.formSet(this, 'checked', !ns.formGet(this, 'checked'));
      }
      var cev = new Event('change', { bubbles: true, cancelable: false });
      dispatchEventOn(this, cev);
      return;
    }
    if ((tag === 'INPUT' && (type === 'submit' || type === 'image')) ||
        (tag === 'BUTTON' && (type === 'submit' || type === ''))) {
      var f = this.form;
      if (f && typeof f.submit === 'function') f.submit();
      return;
    }
  }
  attachShadow(init) {
    /* Shadow DOM aplanado: la raiz es un elemento real dentro del anfitrion,
     * asi NetSurf dibuja su contenido. */
    var sr = ns.createElement('eon-shadow-root');
    ns.insert(this, sr, ns.first(this));
    sr.host = this;
    sr.mode = (init && init.mode) || 'open';
    sr.delegatesFocus = !!(init && init.delegatesFocus);
    sr.adoptedStyleSheets = [];
    shadowRoots.set(this, sr);
    return sr;
  }
  get shadowRoot() {
    var sr = shadowRoots.get(this);
    return sr && sr.mode === 'open' ? sr : null;
  }
  get slot() { return ns.getAttr(this, 'slot') || ''; }
  get part() { return ns.getAttr(this, 'part') || ''; }
  get assignedSlot() { return null; }
  get innerText() { var t = ns.text(this); return t === null || t === undefined ? '' : String(t); }
  set innerText(v) { this.textContent = v; }
}
defineTag(Element.prototype, 'Element');
installMixin(Element.prototype, PARENT_METHODS);
installMixin(Element.prototype, CHILD_METHODS);

/* ---------------------------------------------------------------- DOMTokenList */

class DOMTokenList {
  constructor(el, attr) { this._el = el; this._attr = attr; }
  _list() {
    var v = ns.getAttr(this._el, this._attr) || '';
    return v.split(/\s+/).filter(function (x) { return x.length > 0; });
  }
  _write(list) {
    var v = list.join(' ');
    if (v) setAttrNotify(this._el, this._attr, v);
    else removeAttrNotify(this._el, this._attr);
  }
  get length() { return this._list().length; }
  get value() { return ns.getAttr(this._el, this._attr) || ''; }
  set value(v) {
    if (v === null || v === undefined || v === '') removeAttrNotify(this._el, this._attr);
    else setAttrNotify(this._el, this._attr, String(v));
  }
  item(i) { return this._list()[i] || null; }
  contains(t) { return this._list().indexOf(String(t)) >= 0; }
  add() {
    var l = this._list();
    for (var i = 0; i < arguments.length; i++) {
      var t = String(arguments[i]);
      if (t && l.indexOf(t) < 0) l.push(t);
    }
    this._write(l);
  }
  remove() {
    var l = this._list();
    for (var i = 0; i < arguments.length; i++) {
      var t = String(arguments[i]);
      var k = l.indexOf(t);
      if (k >= 0) l.splice(k, 1);
    }
    this._write(l);
  }
  toggle(t, force) {
    t = String(t);
    var has = this.contains(t);
    if (force === undefined) {
      if (has) this.remove(t); else this.add(t);
      return !has;
    }
    if (force) { if (!has) this.add(t); return true; }
    if (has) this.remove(t);
    return false;
  }
  replace(oldT, newT) {
    oldT = String(oldT); newT = String(newT);
    var l = this._list(), i = l.indexOf(oldT);
    if (i < 0) return false;
    if (l.indexOf(newT) < 0) l[i] = newT; else l.splice(i, 1);
    this._write(l);
    return true;
  }
  supports() { return true; }
  forEach(cb, thisArg) {
    var l = this._list();
    for (var i = 0; i < l.length; i++) cb.call(thisArg, l[i], i, this);
  }
  entries() {
    var l = this._list(), i = 0;
    return { next: function () { return i < l.length ? { value: [i, l[i++]], done: false } : { value: undefined, done: true }; },
             [Symbol.iterator]: function () { return this; } };
  }
  keys() {
    var l = this._list(), i = 0;
    return { next: function () { return i < l.length ? { value: i++, done: false } : { value: undefined, done: true }; },
             [Symbol.iterator]: function () { return this; } };
  }
  values() {
    var l = this._list(), i = 0;
    return { next: function () { return i < l.length ? { value: l[i++], done: false } : { value: undefined, done: true }; },
             [Symbol.iterator]: function () { return this; } };
  }
  [Symbol.iterator]() { return this.values(); }
  toString() { return this.value; }
}
defineTag(DOMTokenList.prototype, 'DOMTokenList');

/* ---------------------------------------------------------------- CSSStyleDeclaration */

function camelToKebab(name) {
  if (name.indexOf('-') >= 0) return name;
  var s = String(name).replace(/([A-Z])/g, '-$1').toLowerCase();
  if (s.indexOf('webkit-') === 0) s = '-' + s;
  else if (s.indexOf('moz-') === 0) s = '-' + s;
  else if (s.indexOf('ms-') === 0) s = '-' + s;
  return s;
}

class CSSStyleDeclaration {
  constructor(el, computed) {
    this._el = el || null;
    this._computed = !!computed;
  }
  _parse() {
    var text = this._el ? (ns.getAttr(this._el, 'style') || '') : '';
    var out = [], parts = text.split(';');
    for (var i = 0; i < parts.length; i++) {
      var p = parts[i];
      var k = p.indexOf(':');
      if (k < 0) continue;
      var name = p.slice(0, k).trim().toLowerCase();
      var value = p.slice(k + 1).trim();
      if (name) out.push([name, value]);
    }
    return out;
  }
  _serialize(list) {
    var out = [];
    for (var i = 0; i < list.length; i++) out.push(list[i][0] + ': ' + list[i][1]);
    return out.join('; ');
  }
  get cssText() {
    if (!this._el) return '';
    return ns.getAttr(this._el, 'style') || '';
  }
  set cssText(v) {
    if (!this._el) return;
    if (v === null || v === undefined || v === '') removeAttrNotify(this._el, 'style');
    else setAttrNotify(this._el, 'style', String(v));
  }
  get length() { return this._parse().length; }
  item(i) { var d = this._parse(); return d[i] ? d[i][0] : ''; }
  getPropertyValue(name) {
    name = String(name).toLowerCase();
    var d = this._parse();
    for (var i = 0; i < d.length; i++) if (d[i][0] === name) return d[i][1];
    return '';
  }
  getPropertyPriority(name) {
    return /!important\s*$/i.test(this.getPropertyValue(name)) ? 'important' : '';
  }
  setProperty(name, value, priority) {
    if (!this._el) return;
    name = String(name).toLowerCase();
    var val = value === null || value === undefined ? '' : String(value);
    if (priority && String(priority).toLowerCase() === 'important') val += ' !important';
    var d = this._parse(), found = false;
    for (var i = 0; i < d.length; i++) {
      if (d[i][0] === name) { d[i][1] = val; found = true; break; }
    }
    if (!found) d.push([name, val]);
    if (name === 'transform') eonTranslateAux(this._el, d, val);
    if (name === 'opacity') eonOpacityAux(d, val);
    setAttrNotify(this._el, 'style', this._serialize(d));
  }
  removeProperty(name) {
    if (!this._el) return '';
    name = String(name).toLowerCase();
    var d = this._parse(), out = '', removed = false;
    for (var i = 0; i < d.length; i++) {
      if (d[i][0] === name) { out = d[i][1]; removed = true; }
    }
    var keep = [];
    for (var j = 0; j < d.length; j++) if (d[j][0] !== name) keep.push(d[j]);
    if (removed) {
      if (keep.length) setAttrNotify(this._el, 'style', this._serialize(keep));
      else removeAttrNotify(this._el, 'style');
    }
    return out;
  }
}
defineTag(CSSStyleDeclaration.prototype, 'CSSStyleDeclaration');

/* NetSurf no dibuja transform: cuando un script mueve algo con translate
 * (carruseles, menus que entran y salen) se agrega el equivalente con
 * position:relative + left/top, o margenes si ya esta posicionado. Las
 * propiedades agregadas se recuerdan para actualizarlas la proxima vez. */
var translateAux = new WeakMap();
function eonTranslateAux(el, d, val) {
  var x = '', y = '', m;
  var re = /(translate3d|translateX|translateY|translate)\(([^)]*)\)/gi;
  while ((m = re.exec(val)) !== null) {
    var a = m[2].split(',').map(function (s) { return s.trim(); });
    var f = m[1].toLowerCase();
    if (f === 'translatex') x = a[0];
    else if (f === 'translatey') y = a[0];
    else { x = a[0] || ''; y = a[1] || ''; }
  }
  var old = translateAux.get(el) || [];
  for (var i = d.length - 1; i >= 0; i--) if (old.indexOf(d[i][0]) >= 0) d.splice(i, 1);
  var zero = function (v) { return !v || parseFloat(v) === 0; };
  var pos = '';
  for (var j = 0; j < d.length; j++) if (d[j][0] === 'position') pos = String(d[j][1]).toLowerCase();
  var added = [];
  if (!zero(x) || !zero(y)) {
    var abs = pos.indexOf('absolute') === 0 || pos.indexOf('fixed') === 0;
    if (!abs && !pos) { d.push(['position', 'relative']); added.push('position'); }
    if (!zero(x)) { var kx = abs ? 'margin-left' : 'left'; d.push([kx, x]); added.push(kx); }
    if (!zero(y)) { var ky = abs ? 'margin-top' : 'top'; d.push([ky, y]); added.push(ky); }
  }
  translateAux.set(el, added);
}

/* sin opacidad en NetSurf: opacity 0 desde JS = visibility hidden */
function eonOpacityAux(d, val) {
  var o = parseFloat(val);
  if (isNaN(o)) return;
  /* la marca no sobrevive al atributo: se asume que un visibility:hidden en
   * linea junto a opacity lo puso esta funcion */
  for (var i = d.length - 1; i >= 0; i--) if (d[i][0] === 'visibility' && /^hidden/.test(String(d[i][1]))) d.splice(i, 1);
  if (o <= 0.05) d.push(['visibility', 'hidden']);
}

function makeStyle(el) {
  var target = new CSSStyleDeclaration(el, false);
  return new Proxy(target, {
    get: function (t, p, r) {
      if (typeof p === 'symbol') return Reflect.get(t, p, r);
      if (p in t) return Reflect.get(t, p, r);
      if (typeof p === 'string') return t.getPropertyValue(camelToKebab(p));
      return undefined;
    },
    set: function (t, p, v, r) {
      if (typeof p === 'symbol') return Reflect.set(t, p, v, r);
      if (p in t) return Reflect.set(t, p, v, r);
      if (typeof p === 'string') { t.setProperty(camelToKebab(p), v); return true; }
      return false;
    },
    has: function (t, p) { return (typeof p === 'string') || Reflect.has(t, p); },
    deleteProperty: function (t, p) {
      if (typeof p === 'string') { t.removeProperty(camelToKebab(p)); return true; }
      return Reflect.deleteProperty(t, p);
    }
  });
}

/* ---------------------------------------------------------------- dataset */

function kebabToCamel(s) {
  return String(s).replace(/-([a-z])/g, function (_, c) { return c.toUpperCase(); });
}
function camelToDataKebab(s) {
  return String(s).replace(/([A-Z])/g, '-$1').toLowerCase();
}
function makeDataset(el) {
  function attrName(p) { return 'data-' + camelToDataKebab(p); }
  return new Proxy({}, {
    get: function (t, p) {
      if (typeof p !== 'string') return undefined;
      var v = ns.getAttr(el, attrName(p));
      return v === null ? undefined : String(v);
    },
    set: function (t, p, v) {
      if (typeof p !== 'string') return false;
      setAttrNotify(el, attrName(p), v === null || v === undefined ? '' : String(v));
      return true;
    },
    deleteProperty: function (t, p) {
      if (typeof p !== 'string') return false;
      removeAttrNotify(el, attrName(p));
      return true;
    },
    has: function (t, p) {
      return typeof p === 'string' && ns.getAttr(el, attrName(p)) !== null;
    },
    ownKeys: function () {
      var names = ns.attrNames(el) || [], out = [];
      for (var i = 0; i < names.length; i++) {
        var n = String(names[i]).toLowerCase();
        if (n.indexOf('data-') === 0) out.push(kebabToCamel(n.slice(5)));
      }
      return out;
    },
    getOwnPropertyDescriptor: function (t, p) {
      if (typeof p !== 'string') return undefined;
      var v = ns.getAttr(el, attrName(p));
      if (v === null) return undefined;
      return { value: String(v), enumerable: true, configurable: true, writable: true };
    }
  });
}

/* ---------------------------------------------------------------- inline handlers */

function processInlineHandlers(el) {
  if (!isNode(el) || ns.type(el) !== ELEMENT_NODE) return;
  var names = ns.attrNames(el) || [];
  for (var i = 0; i < names.length; i++) {
    var name = String(names[i]);
    var low = name.toLowerCase();
    if (low.length > 2 && low.slice(0, 2) === 'on') {
      var type = low.slice(2);
      var code = ns.getAttr(el, name);
      if (code === null || code === undefined) continue;
      var fn = null;
      try { fn = new Function('event', String(code)); }
      catch (e) { log('bad handler ' + name + ': ' + errText(e)); continue; }
      setHandler(el, type, fn);
    }
  }
}

function scanHandlers(root) {
  if (!isNode(root)) return;
  var stack = [root];
  while (stack.length) {
    var n = stack.pop();
    if (ns.type(n) === ELEMENT_NODE) processInlineHandlers(n);
    var kids = ns.children(n);
    for (var i = 0; i < kids.length; i++) stack.push(kids[i]);
  }
}

function createTempElement() {
  var d = getDocument();
  if (!d) throw new TypeError('no document');
  return ns.createElement('div');
}

/* ---------------------------------------------------------------- reflejos */

function reflectString(proto, prop, attr) {
  try {
    Object.defineProperty(proto, prop, {
      configurable: true, enumerable: true,
      get: function () {
        var v = ns.getAttr(this, attr);
        return v === null ? '' : String(v);
      },
      set: function (v) { setAttrNotify(this, attr, v === null ? '' : String(v)); }
    });
  } catch (_) {}
}
function reflectBool(proto, prop, attr) {
  try {
    Object.defineProperty(proto, prop, {
      configurable: true, enumerable: true,
      get: function () { return ns.getAttr(this, attr) !== null; },
      set: function (v) {
        if (v) setAttrNotify(this, attr, '');
        else removeAttrNotify(this, attr);
      }
    });
  } catch (_) {}
}
function reflectNumber(proto, prop, attr, def) {
  try {
    Object.defineProperty(proto, prop, {
      configurable: true, enumerable: true,
      get: function () {
        var v = parseInt(ns.getAttr(this, attr), 10);
        return isNaN(v) ? def : v;
      },
      set: function (v) { setAttrNotify(this, attr, String(parseInt(v, 10) || 0)); }
    });
  } catch (_) {}
}
function reflectURL(proto, prop, attr) {
  try {
    Object.defineProperty(proto, prop, {
      configurable: true, enumerable: true,
      get: function () {
        var v = ns.getAttr(this, attr);
        if (v === null || v === '') return '';
        try { return resolveURL(String(v), currentHref()); } catch (_) { return String(v); }
      },
      set: function (v) { setAttrNotify(this, attr, v === null ? '' : String(v)); }
    });
  } catch (_) {}
}

/* ---------------------------------------------------------------- HTMLElement */

class HTMLElement extends Element {
  constructor() {
    /* "mejora" de un elemento que ya existe (customElements.define / parser) */
    if (ceUpgrading) { var up = ceUpgrading; ceUpgrading = null; return up; }
    /* new MiElemento() o document.createElement('mi-elemento') */
    var def = ceByCtor.get(new.target);
    if (!def) throw new TypeError('Illegal constructor');
    var el = ns.createElement(def.name);
    Object.setPrototypeOf(el, new.target.prototype);
    ceDone.add(el);
    return el;
  }

  get style() {
    var s = styleCache.get(this);
    if (!s) { s = makeStyle(this); styleCache.set(this, s); }
    return s;
  }
  set style(v) { this.setAttribute('style', v === null ? '' : String(v)); }
  get hidden() { return ns.getAttr(this, 'hidden') !== null; }
  set hidden(v) {
    if (v) setAttrNotify(this, 'hidden', '');
    else removeAttrNotify(this, 'hidden');
  }
  get title() { return ns.getAttr(this, 'title') || ''; }
  set title(v) { setAttrNotify(this, 'title', v === null ? '' : String(v)); }
  get lang() { return ns.getAttr(this, 'lang') || ''; }
  set lang(v) { setAttrNotify(this, 'lang', v === null ? '' : String(v)); }
  get dir() { return ns.getAttr(this, 'dir') || ''; }
  set dir(v) { setAttrNotify(this, 'dir', v === null ? '' : String(v)); }
  get tabIndex() {
    var v = parseInt(ns.getAttr(this, 'tabindex'), 10);
    return isNaN(v) ? -1 : v;
  }
  set tabIndex(v) { setAttrNotify(this, 'tabindex', String(parseInt(v, 10) || 0)); }
  get innerText() { var t = ns.text(this); return t === null || t === undefined ? '' : String(t); }
  set innerText(v) { this.textContent = v; }
  get outerText() { var t = ns.text(this); return t === null || t === undefined ? '' : String(t); }
  set outerText(v) { this.textContent = v; }
  get accessKey() { return ns.getAttr(this, 'accesskey') || ''; }
  set accessKey(v) { setAttrNotify(this, 'accesskey', v === null ? '' : String(v)); }
  get draggable() { return ns.getAttr(this, 'draggable') === 'true'; }
  set draggable(v) { setAttrNotify(this, 'draggable', v ? 'true' : 'false'); }
  get spellcheck() { return ns.getAttr(this, 'spellcheck') !== 'false'; }
  set spellcheck(v) { setAttrNotify(this, 'spellcheck', v ? 'true' : 'false'); }
  get contentEditable() { return ns.getAttr(this, 'contenteditable') || 'inherit'; }
  set contentEditable(v) { setAttrNotify(this, 'contenteditable', v === null ? 'inherit' : String(v)); }
  get isContentEditable() { return this.contentEditable === 'true'; }
  get translate() { return ns.getAttr(this, 'translate') !== 'no'; }
  set translate(v) { setAttrNotify(this, 'translate', v ? 'yes' : 'no'); }
  get offsetParent() { return this.parentElement; }
}
defineTag(HTMLElement.prototype, 'HTMLElement');

/* ---------------------------------------------------------------- subclases HTML */

class HTMLAnchorElement extends HTMLElement {
  get href() {
    var v = ns.getAttr(this, 'href');
    if (v === null || v === '') return '';
    try { return resolveURL(String(v), currentHref()); } catch (_) { return String(v); }
  }
  set href(v) { setAttrNotify(this, 'href', v === null ? '' : String(v)); }
  get target() { return ns.getAttr(this, 'target') || ''; }
  set target(v) { setAttrNotify(this, 'target', v === null ? '' : String(v)); }
  get rel() { return ns.getAttr(this, 'rel') || ''; }
  set rel(v) { setAttrNotify(this, 'rel', v === null ? '' : String(v)); }
  get relList() { return new DOMTokenList(this, 'rel'); }
  get download() { return ns.getAttr(this, 'download') || ''; }
  set download(v) { setAttrNotify(this, 'download', v === null ? '' : String(v)); }
  get text() { var t = ns.text(this); return t === null || t === undefined ? '' : String(t); }
  set text(v) { this.textContent = v; }
  get protocol() { return anchorURL(this).protocol; }
  get host() { return anchorURL(this).host; }
  get hostname() { return anchorURL(this).hostname; }
  get port() { return anchorURL(this).port; }
  get pathname() { return anchorURL(this).pathname; }
  get search() { return anchorURL(this).search; }
  get hash() { return anchorURL(this).hash; }
  get origin() { return anchorURL(this).origin; }
  get username() { return anchorURL(this).username; }
  get password() { return anchorURL(this).password; }
  toString() { return this.href; }
}
defineTag(HTMLAnchorElement.prototype, 'HTMLAnchorElement');

function anchorURL(el) {
  var v = ns.getAttr(el, 'href');
  try { return new URL(v === null ? '' : String(v), currentHref()); }
  catch (_) { try { return new URL(currentHref()); } catch (_2) { return new URL('about:blank'); } }
}

class HTMLImageElement extends HTMLElement {
  get src() {
    var v = ns.getAttr(this, 'src');
    if (v === null || v === '') return '';
    try { return resolveURL(String(v), currentHref()); } catch (_) { return String(v); }
  }
  set src(v) { setAttrNotify(this, 'src', v === null ? '' : String(v)); }
  get currentSrc() { return this.src; }
  get alt() { return ns.getAttr(this, 'alt') || ''; }
  set alt(v) { setAttrNotify(this, 'alt', v === null ? '' : String(v)); }
  get srcset() { return ns.getAttr(this, 'srcset') || ''; }
  set srcset(v) { setAttrNotify(this, 'srcset', v === null ? '' : String(v)); }
  get sizes() { return ns.getAttr(this, 'sizes') || ''; }
  set sizes(v) { setAttrNotify(this, 'sizes', v === null ? '' : String(v)); }
  get crossOrigin() { return ns.getAttr(this, 'crossorigin'); }
  set crossOrigin(v) { setAttrNotify(this, 'crossorigin', v === null ? '' : String(v)); }
  get useMap() { return ns.getAttr(this, 'usemap') || ''; }
  set useMap(v) { setAttrNotify(this, 'usemap', v === null ? '' : String(v)); }
  get isMap() { return ns.getAttr(this, 'ismap') !== null; }
  set isMap(v) { if (v) setAttrNotify(this, 'ismap', ''); else removeAttrNotify(this, 'ismap'); }
  get loading() { return ns.getAttr(this, 'loading') || 'eager'; }
  set loading(v) { setAttrNotify(this, 'loading', v === null ? 'eager' : String(v)); }
  get complete() { return true; }
  get naturalWidth() {
    var w = parseInt(ns.getAttr(this, 'width'), 10);
    if (!isNaN(w)) return w;
    var r = this.getBoundingClientRect();
    return Math.round(r.width);
  }
  get naturalHeight() {
    var h = parseInt(ns.getAttr(this, 'height'), 10);
    if (!isNaN(h)) return h;
    var r = this.getBoundingClientRect();
    return Math.round(r.height);
  }
  get width() { return this.naturalWidth; }
  set width(v) { setAttrNotify(this, 'width', String(parseInt(v, 10) || 0)); }
  get height() { return this.naturalHeight; }
  set height(v) { setAttrNotify(this, 'height', String(parseInt(v, 10) || 0)); }
  decode() { return Promise.resolve(); }
}
defineTag(HTMLImageElement.prototype, 'HTMLImageElement');

class HTMLInputElement extends HTMLElement {
  get value() {
    var v = ns.formGet(this, 'value');
    return v === null || v === undefined ? '' : String(v);
  }
  set value(v) {
    ns.formSet(this, 'value', v === null ? '' : String(v));
    notifyMutation(this, { type: 'attributes', attributeName: 'value', oldValue: null });
  }
  get defaultValue() { return ns.getAttr(this, 'value') || ''; }
  set defaultValue(v) { setAttrNotify(this, 'value', v === null ? '' : String(v)); }
  get checked() { return !!ns.formGet(this, 'checked'); }
  set checked(v) {
    ns.formSet(this, 'checked', !!v);
    notifyMutation(this, { type: 'attributes', attributeName: 'checked', oldValue: null });
  }
  get defaultChecked() { return ns.getAttr(this, 'checked') !== null; }
  set defaultChecked(v) {
    if (v) setAttrNotify(this, 'checked', '');
    else removeAttrNotify(this, 'checked');
  }
  get type() { return (ns.getAttr(this, 'type') || 'text').toLowerCase(); }
  set type(v) { setAttrNotify(this, 'type', v === null ? 'text' : String(v)); }
  get name() { return ns.getAttr(this, 'name') || ''; }
  set name(v) { setAttrNotify(this, 'name', v === null ? '' : String(v)); }
  get placeholder() { return ns.getAttr(this, 'placeholder') || ''; }
  set placeholder(v) { setAttrNotify(this, 'placeholder', v === null ? '' : String(v)); }
  get accept() { return ns.getAttr(this, 'accept') || ''; }
  set accept(v) { setAttrNotify(this, 'accept', v === null ? '' : String(v)); }
  get autocomplete() { return ns.getAttr(this, 'autocomplete') || ''; }
  set autocomplete(v) { setAttrNotify(this, 'autocomplete', v === null ? '' : String(v)); }
  get pattern() { return ns.getAttr(this, 'pattern') || ''; }
  set pattern(v) { setAttrNotify(this, 'pattern', v === null ? '' : String(v)); }
  get min() { return ns.getAttr(this, 'min') || ''; }
  set min(v) { setAttrNotify(this, 'min', v === null ? '' : String(v)); }
  get max() { return ns.getAttr(this, 'max') || ''; }
  set max(v) { setAttrNotify(this, 'max', v === null ? '' : String(v)); }
  get step() { return ns.getAttr(this, 'step') || ''; }
  set step(v) { setAttrNotify(this, 'step', v === null ? '' : String(v)); }
  get form() { return closestForm(this); }
  get formAction() {
    var v = ns.getAttr(this, 'formaction');
    if (v === null) return '';
    try { return resolveURL(String(v), currentHref()); } catch (_) { return String(v); }
  }
  get formMethod() { return (ns.getAttr(this, 'formmethod') || 'get').toLowerCase(); }
  get formNoValidate() { return ns.getAttr(this, 'formnovalidate') !== null; }
  get files() { return null; }
  get valueAsNumber() { var n = parseFloat(this.value); return isNaN(n) ? NaN : n; }
  set valueAsNumber(v) { this.value = String(v); }
  get selectionStart() { return 0; }
  set selectionStart(v) {}
  get selectionEnd() { return 0; }
  set selectionEnd(v) {}
  select() {}
  setSelectionRange() {}
  setRangeText() {}
  checkValidity() { return true; }
  reportValidity() { return true; }
  setCustomValidity() {}
}
defineTag(HTMLInputElement.prototype, 'HTMLInputElement');
reflectBool(HTMLInputElement.prototype, 'disabled', 'disabled');
reflectBool(HTMLInputElement.prototype, 'readOnly', 'readonly');
reflectBool(HTMLInputElement.prototype, 'required', 'required');
reflectBool(HTMLInputElement.prototype, 'multiple', 'multiple');
reflectBool(HTMLInputElement.prototype, 'autofocus', 'autofocus');
reflectNumber(HTMLInputElement.prototype, 'maxLength', 'maxlength', 524288);
reflectNumber(HTMLInputElement.prototype, 'minLength', 'minlength', -1);
reflectNumber(HTMLInputElement.prototype, 'size', 'size', 20);

class HTMLTextAreaElement extends HTMLElement {
  get value() {
    var v = ns.formGet(this, 'value');
    return v === null || v === undefined ? '' : String(v);
  }
  set value(v) {
    ns.formSet(this, 'value', v === null ? '' : String(v));
    notifyMutation(this, { type: 'attributes', attributeName: 'value', oldValue: null });
  }
  get defaultValue() { return ns.text(this); }
  set defaultValue(v) { this.textContent = v; }
  get textLength() { return this.value.length; }
  get type() { return 'textarea'; }
  get name() { return ns.getAttr(this, 'name') || ''; }
  set name(v) { setAttrNotify(this, 'name', v === null ? '' : String(v)); }
  get placeholder() { return ns.getAttr(this, 'placeholder') || ''; }
  set placeholder(v) { setAttrNotify(this, 'placeholder', v === null ? '' : String(v)); }
  get form() { return closestForm(this); }
  get rows() { var v = parseInt(ns.getAttr(this, 'rows'), 10); return isNaN(v) ? 2 : v; }
  set rows(v) { setAttrNotify(this, 'rows', String(parseInt(v, 10) || 2)); }
  get cols() { var v = parseInt(ns.getAttr(this, 'cols'), 10); return isNaN(v) ? 20 : v; }
  set cols(v) { setAttrNotify(this, 'cols', String(parseInt(v, 10) || 20)); }
  select() {}
  setSelectionRange() {}
  checkValidity() { return true; }
  reportValidity() { return true; }
  setCustomValidity() {}
}
defineTag(HTMLTextAreaElement.prototype, 'HTMLTextAreaElement');
reflectBool(HTMLTextAreaElement.prototype, 'disabled', 'disabled');
reflectBool(HTMLTextAreaElement.prototype, 'readOnly', 'readonly');
reflectBool(HTMLTextAreaElement.prototype, 'required', 'required');
reflectBool(HTMLTextAreaElement.prototype, 'autofocus', 'autofocus');
reflectNumber(HTMLTextAreaElement.prototype, 'maxLength', 'maxlength', 524288);
reflectNumber(HTMLTextAreaElement.prototype, 'minLength', 'minlength', -1);

function closestForm(el) {
  var n = el;
  while (n && ns.type(n) === ELEMENT_NODE) {
    if (ns.name(n).toUpperCase() === 'FORM') return n;
    n = ns.parent(n);
  }
  var id = ns.getAttr(el, 'form');
  if (id) {
    var d = getDocument();
    var f = d ? d.getElementById(id) : null;
    if (f && ns.name(f).toUpperCase() === 'FORM') return f;
  }
  return null;
}
function formControls(form) {
  var all = ns.byTag(form, '*') || [], out = [];
  for (var i = 0; i < all.length; i++) {
    var t = ns.name(all[i]).toUpperCase();
    if (t === 'INPUT' || t === 'SELECT' || t === 'TEXTAREA' ||
        t === 'BUTTON' || t === 'FIELDSET' || t === 'OUTPUT' || t === 'OBJECT') {
      out.push(all[i]);
    }
  }
  return out;
}
function urlEncode(s) {
  return encodeURIComponent(String(s)).replace(/%20/g, '+');
}
function gatherFormPairs(form) {
  var controls = formControls(form), out = [];
  for (var i = 0; i < controls.length; i++) {
    var el = controls[i];
    var tag = ns.name(el).toUpperCase();
    var name = ns.getAttr(el, 'name');
    if (!name) continue;
    if (ns.getAttr(el, 'disabled') !== null) continue;
    if (tag === 'INPUT') {
      var type = (ns.getAttr(el, 'type') || 'text').toLowerCase();
      if (type === 'submit' || type === 'button' || type === 'image' ||
          type === 'reset' || type === 'file') continue;
      if (type === 'checkbox' || type === 'radio') {
        if (!ns.formGet(el, 'checked')) continue;
        out.push([name, ns.getAttr(el, 'value') === null ? 'on' : String(ns.getAttr(el, 'value'))]);
      } else {
        out.push([name, String(ns.formGet(el, 'value') || '')]);
      }
    } else if (tag === 'TEXTAREA') {
      out.push([name, String(ns.formGet(el, 'value') || '')]);
    } else if (tag === 'SELECT') {
      out.push([name, String(ns.formGet(el, 'value') || '')]);
    }
  }
  return out;
}
function resetForm(form) {
  var controls = formControls(form);
  for (var i = 0; i < controls.length; i++) {
    var el = controls[i];
    var tag = ns.name(el).toUpperCase();
    if (tag === 'INPUT') {
      var type = (ns.getAttr(el, 'type') || 'text').toLowerCase();
      if (type === 'checkbox' || type === 'radio') {
        ns.formSet(el, 'checked', ns.getAttr(el, 'checked') !== null);
      } else {
        ns.formSet(el, 'value', ns.getAttr(el, 'value') === null ? '' : String(ns.getAttr(el, 'value')));
      }
    } else if (tag === 'TEXTAREA') {
      ns.formSet(el, 'value', ns.text(el));
    } else if (tag === 'SELECT') {
      var opts = ns.byTag(el, 'option') || [];
      var chosen = '';
      for (var j = 0; j < opts.length; j++) {
        if (ns.getAttr(opts[j], 'selected') !== null) {
          var v = ns.getAttr(opts[j], 'value');
          chosen = v === null ? (ns.text(opts[j]) || '') : String(v);
          break;
        }
      }
      ns.formSet(el, 'value', chosen);
    }
  }
}

class HTMLSelectElement extends HTMLElement {
  get value() {
    var v = ns.formGet(this, 'value');
    return v === null || v === undefined ? '' : String(v);
  }
  set value(v) { ns.formSet(this, 'value', v === null ? '' : String(v)); }
  get selectedIndex() {
    var i = ns.formGet(this, 'selectedIndex');
    return typeof i === 'number' ? i : -1;
  }
  set selectedIndex(i) {
    i = Number(i);
    if (isNaN(i)) i = 0;
    var opts = this.options;
    if (i < 0 || i >= opts.length) { this.value = ''; return; }
    var opt = opts[i];
    var v = ns.getAttr(opt, 'value');
    this.value = v === null ? String(ns.text(opt) || '') : String(v);
  }
  get options() {
    var self = this;
    return liveCollection(function () { return ns.byTag(self, 'option') || []; });
  }
  get selectedOptions() {
    var self = this;
    return liveCollection(function () {
      var opts = ns.byTag(self, 'option') || [], out = [];
      for (var i = 0; i < opts.length; i++) if (ns.formGet(opts[i], 'selected')) out.push(opts[i]);
      return out;
    });
  }
  get length() { return this.options.length; }
  get type() { return ns.getAttr(this, 'multiple') !== null ? 'select-multiple' : 'select-one'; }
  get name() { return ns.getAttr(this, 'name') || ''; }
  set name(v) { setAttrNotify(this, 'name', v === null ? '' : String(v)); }
  get form() { return closestForm(this); }
  item(i) { return this.options.item(i); }
  namedItem(n) { return this.options.namedItem(n); }
  add(el, before) {
    if (before === undefined || before === null) this.appendChild(el);
    else this.insertBefore(el, before);
  }
  remove(index) {
    if (index === undefined) { if (this.parentNode) this.parentNode.removeChild(this); return; }
    var o = this.options[index];
    if (o && o.parentNode) o.parentNode.removeChild(o);
  }
  checkValidity() { return true; }
  reportValidity() { return true; }
  setCustomValidity() {}
}
defineTag(HTMLSelectElement.prototype, 'HTMLSelectElement');
reflectBool(HTMLSelectElement.prototype, 'disabled', 'disabled');
reflectBool(HTMLSelectElement.prototype, 'multiple', 'multiple');
reflectBool(HTMLSelectElement.prototype, 'required', 'required');
reflectBool(HTMLSelectElement.prototype, 'autofocus', 'autofocus');
reflectNumber(HTMLSelectElement.prototype, 'size', 'size', 0);

class HTMLOptionElement extends HTMLElement {
  get value() {
    var v = ns.getAttr(this, 'value');
    if (v === null) { var t = ns.text(this); return t === null || t === undefined ? '' : String(t); }
    return String(v);
  }
  set value(v) { setAttrNotify(this, 'value', v === null ? '' : String(v)); }
  get text() {
    var t = ns.text(this);
    return (t === null || t === undefined ? '' : String(t)).replace(/\s+/g, ' ').trim();
  }
  set text(v) { this.textContent = v; }
  get label() { var v = ns.getAttr(this, 'label'); return v === null ? this.text : String(v); }
  set label(v) { setAttrNotify(this, 'label', v === null ? '' : String(v)); }
  get selected() { return !!ns.formGet(this, 'selected'); }
  set selected(v) {
    var sel = this.parentNode;
    while (sel && ns.name(sel).toUpperCase() !== 'SELECT') sel = ns.parent(sel);
    if (!v) { removeAttrNotify(this, 'selected'); return; }
    if (sel && ns.getAttr(sel, 'multiple') === null) {
      var opts = ns.byTag(sel, 'option') || [];
      for (var i = 0; i < opts.length; i++) {
        if (opts[i] !== this) removeAttrNotify(opts[i], 'selected');
      }
    }
    setAttrNotify(this, 'selected', '');
    if (sel) {
      var val = ns.getAttr(this, 'value');
      ns.formSet(sel, 'value', val === null ? String(ns.text(this) || '') : String(val));
    }
  }
  get defaultSelected() { return ns.getAttr(this, 'selected') !== null; }
  set defaultSelected(v) {
    if (v) setAttrNotify(this, 'selected', '');
    else removeAttrNotify(this, 'selected');
  }
  get index() {
    var sel = this.parentNode;
    while (sel && ns.name(sel).toUpperCase() !== 'SELECT') sel = ns.parent(sel);
    if (!sel) return 0;
    var opts = ns.byTag(sel, 'option') || [];
    return opts.indexOf(this);
  }
  get form() { return closestForm(this); }
  get disabled() { return ns.getAttr(this, 'disabled') !== null; }
  set disabled(v) { if (v) setAttrNotify(this, 'disabled', ''); else removeAttrNotify(this, 'disabled'); }
}
defineTag(HTMLOptionElement.prototype, 'HTMLOptionElement');

class HTMLFormElement extends HTMLElement {
  get elements() {
    var self = this;
    return liveCollection(function () { return formControls(self); });
  }
  get length() { return formControls(this).length; }
  get action() {
    var v = ns.getAttr(this, 'action');
    if (v === null || v === '') return currentHref();
    try { return resolveURL(String(v), currentHref()); } catch (_) { return String(v); }
  }
  set action(v) { setAttrNotify(this, 'action', v === null ? '' : String(v)); }
  get method() { return (ns.getAttr(this, 'method') || 'get').toLowerCase(); }
  set method(v) { setAttrNotify(this, 'method', v === null ? 'get' : String(v)); }
  get name() { return ns.getAttr(this, 'name') || ''; }
  set name(v) { setAttrNotify(this, 'name', v === null ? '' : String(v)); }
  get target() { return ns.getAttr(this, 'target') || ''; }
  set target(v) { setAttrNotify(this, 'target', v === null ? '' : String(v)); }
  get enctype() { return (ns.getAttr(this, 'enctype') || 'application/x-www-form-urlencoded').toLowerCase(); }
  set enctype(v) { setAttrNotify(this, 'enctype', v === null ? '' : String(v)); }
  get noValidate() { return ns.getAttr(this, 'novalidate') !== null; }
  set noValidate(v) { if (v) setAttrNotify(this, 'novalidate', ''); else removeAttrNotify(this, 'novalidate'); }
  submit() {
    var pairs = gatherFormPairs(this);
    var q = '';
    for (var i = 0; i < pairs.length; i++) {
      if (i) q += '&';
      q += urlEncode(pairs[i][0]) + '=' + urlEncode(pairs[i][1]);
    }
    var action = this.action || currentHref();
    if (q) action += (action.indexOf('?') < 0 ? '?' : '&') + q;
    navigateTo(action);
  }
  requestSubmit() { this.submit(); }
  reset() { resetForm(this); }
  checkValidity() { return true; }
  reportValidity() { return true; }
}
defineTag(HTMLFormElement.prototype, 'HTMLFormElement');
reflectString(HTMLFormElement.prototype, 'autocomplete', 'autocomplete');

class HTMLButtonElement extends HTMLElement {
  get type() { return (ns.getAttr(this, 'type') || 'submit').toLowerCase(); }
  set type(v) { setAttrNotify(this, 'type', v === null ? 'submit' : String(v)); }
  get name() { return ns.getAttr(this, 'name') || ''; }
  set name(v) { setAttrNotify(this, 'name', v === null ? '' : String(v)); }
  get value() { return ns.getAttr(this, 'value') || ''; }
  set value(v) { setAttrNotify(this, 'value', v === null ? '' : String(v)); }
  get form() { return closestForm(this); }
  get formAction() {
    var v = ns.getAttr(this, 'formaction');
    if (v === null) return '';
    try { return resolveURL(String(v), currentHref()); } catch (_) { return String(v); }
  }
  get formMethod() { return (ns.getAttr(this, 'formmethod') || 'get').toLowerCase(); }
  get formNoValidate() { return ns.getAttr(this, 'formnovalidate') !== null; }
  checkValidity() { return true; }
  reportValidity() { return true; }
  setCustomValidity() {}
}
defineTag(HTMLButtonElement.prototype, 'HTMLButtonElement');
reflectBool(HTMLButtonElement.prototype, 'disabled', 'disabled');
reflectBool(HTMLButtonElement.prototype, 'autofocus', 'autofocus');

class HTMLScriptElement extends HTMLElement {
  get src() {
    var v = ns.getAttr(this, 'src');
    if (v === null || v === '') return '';
    try { return resolveURL(String(v), currentHref()); } catch (_) { return String(v); }
  }
  set src(v) { setAttrNotify(this, 'src', v === null ? '' : String(v)); }
  get type() { return ns.getAttr(this, 'type') || ''; }
  set type(v) { setAttrNotify(this, 'type', v === null ? '' : String(v)); }
  get text() { var t = ns.text(this); return t === null || t === undefined ? '' : String(t); }
  set text(v) { this.textContent = v; }
  get charset() { return ns.getAttr(this, 'charset') || ''; }
  set charset(v) { setAttrNotify(this, 'charset', v === null ? '' : String(v)); }
  get crossOrigin() { return ns.getAttr(this, 'crossorigin'); }
  set crossOrigin(v) { setAttrNotify(this, 'crossorigin', v === null ? '' : String(v)); }
  get noModule() { return ns.getAttr(this, 'nomodule') !== null; }
  set noModule(v) { if (v) setAttrNotify(this, 'nomodule', ''); else removeAttrNotify(this, 'nomodule'); }
}
defineTag(HTMLScriptElement.prototype, 'HTMLScriptElement');
reflectBool(HTMLScriptElement.prototype, 'async', 'async');
reflectBool(HTMLScriptElement.prototype, 'defer', 'defer');

class HTMLStyleElement extends HTMLElement {
  get type() { return ns.getAttr(this, 'type') || 'text/css'; }
  set type(v) { setAttrNotify(this, 'type', v === null ? '' : String(v)); }
  get media() { return ns.getAttr(this, 'media') || ''; }
  set media(v) { setAttrNotify(this, 'media', v === null ? '' : String(v)); }
  get sheet() { return null; }
  get text() { var t = ns.text(this); return t === null || t === undefined ? '' : String(t); }
  set text(v) { this.textContent = v; }
}
defineTag(HTMLStyleElement.prototype, 'HTMLStyleElement');

class HTMLLinkElement extends HTMLElement {
  get href() {
    var v = ns.getAttr(this, 'href');
    if (v === null || v === '') return '';
    try { return resolveURL(String(v), currentHref()); } catch (_) { return String(v); }
  }
  set href(v) { setAttrNotify(this, 'href', v === null ? '' : String(v)); }
  get rel() { return ns.getAttr(this, 'rel') || ''; }
  set rel(v) { setAttrNotify(this, 'rel', v === null ? '' : String(v)); }
  get relList() { return new DOMTokenList(this, 'rel'); }
  get type() { return ns.getAttr(this, 'type') || ''; }
  set type(v) { setAttrNotify(this, 'type', v === null ? '' : String(v)); }
  get media() { return ns.getAttr(this, 'media') || ''; }
  set media(v) { setAttrNotify(this, 'media', v === null ? '' : String(v)); }
  get as() { return ns.getAttr(this, 'as') || ''; }
  set as(v) { setAttrNotify(this, 'as', v === null ? '' : String(v)); }
  get crossOrigin() { return ns.getAttr(this, 'crossorigin'); }
  set crossOrigin(v) { setAttrNotify(this, 'crossorigin', v === null ? '' : String(v)); }
  get integrity() { return ns.getAttr(this, 'integrity') || ''; }
  set integrity(v) { setAttrNotify(this, 'integrity', v === null ? '' : String(v)); }
  get sheet() { return null; }
}
defineTag(HTMLLinkElement.prototype, 'HTMLLinkElement');

class HTMLIFrameElement extends HTMLElement {
  get src() {
    var v = ns.getAttr(this, 'src');
    if (v === null || v === '') return '';
    try { return resolveURL(String(v), currentHref()); } catch (_) { return String(v); }
  }
  set src(v) { setAttrNotify(this, 'src', v === null ? '' : String(v)); }
  get name() { return ns.getAttr(this, 'name') || ''; }
  set name(v) { setAttrNotify(this, 'name', v === null ? '' : String(v)); }
  get width() { return ns.getAttr(this, 'width') || ''; }
  set width(v) { setAttrNotify(this, 'width', v === null ? '' : String(v)); }
  get height() { return ns.getAttr(this, 'height') || ''; }
  set height(v) { setAttrNotify(this, 'height', v === null ? '' : String(v)); }
  get allow() { return ns.getAttr(this, 'allow') || ''; }
  set allow(v) { setAttrNotify(this, 'allow', v === null ? '' : String(v)); }
  get sandbox() { return new DOMTokenList(this, 'sandbox'); }
  get srcdoc() { return ns.getAttr(this, 'srcdoc') || ''; }
  set srcdoc(v) { setAttrNotify(this, 'srcdoc', v === null ? '' : String(v)); }
  get loading() { return ns.getAttr(this, 'loading') || 'eager'; }
  set loading(v) { setAttrNotify(this, 'loading', v === null ? 'eager' : String(v)); }
  get contentWindow() { return G; }
  get contentDocument() { return null; }
}
defineTag(HTMLIFrameElement.prototype, 'HTMLIFrameElement');

class HTMLCanvasElement extends HTMLElement {
  get width() { var v = parseInt(ns.getAttr(this, 'width'), 10); return isNaN(v) ? 300 : v; }
  set width(v) { setAttrNotify(this, 'width', String(parseInt(v, 10) || 0)); }
  get height() { var v = parseInt(ns.getAttr(this, 'height'), 10); return isNaN(v) ? 150 : v; }
  set height(v) { setAttrNotify(this, 'height', String(parseInt(v, 10) || 0)); }
  getContext() { return null; }
  toDataURL() { return 'data:,'; }
  toBlob(cb) { if (typeof cb === 'function') setTimeout(function () { cb(null); }, 0); }
}
defineTag(HTMLCanvasElement.prototype, 'HTMLCanvasElement');

class HTMLMediaElement extends HTMLElement {
  get src() {
    var v = ns.getAttr(this, 'src');
    if (v === null || v === '') return '';
    try { return resolveURL(String(v), currentHref()); } catch (_) { return String(v); }
  }
  set src(v) { setAttrNotify(this, 'src', v === null ? '' : String(v)); }
  get currentSrc() { return this.src; }
  get currentTime() { return 0; }
  set currentTime(v) {}
  get duration() { return NaN; }
  get paused() { return true; }
  get ended() { return false; }
  get muted() { return ns.getAttr(this, 'muted') !== null; }
  set muted(v) { if (v) setAttrNotify(this, 'muted', ''); else removeAttrNotify(this, 'muted'); }
  get volume() { return 1; }
  set volume(v) {}
  get playbackRate() { return 1; }
  set playbackRate(v) {}
  get readyState() { return 0; }
  get networkState() { return 0; }
  get error() { return null; }
  get buffered() { return { length: 0 }; }
  get played() { return { length: 0 }; }
  get seekable() { return { length: 0 }; }
  get defaultMuted() { return this.muted; }
  set defaultMuted(v) { this.muted = v; }
  play() {
    return Promise.reject(new DOMException('play() no soportado', 'NotSupportedError'));
  }
  pause() {}
  load() {}
  canPlayType() { return ''; }
  addTextTrack() { return null; }
}
defineTag(HTMLMediaElement.prototype, 'HTMLMediaElement');
reflectBool(HTMLMediaElement.prototype, 'autoplay', 'autoplay');
reflectBool(HTMLMediaElement.prototype, 'loop', 'loop');
reflectBool(HTMLMediaElement.prototype, 'controls', 'controls');

reflectString(HTMLMediaElement.prototype, 'preload', 'preload');

class HTMLVideoElement extends HTMLMediaElement {
  get videoWidth() { return 0; }
  get videoHeight() { return 0; }
  get poster() {
    var v = ns.getAttr(this, 'poster');
    if (v === null || v === '') return '';
    try { return resolveURL(String(v), currentHref()); } catch (_) { return String(v); }
  }
  set poster(v) { setAttrNotify(this, 'poster', v === null ? '' : String(v)); }
}
defineTag(HTMLVideoElement.prototype, 'HTMLVideoElement');

class HTMLAudioElement extends HTMLMediaElement {}
defineTag(HTMLAudioElement.prototype, 'HTMLAudioElement');

var templateContent = new WeakMap();
class HTMLTemplateElement extends HTMLElement {
  get content() {
    /* como en el estandar, los hijos de <template> viven en su fragmento:
     * los que aparezcan (p. ej. por innerHTML) se mudan ahi */
    var f = templateContent.get(this);
    if (!f) { f = ns.createFragment(); templateContent.set(this, f); }
    var kids = ns.children(this);
    for (var i = 0; i < kids.length; i++) ns.append(f, kids[i]);
    return f;
  }
}
defineTag(HTMLTemplateElement.prototype, 'HTMLTemplateElement');

class HTMLBodyElement extends HTMLElement {}
defineTag(HTMLBodyElement.prototype, 'HTMLBodyElement');
class HTMLHtmlElement extends HTMLElement {}
defineTag(HTMLHtmlElement.prototype, 'HTMLHtmlElement');
class HTMLHeadElement extends HTMLElement {}
defineTag(HTMLHeadElement.prototype, 'HTMLHeadElement');
class HTMLTitleElement extends HTMLElement {
  get text() { var t = ns.text(this); return t === null || t === undefined ? '' : String(t); }
  set text(v) { this.textContent = v; }
}
defineTag(HTMLTitleElement.prototype, 'HTMLTitleElement');
class HTMLDivElement extends HTMLElement {}
defineTag(HTMLDivElement.prototype, 'HTMLDivElement');
reflectString(HTMLDivElement.prototype, 'align', 'align');
class HTMLSpanElement extends HTMLElement {}
defineTag(HTMLSpanElement.prototype, 'HTMLSpanElement');
class HTMLParagraphElement extends HTMLElement {}
defineTag(HTMLParagraphElement.prototype, 'HTMLParagraphElement');
reflectString(HTMLParagraphElement.prototype, 'align', 'align');

class HTMLUListElement extends HTMLElement {}
defineTag(HTMLUListElement.prototype, 'HTMLUListElement');
reflectString(HTMLUListElement.prototype, 'type', 'type');
class HTMLOListElement extends HTMLElement {}
defineTag(HTMLOListElement.prototype, 'HTMLOListElement');
reflectString(HTMLOListElement.prototype, 'type', 'type');
reflectNumber(HTMLOListElement.prototype, 'start', 'start', 1);
reflectBool(HTMLOListElement.prototype, 'reversed', 'reversed');
class HTMLLIElement extends HTMLElement {}
defineTag(HTMLLIElement.prototype, 'HTMLLIElement');
reflectNumber(HTMLLIElement.prototype, 'value', 'value', 0);

class HTMLTableElement extends HTMLElement {
  get rows() {
    var self = this;
    return liveCollection(function () { return ns.byTag(self, 'tr') || []; });
  }
  get tBodies() {
    var self = this;
    return liveCollection(function () { return ns.byTag(self, 'tbody') || []; });
  }
  get caption() { return (ns.byTag(this, 'caption') || [])[0] || null; }
  insertRow(index) {
    var d = getDocument();
    var tr = d.createElement('tr');
    var rows = this.rows;
    if (index === undefined || index >= rows.length) this.appendChild(tr);
    else this.insertBefore(tr, rows[index]);
    return tr;
  }
  deleteRow(index) {
    var rows = this.rows;
    var r = rows[index === undefined ? rows.length - 1 : index];
    if (r && r.parentNode) r.parentNode.removeChild(r);
  }
}
defineTag(HTMLTableElement.prototype, 'HTMLTableElement');

class HTMLTableRowElement extends HTMLElement {
  get cells() {
    var self = this;
    return liveCollection(function () {
      var kids = ns.children(self), out = [];
      for (var i = 0; i < kids.length; i++) {
        var t = ns.name(kids[i]).toUpperCase();
        if (t === 'TD' || t === 'TH') out.push(kids[i]);
      }
      return out;
    });
  }
  get rowIndex() {
    var table = this.closest('table');
    if (!table) return -1;
    var rows = table.rows;
    for (var i = 0; i < rows.length; i++) if (rows[i] === this) return i;
    return -1;
  }
  get sectionRowIndex() { return this.rowIndex; }
  insertCell(index) {
    var d = getDocument();
    var td = d.createElement('td');
    var cells = this.cells;
    if (index === undefined || index >= cells.length) this.appendChild(td);
    else this.insertBefore(td, cells[index]);
    return td;
  }
  deleteCell(index) {
    var cells = this.cells;
    var c = cells[index === undefined ? cells.length - 1 : index];
    if (c && c.parentNode) c.parentNode.removeChild(c);
  }
}
defineTag(HTMLTableRowElement.prototype, 'HTMLTableRowElement');

class HTMLTableCellElement extends HTMLElement {
  get cellIndex() {
    var p = this.parentNode;
    if (!p) return -1;
    var kids = ns.children(p), n = 0;
    for (var i = 0; i < kids.length; i++) {
      var t = ns.name(kids[i]).toUpperCase();
      if (t === 'TD' || t === 'TH') {
        if (kids[i] === this) return n;
        n++;
      }
    }
    return -1;
  }
  get headers() { return ns.getAttr(this, 'headers') || ''; }
  set headers(v) { setAttrNotify(this, 'headers', v === null ? '' : String(v)); }
  get abbr() { return ns.getAttr(this, 'abbr') || ''; }
  set abbr(v) { setAttrNotify(this, 'abbr', v === null ? '' : String(v)); }
  get scope() { return ns.getAttr(this, 'scope') || ''; }
  set scope(v) { setAttrNotify(this, 'scope', v === null ? '' : String(v)); }
}
defineTag(HTMLTableCellElement.prototype, 'HTMLTableCellElement');
reflectNumber(HTMLTableCellElement.prototype, 'colSpan', 'colspan', 1);
reflectNumber(HTMLTableCellElement.prototype, 'rowSpan', 'rowspan', 1);
reflectString(HTMLTableCellElement.prototype, 'align', 'align');
reflectString(HTMLTableCellElement.prototype, 'vAlign', 'valign');

class HTMLLabelElement extends HTMLElement {
  get htmlFor() { return ns.getAttr(this, 'for') || ''; }
  set htmlFor(v) { setAttrNotify(this, 'for', v === null ? '' : String(v)); }
  get control() {
    var f = this.htmlFor;
    if (f) {
      var d = getDocument();
      var el = d ? d.getElementById(f) : null;
      if (el) return el;
    }
    var all = ns.byTag(this, '*') || [];
    for (var i = 0; i < all.length; i++) {
      var t = ns.name(all[i]).toUpperCase();
      if (t === 'INPUT' || t === 'SELECT' || t === 'TEXTAREA' || t === 'BUTTON') return all[i];
    }
    return null;
  }
  get form() { return closestForm(this); }
}
defineTag(HTMLLabelElement.prototype, 'HTMLLabelElement');

class HTMLMetaElement extends HTMLElement {
  get name() { return ns.getAttr(this, 'name') || ''; }
  set name(v) { setAttrNotify(this, 'name', v === null ? '' : String(v)); }
  get content() { return ns.getAttr(this, 'content') || ''; }
  set content(v) { setAttrNotify(this, 'content', v === null ? '' : String(v)); }
  get httpEquiv() { return ns.getAttr(this, 'http-equiv') || ''; }
  set httpEquiv(v) { setAttrNotify(this, 'http-equiv', v === null ? '' : String(v)); }
  get charset() { return ns.getAttr(this, 'charset') || ''; }
  set charset(v) { setAttrNotify(this, 'charset', v === null ? '' : String(v)); }
}
defineTag(HTMLMetaElement.prototype, 'HTMLMetaElement');

function byTagAll(root, tag) {
  var t = ns.type(root);
  if (t === DOCUMENT_NODE || t === ELEMENT_NODE) {
    var r = ns.byTag(root, tag);
    return r || [];
  }
  var out = [], want = String(tag).toUpperCase();
  function walk(n) {
    var kids = ns.children(n);
    for (var i = 0; i < kids.length; i++) {
      var c = kids[i];
      if (ns.type(c) === ELEMENT_NODE) {
        if (tag === '*' || ns.name(c).toUpperCase() === want) out.push(c);
        walk(c);
      }
    }
  }
  walk(root);
  return out;
}

/* ---------------------------------------------------------------- TreeWalker */

class TreeWalker {
  constructor(root, whatToShow, filter) {
    this.root = root;
    this.whatToShow = whatToShow === undefined ? 0xFFFFFFFF : whatToShow;
    this.filter = filter || null;
    this.currentNode = root;
  }
  _accept(n) {
    var t = ns.type(n);
    var bit = t === ELEMENT_NODE ? 1 : t === TEXT_NODE ? 4 :
              t === COMMENT_NODE ? 128 : t === DOCUMENT_NODE ? 256 : 1;
    if (!(this.whatToShow & bit)) return false;
    if (!this.filter) return true;
    try {
      if (typeof this.filter === 'function') return this.filter(n) !== 2;
      if (typeof this.filter.acceptNode === 'function') return this.filter.acceptNode(n) !== 2;
    } catch (_) {}
    return true;
  }
  parentNode() {
    var p = ns.parent(this.currentNode);
    while (p) {
      if (p === this.root) { this.currentNode = p; return p; }
      if (this._accept(p)) { this.currentNode = p; return p; }
      p = ns.parent(p);
    }
    return null;
  }
  firstChild() {
    var c = ns.first(this.currentNode);
    while (c) {
      if (this._accept(c)) { this.currentNode = c; return c; }
      c = ns.next(c);
    }
    return null;
  }
  lastChild() {
    var c = ns.last(this.currentNode);
    while (c) {
      if (this._accept(c)) { this.currentNode = c; return c; }
      c = ns.prev(c);
    }
    return null;
  }
  nextSibling() {
    var n = ns.next(this.currentNode);
    while (n) {
      if (this._accept(n)) { this.currentNode = n; return n; }
      n = ns.next(n);
    }
    return null;
  }
  previousSibling() {
    var n = ns.prev(this.currentNode);
    while (n) {
      if (this._accept(n)) { this.currentNode = n; return n; }
      n = ns.prev(n);
    }
    return null;
  }
  /* resultado del filtro como en el estandar: 1 aceptar, 2 rechazar, 3 saltear */
  _code(n) {
    var t = ns.type(n);
    var bit = 1 << (t - 1);
    if (!(this.whatToShow & bit)) return 3;
    if (!this.filter) return 1;
    try {
      var r = typeof this.filter === 'function' ? this.filter(n) : this.filter.acceptNode(n);
      return r === 2 || r === 3 ? r : 1;
    } catch (_) { return 1; }
  }
  /* Algoritmo del estandar: avanza desde currentNode en orden de documento.
   * Lit usa un solo TreeWalker con root=document y le pone como currentNode el
   * fragmento de la plantilla, asi que no se puede partir desde root. */
  nextNode() {
    var node = this.currentNode, root = this.root, r = 1;
    for (;;) {
      var c;
      while (r !== 2 && (c = ns.first(node))) {
        node = c;
        r = this._code(node);
        if (r === 1) { this.currentNode = node; return node; }
      }
      var sib = null, tmp = node;
      while (tmp) {
        if (tmp === root) return null;
        sib = ns.next(tmp);
        if (sib) break;
        tmp = ns.parent(tmp);
      }
      if (!sib) return null;
      node = sib;
      r = this._code(node);
      if (r === 1) { this.currentNode = node; return node; }
    }
  }
  previousNode() {
    var node = this.currentNode, root = this.root;
    while (node && node !== root) {
      var sib = ns.prev(node);
      while (sib) {
        node = sib;
        var r = this._code(node), lc;
        while (r !== 2 && (lc = ns.last(node))) { node = lc; r = this._code(node); }
        if (r === 1) { this.currentNode = node; return node; }
        sib = ns.prev(node);
      }
      var p = ns.parent(node);
      if (node === root || !p) return null;
      node = p;
      if (this._code(node) === 1) { this.currentNode = node; return node; }
    }
    return null;
  }
}
defineTag(TreeWalker.prototype, 'TreeWalker');

/* ---------------------------------------------------------------- Document */

class Document extends Node {
  constructor() { throw new TypeError('Illegal constructor'); }

  get documentElement() {
    var kids = ns.children(this);
    for (var i = 0; i < kids.length; i++) {
      if (ns.type(kids[i]) === ELEMENT_NODE) return kids[i];
    }
    return null;
  }
  get head() {
    var all = ns.byTag(this, 'head') || [];
    return all.length ? all[0] : null;
  }
  get body() {
    var all = ns.byTag(this, 'body') || [];
    return all.length ? all[0] : null;
  }
  get title() {
    var all = ns.byTag(this, 'title') || [];
    if (!all.length) return '';
    var t = ns.text(all[0]);
    return (t === null || t === undefined ? '' : String(t)).replace(/\s+/g, ' ').trim();
  }
  set title(v) {
    var all = ns.byTag(this, 'title') || [];
    if (all.length) { all[0].textContent = v; return; }
    var d = this;
    var t = ns.createElement('title');
    t.textContent = v;
    var h = this.head || this.documentElement;
    if (h) h.appendChild(t);
  }
  get URL() { return currentHref(); }
  get documentURI() { return currentHref(); }
  get location() { return locationObj; }
  set location(v) { navigateTo(v); }
  get domain() {
    try { return new URL(currentHref()).hostname; } catch (_) { return ''; }
  }
  get referrer() { return ''; }
  get cookie() { try { return String(ns.cookie()); } catch (_) { return ''; } }
  set cookie(v) { try { ns.cookie(String(v)); } catch (_) {} }
  get readyState() { return uiState.readyState; }
  set readyState(v) { uiState.readyState = String(v); }
  get visibilityState() { return 'visible'; }
  get hidden() { return false; }
  get defaultView() { return G; }
  get activeElement() { return uiState.activeElement || this.body || null; }
  get characterSet() { return 'UTF-8'; }
  get charset() { return 'UTF-8'; }
  get inputEncoding() { return 'UTF-8'; }
  get contentType() { return 'text/html'; }
  get compatMode() { return 'CSS1Compat'; }
  get doctype() { return null; }
  get documentMode() { return undefined; }
  get fullscreenElement() { return null; }
  get styleSheets() { return []; }
  get scripts() {
    var self = this;
    return liveCollection(function () { return ns.byTag(self, 'script') || []; });
  }
  get images() {
    var self = this;
    return liveCollection(function () { return ns.byTag(self, 'img') || []; });
  }
  get forms() {
    var self = this;
    return liveCollection(function () { return ns.byTag(self, 'form') || []; });
  }
  get links() {
    var self = this;
    return liveCollection(function () {
      var all = ns.byTag(self, '*') || [], out = [];
      for (var i = 0; i < all.length; i++) {
        var t = ns.name(all[i]).toUpperCase();
        if ((t === 'A' || t === 'AREA') && ns.getAttr(all[i], 'href') !== null) out.push(all[i]);
      }
      return out;
    });
  }
  get fonts() {
    return {
      ready: Promise.resolve(),
      status: 'loaded',
      check: function () { return true; },
      load: function () { return Promise.resolve([]); },
      add: function () {}, 'delete': function () { return true; },
      has: function () { return false; }, forEach: function () {}, size: 0
    };
  }
  get implementation() {
    var self = this;
    return {
      createHTMLDocument: function (title) {
        return {
          title: title === undefined ? '' : String(title),
          documentElement: null, head: null, body: null,
          createElement: function (t) { return self.createElement(t); },
          createElementNS: function (ns2, t) { return self.createElement(t); },
          createTextNode: function (t) { return self.createTextNode(t); },
          createComment: function (t) { return self.createComment(t); },
          createDocumentFragment: function () { return self.createDocumentFragment(); },
          getElementById: function () { return null; },
          getElementsByTagName: function () { return []; },
          querySelector: function () { return null; },
          querySelectorAll: function () { return makeList([]); }
        };
      },
      createDocumentType: function (name) {
        return { name: String(name || ''), publicId: '', systemId: '', nodeType: 10 };
      },
      hasFeature: function () { return true; }
    };
  }

  getElementById(id) {
    var r = null;
    try { r = ns.byId(String(id)); } catch (_) { r = null; }
    return r || null;
  }
  getElementsByName(name) {
    name = String(name);
    var self = this;
    return liveCollection(function () {
      var all = ns.byTag(self, '*') || [], out = [];
      for (var i = 0; i < all.length; i++) {
        if (ns.getAttr(all[i], 'name') === name) out.push(all[i]);
      }
      return out;
    });
  }
  createElement(tag) {
    var name = String(tag);
    var el = ns.createElement(name);
    if (!el) throw new DOMException('no document', 'InvalidStateError');
    return el;
  }
  createElementNS(namespace, tag) { return this.createElement(String(tag)); }
  createTextNode(data) { return ns.createText(data === undefined ? '' : String(data)); }
  createComment(data) { return ns.createComment(data === undefined ? '' : String(data)); }
  createDocumentFragment() { return ns.createFragment(); }
  createAttribute(name) {
    return { name: String(name).toLowerCase(), value: '', nodeType: 2,
             specified: true, ownerDocument: this };
  }
  createEvent(type) {
    var t = String(type).toLowerCase();
    var ev;
    if (t === 'customevent') ev = new CustomEvent('');
    else if (t === 'mouseevent' || t === 'mouseevents') ev = new MouseEvent('');
    else if (t === 'keyboardevent' || t === 'keyevents') ev = new KeyboardEvent('');
    else if (t === 'uievent' || t === 'uievents') ev = new UIEvent('');
    else if (t === 'focusevent') ev = new FocusEvent('');
    else if (t === 'inputevent') ev = new InputEvent('');
    else ev = new Event('');
    return ev;
  }
  createRange() {
    var doc = this;
    var range = {
      startContainer: doc, startOffset: 0, endContainer: doc, endOffset: 0,
      collapsed: true, commonAncestorContainer: doc,
      setStart: function (n, o) { this.startContainer = n; this.startOffset = o || 0; },
      setEnd: function (n, o) { this.endContainer = n; this.endOffset = o || 0; },
      setStartBefore: function (n) { this.startContainer = n.parentNode; },
      setStartAfter: function (n) { this.startContainer = n.parentNode; },
      setEndBefore: function (n) { this.endContainer = n.parentNode; },
      setEndAfter: function (n) { this.endContainer = n.parentNode; },
      selectNode: function (n) { this.startContainer = n; this.endContainer = n; },
      selectNodeContents: function (n) { this.startContainer = n; this.endContainer = n; },
      collapse: function () { this.collapsed = true; },
      cloneRange: function () { return doc.createRange(); },
      detach: function () {},
      toString: function () { return ''; },
      getBoundingClientRect: function () {
        return { x: 0, y: 0, left: 0, top: 0, right: 0, bottom: 0, width: 0, height: 0 };
      },
      getClientRects: function () { return []; },
      createContextualFragment: function (html) {
        var temp = doc.createElement('div');
        temp.innerHTML = html;
        var f = doc.createDocumentFragment();
        var kids = ns.children(temp);
        for (var i = 0; i < kids.length; i++) f.appendChild(kids[i]);
        return f;
      }
    };
    return range;
  }
  createTreeWalker(root, whatToShow, filter) {
    return new TreeWalker(root, whatToShow, filter);
  }
  createNodeIterator(root, whatToShow, filter) {
    return new TreeWalker(root, whatToShow, filter);
  }
  hasFocus() { return true; }
  write() {
    var body = this.body;
    if (!body) return;
    var s = '';
    for (var i = 0; i < arguments.length; i++) s += String(arguments[i]);
    body.insertAdjacentHTML('beforeend', s);
  }
  writeln() {
    var body = this.body;
    if (!body) return;
    var s = '';
    for (var i = 0; i < arguments.length; i++) s += String(arguments[i]);
    body.insertAdjacentHTML('beforeend', s + '\n');
  }
  open() { return this; }
  close() {}
  adoptNode(node) { return node; }
  importNode(node, deep) { return ns.clone(node, !!deep); }
  elementFromPoint() { return null; }
  elementsFromPoint() { return []; }
  getSelection() {
    return { rangeCount: 0, isCollapsed: true, type: 'None', toString: function () { return ''; },
             removeAllRanges: function () {}, addRange: function () {}, getRangeAt: function () { return null; } };
  }
  execCommand() { return false; }
  queryCommandSupported() { return false; }
  exitFullscreen() { return Promise.resolve(); }
}
defineTag(Document.prototype, 'Document');
installMixin(Document.prototype, PARENT_METHODS);

installMixin(DocumentFragment.prototype, PARENT_METHODS);

/* ---------------------------------------------------------------- selectores CSS */

var selectorCache = new Map();

function trimSel(s) { return String(s).replace(/^\s+|\s+$/g, ''); }

function splitTopLevel(s, ch) {
  var out = [], depth = 0, q = null, cur = '';
  for (var i = 0; i < s.length; i++) {
    var c = s.charAt(i);
    if (q) { if (c === q) q = null; cur += c; continue; }
    if (c === '"' || c === "'") { q = c; cur += c; continue; }
    if (c === '(' || c === '[') depth++;
    if (c === ')' || c === ']') depth--;
    if (c === ch && depth === 0) { out.push(cur); cur = ''; continue; }
    cur += c;
  }
  out.push(cur);
  return out;
}

function parseSelector(sel) {
  sel = String(sel);
  if (selectorCache.has(sel)) return selectorCache.get(sel);
  var groups = splitTopLevel(sel, ',').map(function (grp) {
    grp = trimSel(grp).replace(/\s*([>+~])\s*/g, '$1');
    var steps = [], cur = '', comb = ' ', depth = 0, q = null;
    for (var i = 0; i <= grp.length; i++) {
      var c = grp.charAt(i);
      if (q) { if (c === q) q = null; cur += c; continue; }
      if (c === '"' || c === "'") { q = c; cur += c; continue; }
      if (c === '(' || c === '[') depth++;
      if (c === ')' || c === ']') depth--;
      if (i === grp.length || (depth === 0 && (c === ' ' || c === '>' || c === '+' || c === '~'))) {
        if (cur) { steps.push({ comb: comb, simple: parseCompound(cur) }); cur = ''; }
        if (i < grp.length) comb = c;
        continue;
      }
      cur += c;
    }
    return steps;
  });
  selectorCache.set(sel, groups);
  return groups;
}

function parseCompound(s) {
  var parts = [], pos = 0;
  var re = /^(\*|[\w-]+)|#([\w-]+)|\.([\w-]+)|\[\s*([\w:-]+)\s*(?:([~|^$*]?=)\s*(?:"([^"]*)"|'([^']*)'|([^\]\s]*)))?\s*\]|:([\w-]+)(?:\(((?:[^()]|\([^()]*\))*)\))?/g;
  var m;
  while ((m = re.exec(s)) !== null) {
    if (m.index !== pos) throw new Error('Selector no soportado: ' + s);
    pos = re.lastIndex;
    if (m[1]) {
      if (m[1] !== '*') parts.push({ t: 'tag', v: m[1].toUpperCase() });
    } else if (m[2]) parts.push({ t: 'id', v: m[2] });
    else if (m[3]) parts.push({ t: 'cls', v: m[3] });
    else if (m[4]) {
      parts.push({ t: 'attr', n: m[4].toLowerCase(), op: m[5],
        v: m[6] !== undefined ? m[6] : m[7] !== undefined ? m[7] : m[8] });
    } else if (m[9]) parts.push({ t: 'pseudo', n: m[9].toLowerCase(), a: m[10] });
    if (re.lastIndex === m.index) re.lastIndex++;
  }
  if (pos !== s.length) throw new Error('Selector no soportado: ' + s);
  return parts;
}

function elementSiblings(e) {
  var p = ns.parent(e);
  if (!p) return [e];
  var kids = ns.children(p), out = [];
  for (var i = 0; i < kids.length; i++) {
    if (ns.type(kids[i]) === ELEMENT_NODE) out.push(kids[i]);
  }
  return out;
}

function typeSiblings(e) {
  var name = ns.name(e).toUpperCase();
  var all = elementSiblings(e), out = [];
  for (var i = 0; i < all.length; i++) {
    if (ns.name(all[i]).toUpperCase() === name) out.push(all[i]);
  }
  return out;
}

function nthMatch(expr, idx) {
  expr = trimSel(expr).toLowerCase();
  if (expr === 'odd') return idx % 2 === 1;
  if (expr === 'even') return idx % 2 === 0;
  var m = /^([+-]?\d*)n\s*(?:([+-])\s*(\d+))?$/.exec(expr);
  if (!m) return idx === parseInt(expr, 10);
  var a = m[1] === '' || m[1] === '+' ? 1 : m[1] === '-' ? -1 : parseInt(m[1], 10);
  var b = m[2] ? parseInt(m[3], 10) * (m[2] === '-' ? -1 : 1) : 0;
  if (a === 0) return idx === b;
  return (idx - b) / a >= 0 && (idx - b) % a === 0;
}

function matchSimple(e, parts) {
  for (var i = 0; i < parts.length; i++) {
    var p = parts[i], v;
    if (p.t === 'tag') {
      if (ns.name(e).toUpperCase() !== p.v) return false;
    } else if (p.t === 'id') {
      if (ns.getAttr(e, 'id') !== p.v) return false;
    } else if (p.t === 'cls') {
      var cls = ' ' + (ns.getAttr(e, 'class') || '').replace(/\s+/g, ' ') + ' ';
      if (cls.indexOf(' ' + p.v + ' ') < 0) return false;
    } else if (p.t === 'attr') {
      v = ns.getAttr(e, p.n);
      if (v === null) return false;
      if (!p.op) continue;
      v = String(v);
      var want = p.v === undefined ? '' : String(p.v);
      if (p.op === '=' && v !== want) return false;
      if (p.op === '^=' && v.indexOf(want) !== 0) return false;
      if (p.op === '$=' && (want.length > v.length || v.slice(-want.length) !== want)) return false;
      if (p.op === '*=' && v.indexOf(want) < 0) return false;
      if (p.op === '~=' && (' ' + v + ' ').indexOf(' ' + want + ' ') < 0) return false;
      if (p.op === '|=' && v !== want && v.indexOf(want + '-') !== 0) return false;
    } else if (p.t === 'pseudo') {
      var par = ns.parent(e);
      var sib = par ? elementSiblings(e) : [e];
      var idx = sib.indexOf(e);
      var tsib = typeSiblings(e);
      var tidx = tsib.indexOf(e);
      var n = p.n;
      if (n === 'first-child') { if (idx !== 0) return false; }
      else if (n === 'last-child') { if (idx !== sib.length - 1) return false; }
      else if (n === 'only-child') { if (sib.length !== 1) return false; }
      else if (n === 'nth-child') { if (!nthMatch(p.a, idx + 1)) return false; }
      else if (n === 'nth-last-child') { if (!nthMatch(p.a, sib.length - idx)) return false; }
      else if (n === 'first-of-type') { if (tidx !== 0) return false; }
      else if (n === 'last-of-type') { if (tidx !== tsib.length - 1) return false; }
      else if (n === 'only-of-type') { if (tsib.length !== 1) return false; }
      else if (n === 'nth-of-type') { if (!nthMatch(p.a, tidx + 1)) return false; }
      else if (n === 'nth-last-of-type') { if (!nthMatch(p.a, tsib.length - tidx)) return false; }
      else if (n === 'not') { if (matchesSelector(e, p.a)) return false; }
      else if (n === 'is' || n === 'where' || n === 'matches') {
        if (!matchesSelector(e, p.a)) return false;
      }
      else if (n === 'has') { if (!qsa(e, p.a, true)) return false; }
      else if (n === 'checked') {
        if (!e.checked && ns.getAttr(e, 'selected') === null) return false;
      }
      else if (n === 'disabled') { if (ns.getAttr(e, 'disabled') === null) return false; }
      else if (n === 'enabled') { if (ns.getAttr(e, 'disabled') !== null) return false; }
      else if (n === 'required') { if (ns.getAttr(e, 'required') === null) return false; }
      else if (n === 'optional') { if (ns.getAttr(e, 'required') !== null) return false; }
      else if (n === 'empty') { if (ns.first(e) !== null) return false; }
      else if (n === 'root') { if (par && ns.type(par) === ELEMENT_NODE) return false; }
      else if (n === 'link') {
        var tag = ns.name(e).toUpperCase();
        if ((tag !== 'A' && tag !== 'AREA') || ns.getAttr(e, 'href') === null) return false;
      }
      else if (n === 'visited' || n === 'focus' || n === 'hover' ||
               n === 'active' || n === 'target' || n === 'focus-within' ||
               n === 'focus-visible') return false;
      else if (n === 'scope') { /* aceptar */ }
      else if (n === 'lang') {
        var wantLang = String(p.a || '').toLowerCase();
        var cur = e, lang = null;
        while (cur && ns.type(cur) === ELEMENT_NODE) {
          var l = ns.getAttr(cur, 'lang');
          if (l !== null) { lang = String(l).toLowerCase(); break; }
          cur = ns.parent(cur);
        }
        if (!lang || (lang !== wantLang && lang.indexOf(wantLang + '-') !== 0)) return false;
      }
      else if (n === 'dir') { /* aceptar */ }
      else return false;
    }
  }
  return true;
}

function matchSteps(e, steps, k) {
  if (!matchSimple(e, steps[k].simple)) return false;
  if (k === 0) return true;
  var comb = steps[k].comb, n;
  if (comb === '>') {
    n = ns.parent(e);
    return !!n && ns.type(n) === ELEMENT_NODE && matchSteps(n, steps, k - 1);
  }
  if (comb === '+') {
    n = previousElement(e);
    return !!n && matchSteps(n, steps, k - 1);
  }
  if (comb === '~') {
    for (n = previousElement(e); n; n = previousElement(n)) {
      if (matchSteps(n, steps, k - 1)) return true;
    }
    return false;
  }
  for (n = ns.parent(e); n && ns.type(n) === ELEMENT_NODE; n = ns.parent(n)) {
    if (matchSteps(n, steps, k - 1)) return true;
  }
  return false;
}

function matchesSelector(e, sel) {
  if (!isNode(e) || ns.type(e) !== ELEMENT_NODE) return false;
  var groups;
  try { groups = parseSelector(sel); } catch (_) { return false; }
  for (var i = 0; i < groups.length; i++) {
    if (groups[i].length && matchSteps(e, groups[i], groups[i].length - 1)) return true;
  }
  return false;
}

/* Candidatos segun la ultima parte del selector: #id o etiqueta evitan
 * recorrer toda la pagina (en el PPU eso tarda segundos en sitios grandes) */
function qsaCandidates(root, sel) {
  var groups;
  try { groups = parseSelector(sel); } catch (_) { return null; }
  if (groups.length !== 1 || !groups[0].length) return byTagAll(root, '*');
  var last = groups[0][groups[0].length - 1].simple, tag = null;
  for (var i = 0; i < last.length; i++) {
    if (last[i].t === 'id') {
      var el = ns.byId(last[i].v);
      if (!el) return [];
      if (root === getDocument() || ns.type(root) === DOCUMENT_NODE) return [el];
      for (var p = ns.parent(el); p; p = ns.parent(p)) if (p === root) return [el];
      return [];
    }
    if (last[i].t === 'tag') tag = last[i].v;
  }
  return byTagAll(root, tag ? tag.toLowerCase() : '*');
}

function qsa(root, sel, one) {
  var all = qsaCandidates(root, sel) || [], out = [];
  for (var i = 0; i < all.length; i++) {
    if (matchesSelector(all[i], sel)) {
      if (one) return all[i];
      out.push(all[i]);
    }
  }
  return one ? null : out;
}

/* ---------------------------------------------------------------- URL */

var URL_SCHEME_RE = /^[a-zA-Z][a-zA-Z0-9+.-]*:/;
var URL_FULL_RE = /^([a-zA-Z][a-zA-Z0-9+.-]*):(?:\/\/([^\/?#]*))?([^?#]*)(\?[^#]*)?(#.*)?$/;

function decodeSafe(s) {
  try { return decodeURIComponent(String(s)); } catch (_) { return String(s); }
}

function parseURLParts(str) {
  var m = URL_FULL_RE.exec(String(str));
  if (!m) return null;
  var p = {
    protocol: m[1].toLowerCase() + ':',
    authority: m[2] !== undefined,
    username: '', password: '', hostname: '', port: '',
    path: m[3] || '', search: m[4] || '', hash: m[5] || ''
  };
  if (p.authority) {
    var auth = m[2];
    var at = auth.lastIndexOf('@');
    if (at >= 0) {
      var ui = auth.slice(0, at);
      auth = auth.slice(at + 1);
      var c0 = ui.indexOf(':');
      if (c0 >= 0) {
        p.username = decodeSafe(ui.slice(0, c0));
        p.password = decodeSafe(ui.slice(c0 + 1));
      } else {
        p.username = decodeSafe(ui);
      }
    }
    if (auth.charAt(0) === '[') {
      var e = auth.indexOf(']');
      if (e >= 0) {
        p.hostname = auth.slice(0, e + 1).toLowerCase();
        if (auth.charAt(e + 1) === ':') p.port = auth.slice(e + 2);
      } else {
        p.hostname = auth.toLowerCase();
      }
    } else {
      var c = auth.lastIndexOf(':');
      if (c >= 0 && /^\d*$/.test(auth.slice(c + 1))) {
        p.hostname = auth.slice(0, c).toLowerCase();
        p.port = auth.slice(c + 1);
      } else {
        p.hostname = auth.toLowerCase();
      }
    }
    if (p.path === '') p.path = '/';
  }
  return p;
}

function normalizePath(path) {
  path = String(path);
  var lead = path.charAt(0) === '/';
  var trail = /\/$/.test(path) || /\/\.$/.test(path) || /\/\.\.$/.test(path);
  var segs = path.split('/'), out = [];
  for (var i = 0; i < segs.length; i++) {
    var s = segs[i];
    if (s === '') {
      if (i === 0 && lead) continue;
      if (i === segs.length - 1) continue;
      out.push('');
      continue;
    }
    if (s === '.') continue;
    if (s === '..') {
      if (out.length && out[out.length - 1] !== '..') out.pop();
      else if (!lead) out.push('..');
      continue;
    }
    out.push(s);
  }
  var r = (lead ? '/' : '') + out.join('/');
  if (trail && r.charAt(r.length - 1) !== '/') r += '/';
  if (r === '' && lead) r = '/';
  return r;
}

function composeURL(p) {
  var s = p.protocol;
  if (p.authority) {
    s += '//';
    if (p.username || p.password) {
      s += encodeURIComponent(p.username);
      if (p.password) s += ':' + encodeURIComponent(p.password);
      s += '@';
    }
    s += p.hostname;
    if (p.port) s += ':' + p.port;
    if (p.path && p.path.charAt(0) !== '/') s += '/';
  }
  s += p.path || '';
  s += p.search || '';
  s += p.hash || '';
  return s;
}

function splitRef(rel) {
  var hash = '', search = '', path = String(rel);
  var i = path.indexOf('#');
  if (i >= 0) { hash = path.slice(i); path = path.slice(0, i); }
  i = path.indexOf('?');
  if (i >= 0) { search = path.slice(i); path = path.slice(0, i); }
  return { path: path, search: search, hash: hash };
}

function resolveURL(rel, base) {
  rel = String(rel);
  if (URL_SCHEME_RE.test(rel)) {
    var abs = parseURLParts(rel);
    return abs ? composeURL(abs) : rel;
  }
  var b = parseURLParts(base || 'about:blank');
  if (!b) {
    b = { protocol: 'about:', authority: false, username: '', password: '',
          hostname: '', port: '', path: 'blank', search: '', hash: '' };
  }
  if (rel.slice(0, 2) === '//') {
    var q = parseURLParts(b.protocol + rel);
    return q ? composeURL(q) : b.protocol + rel;
  }
  var ref = splitRef(rel);
  var p = {
    protocol: b.protocol, authority: b.authority,
    username: b.username, password: b.password,
    hostname: b.hostname, port: b.port,
    path: b.path, search: '', hash: ''
  };
  if (ref.path === '') {
    p.path = b.path;
    p.search = ref.search !== '' ? ref.search : b.search;
    p.hash = ref.hash;
    if (rel === '') p.hash = '';
  } else if (ref.path.charAt(0) === '/') {
    p.path = normalizePath(ref.path);
    p.search = ref.search;
    p.hash = ref.hash;
  } else {
    var bp = b.path || '';
    var slash = bp.lastIndexOf('/');
    var dir = slash >= 0 ? bp.slice(0, slash + 1) : (b.authority ? '/' : '');
    p.path = normalizePath(dir + ref.path);
    p.search = ref.search;
    p.hash = ref.hash;
  }
  return composeURL(p);
}

function encParam(s) {
  return encodeURIComponent(String(s)).replace(/%20/g, '+');
}

class URLSearchParams {
  constructor(init) {
    this._list = [];
    this._url = null;
    if (init === null || init === undefined) return;
    if (typeof init === 'string') {
      this._parse(init);
    } else if (init instanceof URLSearchParams) {
      for (var i = 0; i < init._list.length; i++) {
        this._list.push([init._list[i][0], init._list[i][1]]);
      }
    } else if (typeof init === 'object' && typeof init[Symbol.iterator] === 'function') {
      var it = init[Symbol.iterator]();
      var step;
      while (!(step = it.next()).done) {
        var pair = step.value;
        if (pair) this._list.push([String(pair[0]), pair[1] === undefined ? '' : String(pair[1])]);
      }
    } else if (typeof init === 'object') {
      var keys = Object.keys(init);
      for (var j = 0; j < keys.length; j++) {
        this._list.push([keys[j], String(init[keys[j]])]);
      }
    }
  }
  _parse(s) {
    s = String(s).replace(/^\?/, '');
    if (!s) return;
    var parts = s.split('&');
    for (var i = 0; i < parts.length; i++) {
      var part = parts[i];
      if (!part) continue;
      var k = part.indexOf('=');
      var name = k < 0 ? part : part.slice(0, k);
      var value = k < 0 ? '' : part.slice(k + 1);
      this._list.push([decodeSafe(name.replace(/\+/g, ' ')), decodeSafe(value.replace(/\+/g, ' '))]);
    }
  }
  _changed() {
    if (this._url && typeof this._url._syncSearch === 'function') this._url._syncSearch();
  }
  get size() { return this._list.length; }
  get(k) {
    k = String(k);
    for (var i = 0; i < this._list.length; i++) if (this._list[i][0] === k) return this._list[i][1];
    return null;
  }
  getAll(k) {
    k = String(k);
    var out = [];
    for (var i = 0; i < this._list.length; i++) if (this._list[i][0] === k) out.push(this._list[i][1]);
    return out;
  }
  has(k) {
    k = String(k);
    for (var i = 0; i < this._list.length; i++) if (this._list[i][0] === k) return true;
    return false;
  }
  set(k, v) {
    k = String(k); v = String(v);
    var first = -1;
    for (var i = 0; i < this._list.length;) {
      if (this._list[i][0] === k) {
        if (first < 0) { first = i; i++; }
        else this._list.splice(i, 1);
      } else i++;
    }
    if (first < 0) this._list.push([k, v]);
    else this._list[first][1] = v;
    this._changed();
  }
  append(k, v) {
    this._list.push([String(k), String(v)]);
    this._changed();
  }
  'delete'(k) {
    k = String(k);
    for (var i = this._list.length - 1; i >= 0; i--) {
      if (this._list[i][0] === k) this._list.splice(i, 1);
    }
    this._changed();
  }
  sort() {
    this._list.sort(function (a, b) { return a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : 0; });
    this._changed();
  }
  toString() {
    var out = [];
    for (var i = 0; i < this._list.length; i++) {
      out.push(encParam(this._list[i][0]) + '=' + encParam(this._list[i][1]));
    }
    return out.join('&');
  }
  forEach(cb, thisArg) {
    for (var i = 0; i < this._list.length; i++) {
      cb.call(thisArg, this._list[i][1], this._list[i][0], this);
    }
  }
  entries() {
    var l = this._list, i = 0;
    return { next: function () { return i < l.length ? { value: [l[i][0], l[i][1]], done: false } : { value: undefined, done: true }; },
             [Symbol.iterator]: function () { return this; } };
  }
  keys() {
    var l = this._list, i = 0;
    return { next: function () { return i < l.length ? { value: l[i++][0], done: false } : { value: undefined, done: true }; },
             [Symbol.iterator]: function () { return this; } };
  }
  values() {
    var l = this._list, i = 0;
    return { next: function () { return i < l.length ? { value: l[i++][1], done: false } : { value: undefined, done: true }; },
             [Symbol.iterator]: function () { return this; } };
  }
  [Symbol.iterator]() { return this.entries(); }
}
defineTag(URLSearchParams.prototype, 'URLSearchParams');

class URL {
  constructor(url, base) {
    if (arguments.length === 0) throw new TypeError('URL: missing argument');
    var str = String(url);
    if (base !== undefined && base !== null) str = resolveURL(str, String(base));
    var p = parseURLParts(str);
    if (!p) throw new TypeError('Invalid URL: ' + str);
    this._p = p;
    this._params = null;
  }
  _href() { return composeURL(this._p); }
  get href() { return this._href(); }
  set href(v) {
    var p = parseURLParts(String(v));
    if (!p) throw new TypeError('Invalid URL: ' + v);
    this._p = p;
    this._params = null;
  }
  get origin() {
    var pr = this._p.protocol;
    if (pr === 'http:' || pr === 'https:' || pr === 'ws:' || pr === 'wss:' || pr === 'ftp:') {
      return pr + '//' + this.host;
    }
    return 'null';
  }
  get protocol() { return this._p.protocol; }
  set protocol(v) {
    var s = String(v);
    if (s.charAt(s.length - 1) !== ':') s += ':';
    this._p.protocol = s.toLowerCase();
    this._params = null;
  }
  get username() { return this._p.username; }
  set username(v) { this._p.username = String(v); this._params = null; }
  get password() { return this._p.password; }
  set password(v) { this._p.password = String(v); this._params = null; }
  get host() { return this._p.hostname + (this._p.port ? ':' + this._p.port : ''); }
  set host(v) {
    v = String(v);
    var c = v.lastIndexOf(':');
    if (c >= 0 && /^\d*$/.test(v.slice(c + 1))) {
      this._p.hostname = v.slice(0, c).toLowerCase();
      this._p.port = v.slice(c + 1);
    } else {
      this._p.hostname = v.toLowerCase();
      this._p.port = '';
    }
    this._params = null;
  }
  get hostname() { return this._p.hostname; }
  set hostname(v) { this._p.hostname = String(v).toLowerCase(); this._params = null; }
  get port() { return this._p.port; }
  set port(v) { this._p.port = String(v); this._params = null; }
  get pathname() { return this._p.path || '/'; }
  set pathname(v) {
    var s = String(v);
    this._p.path = normalizePath(s.charAt(0) === '/' ? s : '/' + s);
    this._params = null;
  }
  get search() { return this._p.search; }
  set search(v) {
    var s = String(v);
    if (s && s.charAt(0) !== '?') s = '?' + s;
    this._p.search = s;
    this._params = null;
  }
  get searchParams() {
    if (!this._params) {
      this._params = new URLSearchParams(this._p.search.replace(/^\?/, ''));
      this._params._url = this;
    }
    return this._params;
  }
  _syncSearch() {
    if (!this._params) return;
    var s = this._params.toString();
    this._p.search = s ? '?' + s : '';
  }
  get hash() { return this._p.hash; }
  set hash(v) {
    var s = String(v);
    if (s && s.charAt(0) !== '#') s = '#' + s;
    this._p.hash = s;
    this._params = null;
  }
  toString() { return this._href(); }
  toJSON() { return this._href(); }
  static canParse(url, base) {
    try { new URL(url, base); return true; } catch (_) { return false; }
  }
}
defineTag(URL.prototype, 'URL');

/* ---------------------------------------------------------------- location / history */

function currentHref() {
  try { return String(ns.href()); } catch (_) { return 'about:blank'; }
}

function navigateTo(url) {
  var abs;
  try { abs = resolveURL(String(url), currentHref()); } catch (_) { abs = String(url); }
  try { ns.navigate(abs); } catch (_) {}
  if (locationObj) locationObj._override = abs;
}

class Location {
  constructor() { this._override = null; }
  _current() { return this._override !== null ? this._override : currentHref(); }
  _url() {
    try { return new URL(this._current()); } catch (_) { return null; }
  }
  get href() { return this._current(); }
  set href(v) { navigateTo(v); }
  get protocol() { var u = this._url(); return u ? u.protocol : ''; }
  set protocol(v) { var u = this._url(); if (u) { u.protocol = v; navigateTo(u.href); } }
  get host() { var u = this._url(); return u ? u.host : ''; }
  set host(v) { var u = this._url(); if (u) { u.host = v; navigateTo(u.href); } }
  get hostname() { var u = this._url(); return u ? u.hostname : ''; }
  set hostname(v) { var u = this._url(); if (u) { u.hostname = v; navigateTo(u.href); } }
  get port() { var u = this._url(); return u ? u.port : ''; }
  set port(v) { var u = this._url(); if (u) { u.port = v; navigateTo(u.href); } }
  get pathname() { var u = this._url(); return u ? u.pathname : ''; }
  set pathname(v) { var u = this._url(); if (u) { u.pathname = v; navigateTo(u.href); } }
  get search() { var u = this._url(); return u ? u.search : ''; }
  set search(v) { var u = this._url(); if (u) { u.search = v; navigateTo(u.href); } }
  get hash() { var u = this._url(); return u ? u.hash : ''; }
  set hash(v) { var u = this._url(); if (u) { u.hash = v; navigateTo(u.href); } }
  get origin() { var u = this._url(); return u ? u.origin : 'null'; }
  get username() { var u = this._url(); return u ? u.username : ''; }
  get password() { var u = this._url(); return u ? u.password : ''; }
  assign(u) { navigateTo(u); }
  replace(u) { navigateTo(u); }
  reload() { try { ns.history(0); } catch (_) {} }
  toString() { return this._current(); }
  valueOf() { return this._current(); }
}
defineTag(Location.prototype, 'Location');

var locationObj = new Location();

class History {
  constructor() { this._state = null; this._length = 1; }
  get state() { return this._state; }
  get length() { return this._length; }
  get scrollRestoration() { return 'auto'; }
  set scrollRestoration(v) {}
  back() { try { ns.history(-1); } catch (_) {} }
  forward() { try { ns.history(1); } catch (_) {} }
  go(n) {
    n = Number(n) || 0;
    try { ns.history(n === 0 ? 0 : (n < 0 ? -1 : 1)); } catch (_) {}
  }
  pushState(state, title, url) {
    this._state = state === undefined ? null : state;
    if (url !== null && url !== undefined && url !== '') {
      try { locationObj._override = resolveURL(String(url), currentHref()); } catch (_) {}
    }
  }
  replaceState(state, title, url) { this.pushState(state, title, url); }
}
defineTag(History.prototype, 'History');

/* ---------------------------------------------------------------- navigator / screen */

var navigatorObj = {
  userAgent: 'Mozilla/5.0 (PlayStation 3; EonSurf) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0 Safari/537.36 EonSurf/1.0',
  appName: 'Netscape',
  appCodeName: 'Mozilla',
  appVersion: '5.0 (PlayStation 3) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0 Safari/537.36 EonSurf/1.0',
  platform: 'PLAYSTATION 3',
  language: 'es-AR',
  languages: ['es-AR', 'es', 'en'],
  onLine: true,
  cookieEnabled: true,
  vendor: 'EonSurf',
  vendorSub: '',
  product: 'Gecko',
  productSub: '20030107',
  maxTouchPoints: 0,
  hardwareConcurrency: 2,
  deviceMemory: 0.5,
  doNotTrack: null,
  pdfViewerEnabled: false,
  webdriver: false,
  clipboard: {
    readText: function () { return Promise.resolve(''); },
    writeText: function () { return Promise.resolve(); },
    read: function () { return Promise.reject(new DOMException('no soportado', 'NotSupportedError')); },
    write: function () { return Promise.resolve(); }
  },
  javaEnabled: function () { return false; },
  sendBeacon: function () { return false; },
  vibrate: function () { return false; },
  getGamepads: function () { return []; },
  registerProtocolHandler: function () {},
  share: function () { return Promise.reject(new DOMException('no soportado', 'NotSupportedError')); }
};

var screenObj = {
  width: 1280,
  height: 720,
  availWidth: 1280,
  availHeight: 720,
  availLeft: 0,
  availTop: 0,
  colorDepth: 24,
  pixelDepth: 24,
  orientation: { type: 'landscape-primary', angle: 0, addEventListener: function () {}, removeEventListener: function () {} }
};

/* ---------------------------------------------------------------- console */

var consoleTimers = new Map();
var consoleCounts = new Map();
var consoleGroupDepth = 0;

function formatConsoleValue(v) {
  if (typeof v === 'string') return v;
  if (v === null) return 'null';
  if (v === undefined) return 'undefined';
  if (typeof v === 'function') return '[Function ' + (v.name || 'anonymous') + ']';
  if (typeof v === 'number' || typeof v === 'boolean' || typeof v === 'bigint') return String(v);
  if (v instanceof Error) return v.stack || (v.name + ': ' + v.message);
  if (isNode(v)) return '<' + String(ns.name(v)).toLowerCase() + '>';
  try {
    var s = JSON.stringify(v);
    return s === undefined ? String(v) : s;
  } catch (_) { return String(v); }
}
function consoleWrite(args) {
  var parts = [];
  for (var i = 0; i < args.length; i++) parts.push(formatConsoleValue(args[i]));
  var prefix = '';
  for (var j = 0; j < consoleGroupDepth; j++) prefix += '  ';
  log(prefix + parts.join(' '));
}

var consoleObj = {};
(function () {
  var simple = ['log', 'info', 'warn', 'error', 'debug', 'trace', 'dir', 'table'];
  for (var i = 0; i < simple.length; i++) {
    (function (name) {
      consoleObj[name] = function () { consoleWrite(arguments); };
    })(simple[i]);
  }
})();
consoleObj.group = function () { consoleWrite(arguments); consoleGroupDepth++; };
consoleObj.groupCollapsed = consoleObj.group;
consoleObj.groupEnd = function () { if (consoleGroupDepth > 0) consoleGroupDepth--; };
consoleObj.time = function (label) {
  consoleTimers.set(label === undefined ? 'default' : String(label), nowMs());
};
consoleObj.timeLog = function (label) {
  var key = label === undefined ? 'default' : String(label);
  var t = consoleTimers.get(key);
  if (t !== undefined) log(key + ': ' + (nowMs() - t) + 'ms');
};
consoleObj.timeEnd = function (label) {
  var key = label === undefined ? 'default' : String(label);
  var t = consoleTimers.get(key);
  if (t !== undefined) {
    log(key + ': ' + (nowMs() - t) + 'ms');
    consoleTimers.delete(key);
  }
};
consoleObj.assert = function (cond) {
  if (cond) return;
  var args = Array.prototype.slice.call(arguments, 1);
  consoleWrite(['Assertion failed:'].concat(args));
};
consoleObj.count = function (label) {
  var key = label === undefined ? 'default' : String(label);
  var n = (consoleCounts.get(key) || 0) + 1;
  consoleCounts.set(key, n);
  log(key + ': ' + n);
};
consoleObj.countReset = function (label) {
  consoleCounts.delete(label === undefined ? 'default' : String(label));
};
consoleObj.clear = function () {};
consoleObj.dirxml = consoleObj.log;
consoleObj.profile = function () {};
consoleObj.profileEnd = function () {};

/* ---------------------------------------------------------------- performance */

var perfStart = nowMs();

var performanceObj = {
  get timeOrigin() { return perfStart; },
  now: function () { return nowMs() - perfStart; },
  mark: function () {},
  measure: function () {},
  clearMarks: function () {},
  clearMeasures: function () {},
  getEntries: function () { return []; },
  getEntriesByType: function () { return []; },
  getEntriesByName: function () { return []; },
  toJSON: function () { return { timeOrigin: perfStart }; }
};

/* ---------------------------------------------------------------- matchMedia */

function evalMediaQuery(q) {
  q = String(q).trim().toLowerCase();
  if (!q || q === 'all' || q === 'screen') return true;
  if (q === 'print') return false;
  var result = true;
  var parts = q.split(/\s+and\s+/);
  for (var i = 0; i < parts.length; i++) {
    var p = parts[i].trim();
    if (!p) continue;
    if (p === 'all' || p === 'screen') continue;
    if (p === 'print') { result = false; continue; }
    if (p.slice(0, 4) === 'not ') { p = p.slice(4).trim(); }
    if (p.charAt(0) === '(' && p.charAt(p.length - 1) === ')') {
      var inner = p.slice(1, -1).trim();
      var m;
      if ((m = /^min-width\s*:\s*(\d+(?:\.\d+)?)px$/.exec(inner))) {
        if (!(1280 >= parseFloat(m[1]))) result = false;
      } else if ((m = /^max-width\s*:\s*(\d+(?:\.\d+)?)px$/.exec(inner))) {
        if (!(1280 <= parseFloat(m[1]))) result = false;
      } else if ((m = /^min-height\s*:\s*(\d+(?:\.\d+)?)px$/.exec(inner))) {
        if (!(668 >= parseFloat(m[1]))) result = false;
      } else if ((m = /^max-height\s*:\s*(\d+(?:\.\d+)?)px$/.exec(inner))) {
        if (!(668 <= parseFloat(m[1]))) result = false;
      } else if (/^prefers-color-scheme\s*:\s*dark$/.test(inner)) {
        result = false;
      } else if (/^prefers-color-scheme\s*:\s*light$/.test(inner)) {
        /* verdadero */
      } else if (/^orientation\s*:\s*landscape$/.test(inner)) {
        /* verdadero */
      } else if (/^orientation\s*:\s*portrait$/.test(inner)) {
        result = false;
      }
    } else if (p === 'not' || p === 'only') {
      /* ignorar */
    }
  }
  return result;
}

class MediaQueryList extends EventTarget {
  constructor(query) {
    super();
    this.media = String(query);
    this.matches = evalMediaQuery(this.media);
  }
  addListener(fn) { addListener(this, 'change', fn, false); }
  removeListener(fn) { removeListener(this, 'change', fn, false); }
}
defineTag(MediaQueryList.prototype, 'MediaQueryList');

function matchMedia(q) { return new MediaQueryList(q); }

/* ---------------------------------------------------------------- base64 */

var B64_CHARS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';

function btoa(input) {
  var s = String(input);
  var out = '', i = 0;
  while (i < s.length) {
    var c1 = s.charCodeAt(i++);
    var c2 = i < s.length ? s.charCodeAt(i++) : NaN;
    var c3 = i < s.length ? s.charCodeAt(i++) : NaN;
    if (c1 > 255 || (!isNaN(c2) && c2 > 255) || (!isNaN(c3) && c3 > 255)) {
      throw new DOMException('btoa: caracter fuera de Latin-1', 'InvalidCharacterError');
    }
    var b1 = c1 & 0xFF;
    var b2 = isNaN(c2) ? 0 : c2 & 0xFF;
    var b3 = isNaN(c3) ? 0 : c3 & 0xFF;
    var n = (b1 << 16) | (b2 << 8) | b3;
    out += B64_CHARS.charAt((n >> 18) & 63);
    out += B64_CHARS.charAt((n >> 12) & 63);
    out += isNaN(c2) ? '=' : B64_CHARS.charAt((n >> 6) & 63);
    out += isNaN(c3) ? '=' : B64_CHARS.charAt(n & 63);
  }
  return out;
}

function atob(input) {
  var s = String(input).replace(/[\s]/g, '');
  if (s.length % 4 === 0) {
    if (s.slice(-2) === '==') s = s.slice(0, -2);
    else if (s.slice(-1) === '=') s = s.slice(0, -1);
  }
  var out = '', buf = 0, bits = 0;
  for (var i = 0; i < s.length; i++) {
    var idx = B64_CHARS.indexOf(s.charAt(i));
    if (idx < 0) throw new DOMException('atob: cadena invalida', 'InvalidCharacterError');
    buf = (buf << 6) | idx;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out += String.fromCharCode((buf >> bits) & 0xFF);
    }
  }
  return out;
}

/* ---------------------------------------------------------------- TextEncoder / TextDecoder */

class TextEncoder {
  get encoding() { return 'utf-8'; }
  encode(str) {
    str = String(str === undefined ? '' : str);
    var out = [];
    for (var i = 0; i < str.length; i++) {
      var c = str.charCodeAt(i);
      if (c >= 0xD800 && c <= 0xDBFF && i + 1 < str.length) {
        var d = str.charCodeAt(i + 1);
        if (d >= 0xDC00 && d <= 0xDFFF) {
          c = 0x10000 + ((c - 0xD800) << 10) + (d - 0xDC00);
          i++;
        }
      }
      if (c < 0x80) out.push(c);
      else if (c < 0x800) out.push(0xC0 | (c >> 6), 0x80 | (c & 63));
      else if (c < 0x10000) out.push(0xE0 | (c >> 12), 0x80 | ((c >> 6) & 63), 0x80 | (c & 63));
      else out.push(0xF0 | (c >> 18), 0x80 | ((c >> 12) & 63), 0x80 | ((c >> 6) & 63), 0x80 | (c & 63));
    }
    return new Uint8Array(out);
  }
  encodeInto(str, dest) {
    var u = this.encode(str);
    var n = Math.min(u.length, dest.length);
    for (var i = 0; i < n; i++) dest[i] = u[i];
    return { read: String(str).length, written: n };
  }
}
defineTag(TextEncoder.prototype, 'TextEncoder');

class TextDecoder {
  constructor(label, options) {
    this.encoding = 'utf-8';
    this.fatal = !!(options && options.fatal);
    this.ignoreBOM = !!(options && options.ignoreBOM);
  }
  decode(input) {
    if (input === null || input === undefined) return '';
    var bytes;
    if (typeof ArrayBuffer !== 'undefined' && input instanceof ArrayBuffer) {
      bytes = new Uint8Array(input);
    } else if (typeof ArrayBuffer !== 'undefined' && ArrayBuffer.isView && ArrayBuffer.isView(input)) {
      bytes = new Uint8Array(input.buffer, input.byteOffset, input.byteLength);
    } else {
      bytes = input;
    }
    var out = '', i = 0;
    while (i < bytes.length) {
      var b = bytes[i++] & 0xFF;
      if (b < 0x80) { out += String.fromCharCode(b); continue; }
      if (b >= 0xC0 && b < 0xE0) {
        var b2 = i < bytes.length ? bytes[i++] & 0xFF : 0;
        out += String.fromCharCode(((b & 31) << 6) | (b2 & 63));
      } else if (b >= 0xE0 && b < 0xF0) {
        var b3 = i < bytes.length ? bytes[i++] & 0xFF : 0;
        var b4 = i < bytes.length ? bytes[i++] & 0xFF : 0;
        out += String.fromCharCode(((b & 15) << 12) | ((b3 & 63) << 6) | (b4 & 63));
      } else if (b >= 0xF0) {
        var b5 = i < bytes.length ? bytes[i++] & 0xFF : 0;
        var b6 = i < bytes.length ? bytes[i++] & 0xFF : 0;
        var b7 = i < bytes.length ? bytes[i++] & 0xFF : 0;
        var cp = ((b & 7) << 18) | ((b5 & 63) << 12) | ((b6 & 63) << 6) | (b7 & 63);
        cp -= 0x10000;
        out += String.fromCharCode(0xD800 + (cp >> 10), 0xDC00 + (cp & 1023));
      } else {
        out += '\uFFFD';
      }
    }
    return out;
  }
}
defineTag(TextDecoder.prototype, 'TextDecoder');

function structuredClone(value) {
  if (value === undefined) return undefined;
  try { return JSON.parse(JSON.stringify(value)); }
  catch (_) { throw new DOMException('No se pudo clonar', 'DataCloneError'); }
}

/* ---------------------------------------------------------------- crypto */

var cryptoObj = {
  getRandomValues: function (arr) {
    if (!arr || typeof arr.length !== 'number') throw new TypeError('getRandomValues: arreglo requerido');
    var view;
    try {
      if (arr.buffer) view = new Uint8Array(arr.buffer, arr.byteOffset || 0, arr.byteLength);
      else view = new Uint8Array(arr.length);
    } catch (_) { view = new Uint8Array(arr.length); }
    for (var i = 0; i < view.length; i++) view[i] = Math.floor(Math.random() * 256);
    if (!arr.buffer) {
      for (var j = 0; j < arr.length; j++) arr[j] = view[j];
    }
    return arr;
  },
  randomUUID: function () {
    var b = new Uint8Array(16);
    cryptoObj.getRandomValues(b);
    b[6] = (b[6] & 0x0F) | 0x40;
    b[8] = (b[8] & 0x3F) | 0x80;
    var hex = '';
    for (var i = 0; i < 16; i++) {
      var h = b[i].toString(16);
      hex += h.length < 2 ? '0' + h : h;
    }
    return hex.slice(0, 8) + '-' + hex.slice(8, 12) + '-' + hex.slice(12, 16) +
           '-' + hex.slice(16, 20) + '-' + hex.slice(20);
  }
};

/* ---------------------------------------------------------------- DOMException / Blob */

var DOM_EXC_CODES = {
  IndexSizeError: 1, HierarchyRequestError: 3, WrongDocumentError: 4,
  InvalidCharacterError: 5, NoModificationAllowedError: 7, NotFoundError: 8,
  NotSupportedError: 9, InvalidStateError: 11, SyntaxError: 12,
  InvalidModificationError: 13, NamespaceError: 14, InvalidAccessError: 15,
  TypeMismatchError: 17, SecurityError: 18, NetworkError: 19, AbortError: 20,
  URLMismatchError: 21, QuotaExceededError: 22, TimeoutError: 23,
  InvalidNodeTypeError: 24, DataCloneError: 25
};

class DOMException extends Error {
  constructor(message, name) {
    super(message === undefined ? '' : String(message));
    this.name = name === undefined ? 'Error' : String(name);
    this.code = DOM_EXC_CODES[this.name] || 0;
  }
}
defineTag(DOMException.prototype, 'DOMException');

class Blob {
  constructor(parts, options) {
    options = options || {};
    var text = '';
    parts = parts === undefined || parts === null ? [] : parts;
    if (typeof parts === 'string') parts = [parts];
    for (var i = 0; i < parts.length; i++) {
      var p = parts[i];
      if (p instanceof Blob) text += p._text;
      else if (typeof p === 'string') text += p;
      else if (p && p.buffer && p.byteLength !== undefined) text += new TextDecoder().decode(p);
      else if (typeof ArrayBuffer !== 'undefined' && p instanceof ArrayBuffer) text += new TextDecoder().decode(new Uint8Array(p));
      else text += String(p);
    }
    this._text = text;
    this.type = options.type ? String(options.type).toLowerCase() : '';
  }
  get size() { return new TextEncoder().encode(this._text).length; }
  text() { return Promise.resolve(this._text); }
  arrayBuffer() {
    var u = new TextEncoder().encode(this._text);
    return Promise.resolve(u.buffer.slice(u.byteOffset, u.byteOffset + u.byteLength));
  }
  bytes() { return Promise.resolve(new TextEncoder().encode(this._text)); }
  slice(start, end, type) {
    var s = this._text.slice(start, end);
    return new Blob([s], { type: type === undefined ? this.type : type });
  }
  stream() { return null; }
}
defineTag(Blob.prototype, 'Blob');

/* ---------------------------------------------------------------- almacenamiento */

function storageOrigin() {
  try {
    var u = new URL(currentHref());
    if (u.protocol === 'http:' || u.protocol === 'https:') return u.protocol + '//' + u.host;
  } catch (_) {}
  return 'local';
}

class Storage {
  constructor(persist) {
    this._persist = !!persist;
    this._data = {};
    this._dirty = false;
    this._timer = 0;
    this._origin = storageOrigin();
    if (this._persist) {
      var raw = undefined;
      try { raw = ns.storage(this._origin); } catch (_) { raw = undefined; }
      if (typeof raw === 'string' && raw) {
        try {
          var parsed = JSON.parse(raw);
          if (parsed && typeof parsed === 'object' && !Array.isArray(parsed)) this._data = parsed;
        } catch (_) { this._data = {}; }
      }
    }
  }
  get length() { return Object.keys(this._data).length; }
  key(i) {
    var ks = Object.keys(this._data);
    i = Number(i) || 0;
    return i >= 0 && i < ks.length ? ks[i] : null;
  }
  getItem(k) {
    k = String(k);
    return hasOwn.call(this._data, k) ? this._data[k] : null;
  }
  setItem(k, v) {
    this._data[String(k)] = String(v);
    this._schedule();
  }
  removeItem(k) {
    delete this._data[String(k)];
    this._schedule();
  }
  clear() {
    this._data = {};
    this._schedule();
  }
  _schedule() {
    if (!this._persist) return;
    this._dirty = true;
    if (this._timer) return;
    var self = this;
    this._timer = setTimeout(function () { self._timer = 0; self._flush(); }, 500);
  }
  _flush() {
    if (!this._persist || !this._dirty) return;
    this._dirty = false;
    try { ns.storage(this._origin, JSON.stringify(this._data)); } catch (_) {}
  }
}
defineTag(Storage.prototype, 'Storage');

function makeStorageProxy(storage) {
  return new Proxy(storage, {
    get: function (t, p, r) {
      if (typeof p === 'symbol') return Reflect.get(t, p, r);
      if (p in t) return Reflect.get(t, p, r);
      if (typeof p === 'string') {
        var v = t.getItem(p);
        return v === null ? undefined : v;
      }
      return undefined;
    },
    set: function (t, p, v, r) {
      if (typeof p === 'symbol' || (typeof p === 'string' && p in t)) return Reflect.set(t, p, v, r);
      if (typeof p === 'string') { t.setItem(p, v); return true; }
      return false;
    },
    deleteProperty: function (t, p) {
      if (typeof p === 'string') { t.removeItem(p); return true; }
      return Reflect.deleteProperty(t, p);
    },
    has: function (t, p) {
      return (typeof p === 'string' && (p in t || t.getItem(p) !== null)) || Reflect.has(t, p);
    },
    ownKeys: function (t) { return Object.keys(t._data); },
    getOwnPropertyDescriptor: function (t, p) {
      if (typeof p !== 'string') return undefined;
      if (p in t) return Object.getOwnPropertyDescriptor(t, p);
      var v = t.getItem(p);
      if (v === null) return undefined;
      return { value: v, writable: true, enumerable: true, configurable: true };
    }
  });
}

var localStorageObj = makeStorageProxy(new Storage(true));
var sessionStorageObj = makeStorageProxy(new Storage(false));

/* ---------------------------------------------------------------- red: base */

var pendingHttp = new Map();
var nextHttpId = 1;

/* EonFetch (__ns.http2): metodo, cabeceras y cuerpo reales. El cuerpo viaja
 * como texto o ArrayBuffer; el Content-Type por defecto sigue al tipo de cuerpo. */
function bodyForWire(body) {
  if (body === null || body === undefined) return { data: null, type: null };
  if (body instanceof FormData) return { data: body._toQuery(), type: 'application/x-www-form-urlencoded;charset=UTF-8' };
  if (body instanceof URLSearchParams) return { data: body.toString(), type: 'application/x-www-form-urlencoded;charset=UTF-8' };
  if (body instanceof Blob) return { data: body._text, type: body.type || null };
  if (typeof body === 'string') return { data: body, type: 'text/plain;charset=UTF-8' };
  if (body instanceof ArrayBuffer) return { data: body, type: null };
  if (body && body.buffer instanceof ArrayBuffer && body.byteLength !== undefined)
    return { data: body.buffer.slice(body.byteOffset, body.byteOffset + body.byteLength), type: null };
  return { data: String(body), type: 'text/plain;charset=UTF-8' };
}
function sendHTTP(id, method, url, headerPairs, body, binary) {
  if (typeof ns.http2 !== 'function') {
    ns.http(id, String(method), String(url), body === null || body === undefined ? null : bodyToString(body));
    return;
  }
  var w = bodyForWire(body), lines = [], hasType = false;
  (headerPairs || []).forEach(function (h) {
    var k = String(h[0]);
    if (k.toLowerCase() === 'content-type') hasType = true;
    /* el navegador pone estas solo */
    if (/^(host|content-length|connection|cookie|user-agent|accept-encoding)$/i.test(k)) return;
    lines.push(k + ': ' + String(h[1]));
  });
  if (w.data !== null && !hasType && w.type) lines.push('Content-Type: ' + w.type);
  ns.http2(id, String(method), String(url), lines, w.data, !!binary);
}
/* cabeceras crudas de la respuesta -> { statusText, list: [[nombre, valor]] } */
function parseRawHeaders(raw) {
  var out = { statusText: '', list: [] };
  String(raw || '').split(/\r?\n/).forEach(function (l) {
    if (/^HTTP\//.test(l)) {
      var m = /^HTTP\/\S+\s+\d+\s*(.*)$/.exec(l);
      out.statusText = m ? m[1].trim() : '';
      out.list = [];
      return;
    }
    var k = l.indexOf(':');
    if (k > 0) out.list.push([l.slice(0, k).trim().toLowerCase(), l.slice(k + 1).trim()]);
  });
  return out;
}

function requestHTTP(method, url, body, cb) {
  var id = nextHttpId++;
  pendingHttp.set(id, cb);
  try {
    sendHTTP(id, method, url, [], body, false);
  } catch (_) {
    pendingHttp.delete(id);
    setTimeout(function () { cb(0, '', ''); }, 0);
  }
  return id;
}

function bodyToString(body) {
  if (body === null || body === undefined) return null;
  if (body instanceof FormData) return body._toQuery();
  if (body instanceof URLSearchParams) return body.toString();
  if (body instanceof Blob) return body._text;
  if (typeof body === 'string') return body;
  if (typeof ArrayBuffer !== 'undefined' && body instanceof ArrayBuffer) {
    return new TextDecoder().decode(new Uint8Array(body));
  }
  if (body && body.buffer && body.byteLength !== undefined) {
    return new TextDecoder().decode(body);
  }
  return String(body);
}

/* ---------------------------------------------------------------- Headers */

class Headers {
  constructor(init) {
    this._map = new Map();
    if (!init) return;
    if (init instanceof Headers) {
      var self = this;
      init.forEach(function (v, k) { self._map.set(k, v); });
    } else if (typeof init === 'object' && typeof init[Symbol.iterator] === 'function' &&
               typeof init !== 'string') {
      var it = init[Symbol.iterator](), step;
      while (!(step = it.next()).done) {
        var pair = step.value;
        if (pair) this.append(pair[0], pair[1]);
      }
    } else if (typeof init === 'object') {
      var keys = Object.keys(init);
      for (var i = 0; i < keys.length; i++) this.append(keys[i], init[keys[i]]);
    }
  }
  _norm(k) { return String(k).toLowerCase(); }
  get(k) { var v = this._map.get(this._norm(k)); return v === undefined ? null : v; }
  set(k, v) { this._map.set(this._norm(k), String(v)); }
  append(k, v) {
    var key = this._norm(k), val = String(v);
    if (this._map.has(key)) this._map.set(key, this._map.get(key) + ', ' + val);
    else this._map.set(key, val);
  }
  has(k) { return this._map.has(this._norm(k)); }
  'delete'(k) { this._map.delete(this._norm(k)); }
  forEach(cb, thisArg) {
    var self = this;
    this._map.forEach(function (v, k) { cb.call(thisArg, v, k, self); });
  }
  entries() { return this._map.entries(); }
  keys() { return this._map.keys(); }
  values() { return this._map.values(); }
  [Symbol.iterator]() { return this._map.entries(); }
  getSetCookie() {
    var v = this.get('set-cookie');
    return v === null ? [] : [v];
  }
}
defineTag(Headers.prototype, 'Headers');

/* ---------------------------------------------------------------- FormData */

class FormData {
  constructor() { this._list = []; }
  append(name, value) {
    this._list.push([String(name), value instanceof Blob ? value._text : String(value)]);
  }
  set(name, value) {
    name = String(name);
    this['delete'](name);
    this.append(name, value);
  }
  get(name) {
    name = String(name);
    for (var i = 0; i < this._list.length; i++) if (this._list[i][0] === name) return this._list[i][1];
    return null;
  }
  getAll(name) {
    name = String(name);
    var out = [];
    for (var i = 0; i < this._list.length; i++) if (this._list[i][0] === name) out.push(this._list[i][1]);
    return out;
  }
  has(name) {
    name = String(name);
    for (var i = 0; i < this._list.length; i++) if (this._list[i][0] === name) return true;
    return false;
  }
  'delete'(name) {
    name = String(name);
    for (var i = this._list.length - 1; i >= 0; i--) {
      if (this._list[i][0] === name) this._list.splice(i, 1);
    }
  }
  forEach(cb, thisArg) {
    for (var i = 0; i < this._list.length; i++) {
      cb.call(thisArg, this._list[i][1], this._list[i][0], this);
    }
  }
  entries() {
    var l = this._list, i = 0;
    return { next: function () { return i < l.length ? { value: [l[i][0], l[i][1]], done: false } : { value: undefined, done: true }; },
             [Symbol.iterator]: function () { return this; } };
  }
  keys() {
    var l = this._list, i = 0;
    return { next: function () { return i < l.length ? { value: l[i++][0], done: false } : { value: undefined, done: true }; },
             [Symbol.iterator]: function () { return this; } };
  }
  values() {
    var l = this._list, i = 0;
    return { next: function () { return i < l.length ? { value: l[i++][1], done: false } : { value: undefined, done: true }; },
             [Symbol.iterator]: function () { return this; } };
  }
  [Symbol.iterator]() { return this.entries(); }
  _toQuery() {
    var out = [];
    for (var i = 0; i < this._list.length; i++) {
      out.push(urlEncode(this._list[i][0]) + '=' + urlEncode(this._list[i][1]));
    }
    return out.join('&');
  }
}
defineTag(FormData.prototype, 'FormData');

/* ---------------------------------------------------------------- Abort */

class AbortSignal extends EventTarget {
  constructor() {
    super();
    this.aborted = false;
    this.reason = undefined;
  }
  throwIfAborted() { if (this.aborted) throw this.reason; }
  static abort(reason) {
    var s = new AbortSignal();
    s.aborted = true;
    s.reason = reason === undefined ? new DOMException('Aborted', 'AbortError') : reason;
    return s;
  }
  static timeout(ms) {
    var s = new AbortSignal();
    setTimeout(function () {
      if (!s.aborted) {
        s.aborted = true;
        s.reason = new DOMException('Timeout', 'TimeoutError');
        var ev = new Event('abort');
        dispatchEventOn(s, ev);
      }
    }, Number(ms) || 0);
    return s;
  }
}
defineTag(AbortSignal.prototype, 'AbortSignal');

class AbortController {
  constructor() { this.signal = new AbortSignal(); }
  abort(reason) {
    if (this.signal.aborted) return;
    this.signal.aborted = true;
    this.signal.reason = reason === undefined ? new DOMException('The operation was aborted', 'AbortError') : reason;
    dispatchEventOn(this.signal, new Event('abort'));
  }
}
defineTag(AbortController.prototype, 'AbortController');

/* ---------------------------------------------------------------- Response / Request */

class Response {
  constructor(body, init) {
    init = init || {};
    this.status = init.status === undefined ? 200 : Number(init.status);
    this.ok = this.status >= 200 && this.status < 300;
    this.statusText = init.statusText === undefined ? '' : String(init.statusText);
    this.headers = init.headers instanceof Headers ? init.headers : new Headers(init.headers);
    this.url = init.url === undefined ? '' : String(init.url);
    this.redirected = !!init.redirected;
    this.type = init.type === undefined ? 'basic' : String(init.type);
    this.body = null;
    this.bodyUsed = false;
    this._body = body === null || body === undefined ? '' : bodyToString(body);
  }
  get bodyUsed() { return this._bodyUsed; }
  set bodyUsed(v) { this._bodyUsed = !!v; }
  _consume() {
    if (this._bodyUsed) return Promise.reject(new TypeError('Body already used'));
    this._bodyUsed = true;
    return null;
  }
  text() {
    var r = this._consume();
    if (r) return r;
    return Promise.resolve(this._body === null ? '' : this._body);
  }
  json() {
    var r = this._consume();
    if (r) return r;
    var self = this;
    return new Promise(function (resolve, reject) {
      try { resolve(JSON.parse(self._body === null ? '' : self._body)); }
      catch (e) { reject(e); }
    });
  }
  arrayBuffer() {
    var r = this._consume();
    if (r) return r;
    var u = new TextEncoder().encode(this._body === null ? '' : this._body);
    return Promise.resolve(u.buffer.slice(u.byteOffset, u.byteOffset + u.byteLength));
  }
  blob() {
    var r = this._consume();
    if (r) return r;
    var ct = this.headers.get('content-type') || '';
    return Promise.resolve(new Blob([this._body === null ? '' : this._body], { type: ct }));
  }
  clone() {
    return new Response(this._body, {
      status: this.status, statusText: this.statusText,
      headers: this.headers, url: this.url, type: this.type
    });
  }
  static json(data, init) {
    init = init || {};
    var h = init.headers instanceof Headers ? init.headers : new Headers(init.headers);
    if (!h.has('content-type')) h.set('content-type', 'application/json');
    return new Response(JSON.stringify(data), {
      status: init.status === undefined ? 200 : init.status,
      statusText: init.statusText, headers: h
    });
  }
  static error() {
    return new Response('', { status: 0, statusText: '', type: 'error' });
  }
  static redirect(url, status) {
    var h = new Headers();
    h.set('location', String(url));
    return new Response('', { status: status === undefined ? 302 : status, headers: h });
  }
}
defineTag(Response.prototype, 'Response');

class Request {
  constructor(input, init) {
    init = init || {};
    var src = input instanceof Request ? input : null;
    var rawUrl = src ? src.url : String(input);
    this._url = resolveURL(rawUrl, currentHref());
    this.method = String(init.method || (src ? src.method : 'GET')).toUpperCase();
    this.headers = init.headers instanceof Headers ? init.headers : new Headers(init.headers);
    this.signal = init.signal || (src ? src.signal : null) || new AbortSignal();
    this.credentials = init.credentials || (src ? src.credentials : 'same-origin');
    this.mode = init.mode || (src ? src.mode : 'cors');
    this.cache = init.cache || (src ? src.cache : 'default');
    this.redirect = init.redirect || (src ? src.redirect : 'follow');
    this.referrer = init.referrer === undefined ? (src ? src.referrer : 'about:client') : init.referrer;
    this.referrerPolicy = init.referrerPolicy || (src ? src.referrerPolicy : '');
    this.integrity = init.integrity || (src ? src.integrity : '');
    this.keepalive = !!init.keepalive;
    this.destination = '';
    this.body = null;
    this._bodyUsed = false;
    var b = init.body !== undefined ? init.body : (src ? src._body : null);
    this._rawBody = b === null || b === undefined ? null : b;
    this._body = b === null || b === undefined ? null : bodyToString(b);
  }
  get url() { return this._url; }
  get bodyUsed() { return this._bodyUsed; }
  _consume() {
    if (this._bodyUsed) return Promise.reject(new TypeError('Body already used'));
    this._bodyUsed = true;
    return null;
  }
  clone() { return new Request(this); }
  text() {
    var r = this._consume();
    if (r) return r;
    return Promise.resolve(this._body === null ? '' : this._body);
  }
  json() {
    var r = this._consume();
    if (r) return r;
    var self = this;
    return new Promise(function (resolve, reject) {
      try { resolve(JSON.parse(self._body === null ? '' : self._body)); }
      catch (e) { reject(e); }
    });
  }
  arrayBuffer() {
    var r = this._consume();
    if (r) return r;
    var u = new TextEncoder().encode(this._body === null ? '' : this._body);
    return Promise.resolve(u.buffer.slice(u.byteOffset, u.byteOffset + u.byteLength));
  }
  blob() {
    var r = this._consume();
    if (r) return r;
    return Promise.resolve(new Blob([this._body === null ? '' : this._body]));
  }
}
defineTag(Request.prototype, 'Request');

/* ---------------------------------------------------------------- XMLHttpRequest */

class XMLHttpRequest extends EventTarget {
  constructor() {
    super();
    this.readyState = 0;
    this.status = 0;
    this.statusText = '';
    this.responseText = '';
    this.response = '';
    this.responseType = '';
    this.responseURL = '';
    this.responseXML = null;
    this.timeout = 0;
    this.withCredentials = false;
    this.upload = new EventTarget();
    this._method = 'GET';
    this._url = '';
    this._headers = {};
    this._contentType = '';
    this._overrideMime = '';
    this._aborted = false;
    this._done = false;
    this._id = 0;
    this._timeoutTimer = 0;
    this._bodyUsed = false;
  }
  _fire(type, init) {
    var ev = new Event(type, init || {});
    dispatchEventOn(this, ev);
    return !ev.defaultPrevented;
  }
  open(method, url, async, user, password) {
    this._method = String(method || 'GET').toUpperCase();
    try { this._url = resolveURL(String(url), currentHref()); }
    catch (_) { this._url = String(url); }
    this._async = async === undefined ? true : !!async;
    this._user = user;
    this._password = password;
    this._aborted = false;
    this._done = false;
    this._headers = {};
    this.status = 0;
    this.statusText = '';
    this.responseText = '';
    this.response = '';
    this._contentType = '';
    this.readyState = 1;
    this._fire('readystatechange');
  }
  setRequestHeader(name, value) {
    this._headers[String(name).toLowerCase()] = String(value);
  }
  overrideMimeType(mime) { this._overrideMime = String(mime); }
  getResponseHeader(name) {
    var k = String(name).toLowerCase();
    if (this._respHeaders) {
      var v = this._respHeaders.filter(function (h) { return h[0] === k; }).map(function (h) { return h[1]; });
      return v.length ? v.join(', ') : null;
    }
    if (k === 'content-type') return this._contentType || null;
    return null;
  }
  getAllResponseHeaders() {
    if (this._respHeaders) return this._respHeaders.map(function (h) { return h[0] + ': ' + h[1] + '\r\n'; }).join('');
    if (!this._contentType) return '';
    return 'content-type: ' + this._contentType + '\r\n';
  }
  _computeResponse(text) {
    var rt = this.responseType;
    if (rt === '' || rt === 'text') {
      this.response = text;
    } else if (rt === 'json') {
      try { this.response = JSON.parse(text); } catch (_) { this.response = null; }
    } else if (rt === 'arraybuffer' && text instanceof ArrayBuffer) {
      this.response = text;
    } else if (rt === 'blob' && text instanceof ArrayBuffer) {
      this.response = new Blob([new TextDecoder().decode(new Uint8Array(text))], { type: this._contentType });
    } else if (rt === 'arraybuffer') {
      var u = new TextEncoder().encode(text);
      this.response = u.buffer.slice(u.byteOffset, u.byteOffset + u.byteLength);
    } else if (rt === 'blob') {
      this.response = new Blob([text], { type: this._contentType });
    } else if (rt === 'document') {
      this.response = null;
    } else {
      this.response = text;
    }
  }
  _onDone(status, text, type, ph, finalUrl) {
    if (this._aborted || this._done) return;
    this._done = true;
    if (this._timeoutTimer) { clearTimeout(this._timeoutTimer); this._timeoutTimer = 0; }
    this.status = status;
    this._respHeaders = ph ? ph.list : null;
    this.statusText = ph && ph.statusText ? ph.statusText : (status === 200 ? 'OK' : (status ? String(status) : ''));
    this._contentType = this._overrideMime || type || '';
    this.responseURL = finalUrl || this._url;
    if (text instanceof ArrayBuffer) {
      /* respuesta binaria: responseText no aplica */
      this.readyState = 2;
      this._fire('readystatechange');
      this.readyState = 3;
      this._fire('readystatechange');
      this._fire('progress', { lengthComputable: true, loaded: text.byteLength, total: text.byteLength });
      this._computeResponse(text);
      this.readyState = 4;
      this._fire('readystatechange');
      this._fire(status ? 'load' : 'error');
      this._fire('loadend');
      return;
    }
    this.readyState = 2;
    this._fire('readystatechange');
    this.responseText = text;
    this.readyState = 3;
    this._fire('readystatechange');
    this._fire('progress', { lengthComputable: true, loaded: text.length, total: text.length });
    this._computeResponse(text);
    this.readyState = 4;
    this._fire('readystatechange');
    if (status) this._fire('load');
    else this._fire('error');
    this._fire('loadend');
  }
  send(body) {
    if (this.readyState !== 1) throw new DOMException('XMLHttpRequest: not opened', 'InvalidStateError');
    this._aborted = false;
    this._done = false;
    var b = (this._method === 'GET' || this._method === 'HEAD') ? null : body;
    var hp = Object.keys(this._headers).map(function (k) { return [k, this._headers[k]]; }, this);
    var binary = this.responseType === 'arraybuffer' || this.responseType === 'blob';
    var self = this;
    var id = nextHttpId++;
    this._id = id;
    pendingHttp.set(id, function (status, text, type, ph, finalUrl) { self._onDone(status, text, type, ph, finalUrl); });
    if (this.timeout > 0) {
      this._timeoutTimer = setTimeout(function () {
        if (self._done || self._aborted) return;
        self._aborted = true;
        pendingHttp.delete(id);
        self.readyState = 4;
        self.status = 0;
        self._fire('readystatechange');
        self._fire('timeout');
        self._fire('loadend');
      }, this.timeout);
    }
    try { sendHTTP(id, this._method, this._url, hp, b, binary); }
    catch (_) {
      pendingHttp.delete(id);
      setTimeout(function () { self._onDone(0, '', ''); }, 0);
    }
  }
  abort() {
    if (this._aborted) return;
    this._aborted = true;
    if (this._id) { pendingHttp.delete(this._id); try { if (ns.httpAbort) ns.httpAbort(this._id); } catch (_) {} this._id = 0; }
    if (this._timeoutTimer) { clearTimeout(this._timeoutTimer); this._timeoutTimer = 0; }
    var was = this.readyState;
    this.readyState = 0;
    this.status = 0;
    this.statusText = '';
    if (was !== 0) {
      this._fire('readystatechange');
      this._fire('abort');
      this._fire('loadend');
    }
  }
}
XMLHttpRequest.UNSENT = 0;
XMLHttpRequest.OPENED = 1;
XMLHttpRequest.HEADERS_RECEIVED = 2;
XMLHttpRequest.LOADING = 3;
XMLHttpRequest.DONE = 4;
(function () {
  var names = ['UNSENT', 'OPENED', 'HEADERS_RECEIVED', 'LOADING', 'DONE'];
  for (var i = 0; i < names.length; i++) {
    try {
      Object.defineProperty(XMLHttpRequest.prototype, names[i],
        { value: XMLHttpRequest[names[i]], enumerable: false, configurable: true });
    } catch (_) {}
  }
})();
defineTag(XMLHttpRequest.prototype, 'XMLHttpRequest');

/* ---------------------------------------------------------------- fetch */

function fetch(input, init) {
  init = init || {};
  var req;
  try { req = new Request(input, init); }
  catch (e) { return Promise.reject(e); }
  return new Promise(function (resolve, reject) {
    var signal = init.signal || req.signal;
    if (signal && signal.aborted) {
      reject(new DOMException('Aborted', 'AbortError'));
      return;
    }
    var settled = false;
    function onAbort() {
      if (settled) return;
      settled = true;
      reject(new DOMException('Aborted', 'AbortError'));
    }
    if (signal) addListener(signal, 'abort', onAbort, false);
    var body = (req.method === 'GET' || req.method === 'HEAD') ? null : req._rawBody;
    var hp = [];
    try { req.headers.forEach(function (v, k) { hp.push([k, v]); }); } catch (_) {}
    var id = nextHttpId++;
    if (signal) addListener(signal, 'abort', function () { try { if (ns.httpAbort) ns.httpAbort(id); } catch (_) {} }, false);
    pendingHttp.set(id, function (status, text, type, ph, finalUrl) {
      pendingHttp.delete(id);
      if (signal) removeListener(signal, 'abort', onAbort, false);
      if (settled) return;
      settled = true;
      if (!status) { reject(new TypeError('Failed to fetch')); return; }
      var h = new Headers();
      if (ph) ph.list.forEach(function (x) { try { h.append(x[0], x[1]); } catch (_) {} });
      else if (type) h.set('content-type', type);
      var res = new Response(text, {
        status: status,
        statusText: ph && ph.statusText ? ph.statusText : (status === 200 ? 'OK' : ''),
        headers: h,
        url: finalUrl || req.url
      });
      try { Object.defineProperty(res, 'redirected', { value: !!finalUrl && finalUrl !== req.url }); } catch (_) {}
      resolve(res);
    });
    try {
      sendHTTP(id, req.method, req.url, hp, body, false);
    } catch (_) {
      pendingHttp.delete(id);
      if (signal) removeListener(signal, 'abort', onAbort, false);
      if (!settled) { settled = true; reject(new TypeError('Failed to fetch')); }
    }
  });
}

/* ---------------------------------------------------------------- observers de geometria */

class IntersectionObserver {
  constructor(cb, options) {
    if (typeof cb !== 'function') throw new TypeError('IntersectionObserver: callback required');
    this._cb = cb;
    this.root = options && options.root ? options.root : null;
    this.rootMargin = options && options.rootMargin ? String(options.rootMargin) : '0px';
    this.thresholds = options && options.threshold ? [].concat(options.threshold) : [0];
    this._targets = [];
    this._records = [];
  }
  observe(el) {
    if (!isNode(el)) return;
    if (this._targets.indexOf(el) >= 0) return;
    this._targets.push(el);
    var self = this;
    queueMicrotask(function () {
      if (self._targets.indexOf(el) < 0) return;
      var r = el.getBoundingClientRect ? el.getBoundingClientRect() : null;
      var entry = {
        target: el, isIntersecting: true, intersectionRatio: 1,
        boundingClientRect: r, intersectionRect: r, rootBounds: null,
        time: performanceObj.now()
      };
      try { self._cb([entry], self); } catch (e) { log('IntersectionObserver: ' + errText(e)); }
    });
  }
  unobserve(el) {
    var i = this._targets.indexOf(el);
    if (i >= 0) this._targets.splice(i, 1);
  }
  disconnect() { this._targets = []; }
  takeRecords() { var r = this._records; this._records = []; return r; }
}
defineTag(IntersectionObserver.prototype, 'IntersectionObserver');

class ResizeObserver {
  constructor(cb) {
    if (typeof cb !== 'function') throw new TypeError('ResizeObserver: callback required');
    this._cb = cb;
    this._targets = [];
    this._records = [];
  }
  observe(el) {
    if (!isNode(el)) return;
    if (this._targets.indexOf(el) >= 0) return;
    this._targets.push(el);
    var self = this;
    queueMicrotask(function () {
      if (self._targets.indexOf(el) < 0) return;
      var r = el.getBoundingClientRect ? el.getBoundingClientRect() : { width: 0, height: 0 };
      var entry = {
        target: el, contentRect: r,
        borderBoxSize: [{ inlineSize: r.width, blockSize: r.height }],
        contentBoxSize: [{ inlineSize: r.width, blockSize: r.height }],
        devicePixelContentBoxSize: [{ inlineSize: r.width, blockSize: r.height }]
      };
      try { self._cb([entry], self); } catch (e) { log('ResizeObserver: ' + errText(e)); }
    });
  }
  unobserve(el) {
    var i = this._targets.indexOf(el);
    if (i >= 0) this._targets.splice(i, 1);
  }
  disconnect() { this._targets = []; }
  takeRecords() { var r = this._records; this._records = []; return r; }
}
defineTag(ResizeObserver.prototype, 'ResizeObserver');

/* ---------------------------------------------------------------- customElements */

/* ---------------------------------------------------------------- customElements */
/* Elementos personalizados de verdad: define() mejora los que ya existen,
 * el parser y las inserciones llaman connectedCallback y compania. */

var shadowRoots = new WeakMap();
var ceDefs = new Map();      /* nombre -> {name, ctor, observed} */
var ceByCtor = new Map();    /* constructor -> definicion */
var ceDone = new WeakSet();  /* elementos ya construidos */
var ceUpgrading = null;
var ceWaiting = new Map();

function ceDefFor(el) {
  if (!ceDefs.size || ns.type(el) !== ELEMENT_NODE) return null;
  var n = ns.name(el).toLowerCase();
  return n.indexOf('-') > 0 ? ceDefs.get(n) || null : null;
}
function ceCall(el, cb, args) {
  if (typeof el[cb] !== 'function') return;
  try { el[cb].apply(el, args || []); } catch (e) { log(cb + ': ' + errText(e)); }
}
function ceConnected(el) {
  for (var n = el; n; n = ns.parent(n)) if (ns.type(n) === DOCUMENT_NODE) return true;
  return false;
}
function ceUpgrade(el, def) {
  def = def || ceDefFor(el);
  if (!def || ceDone.has(el)) return;
  ceDone.add(el);
  Object.setPrototypeOf(el, def.ctor.prototype);
  ceUpgrading = el;
  try { new def.ctor(); } catch (e) { log('custom element ' + def.name + ': ' + errText(e)); }
  ceUpgrading = null;
  def.observed.forEach(function (a) {
    var v = ns.getAttr(el, a);
    if (v !== null) ceCall(el, 'attributeChangedCallback', [a, null, v]);
  });
  if (ceConnected(el)) ceCall(el, 'connectedCallback');
}
/* despues de insertar: mejorar y conectar los elementos personalizados del subarbol */
function ceInserted(node) {
  if (!ceDefs.size || !node) return;
  var t = ns.type(node);
  if (t !== ELEMENT_NODE || !ceConnected(node)) return;
  [node].concat(ns.byTag(node, '*')).forEach(function (el) {
    var def = ceDefFor(el);
    if (!def) return;
    if (!ceDone.has(el)) ceUpgrade(el, def);
    else ceCall(el, 'connectedCallback');
  });
}
function ceRemoved(node) {
  if (!ceDefs.size || !node || ns.type(node) !== ELEMENT_NODE) return;
  [node].concat(ns.byTag(node, '*')).forEach(function (el) {
    if (ceDone.has(el) && ceDefFor(el)) ceCall(el, 'disconnectedCallback');
  });
}
function ceAttr(el, name, oldV, newV) {
  var def = ceDone.has(el) ? ceDefFor(el) : null;
  if (def && def.observed.indexOf(name) >= 0 && oldV !== newV) {
    ceCall(el, 'attributeChangedCallback', [name, oldV, newV]);
  }
}

/* Envolver las primitivas del C: todo lo que inserta, saca o cambia atributos
 * (appendChild, innerHTML, append, setAttribute...) pasa por aca. */
(function () {
  var A = ns.append, I = ns.insert, R = ns.remove, P = ns.replace, H = ns.setInnerHTML,
      S = ns.setAttr, D = ns.removeAttr;
  function kidsIfFragment(c) {
    return ceDefs.size && ns.type(c) === DOCUMENT_FRAGMENT_NODE ? ns.children(c) : null;
  }
  ns.append = function (p, c) {
    var frag = kidsIfFragment(c);
    var r = A(p, c);
    if (ceDefs.size) { if (frag) frag.forEach(ceInserted); else ceInserted(c); }
    return r;
  };
  ns.insert = function (p, c, ref) {
    var frag = kidsIfFragment(c);
    var r = I(p, c, ref);
    if (ceDefs.size) { if (frag) frag.forEach(ceInserted); else ceInserted(c); }
    return r;
  };
  ns.remove = function (p, c) {
    var wasIn = ceDefs.size && ceConnected(p);
    var r = R(p, c);
    if (wasIn) ceRemoved(c);
    return r;
  };
  ns.replace = function (p, nw, old) {
    var wasIn = ceDefs.size && ceConnected(p);
    var r = P(p, nw, old);
    if (wasIn) ceRemoved(old);
    if (ceDefs.size) ceInserted(nw);
    return r;
  };
  ns.setInnerHTML = function (n, html) {
    var r = H(n, html);
    if (ceDefs.size) ns.children(n).forEach(ceInserted);
    return r;
  };
  ns.setAttr = function (n, k, v) {
    var old = ceDefs.size ? ns.getAttr(n, k) : null;
    var r = S(n, k, v);
    if (ceDefs.size) ceAttr(n, String(k), old, String(v));
    return r;
  };
  ns.removeAttr = function (n, k) {
    var old = ceDefs.size ? ns.getAttr(n, k) : null;
    var r = D(n, k);
    if (ceDefs.size) ceAttr(n, String(k), old, null);
    return r;
  };
})();

var customElementsObj = {
  define: function (name, ctor) {
    name = String(name).toLowerCase();
    if (ceDefs.has(name)) {
      throw new DOMException('customElements: ya definido ' + name, 'NotSupportedError');
    }
    if (typeof ctor !== 'function') throw new TypeError('customElements.define: falta la clase');
    var def = { name: name, ctor: ctor, observed: Array.from(ctor.observedAttributes || []).map(String) };
    ceDefs.set(name, def);
    ceByCtor.set(ctor, def);
    var d = getDocument();
    if (d) byTagAll(d, name).forEach(function (el) { ceUpgrade(el, def); });
    var w = ceWaiting.get(name);
    if (w) { w.resolve(ctor); ceWaiting['delete'](name); }
  },
  get: function (name) { var d = ceDefs.get(String(name).toLowerCase()); return d ? d.ctor : undefined; },
  getName: function (ctor) { var d = ceByCtor.get(ctor); return d ? d.name : null; },
  whenDefined: function (name) {
    name = String(name).toLowerCase();
    if (ceDefs.has(name)) return Promise.resolve(ceDefs.get(name).ctor);
    if (!ceWaiting.has(name)) {
      var res;
      var pr = new Promise(function (r) { res = r; });
      ceWaiting.set(name, { promise: pr, resolve: res });
    }
    return ceWaiting.get(name).promise;
  },
  upgrade: function (root) {
    if (root) [root].concat(byTagAll(root, '*')).forEach(function (el) { ceUpgrade(el); });
  }
};

/* ---------------------------------------------------------------- temporizadores */

var timerMap = new Map();
var nextTimerId = 1;

function toTimerFn(fn) {
  if (typeof fn === 'function') return fn;
  if (typeof fn === 'string') {
    try {
      var v = (0, eval)('(function(event){' + fn + '\n})');
      if (typeof v === 'function') return v;
    } catch (e) { log('setTimeout string: ' + errText(e)); }
  }
  return null;
}

function setTimeout(fn, ms) {
  var f = toTimerFn(fn);
  if (!f) return 0;
  var args = Array.prototype.slice.call(arguments, 2);
  var id = nextTimerId++;
  var delay = Math.max(0, Number(ms) || 0);
  timerMap.set(id, { fn: f, args: args, ms: delay, repeat: false });
  try { ns.setTimer(id, delay); } catch (_) {}
  return id;
}

function setInterval(fn, ms) {
  var f = toTimerFn(fn);
  if (!f) return 0;
  var args = Array.prototype.slice.call(arguments, 2);
  var id = nextTimerId++;
  var delay = Math.max(0, Number(ms) || 0);
  timerMap.set(id, { fn: f, args: args, ms: delay, repeat: true });
  try { ns.setTimer(id, delay); } catch (_) {}
  return id;
}

function clearTimeout(id) {
  id = Number(id);
  if (!timerMap.has(id)) return;
  timerMap['delete'](id);
  try { ns.clearTimer(id); } catch (_) {}
}
var clearInterval = clearTimeout;

function setImmediate(fn) {
  var args = Array.prototype.slice.call(arguments, 1);
  return setTimeout.apply(null, [fn, 0].concat(args));
}
var clearImmediate = clearTimeout;

function requestAnimationFrame(cb) {
  return setTimeout(function () {
    try { cb(performanceObj.now()); } catch (e) { log('rAF: ' + errText(e)); }
  }, 16);
}
function cancelAnimationFrame(id) { clearTimeout(id); }

function requestIdleCallback(cb) {
  return setTimeout(function () {
    try {
      cb({ didTimeout: false, timeRemaining: function () { return 50; } });
    } catch (e) { log('rIC: ' + errText(e)); }
  }, 1);
}
function cancelIdleCallback(id) { clearTimeout(id); }

function queueMicrotaskFn(fn) {
  if (typeof fn !== 'function') throw new TypeError('queueMicrotask: function required');
  if (typeof G.queueMicrotask === 'function' && G.queueMicrotask !== queueMicrotaskFn) {
    return G.queueMicrotask(fn);
  }
  Promise.resolve().then(function () {
    try { fn(); } catch (e) { log('microtask: ' + errText(e)); }
  });
}

/* ---------------------------------------------------------------- globales */

function defGlobal(name, value) {
  try {
    Object.defineProperty(G, name,
      { value: value, writable: true, configurable: true, enumerable: true });
  } catch (_) {
    try { G[name] = value; } catch (_) {}
  }
}
function defGlobalGetter(name, getter, setter) {
  try {
    Object.defineProperty(G, name,
      { get: getter, set: setter, configurable: true, enumerable: true });
  } catch (_) {}
}

function getComputedStyle(el) { return makeStyle(el); }

function alert(msg) { log('alert: ' + String(msg)); }
function confirm(msg) { log('confirm: ' + String(msg)); return false; }
function prompt(msg, def) { log('prompt: ' + String(msg)); return null; }

function Image(width, height) {
  var d = getDocument();
  if (!d) throw new TypeError('no document');
  var el = ns.createElement('img');
  if (width !== undefined) setAttrNotify(el, 'width', String(width));
  if (height !== undefined) setAttrNotify(el, 'height', String(height));
  return el;
}
Image.prototype = HTMLImageElement.prototype;

var CSSObj = {
  escape: function (s) {
    return String(s).replace(/[^a-zA-Z0-9_\u00A0-\uFFFF-]/g, function (c) {
      return '\\' + c;
    });
  },
  supports: function () { return true; }
};

var NodeFilterObj = {
  FILTER_ACCEPT: 1, FILTER_REJECT: 2, FILTER_SKIP: 3,
  SHOW_ALL: 0xFFFFFFFF, SHOW_ELEMENT: 1, SHOW_ATTRIBUTE: 2, SHOW_TEXT: 4,
  SHOW_CDATA_SECTION: 8, SHOW_ENTITY_REFERENCE: 16, SHOW_ENTITY: 32,
  SHOW_PROCESSING_INSTRUCTION: 64, SHOW_COMMENT: 128, SHOW_DOCUMENT: 256,
  SHOW_DOCUMENT_TYPE: 512, SHOW_DOCUMENT_FRAGMENT: 1024, SHOW_NOTATION: 2048
};

function installGlobals() {
  var i;

  defGlobal('window', G);
  defGlobal('self', G);
  defGlobal('frames', G);
  defGlobal('parent', G);
  defGlobal('top', G);
  defGlobal('globalThis', G);

  defGlobal('addEventListener', function (type, fn, opts) { addListener(G, type, fn, opts); });
  defGlobal('removeEventListener', function (type, fn, opts) { removeListener(G, type, fn, opts); });
  defGlobal('dispatchEvent', function (ev) { return dispatchEventOn(G, ev); });

  defGlobalGetter('document', getDocument, function () {});
  defGlobalGetter('location', function () { return locationObj; }, function (v) { navigateTo(v); });

  defGlobal('history', new History());
  defGlobal('navigator', navigatorObj);
  defGlobal('screen', screenObj);
  defGlobal('console', consoleObj);
  defGlobal('performance', performanceObj);
  defGlobal('crypto', cryptoObj);
  defGlobal('localStorage', localStorageObj);
  defGlobal('sessionStorage', sessionStorageObj);

  defGlobal('innerWidth', 1280);
  defGlobal('innerHeight', 668);
  defGlobal('outerWidth', 1280);
  defGlobal('outerHeight', 720);
  defGlobal('devicePixelRatio', 1);
  defGlobal('screenX', 0);
  defGlobal('screenY', 0);
  defGlobal('screenLeft', 0);
  defGlobal('screenTop', 0);
  defGlobal('scrollX', 0);
  defGlobal('scrollY', 0);
  defGlobal('pageXOffset', 0);
  defGlobal('pageYOffset', 0);
  defGlobal('name', '');
  defGlobal('closed', false);
  defGlobal('length', 0);
  defGlobal('opener', null);
  defGlobal('isSecureContext', true);
  defGlobal('crossOriginIsolated', false);
  defGlobal('frameElement', null);

  defGlobal('setTimeout', setTimeout);
  defGlobal('setInterval', setInterval);
  defGlobal('clearTimeout', clearTimeout);
  defGlobal('clearInterval', clearInterval);
  defGlobal('setImmediate', setImmediate);
  defGlobal('clearImmediate', clearImmediate);
  defGlobal('requestAnimationFrame', requestAnimationFrame);
  defGlobal('cancelAnimationFrame', cancelAnimationFrame);
  defGlobal('requestIdleCallback', requestIdleCallback);
  defGlobal('cancelIdleCallback', cancelIdleCallback);
  defGlobal('queueMicrotask', queueMicrotaskFn);

  defGlobal('alert', alert);
  defGlobal('confirm', confirm);
  defGlobal('prompt', prompt);
  defGlobal('getComputedStyle', getComputedStyle);
  defGlobal('matchMedia', matchMedia);

  defGlobal('atob', atob);
  defGlobal('btoa', btoa);
  defGlobal('structuredClone', structuredClone);

  defGlobal('fetch', fetch);
  defGlobal('XMLHttpRequest', XMLHttpRequest);
  defGlobal('Headers', Headers);
  defGlobal('Request', Request);
  defGlobal('Response', Response);
  defGlobal('FormData', FormData);
  defGlobal('Blob', Blob);
  defGlobal('URLSearchParams', URLSearchParams);
  defGlobal('URL', URL);
  defGlobal('AbortController', AbortController);
  defGlobal('AbortSignal', AbortSignal);
  defGlobal('DOMException', DOMException);
  defGlobal('TextEncoder', TextEncoder);
  defGlobal('TextDecoder', TextDecoder);

  defGlobal('MutationObserver', MutationObserver);
  defGlobal('IntersectionObserver', IntersectionObserver);
  defGlobal('ResizeObserver', ResizeObserver);
  defGlobal('customElements', customElementsObj);

  defGlobal('Event', Event);
  defGlobal('CustomEvent', CustomEvent);
  defGlobal('UIEvent', UIEvent);
  defGlobal('MouseEvent', MouseEvent);
  defGlobal('KeyboardEvent', KeyboardEvent);
  defGlobal('FocusEvent', FocusEvent);
  defGlobal('InputEvent', InputEvent);
  defGlobal('EventTarget', EventTarget);

  defGlobal('Node', Node);
  defGlobal('Element', Element);
  defGlobal('HTMLElement', HTMLElement);
  defGlobal('HTMLAnchorElement', HTMLAnchorElement);
  defGlobal('HTMLImageElement', HTMLImageElement);
  defGlobal('HTMLInputElement', HTMLInputElement);
  defGlobal('HTMLTextAreaElement', HTMLTextAreaElement);
  defGlobal('HTMLSelectElement', HTMLSelectElement);
  defGlobal('HTMLOptionElement', HTMLOptionElement);
  defGlobal('HTMLFormElement', HTMLFormElement);
  defGlobal('HTMLButtonElement', HTMLButtonElement);
  defGlobal('HTMLScriptElement', HTMLScriptElement);
  defGlobal('HTMLStyleElement', HTMLStyleElement);
  defGlobal('HTMLLinkElement', HTMLLinkElement);
  defGlobal('HTMLIFrameElement', HTMLIFrameElement);
  defGlobal('HTMLCanvasElement', HTMLCanvasElement);
  defGlobal('HTMLBodyElement', HTMLBodyElement);
  defGlobal('HTMLHtmlElement', HTMLHtmlElement);
  defGlobal('HTMLHeadElement', HTMLHeadElement);
  defGlobal('HTMLTitleElement', HTMLTitleElement);
  defGlobal('HTMLDivElement', HTMLDivElement);
  defGlobal('HTMLSpanElement', HTMLSpanElement);
  defGlobal('HTMLParagraphElement', HTMLParagraphElement);
  defGlobal('HTMLUListElement', HTMLUListElement);
  defGlobal('HTMLOListElement', HTMLOListElement);
  defGlobal('HTMLLIElement', HTMLLIElement);
  defGlobal('HTMLTableElement', HTMLTableElement);
  defGlobal('HTMLTableRowElement', HTMLTableRowElement);
  defGlobal('HTMLTableCellElement', HTMLTableCellElement);
  defGlobal('HTMLLabelElement', HTMLLabelElement);
  defGlobal('HTMLMediaElement', HTMLMediaElement);
  defGlobal('HTMLVideoElement', HTMLVideoElement);
  defGlobal('HTMLAudioElement', HTMLAudioElement);
  defGlobal('HTMLTemplateElement', HTMLTemplateElement);
  defGlobal('HTMLMetaElement', HTMLMetaElement);

  defGlobal('Text', Text);
  defGlobal('Comment', Comment);
  defGlobal('Document', Document);
  defGlobal('DocumentFragment', DocumentFragment);
  defGlobal('NodeList', NodeList);
  defGlobal('HTMLCollection', HTMLCollection);
  defGlobal('DOMTokenList', DOMTokenList);
  defGlobal('CSSStyleDeclaration', CSSStyleDeclaration);
  defGlobal('Image', Image);
  defGlobal('MediaQueryList', MediaQueryList);
  defGlobal('Storage', Storage);
  defGlobal('TreeWalker', TreeWalker);
  defGlobal('NodeFilter', NodeFilterObj);
  defGlobal('CSS', CSSObj);

  defGlobal('scrollTo', function () {});
  defGlobal('scrollBy', function () {});
  defGlobal('scroll', function () {});
  defGlobal('open', function () { return null; });
  defGlobal('close', function () {});
  defGlobal('focus', function () {});
  defGlobal('blur', function () {});
  defGlobal('stop', function () {});
  defGlobal('print', function () {});
  defGlobal('postMessage', function () {});
  defGlobal('moveTo', function () {});
  defGlobal('resizeTo', function () {});
  defGlobal('getSelection', function () {
    return { rangeCount: 0, isCollapsed: true, type: 'None',
             toString: function () { return ''; }, removeAllRanges: function () {},
             addRange: function () {}, getRangeAt: function () { return null; } };
  });

  defineHandlerProps(G);
}

/* ---------------------------------------------------------------- ganchos de __ns */

var TAG_PROTOTYPES = {
  A: HTMLAnchorElement.prototype,
  IMG: HTMLImageElement.prototype,
  INPUT: HTMLInputElement.prototype,
  TEXTAREA: HTMLTextAreaElement.prototype,
  SELECT: HTMLSelectElement.prototype,
  OPTION: HTMLOptionElement.prototype,
  FORM: HTMLFormElement.prototype,
  BUTTON: HTMLButtonElement.prototype,
  SCRIPT: HTMLScriptElement.prototype,
  STYLE: HTMLStyleElement.prototype,
  LINK: HTMLLinkElement.prototype,
  IFRAME: HTMLIFrameElement.prototype,
  CANVAS: HTMLCanvasElement.prototype,
  BODY: HTMLBodyElement.prototype,
  HTML: HTMLHtmlElement.prototype,
  HEAD: HTMLHeadElement.prototype,
  TITLE: HTMLTitleElement.prototype,
  DIV: HTMLDivElement.prototype,
  SPAN: HTMLSpanElement.prototype,
  P: HTMLParagraphElement.prototype,
  UL: HTMLUListElement.prototype,
  OL: HTMLOListElement.prototype,
  LI: HTMLLIElement.prototype,
  TABLE: HTMLTableElement.prototype,
  TR: HTMLTableRowElement.prototype,
  TD: HTMLTableCellElement.prototype,
  TH: HTMLTableCellElement.prototype,
  LABEL: HTMLLabelElement.prototype,
  VIDEO: HTMLVideoElement.prototype,
  AUDIO: HTMLAudioElement.prototype,
  TEMPLATE: HTMLTemplateElement.prototype,
  META: HTMLMetaElement.prototype
};

function setupNode(obj, type, name) {
  var proto;
  if (type === ELEMENT_NODE) {
    var tag = String(name || '').toUpperCase();
    proto = TAG_PROTOTYPES[tag] || HTMLElement.prototype;
    if (tag.indexOf('-') > 0 && ceDefs.size) {
      var cdef = ceDefs.get(tag.toLowerCase());
      if (cdef) proto = cdef.ctor.prototype;
    }
  } else if (type === TEXT_NODE) {
    proto = Text.prototype;
  } else if (type === COMMENT_NODE) {
    proto = Comment.prototype;
  } else if (type === DOCUMENT_NODE) {
    proto = Document.prototype;
  } else if (type === DOCUMENT_FRAGMENT_NODE) {
    proto = DocumentFragment.prototype;
  } else {
    proto = Node.prototype;
  }
  try { Object.setPrototypeOf(obj, proto); } catch (_) {}
  nodeSet.add(obj);
  return obj;
}

function hook(name, fn) {
  ns[name] = function () {
    try { return fn.apply(this, arguments); }
    catch (e) { log('dom.js ' + name + ': ' + errText(e)); }
  };
}

function nativeEventFor(type) {
  type = String(type);
  var bubbles = true, cancelable = false;
  if (type === 'focus' || type === 'blur' || type === 'load' || type === 'scroll' ||
      type === 'mouseenter' || type === 'mouseleave' || type === 'unload' ||
      type === 'DOMContentLoaded' || type === 'readystatechange') {
    bubbles = false;
  }
  if (type === 'click' || type === 'dblclick' || type === 'submit' ||
      type.indexOf('key') === 0 || type.indexOf('mouse') === 0 ||
      type === 'contextmenu' || type === 'wheel') {
    cancelable = true;
  }
  if (type === 'click' || type === 'dblclick' || type === 'mousedown' ||
      type === 'mouseup' || type === 'mousemove' || type === 'mouseover' ||
      type === 'mouseout' || type === 'mouseenter' || type === 'mouseleave' ||
      type === 'contextmenu' || type === 'wheel') {
    return new MouseEvent(type, { bubbles: bubbles, cancelable: cancelable, button: 0 });
  }
  if (type === 'keydown' || type === 'keyup' || type === 'keypress') {
    return new KeyboardEvent(type, { bubbles: bubbles, cancelable: cancelable });
  }
  if (type === 'focus' || type === 'blur' || type === 'focusin' || type === 'focusout') {
    return new FocusEvent(type, { bubbles: bubbles, cancelable: cancelable });
  }
  if (type === 'input' || type === 'beforeinput') {
    return new InputEvent(type, { bubbles: bubbles, cancelable: cancelable });
  }
  return new Event(type, { bubbles: bubbles, cancelable: cancelable });
}

hook('setup', function (obj, type, name) {
  return setupNode(obj, type, name);
});

hook('onTimer', function (id) {
  id = Number(id);
  var t = timerMap.get(id);
  if (!t) return;
  if (!t.repeat) timerMap['delete'](id);
  try { t.fn.apply(G, t.args); }
  catch (e) { log('timer ' + id + ': ' + errText(e)); }
  if (t.repeat && timerMap.get(id) === t) {
    try { ns.setTimer(id, t.ms); } catch (_) {}
  }
});

hook('onEvent', function (target, type) {
  type = String(type);
  var ev = nativeEventFor(type);
  ev.isTrusted = true;
  var ok = dispatchEventOn(target, ev);
  return !ok;
});

hook('onFire', function (type, target) {
  type = String(type);
  if (type === 'load') {
    var d = getDocument();
    if (d) {
      var dc = new Event('DOMContentLoaded', { bubbles: true, cancelable: false });
      dc.isTrusted = true;
      dispatchEventOn(d, dc);
    }
    uiState.readyState = 'complete';
    if (d) {
      var re = new Event('readystatechange', { bubbles: false, cancelable: false });
      re.isTrusted = true;
      dispatchEventOn(d, re);
    }
    var ev = new Event('load', { bubbles: false, cancelable: false });
    ev.isTrusted = true;
    var ok = dispatchEventOn(G, ev);
    if (d) {
      var body = d.body;
      if (body) {
        var h = handlerFor(body, 'load');
        if (typeof h === 'function') {
          try { h.call(body, ev); } catch (e) { log('body onload: ' + errText(e)); }
        }
      }
    }
    return !ok;
  }
  var ev2 = nativeEventFor(type);
  ev2.isTrusted = true;
  var targetNode = target || G;
  var ok2 = dispatchEventOn(targetNode, ev2);
  return !ok2;
});

hook('onNewElement', function (el) {
  processInlineHandlers(el);
  var def = ceDefFor(el);
  if (def && !ceDone.has(el)) ceUpgrade(el, def);
});

hook('onHttp2', function (id, status, body, raw, finalUrl) {
  id = Number(id);
  var cb = pendingHttp.get(id);
  if (!cb) return;
  pendingHttp['delete'](id);
  var ph = parseRawHeaders(raw), ct = '';
  ph.list.forEach(function (h) { if (h[0] === 'content-type') ct = h[1]; });
  try { cb(Number(status) || 0, body === null || body === undefined ? '' : body, ct, ph, String(finalUrl || '')); }
  catch (e) { log('onHttp2 ' + id + ': ' + errText(e)); }
});

hook('onHttp', function (id, status, text, type) {
  id = Number(id);
  var cb = pendingHttp.get(id);
  if (!cb) return;
  pendingHttp['delete'](id);
  try { cb(Number(status) || 0, text === undefined || text === null ? '' : String(text),
              type === undefined || type === null ? '' : String(type)); }
  catch (e) { log('onHttp ' + id + ': ' + errText(e)); }
});

hook('onClose', function () {
  try {
    var ph = new Event('pagehide', { bubbles: false, cancelable: false });
    dispatchEventOn(G, ph);
  } catch (_) {}
  try {
    var ul = new Event('unload', { bubbles: false, cancelable: false });
    dispatchEventOn(G, ul);
  } catch (_) {}
  timerMap.clear();
  try { localStorageObj._flush(); } catch (_) {}
});

/* ---------------------------------------------------------------- extras (prueba de sitios) */
/* APIs que pidieron sitios reales: reCAPTCHA (MessageChannel, Worker),
 * MercadoLibre (CSSStyleSheet), YouTube (CharacterData, performance.timing). */

class MessagePort extends EventTarget {
  constructor() { super(); this._other = null; this._onmessage = null; this._started = false; }
  get onmessage() { return this._onmessage; }
  set onmessage(fn) { this._onmessage = fn; this._started = true; }
  postMessage(data) {
    var to = this._other;
    if (!to) return;
    setTimeout(function () {
      var ev = new Event('message');
      ev.data = data;
      ev.ports = [];
      if (typeof to._onmessage === 'function') {
        try { to._onmessage.call(to, ev); } catch (e) { log('MessagePort: ' + errText(e)); }
      }
      to.dispatchEvent(ev);
    }, 0);
  }
  start() { this._started = true; }
  close() { this._other = null; }
}
defineTag(MessagePort.prototype, 'MessagePort');

class MessageChannel {
  constructor() {
    this.port1 = new MessagePort();
    this.port2 = new MessagePort();
    this.port1._other = this.port2;
    this.port2._other = this.port1;
  }
}
defineTag(MessageChannel.prototype, 'MessageChannel');

class BroadcastChannel extends EventTarget {
  constructor(name) { super(); this.name = String(name); this.onmessage = null; }
  postMessage() {}
  close() {}
}

/* Sin hilos en la PS3: el Worker avisa un error en vez de colgar la pagina */
class Worker extends EventTarget {
  constructor(url) {
    super();
    this.onmessage = null;
    this.onerror = null;
    var self = this;
    setTimeout(function () {
      var ev = new Event('error');
      ev.message = 'Worker no disponible en EonSurf';
      if (typeof self.onerror === 'function') { try { self.onerror(ev); } catch (_) {} }
      self.dispatchEvent(ev);
    }, 0);
  }
  postMessage() {}
  terminate() {}
}
defineTag(Worker.prototype, 'Worker');

class CSSRuleList extends Array {}
class CSSStyleSheet {
  constructor(opts) {
    opts = opts || {};
    this.cssRules = [];
    this.media = opts.media || '';
    this.disabled = !!opts.disabled;
    this.ownerNode = null;
    this._text = '';
  }
  get rules() { return this.cssRules; }
  replaceSync(text) {
    this._text = String(text);
    this.cssRules = this._text.split('}').filter(function (r) { return r.trim(); })
      .map(function (r) { return { cssText: r.trim() + '}' }; });
    eonApplySheets();
  }
  replace(text) { this.replaceSync(text); return Promise.resolve(this); }
  insertRule(rule, index) {
    index = index === undefined ? 0 : index;
    this.cssRules.splice(index, 0, { cssText: String(rule) });
    this._text = this.cssRules.map(function (r) { return r.cssText; }).join('\n');
    eonApplySheets();
    return index;
  }
  deleteRule(index) {
    this.cssRules.splice(index, 1);
    this._text = this.cssRules.map(function (r) { return r.cssText; }).join('\n');
    eonApplySheets();
  }
}
defineTag(CSSStyleSheet.prototype, 'CSSStyleSheet');

/* document.adoptedStyleSheets: se vuelcan en un <style> propio del head */
var adoptedSheets = [];
function eonApplySheets() {
  var d = getDocument();
  if (!d || !d.head) return;
  var st = d.getElementById('__eon_adopted');
  var css = adoptedSheets.map(function (s) { return s._text; }).join('\n');
  if (!st) {
    if (!css) return;
    st = d.createElement('style');
    st.id = '__eon_adopted';
    d.head.appendChild(st);
  }
  if (st.textContent !== css) st.textContent = css;
}
try {
  Object.defineProperty(Document.prototype, 'adoptedStyleSheets', {
    get: function () { return adoptedSheets; },
    set: function (v) { adoptedSheets = Array.from(v || []); eonApplySheets(); },
    configurable: true
  });
} catch (_) {}

/* ---------------------------------------------------------------- CSSOM */
/* style.sheet / insertRule / cssRules / document.styleSheets.
 * emotion, styled-components y otras librerias de CSS-en-JS, en produccion,
 * agregan las reglas con sheet.insertRule() en vez de escribir texto: sin esto
 * sus estilos no se aplican nunca. Aca cada hoja esta atada a su <style>, y los
 * cambios se vuelcan al texto del elemento (agrupados en una sola escritura
 * por vuelta) para que NetSurf lo vuelva a leer. */

/* parte un texto CSS en reglas de primer nivel */
function cssSplitRules(text) {
  var out = [], depth = 0, q = null, start = 0, i, c;
  text = String(text || '');
  for (i = 0; i < text.length; i++) {
    c = text.charAt(i);
    if (q) { if (c === '\\') i++; else if (c === q) q = null; continue; }
    if (c === '"' || c === "'") { q = c; continue; }
    if (c === '/' && text.charAt(i + 1) === '*') {
      var e = text.indexOf('*/', i + 2);
      i = e < 0 ? text.length : e + 1;
      continue;
    }
    if (c === '{') depth++;
    else if (c === '}') {
      depth--;
      if (depth <= 0) { depth = 0; var r = text.slice(start, i + 1).trim(); if (r) out.push(r); start = i + 1; }
    } else if (c === ';' && depth === 0) {
      var r2 = text.slice(start, i + 1).trim();
      if (r2) out.push(r2);
      start = i + 1;
    }
  }
  var rest = text.slice(start).trim();
  if (rest) out.push(rest);
  return out;
}

class CSSRule {
  constructor(text, sheet, parent) {
    this._text = String(text);
    this.parentStyleSheet = sheet || null;
    this.parentRule = parent || null;
  }
  get cssText() { return this._text; }
  set cssText(v) { /* solo lectura en la practica */ }
  get type() { return this.constructor.TYPE || 0; }
}
CSSRule.STYLE_RULE = 1; CSSRule.IMPORT_RULE = 3; CSSRule.MEDIA_RULE = 4;
CSSRule.FONT_FACE_RULE = 5; CSSRule.PAGE_RULE = 6; CSSRule.KEYFRAMES_RULE = 7;
CSSRule.KEYFRAME_RULE = 8; CSSRule.SUPPORTS_RULE = 12;

/* declaraciones de una regla como objeto parecido a CSSStyleDeclaration */
function cssDeclObject(rule, body) {
  var decls = [];
  splitTopLevel(body, ';').forEach(function (d) {
    var k = d.indexOf(':');
    if (k > 0) decls.push([d.slice(0, k).trim().toLowerCase(), d.slice(k + 1).trim()]);
  });
  var o = {
    get cssText() { return decls.map(function (d) { return d[0] + ': ' + d[1]; }).join('; '); },
    get length() { return decls.length; },
    item: function (i) { return decls[i] ? decls[i][0] : ''; },
    getPropertyValue: function (n) {
      n = String(n).toLowerCase();
      for (var i = decls.length - 1; i >= 0; i--) if (decls[i][0] === n) return decls[i][1].replace(/\s*!important\s*$/i, '');
      return '';
    },
    getPropertyPriority: function (n) { return /!important\s*$/i.test(o.getPropertyValue(n)) ? 'important' : ''; },
    setProperty: function (n, v, pri) {
      n = String(n).toLowerCase();
      v = String(v) + (pri === 'important' ? ' !important' : '');
      for (var i = 0; i < decls.length; i++) if (decls[i][0] === n) { decls[i][1] = v; rule._rebuild(o.cssText); return; }
      decls.push([n, v]);
      rule._rebuild(o.cssText);
    },
    removeProperty: function (n) {
      n = String(n).toLowerCase();
      var old = o.getPropertyValue(n);
      decls = decls.filter(function (d) { return d[0] !== n; });
      rule._rebuild(o.cssText);
      return old;
    }
  };
  return new Proxy(o, {
    get: function (t, p) {
      if (typeof p !== 'string' || p in t) return t[p];
      return t.getPropertyValue(p.replace(/[A-Z]/g, function (m) { return '-' + m.toLowerCase(); }));
    },
    set: function (t, p, v) {
      if (typeof p !== 'string' || p in t) { t[p] = v; return true; }
      t.setProperty(p.replace(/[A-Z]/g, function (m) { return '-' + m.toLowerCase(); }), v);
      return true;
    }
  });
}

class CSSStyleRule extends CSSRule {
  constructor(text, sheet, parent) {
    super(text, sheet, parent);
    var b = this._text.indexOf('{');
    this.selectorText = b < 0 ? '' : this._text.slice(0, b).trim();
    this.style = cssDeclObject(this, b < 0 ? '' : this._text.slice(b + 1, this._text.lastIndexOf('}')));
  }
  _rebuild(body) {
    this._text = this.selectorText + ' { ' + body + ' }';
    if (this.parentStyleSheet && this.parentStyleSheet._changed) this.parentStyleSheet._changed();
  }
}
CSSStyleRule.TYPE = 1;

class CSSGroupingRule extends CSSRule {
  constructor(text, sheet, parent) {
    super(text, sheet, parent);
    var b = this._text.indexOf('{');
    this._head = b < 0 ? this._text : this._text.slice(0, b).trim();
    var inner = b < 0 ? '' : this._text.slice(b + 1, this._text.lastIndexOf('}'));
    var self = this;
    this.cssRules = cssSplitRules(inner).map(function (r) { return makeCSSRule(r, sheet, self); });
  }
  get cssText() {
    return this._head + ' { ' + this.cssRules.map(function (r) { return r.cssText; }).join(' ') + ' }';
  }
  insertRule(rule, index) {
    index = index === undefined ? 0 : index;
    this.cssRules.splice(index, 0, makeCSSRule(rule, this.parentStyleSheet, this));
    if (this.parentStyleSheet && this.parentStyleSheet._changed) this.parentStyleSheet._changed();
    return index;
  }
  deleteRule(index) {
    this.cssRules.splice(index, 1);
    if (this.parentStyleSheet && this.parentStyleSheet._changed) this.parentStyleSheet._changed();
  }
}
class CSSMediaRule extends CSSGroupingRule {
  get media() {
    var m = this._head.replace(/^@media\s*/i, '');
    return { mediaText: m, length: 1, item: function () { return m; }, toString: function () { return m; } };
  }
  get conditionText() { return this._head.replace(/^@media\s*/i, ''); }
}
CSSMediaRule.TYPE = 4;
class CSSSupportsRule extends CSSGroupingRule {
  get conditionText() { return this._head.replace(/^@supports\s*/i, ''); }
}
CSSSupportsRule.TYPE = 12;
class CSSFontFaceRule extends CSSStyleRule {}
CSSFontFaceRule.TYPE = 5;
class CSSKeyframesRule extends CSSGroupingRule {
  get name() { return this._head.replace(/^@(-webkit-)?keyframes\s*/i, ''); }
}
CSSKeyframesRule.TYPE = 7;
class CSSImportRule extends CSSRule {
  get href() { var m = /url\(\s*['"]?([^'")]+)|['"]([^'"]+)['"]/.exec(this._text); return m ? (m[1] || m[2]) : ''; }
}
CSSImportRule.TYPE = 3;

function makeCSSRule(text, sheet, parent) {
  var t = String(text).trim();
  if (/^@media/i.test(t)) return new CSSMediaRule(t, sheet, parent);
  if (/^@supports/i.test(t) || /^@layer/i.test(t) || /^@container/i.test(t)) return new CSSSupportsRule(t, sheet, parent);
  if (/^@font-face/i.test(t)) return new CSSFontFaceRule(t, sheet, parent);
  if (/^@(-webkit-)?keyframes/i.test(t)) return new CSSKeyframesRule(t, sheet, parent);
  if (/^@import/i.test(t)) return new CSSImportRule(t, sheet, parent);
  return new CSSStyleRule(t, sheet, parent);
}

/* hoja atada a un <style> (o vacia para un <link>) */
var sheetOf = new WeakMap();
var dirtySheets = new Set();
function flushSheets() {
  dirtySheets.forEach(function (sh) {
    var text = sh.cssRules.map(function (r) { return r.cssText; }).join('\n');
    sh._written = text;
    if (sh.ownerNode && ns.text(sh.ownerNode) !== text) ns.setText(sh.ownerNode, text);
  });
  dirtySheets.clear();
}

class EonElementSheet extends CSSStyleSheet {
  constructor(node, readOnlyLink) {
    super({});
    this.ownerNode = node;
    this.type = 'text/css';
    this._link = !!readOnlyLink;
    this._written = null;
    this._rules = [];
    this._sync();
  }
  _sync() {
    if (this._link) return;
    var t = ns.text(this.ownerNode) || '';
    if (t !== this._written) {
      var self = this;
      this._rules = cssSplitRules(t).map(function (r) { return makeCSSRule(r, self, null); });
      this._written = t;
    }
  }
  _changed() {
    if (this._link) return;
    if (!dirtySheets.size) queueMicrotaskFn(flushSheets);
    dirtySheets.add(this);
  }
  get cssRules() { if (!dirtySheets.has(this)) this._sync(); return this._rules; }
  set cssRules(v) { /* CSSStyleSheet del padre lo asigna al construir */ }
  get rules() { return this.cssRules; }
  get href() { return this._link ? (this.ownerNode.href || null) : null; }
  get title() { return ns.getAttr(this.ownerNode, 'title'); }
  get media() {
    var m = ns.getAttr(this.ownerNode, 'media') || '';
    return { mediaText: m, length: m ? 1 : 0, item: function () { return m; }, toString: function () { return m; } };
  }
  set media(v) { /* la clase base lo asigna al construir */ }
  get disabled() { return false; }
  set disabled(v) {}
  insertRule(rule, index) {
    var rules = this.cssRules;
    index = index === undefined ? 0 : Number(index);
    if (index < 0 || index > rules.length) throw new DOMException('insertRule: indice fuera de rango', 'IndexSizeError');
    rules.splice(index, 0, makeCSSRule(rule, this, null));
    this._changed();
    return index;
  }
  deleteRule(index) {
    var rules = this.cssRules;
    if (index < 0 || index >= rules.length) throw new DOMException('deleteRule: indice fuera de rango', 'IndexSizeError');
    rules.splice(index, 1);
    this._changed();
  }
  addRule(sel, body, index) {
    return this.insertRule(sel + ' { ' + body + ' }', index === undefined ? this.cssRules.length : index);
  }
  removeRule(index) { this.deleteRule(index === undefined ? 0 : index); }
  replaceSync(text) { ns.setText(this.ownerNode, String(text)); this._sync(); }
  replace(text) { this.replaceSync(text); return Promise.resolve(this); }
}

function sheetFor(node, isLink) {
  var sh = sheetOf.get(node);
  if (!sh) { sh = new EonElementSheet(node, isLink); sheetOf.set(node, sh); }
  return sh;
}
try {
  Object.defineProperty(HTMLStyleElement.prototype, 'sheet', {
    get: function () { return ceConnected(this) ? sheetFor(this, false) : null; }, configurable: true
  });
  Object.defineProperty(HTMLLinkElement.prototype, 'sheet', {
    get: function () {
      return /(^|\s)stylesheet(\s|$)/i.test(ns.getAttr(this, 'rel') || '') && ceConnected(this) ? sheetFor(this, true) : null;
    }, configurable: true
  });
  Object.defineProperty(Document.prototype, 'styleSheets', {
    get: function () {
      var list = byTagAll(this, '*').filter(function (el) {
        var n = ns.name(el);
        return n === 'STYLE' || (n === 'LINK' && /(^|\s)stylesheet(\s|$)/i.test(ns.getAttr(el, 'rel') || ''));
      }).map(function (el) { return el.sheet; }).filter(Boolean);
      list.item = function (i) { return this[i] || null; };
      return list;
    }, configurable: true
  });
} catch (e) { log('CSSOM: ' + errText(e)); }
defGlobal('CSSRule', CSSRule);
defGlobal('CSSStyleRule', CSSStyleRule);
defGlobal('CSSGroupingRule', CSSGroupingRule);
defGlobal('CSSMediaRule', CSSMediaRule);
defGlobal('CSSSupportsRule', CSSSupportsRule);
defGlobal('CSSFontFaceRule', CSSFontFaceRule);
defGlobal('CSSKeyframesRule', CSSKeyframesRule);
defGlobal('CSSImportRule', CSSImportRule);
defGlobal('StyleSheet', CSSStyleSheet);
defGlobal('CSS', Object.assign(G.CSS || {}, {
  supports: function (a, b) {
    /* lo que el traductor de EonSurf entiende cuenta como soportado */
    var t = (b === undefined ? String(a) : String(a) + ':' + String(b)).toLowerCase();
    return !/(subgrid|container-type|:has\(|backdrop-filter|mask-image|clip-path|anchor-name)/.test(t);
  },
  escape: function (s) { return String(s).replace(/([^\w-])/g, '\\$1').replace(/^(\d)/, '\\3$1 '); }
}));


/* ---------------------------------------------------------------- blob: y extras de la T5 */
/* URL.createObjectURL: reddit (es-module-shims) arma modulos en memoria y los
 * importa por URL blob:. Los blobs quedan en un registro; fetch, XHR y el
 * cargador de modulos (via __ns.blobText) los leen de ahi. */
var blobRegistry = new Map();
var nextBlobId = 1;
function blobText(b) {
  if (b && typeof b._text === 'string') return b._text;
  if (b instanceof ArrayBuffer) return new TextDecoder().decode(new Uint8Array(b));
  return String(b);
}
URL.createObjectURL = function (b) {
  var u = 'blob:' + (location.origin || 'null') + '/eon-' + (nextBlobId++) + '-' + Math.random().toString(36).slice(2, 10);
  blobRegistry.set(u, b);
  return u;
};
URL.revokeObjectURL = function (u) { blobRegistry['delete'](String(u)); };
hook('blobText', function (u) {
  var b = blobRegistry.get(String(u));
  return b === undefined ? undefined : blobText(b);
});
var fetchNet = fetch;
fetch = function (input, init) {
  var u = typeof input === 'string' ? input : (input && input.url) || String(input);
  if (/^blob:/.test(u)) {
    var b = blobRegistry.get(u);
    if (b === undefined) return Promise.reject(new TypeError('Failed to fetch'));
    return Promise.resolve(new Response(blobText(b), { status: 200, headers: { 'content-type': (b && b.type) || 'text/plain' }, url: u }));
  }
  if (/^data:/.test(u)) {
    var m = /^data:([^,;]*)(;base64)?,(.*)$/s.exec(u);
    if (!m) return Promise.reject(new TypeError('Failed to fetch'));
    var txt = m[2] ? atob(m[3]) : decodeURIComponent(m[3]);
    return Promise.resolve(new Response(txt, { status: 200, headers: { 'content-type': m[1] || 'text/plain' }, url: u }));
  }
  return fetchNet(input, init);
};
defGlobal('fetch', fetch);
/* Interfaces que los sitios usan con instanceof o comprueban al arrancar
 * (Instagram: DOMStringMap). Si ya existen no se tocan. */
[['DOMStringMap'], ['NamedNodeMap'], ['HTMLOptionsCollection'], ['RadioNodeList'], ['DOMRectList'],
 ['StaticRange'], ['History'], ['Location'], ['Navigator'], ['Screen'], ['BarProp'], ['DOMImplementation'],
 ['HTMLAllCollection'], ['TextTrack'], ['TextTrackList'], ['MediaList'], ['DOMStringList'], ['Plugin'],
 ['PluginArray'], ['MimeType'], ['MimeTypeArray'], ['Crypto'], ['SubtleCrypto'], ['CacheStorage']
].forEach(function (n) {
  if (!(n[0] in G)) {
    var C = class {};
    defineTag(C.prototype, n[0]);
    defGlobal(n[0], C);
  }
});
try {
  if (G.history && !(G.history instanceof G.History)) Object.setPrototypeOf(G.history, G.History.prototype);
  if (G.navigator && !(G.navigator instanceof G.Navigator)) Object.setPrototypeOf(G.navigator, G.Navigator.prototype);
  if (G.screen && !(G.screen instanceof G.Screen)) Object.setPrototypeOf(G.screen, G.Screen.prototype);
} catch (_) {}
class VisualViewport extends EventTarget {
  constructor() { super(); this.offsetLeft = 0; this.offsetTop = 0; this.pageLeft = 0; this.pageTop = 0; this.width = 1280; this.height = 668; this.scale = 1; }
}
defGlobal('VisualViewport', VisualViewport);
defGlobal('visualViewport', new VisualViewport());
defGlobal('XPathResult', { ANY_TYPE: 0, NUMBER_TYPE: 1, STRING_TYPE: 2, BOOLEAN_TYPE: 3, UNORDERED_NODE_ITERATOR_TYPE: 4,
  ORDERED_NODE_ITERATOR_TYPE: 5, UNORDERED_NODE_SNAPSHOT_TYPE: 6, ORDERED_NODE_SNAPSHOT_TYPE: 7,
  ANY_UNORDERED_NODE_TYPE: 8, FIRST_ORDERED_NODE_TYPE: 9 });

var nonceOf = new WeakMap();
try {
  Object.defineProperty(HTMLElement.prototype, 'nonce', {
    get: function () { return nonceOf.has(this) ? nonceOf.get(this) : (ns.getAttr(this, 'nonce') || ''); },
    set: function (v) { nonceOf.set(this, String(v)); }, configurable: true
  });
} catch (_) {}

/* document.currentScript: mientras corre un script es el ultimo <script>
 * del documento (el que el analizador acaba de procesar); si no, null */
try {
  Object.defineProperty(Document.prototype, 'currentScript', {
    get: function () {
      if (!ns.executing) return null;
      var all = ns.byTag(this, 'script');
      return all.length ? all[all.length - 1] : null;
    }, configurable: true
  });
} catch (e) { log('currentScript: ' + errText(e)); }
try {
  Object.defineProperty(Element.prototype, 'hasAttributes', {
    value: function () { return ns.attrNames(this).length > 0; }, writable: true, configurable: true
  });
  Object.defineProperty(Node.prototype, 'hasAttributes', {
    value: function () { return ns.type(this) === ELEMENT_NODE && ns.attrNames(this).length > 0; }, writable: true, configurable: true
  });
} catch (e) { log('hasAttributes: ' + errText(e)); }

/* Navigation Timing (YouTube y medidores de rendimiento lo leen al arrancar) */
var navEntry = {
  name: '', entryType: 'navigation', startTime: 0, duration: 0, type: 'navigate',
  redirectCount: 0, fetchStart: 0, domainLookupStart: 0, domainLookupEnd: 0,
  connectStart: 0, connectEnd: 0, secureConnectionStart: 0, requestStart: 0,
  responseStart: 0, responseEnd: 0, domInteractive: 0, domContentLoadedEventStart: 0,
  domContentLoadedEventEnd: 0, domComplete: 0, loadEventStart: 0, loadEventEnd: 0,
  transferSize: 0, encodedBodySize: 0, decodedBodySize: 0, toJSON: function () { return this; }
};
performanceObj.timing = {
  navigationStart: perfStart, fetchStart: perfStart, requestStart: perfStart,
  responseStart: perfStart, responseEnd: perfStart, domLoading: perfStart,
  domInteractive: perfStart, domContentLoadedEventStart: perfStart,
  domContentLoadedEventEnd: perfStart, domComplete: perfStart,
  loadEventStart: perfStart, loadEventEnd: perfStart, toJSON: function () { return this; }
};
performanceObj.navigation = { type: 0, redirectCount: 0 };
performanceObj.memory = { jsHeapSizeLimit: 96 * 1048576, totalJSHeapSize: 0, usedJSHeapSize: 0 };
performanceObj.getEntriesByType = function (t) { return t === 'navigation' ? [navEntry] : []; };
performanceObj.getEntries = function () { return [navEntry]; };
performanceObj.getEntriesByName = function () { return []; };

class PerformanceObserver {
  constructor(cb) { this._cb = cb; }
  observe() {}
  disconnect() {}
  takeRecords() { return []; }
}
PerformanceObserver.supportedEntryTypes = [];

/* Import maps: <script type="importmap"> traduce import "react" a una URL */
var importMap = { imports: {}, scopes: {} };
hook('addImportMap', function (text) {
  var m = JSON.parse(String(text));
  var base = ns.href();
  var abs = function (v) { try { return new URL(v, base).href; } catch (_) { return v; } };
  Object.keys(m.imports || {}).forEach(function (k) { importMap.imports[k] = abs(m.imports[k]); });
  Object.keys(m.scopes || {}).forEach(function (s) {
    var sc = importMap.scopes[abs(s)] = importMap.scopes[abs(s)] || {};
    Object.keys(m.scopes[s]).forEach(function (k) { sc[k] = abs(m.scopes[s][k]); });
  });
});
function mapLookup(table, name) {
  if (hasOwn.call(table, name)) return table[name];
  /* prefijo mas largo terminado en "/" */
  var best = null;
  Object.keys(table).forEach(function (k) {
    if (k.endsWith('/') && name.startsWith(k) && (best === null || k.length > best.length)) best = k;
  });
  return best === null ? null : table[best] + name.slice(best.length);
}
hook('resolveImport', function (name) {
  name = String(name);
  var r = mapLookup(importMap.imports, name);
  if (r === null) {
    Object.keys(importMap.scopes).some(function (s) { r = mapLookup(importMap.scopes[s], name); return r !== null; });
  }
  return r === null ? undefined : r;
});

/* ---------------------------------------------------------------- arranque */

installGlobals();

defGlobal('MessageChannel', MessageChannel);
defGlobal('MessagePort', MessagePort);
defGlobal('BroadcastChannel', BroadcastChannel);
defGlobal('Worker', Worker);
defGlobal('SharedWorker', Worker);
defGlobal('CSSStyleSheet', CSSStyleSheet);
defGlobal('CSSRuleList', CSSRuleList);
defGlobal('PerformanceObserver', PerformanceObserver);
defGlobal('CharacterData', CharacterData);

/* Interfaces que recorre el polyfill de componentes web de YouTube (ShadyDOM) */
class CDATASection extends Text {}
class ProcessingInstruction extends CharacterData {}
class ErrorEvent extends Event {
  constructor(type, init) {
    super(type, init);
    init = init || {};
    this.message = init.message || '';
    this.filename = init.filename || '';
    this.lineno = init.lineno || 0;
    this.colno = init.colno || 0;
    this.error = init.error === undefined ? null : init.error;
  }
}
class PromiseRejectionEvent extends Event {
  constructor(type, init) { super(type, init); this.promise = init && init.promise; this.reason = init && init.reason; }
}
class HTMLSlotElement extends HTMLElement {
  get name() { return ns.getAttr(this, 'name') || ''; }
  set name(v) { ns.setAttr(this, 'name', String(v)); }
  assignedNodes() { return []; }
  assignedElements() { return []; }
  assign() {}
}
TAG_PROTOTYPES.SLOT = HTMLSlotElement.prototype;
defGlobal('CDATASection', CDATASection);
defGlobal('ProcessingInstruction', ProcessingInstruction);
defGlobal('ErrorEvent', ErrorEvent);
defGlobal('PromiseRejectionEvent', PromiseRejectionEvent);
defGlobal('HTMLSlotElement', HTMLSlotElement);

/* customElements es una instancia de CustomElementRegistry y los metodos viven
 * en el prototipo: asi los polyfills que lo parchean (YouTube) funcionan */
class CustomElementRegistry {
  constructor() { throw new TypeError('Illegal constructor'); }
}
Object.keys(customElementsObj).forEach(function (k) {
  Object.defineProperty(CustomElementRegistry.prototype, k,
    { value: customElementsObj[k], writable: true, configurable: true });
  delete customElementsObj[k];
});
Object.setPrototypeOf(customElementsObj, CustomElementRegistry.prototype);
defineTag(CustomElementRegistry.prototype, 'CustomElementRegistry');
defGlobal('CustomElementRegistry', CustomElementRegistry);

/* ---------------------------------------------------------------- interfaces en bloque */
/* Muchos sitios (YouTube, frameworks) tocan estas clases al arrancar aunque no
 * las usen: alcanza con que existan y se comporten de forma razonable. */

class Window extends EventTarget {
  constructor() { super(); throw new TypeError('Illegal constructor'); }
}
defineTag(Window.prototype, 'Window');
try { Object.setPrototypeOf(G, Window.prototype); } catch (_) {}
defGlobal('Window', Window);

/* eventos: copian lo que venga en init */
function eventClass(Base, name, extra) {
  var C = class extends Base {
    constructor(type, init) {
      super(type, init);
      init = init || {};
      for (var k in extra) this[k] = init[k] !== undefined ? init[k] : extra[k];
    }
  };
  defineTag(C.prototype, name);
  defGlobal(name, C);
  return C;
}
var WheelEvent = eventClass(MouseEvent, 'WheelEvent', { deltaX: 0, deltaY: 0, deltaZ: 0, deltaMode: 0 });
WheelEvent.DOM_DELTA_PIXEL = 0; WheelEvent.DOM_DELTA_LINE = 1; WheelEvent.DOM_DELTA_PAGE = 2;
eventClass(MouseEvent, 'PointerEvent', { pointerId: 1, width: 1, height: 1, pressure: 0, pointerType: 'mouse', isPrimary: true, tiltX: 0, tiltY: 0 });
eventClass(MouseEvent, 'DragEvent', { dataTransfer: null });
eventClass(UIEvent, 'TouchEvent', { touches: [], targetTouches: [], changedTouches: [], altKey: false, ctrlKey: false, metaKey: false, shiftKey: false });
eventClass(UIEvent, 'CompositionEvent', { data: '' });
eventClass(Event, 'AnimationEvent', { animationName: '', elapsedTime: 0, pseudoElement: '' });
eventClass(Event, 'TransitionEvent', { propertyName: '', elapsedTime: 0, pseudoElement: '' });
eventClass(Event, 'PopStateEvent', { state: null });
eventClass(Event, 'HashChangeEvent', { oldURL: '', newURL: '' });
eventClass(Event, 'PageTransitionEvent', { persisted: false });
eventClass(Event, 'StorageEvent', { key: null, oldValue: null, newValue: null, url: '', storageArea: null });
eventClass(Event, 'ProgressEvent', { lengthComputable: false, loaded: 0, total: 0 });
eventClass(Event, 'MessageEvent', { data: null, origin: '', lastEventId: '', source: null, ports: [] });
eventClass(Event, 'SubmitEvent', { submitter: null });
eventClass(Event, 'BeforeUnloadEvent', { returnValue: '' });
eventClass(Event, 'SecurityPolicyViolationEvent', { violatedDirective: '', blockedURI: '' });
eventClass(Event, 'ToggleEvent', { oldState: '', newState: '' });
eventClass(Event, 'MediaQueryListEvent', { matches: false, media: '' });

/* elementos HTML que faltaban: prototipo propio con su etiqueta */
[['HTMLHeadingElement', ['H1', 'H2', 'H3', 'H4', 'H5', 'H6']], ['HTMLPreElement', ['PRE']],
 ['HTMLBRElement', ['BR']], ['HTMLHRElement', ['HR']], ['HTMLTableSectionElement', ['THEAD', 'TBODY', 'TFOOT']],
 ['HTMLTableCaptionElement', ['CAPTION']], ['HTMLTableColElement', ['COL', 'COLGROUP']],
 ['HTMLFieldSetElement', ['FIELDSET']], ['HTMLLegendElement', ['LEGEND']], ['HTMLProgressElement', ['PROGRESS']],
 ['HTMLMeterElement', ['METER']], ['HTMLDialogElement', ['DIALOG']], ['HTMLDetailsElement', ['DETAILS']],
 ['HTMLPictureElement', ['PICTURE']], ['HTMLSourceElement', ['SOURCE']], ['HTMLObjectElement', ['OBJECT']],
 ['HTMLEmbedElement', ['EMBED']], ['HTMLAreaElement', ['AREA']], ['HTMLMapElement', ['MAP']],
 ['HTMLMenuElement', ['MENU']], ['HTMLDataListElement', ['DATALIST']], ['HTMLOutputElement', ['OUTPUT']],
 ['HTMLTimeElement', ['TIME']], ['HTMLQuoteElement', ['Q', 'BLOCKQUOTE']], ['HTMLModElement', ['INS', 'DEL']],
 ['HTMLBaseElement', ['BASE']], ['HTMLTrackElement', ['TRACK']], ['HTMLDListElement', ['DL']],
 ['HTMLOptGroupElement', ['OPTGROUP']], ['HTMLDataElement', ['DATA']], ['HTMLFrameElement', ['FRAME']],
 ['HTMLFrameSetElement', ['FRAMESET']], ['HTMLFontElement', ['FONT']], ['HTMLDirectoryElement', ['DIR']],
 ['HTMLMarqueeElement', ['MARQUEE']], ['HTMLParamElement', ['PARAM']], ['HTMLSearchElement', ['SEARCH']]
].forEach(function (d) {
  if (d[0] in G) return;
  var C = class extends HTMLElement {};
  defineTag(C.prototype, d[0]);
  d[1].forEach(function (t) { if (!TAG_PROTOTYPES[t]) TAG_PROTOTYPES[t] = C.prototype; });
  defGlobal(d[0], C);
});
try {
  var dlg = TAG_PROTOTYPES.DIALOG;
  Object.defineProperty(dlg, 'open', { get: function () { return ns.getAttr(this, 'open') !== null; },
    set: function (v) { if (v) ns.setAttr(this, 'open', ''); else ns.removeAttr(this, 'open'); }, configurable: true });
  dlg.show = function () { ns.setAttr(this, 'open', ''); };
  dlg.showModal = dlg.show;
  dlg.close = function (rv) { ns.removeAttr(this, 'open'); this.returnValue = rv === undefined ? '' : String(rv); this.dispatchEvent(new Event('close')); };
  var det = TAG_PROTOTYPES.DETAILS;
  Object.defineProperty(det, 'open', { get: function () { return ns.getAttr(this, 'open') !== null; },
    set: function (v) { if (v) ns.setAttr(this, 'open', ''); else ns.removeAttr(this, 'open'); }, configurable: true });
} catch (_) {}
var SVGBase = typeof G.SVGElement === "function" ? G.SVGElement : Element;
class SVGGraphicsElement extends SVGBase {}
class SVGSVGElement extends SVGGraphicsElement {}
defGlobal('SVGGraphicsElement', SVGGraphicsElement);
defGlobal('SVGSVGElement', SVGSVGElement);
defGlobal('Option', function (text, value, defSel, sel) {
  var o = getDocument().createElement('option');
  if (text !== undefined) o.textContent = String(text);
  if (value !== undefined) o.setAttribute('value', String(value));
  if (defSel || sel) o.setAttribute('selected', '');
  return o;
});
defGlobal('Audio', function (src) {
  var a = getDocument().createElement('audio');
  if (src !== undefined) a.setAttribute('src', String(src));
  return a;
});

/* geometria */
class DOMRectReadOnly {
  constructor(x, y, w, h) { this.x = x || 0; this.y = y || 0; this.width = w || 0; this.height = h || 0; }
  get left() { return Math.min(this.x, this.x + this.width); }
  get right() { return Math.max(this.x, this.x + this.width); }
  get top() { return Math.min(this.y, this.y + this.height); }
  get bottom() { return Math.max(this.y, this.y + this.height); }
  toJSON() { return { x: this.x, y: this.y, width: this.width, height: this.height, left: this.left, right: this.right, top: this.top, bottom: this.bottom }; }
  static fromRect(r) { r = r || {}; return new this(r.x, r.y, r.width, r.height); }
}
class DOMRect extends DOMRectReadOnly {}
class DOMPoint { constructor(x, y, z, w) { this.x = x || 0; this.y = y || 0; this.z = z || 0; this.w = w === undefined ? 1 : w; } }
class DOMMatrix {
  constructor() { this.a = 1; this.b = 0; this.c = 0; this.d = 1; this.e = 0; this.f = 0; this.is2D = true; this.isIdentity = true; }
  translate(x, y) { var m = new DOMMatrix(); m.e = this.e + (x || 0); m.f = this.f + (y || 0); return m; }
  scale() { return new DOMMatrix(); }
  multiply() { return new DOMMatrix(); }
  inverse() { return new DOMMatrix(); }
  transformPoint(p) { return new DOMPoint(p.x + this.e, p.y + this.f); }
  toString() { return 'matrix(1, 0, 0, 1, ' + this.e + ', ' + this.f + ')'; }
}
[['DOMRectReadOnly', DOMRectReadOnly], ['DOMRect', DOMRect], ['DOMPoint', DOMPoint], ['DOMPointReadOnly', DOMPoint],
 ['DOMMatrix', DOMMatrix], ['DOMMatrixReadOnly', DOMMatrix], ['WebKitCSSMatrix', DOMMatrix]].forEach(function (d) { defGlobal(d[0], d[1]); });

/* parsers y serializadores */
class XMLSerializer { serializeToString(n) { return n && n.outerHTML !== undefined ? n.outerHTML : (n && n.textContent) || ''; } }
class DOMParser {
  parseFromString(text) {
    /* documento liviano: un <div> fuera de la pagina con el HTML adentro */
    var d = getDocument(), holder = d.createElement('div');
    holder.innerHTML = String(text);
    return {
      body: holder, documentElement: holder, head: null,
      querySelector: function (s) { return holder.querySelector(s); },
      querySelectorAll: function (s) { return holder.querySelectorAll(s); },
      getElementById: function (id) { return holder.querySelector('#' + id); },
      getElementsByTagName: function (t) { return holder.getElementsByTagName(t); },
      get title() { var t = holder.querySelector('title'); return t ? t.textContent : ''; }
    };
  }
}
defGlobal('XMLSerializer', XMLSerializer);
defGlobal('DOMParser', DOMParser);

/* red y almacenamiento que no hay: fallan de forma prolija, sin colgar */
function failLater(obj, msg) {
  setTimeout(function () {
    var ev = new Event('error');
    ev.message = msg;
    if (typeof obj.onerror === 'function') { try { obj.onerror(ev); } catch (_) {} }
    obj.dispatchEvent(ev);
    if (typeof obj.onclose === 'function') { try { obj.onclose(new Event('close')); } catch (_) {} }
  }, 0);
}
class WebSocket extends EventTarget {
  constructor(url) { super(); this.url = String(url); this.readyState = 3; this.bufferedAmount = 0; this.protocol = ''; failLater(this, 'WebSocket no disponible'); }
  send() {}
  close() {}
}
WebSocket.CONNECTING = 0; WebSocket.OPEN = 1; WebSocket.CLOSING = 2; WebSocket.CLOSED = 3;
class EventSource extends EventTarget {
  constructor(url) { super(); this.url = String(url); this.readyState = 2; failLater(this, 'EventSource no disponible'); }
  close() {}
}
defGlobal('WebSocket', WebSocket);
defGlobal('EventSource', EventSource);
var idbRequest = function () {
  var r = new EventTarget();
  r.result = null; r.error = new DOMException('IndexedDB no disponible', 'UnknownError'); r.readyState = 'done';
  failLater(r, 'IndexedDB no disponible');
  return r;
};
defGlobal('indexedDB', { open: idbRequest, deleteDatabase: idbRequest, databases: function () { return Promise.resolve([]); }, cmp: function (a, b) { return a < b ? -1 : a > b ? 1 : 0; } });
class FileReader extends EventTarget {
  constructor() { super(); this.result = null; this.readyState = 0; this.error = null; }
  _done(v) { var self = this; setTimeout(function () { self.result = v; self.readyState = 2; var ev = new Event('load'); if (typeof self.onload === 'function') self.onload(ev); self.dispatchEvent(ev); if (typeof self.onloadend === 'function') self.onloadend(ev); }, 0); }
  readAsText(b) { var self = this; Promise.resolve(b && b.text ? b.text() : String(b)).then(function (t) { self._done(t); }); }
  readAsDataURL(b) { var self = this; Promise.resolve(b && b.text ? b.text() : String(b)).then(function (t) { self._done('data:' + ((b && b.type) || 'text/plain') + ';base64,' + btoa(unescape(encodeURIComponent(t)))); }); }
  readAsArrayBuffer(b) { var self = this; Promise.resolve(b && b.arrayBuffer ? b.arrayBuffer() : new ArrayBuffer(0)).then(function (t) { self._done(t); }); }
  abort() {}
}
class File extends Blob { constructor(parts, name, opts) { super(parts, opts); this.name = String(name); this.lastModified = Date.now(); } }
defGlobal('FileReader', FileReader);
defGlobal('File', File);
defGlobal('Notification', Object.assign(function () { throw new TypeError('Notification no disponible'); }, { permission: 'denied', requestPermission: function () { return Promise.resolve('denied'); } }));
defGlobal('Path2D', class { constructor() {} addPath() {} moveTo() {} lineTo() {} closePath() {} rect() {} arc() {} });
defGlobal('ImageData', class { constructor(w, h) { this.width = w; this.height = h; this.data = new Uint8ClampedArray(w * h * 4); } });
[['DocumentType'], ['Attr'], ['ShadowRoot'], ['HTMLUnknownElement'], ['SVGElement'], ['Range'], ['Selection']].forEach(function (n) {
  if (!(n[0] in G)) defGlobal(n[0], class extends Node {});
});

try {
  var startDoc = getDocument();
  if (startDoc && startDoc.body) uiState.readyState = 'interactive';
} catch (_) {}

})();
