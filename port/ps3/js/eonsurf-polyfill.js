/* EonSurf: APIs web que le faltan a NetSurf 3.11 + Duktape (ES5).
 *
 * Se agrega al final de polyfill.js. Todo en ES5 (Duktape no entiende ES6).
 * Usa dos funciones nativas que agrega patch-js.py en dukky.c:
 *   __eonHttp(id, method, url, base, body)  -> pide url; al terminar llama
 *                                              __eonHttpDone(id, status, text, type)
 *   __eonStorage(origin[, data])            -> lee / guarda localStorage en disco
 */
(function (g) {
  'use strict';

  function later(fn) { g.setTimeout(fn, 0); }
  function isFn(f) { return typeof f === 'function'; }

  /* ------------------------------------------------------------ extras ES */
  if (!Object.assign) {
    Object.assign = function (t) {
      for (var i = 1; i < arguments.length; i++) {
        var s = arguments[i];
        if (s != null) for (var k in s) if (Object.prototype.hasOwnProperty.call(s, k)) t[k] = s[k];
      }
      return t;
    };
  }
  if (!Object.entries) {
    Object.entries = function (o) { return Object.keys(o).map(function (k) { return [k, o[k]]; }); };
  }
  if (!Object.values) {
    Object.values = function (o) { return Object.keys(o).map(function (k) { return o[k]; }); };
  }
  var AP = Array.prototype, SP = String.prototype;
  if (!AP.includes) AP.includes = function (v, from) { return this.indexOf(v, from) !== -1 || (v !== v && this.some(function (x) { return x !== x; })); };
  if (!AP.find) AP.find = function (f, t) { for (var i = 0; i < this.length; i++) if (f.call(t, this[i], i, this)) return this[i]; };
  if (!AP.findIndex) AP.findIndex = function (f, t) { for (var i = 0; i < this.length; i++) if (f.call(t, this[i], i, this)) return i; return -1; };
  if (!AP.fill) AP.fill = function (v, s, e) { var n = this.length; s = s || 0; e = e === undefined ? n : e; for (var i = s < 0 ? n + s : s; i < (e < 0 ? n + e : e); i++) this[i] = v; return this; };
  if (!AP.flat) AP.flat = function (d) { d = d === undefined ? 1 : d; var r = []; this.forEach(function (x) { if (Array.isArray(x) && d > 0) r = r.concat(x.flat(d - 1)); else r.push(x); }); return r; };
  if (!SP.includes) SP.includes = function (s, p) { return this.indexOf(s, p) !== -1; };
  if (!SP.startsWith) SP.startsWith = function (s, p) { p = p || 0; return this.substr(p, s.length) === s; };
  if (!SP.endsWith) SP.endsWith = function (s, l) { l = l === undefined ? this.length : l; return this.substring(l - s.length, l) === s; };
  if (!SP.repeat) SP.repeat = function (n) { var r = ''; for (var i = 0; i < n; i++) r += this; return r; };
  if (!SP.padStart) SP.padStart = function (n, c) { var s = String(this); c = c === undefined ? ' ' : String(c); while (s.length < n) s = c + s; return s.slice(s.length - Math.max(n, this.length)); };
  if (!SP.padEnd) SP.padEnd = function (n, c) { var s = String(this); c = c === undefined ? ' ' : String(c); while (s.length < n) s += c; return s.slice(0, Math.max(n, this.length)); };
  if (!SP.trimStart) SP.trimStart = function () { return this.replace(/^\s+/, ''); };
  if (!SP.trimEnd) SP.trimEnd = function () { return this.replace(/\s+$/, ''); };
  if (!Number.isInteger) Number.isInteger = function (v) { return typeof v === 'number' && isFinite(v) && Math.floor(v) === v; };
  if (!Number.isNaN) Number.isNaN = function (v) { return v !== v; };
  if (!Math.trunc) Math.trunc = function (v) { return v < 0 ? Math.ceil(v) : Math.floor(v); };
  if (!Math.sign) Math.sign = function (v) { v = +v; return v > 0 ? 1 : v < 0 ? -1 : v; };

  /* ------------------------------------------------------------ Promise */
  if (!isFn(g.Promise)) {
    var P = function (exec) {
      if (!(this instanceof P)) throw new TypeError('Promise must be called with new');
      var self = this;
      this._s = 0; this._v = undefined; this._q = [];
      function res(v) {
        if (self._s) return;
        if (v === self) return rej(new TypeError('cycle'));
        if (v && (typeof v === 'object' || isFn(v))) {
          var then;
          try { then = v.then; } catch (e) { return rej(e); }
          if (isFn(then)) {
            var done = false;
            try {
              then.call(v, function (x) { if (!done) { done = true; res(x); } },
                           function (x) { if (!done) { done = true; rej(x); } });
            } catch (e) { if (!done) { done = true; rej(e); } }
            return;
          }
        }
        self._s = 1; self._v = v; flush();
      }
      function rej(v) { if (self._s) return; self._s = 2; self._v = v; flush(); }
      function flush() { var q = self._q; self._q = []; later(function () { q.forEach(run); }); }
      function run(h) {
        var cb = self._s === 1 ? h.ok : h.ko;
        if (!isFn(cb)) { (self._s === 1 ? h.res : h.rej)(self._v); return; }
        try { h.res(cb(self._v)); } catch (e) { h.rej(e); }
      }
      this._run = run;
      try { exec(res, rej); } catch (e) { rej(e); }
    };
    P.prototype.then = function (ok, ko) {
      var self = this;
      return new P(function (res, rej) {
        var h = { ok: ok, ko: ko, res: res, rej: rej };
        if (self._s) later(function () { self._run(h); }); else self._q.push(h);
      });
    };
    P.prototype['catch'] = function (ko) { return this.then(null, ko); };
    P.prototype['finally'] = function (f) {
      return this.then(function (v) { return P.resolve(f()).then(function () { return v; }); },
                       function (e) { return P.resolve(f()).then(function () { throw e; }); });
    };
    P.resolve = function (v) { return v instanceof P ? v : new P(function (r) { r(v); }); };
    P.reject = function (v) { return new P(function (r, j) { j(v); }); };
    P.all = function (a) {
      return new P(function (res, rej) {
        var out = [], n = a.length;
        if (!n) return res(out);
        a.forEach(function (p, i) { P.resolve(p).then(function (v) { out[i] = v; if (--n === 0) res(out); }, rej); });
      });
    };
    P.allSettled = function (a) {
      return P.all(a.map(function (p) {
        return P.resolve(p).then(function (v) { return { status: 'fulfilled', value: v }; },
                                 function (e) { return { status: 'rejected', reason: e }; });
      }));
    };
    P.race = function (a) { return new P(function (res, rej) { a.forEach(function (p) { P.resolve(p).then(res, rej); }); }); };
    P.any = function (a) {
      return new P(function (res, rej) {
        var n = a.length; if (!n) return rej(new Error('All promises were rejected'));
        a.forEach(function (p) { P.resolve(p).then(res, function () { if (--n === 0) rej(new Error('All promises were rejected')); }); });
      });
    };
    g.Promise = P;
  }
  if (!isFn(g.queueMicrotask)) g.queueMicrotask = function (f) { later(f); };

  /* ------------------------------------------------------------ tiempo */
  if (!g.performance) g.performance = {};
  if (!isFn(g.performance.now)) {
    var t0 = Date.now();
    g.performance.now = function () { return Date.now() - t0; };
  }
  if (!isFn(g.requestAnimationFrame)) {
    g.requestAnimationFrame = function (f) { return g.setTimeout(function () { f(g.performance.now()); }, 16); };
    g.cancelAnimationFrame = function (id) { g.clearTimeout(id); };
  }

  /* ------------------------------------------------------------ red */
  var pending = {}, nextId = 1;
  function baseHref() { try { return String(g.location.href); } catch (e) { return ''; } }
  function http(method, url, body, done) {
    var id = nextId++;
    pending[id] = done;
    if (!isFn(g.__eonHttp)) { delete pending[id]; later(function () { done(0, '', ''); }); return id; }
    g.__eonHttp(id, method, String(url), baseHref(), body == null ? null : String(body));
    return id;
  }
  g.__eonHttpDone = function (id, status, text, type) {
    var cb = pending[id];
    delete pending[id];
    if (cb) cb(status, text || '', type || '');
  };

  function Headers(init) {
    this._h = {};
    if (init) for (var k in init) if (Object.prototype.hasOwnProperty.call(init, k)) this._h[k.toLowerCase()] = String(init[k]);
  }
  Headers.prototype.get = function (k) { k = String(k).toLowerCase(); return k in this._h ? this._h[k] : null; };
  Headers.prototype.has = function (k) { return String(k).toLowerCase() in this._h; };
  Headers.prototype.set = function (k, v) { this._h[String(k).toLowerCase()] = String(v); };
  Headers.prototype.append = Headers.prototype.set;
  Headers.prototype.forEach = function (f, t) { for (var k in this._h) f.call(t, this._h[k], k, this); };
  if (!g.Headers) g.Headers = Headers;

  if (typeof g.XMLHttpRequest === 'undefined') {
    var XHR = function () {
      this.readyState = 0; this.status = 0; this.statusText = '';
      this.responseText = ''; this.response = ''; this.responseType = '';
      this.responseURL = ''; this.timeout = 0; this.withCredentials = false;
      this._type = ''; this._ls = {};
      this.onreadystatechange = this.onload = this.onerror = this.onloadend = null;
    };
    XHR.UNSENT = 0; XHR.OPENED = 1; XHR.HEADERS_RECEIVED = 2; XHR.LOADING = 3; XHR.DONE = 4;
    XHR.prototype.open = function (m, u) { this._m = String(m || 'GET').toUpperCase(); this._u = u; this.readyState = 1; this._fire('readystatechange'); };
    XHR.prototype.setRequestHeader = function () {};
    XHR.prototype.overrideMimeType = function () {};
    XHR.prototype.getResponseHeader = function (k) { return String(k).toLowerCase() === 'content-type' ? this._type : null; };
    XHR.prototype.getAllResponseHeaders = function () { return this._type ? 'content-type: ' + this._type + '\r\n' : ''; };
    XHR.prototype.addEventListener = function (t, f) { (this._ls[t] = this._ls[t] || []).push(f); };
    XHR.prototype.removeEventListener = function (t, f) { var l = this._ls[t]; if (l) { var i = l.indexOf(f); if (i >= 0) l.splice(i, 1); } };
    XHR.prototype._fire = function (t) {
      var ev = { type: t, target: this, currentTarget: this };
      var h = this['on' + t];
      if (isFn(h)) { try { h.call(this, ev); } catch (e) {} }
      (this._ls[t] || []).slice().forEach(function (f) { try { f.call(this, ev); } catch (e) {} }, this);
    };
    XHR.prototype.abort = function () { this._aborted = true; this.readyState = 0; };
    XHR.prototype.send = function (body) {
      var x = this;
      http(this._m, this._u, this._m === 'GET' || this._m === 'HEAD' ? null : body, function (st, text, type) {
        if (x._aborted) return;
        x.status = st; x.statusText = st === 200 ? 'OK' : ''; x._type = type;
        x.responseURL = String(x._u);
        x.responseText = text;
        if (x.responseType === 'json') { try { x.response = JSON.parse(text); } catch (e) { x.response = null; } }
        else x.response = text;
        x.readyState = 4;
        x._fire('readystatechange');
        x._fire(st ? 'load' : 'error');
        x._fire('loadend');
      });
    };
    g.XMLHttpRequest = XHR;
  }

  if (!isFn(g.fetch)) {
    var Response = function (body, init) {
      init = init || {};
      this.status = init.status === undefined ? 200 : init.status;
      this.ok = this.status >= 200 && this.status < 300;
      this.statusText = init.statusText || '';
      this.headers = init.headers instanceof Headers ? init.headers : new Headers(init.headers);
      this.url = init.url || '';
      this.redirected = false; this.type = 'basic';
      this._b = body == null ? '' : String(body);
      this.bodyUsed = false;
    };
    Response.prototype.text = function () { this.bodyUsed = true; return Promise.resolve(this._b); };
    Response.prototype.json = function () { var b = this._b; this.bodyUsed = true; return new Promise(function (r) { r(JSON.parse(b)); }); };
    Response.prototype.clone = function () { return new Response(this._b, this); };
    g.Response = Response;
    g.fetch = function (input, init) {
      init = init || {};
      var url = typeof input === 'string' ? input : (input && input.url) || String(input);
      var method = String(init.method || (input && input.method) || 'GET').toUpperCase();
      return new Promise(function (res, rej) {
        http(method, url, init.body, function (st, text, type) {
          if (!st) return rej(new TypeError('Failed to fetch'));
          res(new Response(text, { status: st, url: url, headers: { 'content-type': type } }));
        });
      });
    };
  }

  /* ------------------------------------------------------------ almacenamiento */
  function makeStorage(persist) {
    var data = {}, origin = '';
    if (persist && isFn(g.__eonStorage)) {
      try { origin = String(g.location.protocol) + '//' + String(g.location.host); } catch (e) { origin = 'local'; }
      try { var raw = g.__eonStorage(origin); if (raw) data = JSON.parse(raw) || {}; } catch (e) { data = {}; }
    }
    function save() { if (persist && isFn(g.__eonStorage)) { try { g.__eonStorage(origin, JSON.stringify(data)); } catch (e) {} } }
    var s = {
      getItem: function (k) { k = String(k); return Object.prototype.hasOwnProperty.call(data, k) ? data[k] : null; },
      setItem: function (k, v) { data[String(k)] = String(v); save(); },
      removeItem: function (k) { delete data[String(k)]; save(); },
      clear: function () { data = {}; save(); },
      key: function (i) { var k = Object.keys(data); return i < k.length ? k[i] : null; }
    };
    Object.defineProperty(s, 'length', { get: function () { return Object.keys(data).length; } });
    return s;
  }
  // Window trae localStorage como propiedad sin valor: se redefine en el global
  function defGlobal(name, v) {
    try { Object.defineProperty(g, name, { value: v, writable: true, configurable: true, enumerable: true }); }
    catch (e) { try { g[name] = v; } catch (e2) {} }
  }
  var hasLS = false;
  try { hasLS = !!g.localStorage && isFn(g.localStorage.setItem); } catch (e) {}
  if (!hasLS) defGlobal('localStorage', makeStorage(true));
  var hasSS = false;
  try { hasSS = !!g.sessionStorage && isFn(g.sessionStorage.setItem); } catch (e) {}
  if (!hasSS) defGlobal('sessionStorage', makeStorage(false));

  /* ------------------------------------------------------------ selectores CSS */
  // Soporta: * tag #id .clase [a] [a=v] [a^=v] [a$=v] [a*=v] [a~=v] [a|=v],
  // :first-child :last-child :only-child :nth-child(n|odd|even|an+b) :not(x)
  // :checked :disabled :enabled :empty, combinadores ' ' '>' '+' '~' y grupos ','.
  function trim(s) { return s.replace(/^\s+|\s+$/g, ''); }
  function splitTop(s, ch) {
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
  var cache = {};
  function parse(sel) {
    if (cache[sel]) return cache[sel];
    var groups = splitTop(sel, ',').map(function (grp) {
      grp = trim(grp).replace(/\s*([>+~])\s*/g, '$1');
      var steps = [], cur = '', comb = ' ', depth = 0, q = null;
      for (var i = 0; i <= grp.length; i++) {
        var c = grp.charAt(i);
        if (q) { if (c === q) q = null; cur += c; continue; }
        if (c === '"' || c === "'") { q = c; cur += c; continue; }
        if (c === '(' || c === '[') depth++;
        if (c === ')' || c === ']') depth--;
        if (i === grp.length || (depth === 0 && (c === ' ' || c === '>' || c === '+' || c === '~'))) {
          if (cur) { steps.push({ comb: comb, simple: compound(cur) }); cur = ''; }
          if (i < grp.length) comb = c;
          continue;
        }
        cur += c;
      }
      return steps;
    });
    cache[sel] = groups;
    return groups;
  }
  function compound(s) {
    var parts = [], re = /^(\*|[\w-]+)|#([\w-]+)|\.([\w-]+)|\[\s*([\w:-]+)\s*(?:([~|^$*]?=)\s*(?:"([^"]*)"|'([^']*)'|([^\]\s]*)))?\s*\]|:([\w-]+)(?:\(((?:[^()]|\([^()]*\))*)\))?/g;
    var m, pos = 0;
    while ((m = re.exec(s)) !== null) {
      if (m.index !== pos) throw new Error('Selector no soportado: ' + s);
      pos = re.lastIndex;
      if (m[1]) { if (m[1] !== '*') parts.push({ t: 'tag', v: m[1].toUpperCase() }); }
      else if (m[2]) parts.push({ t: 'id', v: m[2] });
      else if (m[3]) parts.push({ t: 'cls', v: m[3] });
      else if (m[4]) parts.push({ t: 'attr', n: m[4], op: m[5], v: m[6] !== undefined ? m[6] : m[7] !== undefined ? m[7] : m[8] });
      else if (m[9]) parts.push({ t: 'pseudo', n: m[9].toLowerCase(), a: m[10] });
      if (re.lastIndex === m.index) re.lastIndex++;
    }
    if (pos !== s.length) throw new Error('Selector no soportado: ' + s);
    return parts;
  }
  function elems(n) { var r = [], c = n.childNodes; for (var i = 0; c && i < c.length; i++) if (c[i].nodeType === 1) r.push(c[i]); return r; }
  function prevEl(e) { var p = e.previousSibling; while (p && p.nodeType !== 1) p = p.previousSibling; return p; }
  function nth(expr, idx) {
    expr = trim(expr).toLowerCase();
    if (expr === 'odd') return idx % 2 === 1;
    if (expr === 'even') return idx % 2 === 0;
    var m = /^([+-]?\d*)n\s*(?:([+-])\s*(\d+))?$/.exec(expr);
    if (!m) return idx === parseInt(expr, 10);
    var a = m[1] === '' || m[1] === '+' ? 1 : m[1] === '-' ? -1 : parseInt(m[1], 10);
    var b = m[2] ? parseInt(m[3], 10) * (m[2] === '-' ? -1 : 1) : 0;
    if (a === 0) return idx === b;
    return (idx - b) / a >= 0 && (idx - b) % a === 0;
  }
  function attr(e, n) { return e.getAttribute ? e.getAttribute(n) : null; }
  function matchSimple(e, parts) {
    for (var i = 0; i < parts.length; i++) {
      var p = parts[i], v;
      switch (p.t) {
      case 'tag': if (String(e.nodeName).toUpperCase() !== p.v) return false; break;
      case 'id': if (attr(e, 'id') !== p.v) return false; break;
      case 'cls': if ((' ' + (attr(e, 'class') || '').replace(/\s+/g, ' ') + ' ').indexOf(' ' + p.v + ' ') < 0) return false; break;
      case 'attr':
        v = attr(e, p.n);
        if (v === null) return false;
        if (!p.op) break;
        if (p.op === '=' && v !== p.v) return false;
        if (p.op === '^=' && v.indexOf(p.v) !== 0) return false;
        if (p.op === '$=' && v.slice(-p.v.length) !== p.v) return false;
        if (p.op === '*=' && v.indexOf(p.v) < 0) return false;
        if (p.op === '~=' && (' ' + v + ' ').indexOf(' ' + p.v + ' ') < 0) return false;
        if (p.op === '|=' && v !== p.v && v.indexOf(p.v + '-') !== 0) return false;
        break;
      case 'pseudo':
        var par = e.parentNode, sib = par ? elems(par) : [e], idx = sib.indexOf(e);
        switch (p.n) {
        case 'first-child': if (idx !== 0) return false; break;
        case 'last-child': if (idx !== sib.length - 1) return false; break;
        case 'only-child': if (sib.length !== 1) return false; break;
        case 'nth-child': if (!nth(p.a, idx + 1)) return false; break;
        case 'nth-last-child': if (!nth(p.a, sib.length - idx)) return false; break;
        case 'not': if (matches(e, p.a)) return false; break;
        case 'checked': if (!e.checked && attr(e, 'selected') === null) return false; break;
        case 'disabled': if (attr(e, 'disabled') === null) return false; break;
        case 'enabled': if (attr(e, 'disabled') !== null) return false; break;
        case 'empty': if (e.childNodes && e.childNodes.length) return false; break;
        case 'root': if (e.parentNode && e.parentNode.nodeType === 1) return false; break;
        default: return false;
        }
        break;
      }
    }
    return true;
  }
  function matchSteps(e, steps, k) {
    if (!matchSimple(e, steps[k].simple)) return false;
    if (k === 0) return true;
    var comb = steps[k].comb, n;
    if (comb === '>') { n = e.parentNode; return !!n && n.nodeType === 1 && matchSteps(n, steps, k - 1); }
    if (comb === '+') { n = prevEl(e); return !!n && matchSteps(n, steps, k - 1); }
    if (comb === '~') { for (n = prevEl(e); n; n = prevEl(n)) if (matchSteps(n, steps, k - 1)) return true; return false; }
    for (n = e.parentNode; n && n.nodeType === 1; n = n.parentNode) if (matchSteps(n, steps, k - 1)) return true;
    return false;
  }
  function matches(e, sel) {
    var groups = parse(sel);
    for (var i = 0; i < groups.length; i++) if (groups[i].length && matchSteps(e, groups[i], groups[i].length - 1)) return true;
    return false;
  }
  function qsa(root, sel, one) {
    var all = root.getElementsByTagName('*'), out = [];
    for (var i = 0; i < all.length; i++) {
      if (matches(all[i], sel)) { if (one) return all[i]; out.push(all[i]); }
    }
    return one ? null : out;
  }
  g.__eonMatches = matches;

  // Los metodos de NetSurf no se pueden pisar con '=' (se usa defineProperty)
  // y hay que poner los nuestros en el mismo prototipo donde esta el nativo,
  // o mas abajo en la cadena, para que ganen.
  function def(o, name, fn) {
    try { Object.defineProperty(o, name, { value: fn, writable: true, configurable: true }); }
    catch (e) { try { o[name] = fn; } catch (e2) {} }
  }
  function protoWith(obj, name) {
    for (var p = Object.getPrototypeOf(obj); p; p = Object.getPrototypeOf(p)) if (Object.prototype.hasOwnProperty.call(p, name)) return p;
    return null;
  }
  function put(sample, base, name, fn) {
    var p = protoWith(sample, name);
    def(p || base, name, fn);
    if (p && base && p !== base) def(base, name, fn);
  }
  function qs(s) { return qsa(this, s, true); }
  function qsAll(s) { var r = qsa(this, s, false); r.item = function (i) { return this[i] || null; }; return r; }
  function mt(s) { return matches(this, s); }
  function cl(s) { for (var n = this; n && n.nodeType === 1; n = n.parentNode) if (matches(n, s)) return n; return null; }
  function rm() { if (this.parentNode) this.parentNode.removeChild(this); }
  function ap() { for (var i = 0; i < arguments.length; i++) { var a = arguments[i]; this.appendChild(typeof a === 'string' ? g.document.createTextNode(a) : a); } }
  function pp() { for (var i = arguments.length - 1; i >= 0; i--) { var a = arguments[i]; a = typeof a === 'string' ? g.document.createTextNode(a) : a; this.insertBefore(a, this.firstChild); } }

  g.__eonInstallDom = function () {
    var d = g.document;
    if (!d || g.__eonDomDone) return;
    g.__eonDomDone = true;
    var dbase = protoWith(d, 'getElementsByTagName') || Object.getPrototypeOf(d);
    put(d, dbase, 'querySelector', qs);
    put(d, dbase, 'querySelectorAll', qsAll);
    // varios tipos de elemento para cubrir sus prototipos propios
    ['div', 'span', 'a', 'li', 'ul', 'p', 'input', 'button', 'form', 'img', 'section', 'table'].forEach(function (tag) {
      var e = d.createElement(tag);
      var ebase = protoWith(e, 'getAttribute') || Object.getPrototypeOf(e);
      put(e, ebase, 'querySelector', qs);
      put(e, ebase, 'querySelectorAll', qsAll);
      put(e, ebase, 'matches', mt);
      put(e, ebase, 'webkitMatchesSelector', mt);
      put(e, ebase, 'closest', cl);
      if (!isFn(e.remove)) put(e, ebase, 'remove', rm);
      if (!isFn(e.append)) put(e, ebase, 'append', ap);
      if (!isFn(e.prepend)) put(e, ebase, 'prepend', pp);
    });
  };
  try { g.__eonInstallDom(); } catch (e) {}
})(this);
