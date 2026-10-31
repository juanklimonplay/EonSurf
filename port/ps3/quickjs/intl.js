/* EonSurf: Intl liviano para QuickJS-ng (que no trae Intl).
 * Cubre es / es-AR y en / en-US; cualquier otro idioma cae a en.
 * Sin dependencias. No pisa un Intl que ya exista.
 */
(function () {
'use strict';

var G = globalThis;
if (typeof G.Intl !== 'undefined' && G.Intl) return;

var hasOwn = Object.prototype.hasOwnProperty;

/* ---------------------------------------------------------------- utilidades */

function lit(value) { return { type: 'literal', value: value }; }

function pad2(n) {
  var s = String(n);
  return s.length < 2 ? '0' + s : s;
}

function partsToString(parts) {
  var s = '';
  for (var i = 0; i < parts.length; i++) s += parts[i].value;
  return s;
}

function clampInt(v, lo, hi) {
  v = Number(v);
  if (v !== v) return lo;
  v = Math.floor(v);
  if (v < lo) v = lo;
  if (v > hi) v = hi;
  return v;
}

function isUTCZone(tz) {
  if (tz === undefined || tz === null) return false;
  var s = String(tz).toLowerCase();
  return s === 'utc' || s === 'gmt' || s === 'z' ||
         s === 'etc/utc' || s === 'etc/gmt' || s === 'etc/utc0' || s === 'utc0';
}

function languageOf(tag) {
  return String(tag).slice(0, 2) === 'es' ? 'es' : 'en';
}

/* ------------------------------------------------------------ etiquetas BCP47 */

function canonicalTag(tag) {
  var s = String(tag).replace(/_/g, '-').trim();
  if (!s) return '';
  var parts = s.split('-');
  var lang = parts[0].toLowerCase();
  if (!/^[a-z]{2,3}$/.test(lang)) return '';
  var out = [lang];
  var seenScript = false, seenRegion = false;
  for (var i = 1; i < parts.length; i++) {
    var p = parts[i];
    if (!p) return '';
    if (!seenScript && /^[A-Za-z]{4}$/.test(p)) {
      out.push(p.charAt(0).toUpperCase() + p.slice(1).toLowerCase());
      seenScript = true;
    } else if (!seenRegion && (/^[A-Za-z]{2}$/.test(p) || /^[0-9]{3}$/.test(p))) {
      out.push(p.toUpperCase());
      seenRegion = true;
    } else if (/^[A-Za-z0-9]{5,8}$/.test(p)) {
      out.push(p.toLowerCase());
    } else {
      return '';
    }
  }
  return out.join('-');
}

function parseTag(tag) {
  var c = canonicalTag(tag);
  if (!c) return { language: '', script: '', region: '', variants: [], baseName: '' };
  var parts = c.split('-');
  var language = parts[0], script = '', region = '', variants = [];
  var seenScript = false, seenRegion = false;
  for (var i = 1; i < parts.length; i++) {
    var p = parts[i];
    if (!seenScript && /^[A-Za-z]{4}$/.test(p)) { script = p; seenScript = true; }
    else if (!seenRegion && (/^[A-Za-z]{2}$/.test(p) || /^[0-9]{3}$/.test(p))) { region = p; seenRegion = true; }
    else variants.push(p);
  }
  var base = language;
  if (script) base += '-' + script;
  if (region) base += '-' + region;
  for (var j = 0; j < variants.length; j++) base += '-' + variants[j];
  return { language: language, script: script, region: region, variants: variants, baseName: base };
}

function toLocaleArray(locales) {
  if (locales === undefined || locales === null) return [];
  if (typeof locales === 'string') return [locales];
  if (typeof locales === 'object' && typeof locales.length === 'number') {
    var out = [];
    for (var i = 0; i < locales.length; i++) out.push(String(locales[i]));
    return out;
  }
  return [String(locales)];
}

function matchLocale(locales) {
  var list = toLocaleArray(locales);
  for (var i = 0; i < list.length; i++) {
    var p = parseTag(list[i]);
    if (!p.language) continue;
    if (p.language === 'es') return p.region ? 'es-' + p.region : 'es';
    if (p.language === 'en') return p.region ? 'en-' + p.region : 'en';
  }
  return 'en';
}

function supportedLocalesOf(locales) {
  var list = toLocaleArray(locales);
  var out = [];
  for (var i = 0; i < list.length; i++) {
    var c = canonicalTag(list[i]);
    if (!c) throw new RangeError('Invalid language tag: ' + list[i]);
    var lang = c.split('-')[0];
    if ((lang === 'es' || lang === 'en') && out.indexOf(c) < 0) out.push(c);
  }
  return out;
}

function attachSupported(Ctor) {
  Ctor.supportedLocalesOf = function (locales) { return supportedLocalesOf(locales); };
}

/* --------------------------------------------------------------- fecha base */

var MONTHS = {
  es: {
    long: ['enero', 'febrero', 'marzo', 'abril', 'mayo', 'junio', 'julio',
      'agosto', 'septiembre', 'octubre', 'noviembre', 'diciembre'],
    short: ['ene', 'feb', 'mar', 'abr', 'may', 'jun', 'jul', 'ago', 'sep', 'oct', 'nov', 'dic'],
    narrow: ['E', 'F', 'M', 'A', 'M', 'J', 'J', 'A', 'S', 'O', 'N', 'D']
  },
  en: {
    long: ['January', 'February', 'March', 'April', 'May', 'June', 'July',
      'August', 'September', 'October', 'November', 'December'],
    short: ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'],
    narrow: ['J', 'F', 'M', 'A', 'M', 'J', 'J', 'A', 'S', 'O', 'N', 'D']
  }
};

var WEEKDAYS = {
  es: {
    long: ['domingo', 'lunes', 'martes', 'miércoles', 'jueves', 'viernes', 'sábado'],
    short: ['dom', 'lun', 'mar', 'mié', 'jue', 'vie', 'sáb'],
    narrow: ['D', 'L', 'M', 'X', 'J', 'V', 'S']
  },
  en: {
    long: ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'],
    short: ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'],
    narrow: ['S', 'M', 'T', 'W', 'T', 'F', 'S']
  }
};

function monthName(lang, month, width) {
  var names = MONTHS[lang === 'es' ? 'es' : 'en'][width] || MONTHS.en.long;
  return names[month - 1];
}

function weekdayName(lang, wd, width) {
  var names = WEEKDAYS[lang === 'es' ? 'es' : 'en'][width] || WEEKDAYS.en.long;
  return names[wd];
}

function getFields(date, utc) {
  var d;
  if (date instanceof Date) d = date;
  else if (date === undefined) d = new Date();
  else d = new Date(date);
  var t = d.getTime();
  if (t !== t) return null;
  var f;
  if (utc) {
    f = {
      y: d.getUTCFullYear(), mo: d.getUTCMonth() + 1, day: d.getUTCDate(),
      wd: d.getUTCDay(), h: d.getUTCHours(), mi: d.getUTCMinutes(),
      s: d.getUTCSeconds(), ms: d.getUTCMilliseconds()
    };
  } else {
    f = {
      y: d.getFullYear(), mo: d.getMonth() + 1, day: d.getDate(),
      wd: d.getDay(), h: d.getHours(), mi: d.getMinutes(),
      s: d.getSeconds(), ms: d.getMilliseconds()
    };
  }
  f.date = d;
  return f;
}

function tzLabel(date, utc, lang, style) {
  var long = style === 'long' || style === 'longOffset' || style === 'longGeneric';
  if (utc) {
    if (long) return lang === 'es' ? 'hora universal coordinada' : 'Coordinated Universal Time';
    return 'UTC';
  }
  var off = -date.getTimezoneOffset();
  var sign = off < 0 ? '-' : '+';
  var abs = Math.abs(off);
  var hh = Math.floor(abs / 60), mm = abs % 60;
  if (long) return 'GMT' + sign + pad2(hh) + ':' + pad2(mm);
  return 'GMT' + sign + hh + (mm ? ':' + pad2(mm) : '');
}

/* ---------------------------------------------------------- DateTimeFormat */

function validDateStyle(v) {
  return v === 'full' || v === 'long' || v === 'medium' || v === 'short';
}
function validMonth(v) {
  return v === 'numeric' || v === '2-digit' || v === 'long' || v === 'short' || v === 'narrow';
}
function validDay(v) { return v === 'numeric' || v === '2-digit'; }
function validWeekday(v) { return v === 'long' || v === 'short' || v === 'narrow'; }
function validHour(v) { return v === 'numeric' || v === '2-digit'; }

function initDTF(self, locales, options) {
  options = options || {};
  var resolved = matchLocale(locales);
  var lang = languageOf(resolved);
  var o = {
    locale: resolved, lang: lang, utc: isUTCZone(options.timeZone),
    dateStyle: undefined, timeStyle: undefined,
    year: undefined, month: undefined, day: undefined, weekday: undefined,
    hour: undefined, minute: undefined, second: undefined,
    timeZoneName: undefined, hour12: false
  };
  if (validDateStyle(options.dateStyle)) o.dateStyle = options.dateStyle;
  if (validDateStyle(options.timeStyle)) o.timeStyle = options.timeStyle;
  if (options.year !== undefined) o.year = options.year === '2-digit' ? '2-digit' : 'numeric';
  if (validMonth(options.month)) o.month = options.month;
  if (validDay(options.day)) o.day = options.day;
  if (validWeekday(options.weekday)) o.weekday = options.weekday;
  if (validHour(options.hour)) o.hour = options.hour;
  if (validHour(options.minute)) o.minute = options.minute;
  if (validHour(options.second)) o.second = options.second;
  if (options.timeZoneName !== undefined) o.timeZoneName = String(options.timeZoneName);

  if (o.timeStyle) {
    o.hour = 'numeric';
    o.minute = '2-digit';
    o.second = o.timeStyle === 'short' ? undefined : '2-digit';
    if (o.timeStyle === 'full') o.timeZoneName = 'long';
    else if (o.timeStyle === 'long') o.timeZoneName = 'short';
  }

  if (options.hour12 !== undefined) o.hour12 = !!options.hour12;
  else if (options.hourCycle === 'h11' || options.hourCycle === 'h12') o.hour12 = true;
  else if (options.hourCycle === 'h23' || options.hourCycle === 'h24') o.hour12 = false;
  else o.hour12 = lang === 'en';

  var hasD = !!(o.dateStyle || o.year || o.month || o.day || o.weekday);
  var hasT = !!(o.timeStyle || o.hour || o.minute || o.second || o.timeZoneName);
  if (!hasD && !hasT) {
    o.year = 'numeric';
    o.month = 'numeric';
    o.day = 'numeric';
  }
  self._o = o;
}

function yearPart(y, width) {
  return { type: 'year', value: width === '2-digit' ? pad2(y % 100) : String(y) };
}

function dayPart(day, width) {
  return { type: 'day', value: width === '2-digit' ? pad2(day) : String(day) };
}

function monthPart(lang, month, width) {
  if (width === 'long' || width === 'short' || width === 'narrow') {
    return { type: 'month', value: monthName(lang, month, width) };
  }
  return { type: 'month', value: width === '2-digit' ? pad2(month) : String(month) };
}

function buildDateStyleParts(o, f) {
  var lang = o.lang, style = o.dateStyle, parts = [];
  var mLong = monthName(lang, f.mo, 'long');
  var mShort = monthName(lang, f.mo, 'short');
  if (style === 'full') {
    parts.push({ type: 'weekday', value: weekdayName(lang, f.wd, 'long') });
    if (lang === 'es') {
      parts.push(lit(', '), dayPart(f.day, 'numeric'), lit(' de '),
        { type: 'month', value: mLong }, lit(' de '), yearPart(f.y, 'numeric'));
    } else {
      parts.push(lit(', '), { type: 'month', value: mLong }, lit(' '),
        dayPart(f.day, 'numeric'), lit(', '), yearPart(f.y, 'numeric'));
    }
  } else if (style === 'long') {
    if (lang === 'es') {
      parts.push(dayPart(f.day, 'numeric'), lit(' de '), { type: 'month', value: mLong },
        lit(' de '), yearPart(f.y, 'numeric'));
    } else {
      parts.push({ type: 'month', value: mLong }, lit(' '), dayPart(f.day, 'numeric'),
        lit(', '), yearPart(f.y, 'numeric'));
    }
  } else if (style === 'medium') {
    if (lang === 'es') {
      parts.push(dayPart(f.day, 'numeric'), lit(' '), { type: 'month', value: mShort },
        lit(' '), yearPart(f.y, 'numeric'));
    } else {
      parts.push({ type: 'month', value: mShort }, lit(' '), dayPart(f.day, 'numeric'),
        lit(', '), yearPart(f.y, 'numeric'));
    }
  } else {
    if (lang === 'es') {
      parts.push(dayPart(f.day, 'numeric'), lit('/'), monthPart(lang, f.mo, '2-digit'),
        lit('/'), yearPart(f.y, '2-digit'));
    } else {
      parts.push(monthPart(lang, f.mo, '2-digit'), lit('/'), dayPart(f.day, 'numeric'),
        lit('/'), yearPart(f.y, '2-digit'));
    }
  }
  return parts;
}

function buildDatePartsEs(o, f) {
  var parts = [];
  var hasDate = !!(o.year || o.month || o.day);
  if (o.weekday) {
    parts.push({ type: 'weekday', value: weekdayName('es', f.wd, o.weekday) });
    if (hasDate) parts.push(lit(', '));
    else return parts;
  }
  if (!o.month) {
    if (o.day && o.year) {
      parts.push(dayPart(f.day, o.day), lit('/'), yearPart(f.y, o.year));
    } else if (o.day) {
      parts.push(dayPart(f.day, o.day));
    } else if (o.year) {
      parts.push(yearPart(f.y, o.year));
    }
    return parts;
  }
  var numeric = o.month === 'numeric' || o.month === '2-digit';
  if (numeric) {
    if (o.day) parts.push(dayPart(f.day, o.day), lit('/'));
    parts.push(monthPart('es', f.mo, o.month));
    if (o.year) parts.push(lit('/'), yearPart(f.y, o.year));
  } else {
    if (o.day) parts.push(dayPart(f.day, o.day), lit(' de '));
    parts.push(monthPart('es', f.mo, o.month));
    if (o.year) parts.push(lit(' de '), yearPart(f.y, o.year));
  }
  return parts;
}

function buildDatePartsEn(o, f) {
  var parts = [];
  var hasDate = !!(o.year || o.month || o.day);
  if (o.weekday) {
    parts.push({ type: 'weekday', value: weekdayName('en', f.wd, o.weekday) });
    if (hasDate) parts.push(lit(', '));
    else return parts;
  }
  if (!o.month) {
    if (o.day && o.year) {
      parts.push(dayPart(f.day, o.day), lit('/'), yearPart(f.y, o.year));
    } else if (o.day) {
      parts.push(dayPart(f.day, o.day));
    } else if (o.year) {
      parts.push(yearPart(f.y, o.year));
    }
    return parts;
  }
  var numeric = o.month === 'numeric' || o.month === '2-digit';
  if (numeric) {
    parts.push(monthPart('en', f.mo, o.month));
    if (o.day) parts.push(lit('/'), dayPart(f.day, o.day));
    if (o.year) parts.push(lit('/'), yearPart(f.y, o.year));
  } else {
    parts.push(monthPart('en', f.mo, o.month));
    if (o.day) parts.push(lit(' '), dayPart(f.day, o.day));
    if (o.year) parts.push(lit(o.day ? ', ' : ' '), yearPart(f.y, o.year));
  }
  return parts;
}

function buildDateParts(o, f) {
  if (o.dateStyle) return buildDateStyleParts(o, f);
  return o.lang === 'es' ? buildDatePartsEs(o, f) : buildDatePartsEn(o, f);
}

function buildTimeParts(o, f) {
  var parts = [];
  var hour = f.h;
  var period = null;
  if (o.hour12 && o.hour) {
    if (hour < 12) period = o.lang === 'es' ? 'a. m.' : 'AM';
    else period = o.lang === 'es' ? 'p. m.' : 'PM';
    hour = hour % 12;
    if (hour === 0) hour = 12;
  }
  if (o.hour) {
    parts.push({ type: 'hour', value: o.hour === '2-digit' ? pad2(hour) : String(hour) });
  }
  if (o.minute) {
    if (parts.length) parts.push(lit(':'));
    parts.push({ type: 'minute', value: pad2(f.mi) });
  }
  if (o.second) {
    if (parts.length) parts.push(lit(':'));
    parts.push({ type: 'second', value: pad2(f.s) });
  }
  if (period) {
    parts.push(lit(' '), { type: 'dayPeriod', value: period });
  }
  if (o.timeZoneName) {
    if (parts.length) parts.push(lit(' '));
    parts.push({ type: 'timeZoneName', value: tzLabel(f.date, o.utc, o.lang, o.timeZoneName) });
  }
  return parts;
}

function DateTimeFormat(locales, options) {
  if (!(this instanceof DateTimeFormat)) return new DateTimeFormat(locales, options);
  initDTF(this, locales, options);
  var self = this;
  this.format = function (d) { return partsToString(self._parts(d)); };
  this.formatToParts = function (d) { return self._parts(d); };
  this.formatRange = function (a, b) { return self._range(a, b); };
}

DateTimeFormat.prototype._parts = function (date) {
  var o = this._o;
  if (date === undefined) date = new Date();
  var f = getFields(date, o.utc);
  if (!f) return [{ type: 'literal', value: 'Invalid Date' }];
  var hasD = !!(o.dateStyle || o.year || o.month || o.day || o.weekday);
  var hasT = !!(o.timeStyle || o.hour || o.minute || o.second || o.timeZoneName);
  var dp = hasD ? buildDateParts(o, f) : [];
  var tp = hasT ? buildTimeParts(o, f) : [];
  if (dp.length && tp.length) return dp.concat([lit(', ')], tp);
  return dp.length ? dp : tp;
};

DateTimeFormat.prototype._range = function (a, b) {
  var ta = partsToString(this._parts(a));
  var tb = partsToString(this._parts(b));
  if (ta === tb) return ta;
  return ta + ' – ' + tb;
};

DateTimeFormat.prototype.format = function (date) {
  return partsToString(this._parts(date));
};
DateTimeFormat.prototype.formatToParts = function (date) { return this._parts(date); };
DateTimeFormat.prototype.formatRange = function (a, b) { return this._range(a, b); };
DateTimeFormat.prototype.resolvedOptions = function () {
  var o = this._o;
  return {
    locale: o.locale,
    calendar: 'gregory',
    numberingSystem: 'latn',
    timeZone: o.utc ? 'UTC' : 'local',
    dateStyle: o.dateStyle,
    timeStyle: o.timeStyle,
    year: o.year,
    month: o.month,
    day: o.day,
    weekday: o.weekday,
    hour: o.hour,
    minute: o.minute,
    second: o.second,
    hour12: o.hour12,
    hourCycle: o.hour12 ? 'h12' : 'h23',
    timeZoneName: o.timeZoneName
  };
};

/* -------------------------------------------------------------- NumberFormat */

var ZERO_DECIMAL = {
  JPY: 1, KRW: 1, VND: 1, CLP: 1, ISK: 1, HUF: 1, TWD: 1, PYG: 1
};

var CURRENCY_SYMBOLS = {
  USD: '$', EUR: '€', GBP: '£', JPY: '¥', CNY: '¥', ARS: '$', BRL: 'R$',
  MXN: '$', CLP: '$', UYU: '$U', PEN: 'S/', COP: '$', CHF: 'CHF', CAD: 'CA$',
  AUD: 'A$', INR: '₹', KRW: '₩', RUB: '₽', TWD: 'NT$', PYG: '₲'
};

var CURRENCY_NAMES = {
  USD: ['US dollar', 'US dollars', 'dólar estadounidense', 'dólares estadounidenses'],
  EUR: ['euro', 'euros', 'euro', 'euros'],
  GBP: ['British pound', 'British pounds', 'libra esterlina', 'libras esterlinas'],
  JPY: ['Japanese yen', 'Japanese yen', 'yen japonés', 'yenes japoneses'],
  ARS: ['Argentine peso', 'Argentine pesos', 'peso argentino', 'pesos argentinos'],
  BRL: ['Brazilian real', 'Brazilian reais', 'real brasileño', 'reales brasileños'],
  MXN: ['Mexican peso', 'Mexican pesos', 'peso mexicano', 'pesos mexicanos'],
  CLP: ['Chilean peso', 'Chilean pesos', 'peso chileno', 'pesos chilenos'],
  CNY: ['Chinese yuan', 'Chinese yuan', 'yuan chino', 'yuanes chinos'],
  INR: ['Indian rupee', 'Indian rupees', 'rupia india', 'rupias indias'],
  RUB: ['Russian ruble', 'Russian rubles', 'rublo ruso', 'rublos rusos']
};

var UNITS = {
  kilometer: { sym: 'km', en: ['kilometer', 'kilometers'], es: ['kilómetro', 'kilómetros'] },
  meter: { sym: 'm', en: ['meter', 'meters'], es: ['metro', 'metros'] },
  centimeter: { sym: 'cm', en: ['centimeter', 'centimeters'], es: ['centímetro', 'centímetros'] },
  millimeter: { sym: 'mm', en: ['millimeter', 'millimeters'], es: ['milímetro', 'milímetros'] },
  mile: { sym: 'mi', en: ['mile', 'miles'], es: ['milla', 'millas'] },
  foot: { sym: 'ft', en: ['foot', 'feet'], es: ['pie', 'pies'] },
  inch: { sym: 'in', en: ['inch', 'inches'], es: ['pulgada', 'pulgadas'] },
  yard: { sym: 'yd', en: ['yard', 'yards'], es: ['yarda', 'yardas'] },
  gram: { sym: 'g', en: ['gram', 'grams'], es: ['gramo', 'gramos'] },
  kilogram: { sym: 'kg', en: ['kilogram', 'kilograms'], es: ['kilogramo', 'kilogramos'] },
  tonne: { sym: 't', en: ['tonne', 'tonnes'], es: ['tonelada', 'toneladas'] },
  second: { sym: 's', en: ['second', 'seconds'], es: ['segundo', 'segundos'] },
  minute: { sym: 'min', en: ['minute', 'minutes'], es: ['minuto', 'minutos'] },
  hour: { sym: 'h', en: ['hour', 'hours'], es: ['hora', 'horas'] },
  day: { sym: 'd', en: ['day', 'days'], es: ['día', 'días'] },
  week: { sym: 'sem', en: ['week', 'weeks'], es: ['semana', 'semanas'] },
  month: { sym: 'mes', en: ['month', 'months'], es: ['mes', 'meses'] },
  year: { sym: 'a', en: ['year', 'years'], es: ['año', 'años'] },
  liter: { sym: 'L', en: ['liter', 'liters'], es: ['litro', 'litros'] },
  byte: { sym: 'byte', en: ['byte', 'bytes'], es: ['byte', 'bytes'] },
  kilobyte: { sym: 'kB', en: ['kilobyte', 'kilobytes'], es: ['kilobyte', 'kilobytes'] },
  megabyte: { sym: 'MB', en: ['megabyte', 'megabytes'], es: ['megabyte', 'megabytes'] },
  gigabyte: { sym: 'GB', en: ['gigabyte', 'gigabytes'], es: ['gigabyte', 'gigabytes'] },
  percent: { sym: '%', en: ['percent', 'percent'], es: ['por ciento', 'por ciento'] },
  celsius: { sym: '°C', en: ['degree Celsius', 'degrees Celsius'], es: ['grado Celsius', 'grados Celsius'] },
  fahrenheit: { sym: '°F', en: ['degree Fahrenheit', 'degrees Fahrenheit'], es: ['grado Fahrenheit', 'grados Fahrenheit'] },
  acre: { sym: 'ac', en: ['acre', 'acres'], es: ['acre', 'acres'] },
  hectare: { sym: 'ha', en: ['hectare', 'hectares'], es: ['hectárea', 'hectáreas'] }
};

function currencySymbol(code) {
  return CURRENCY_SYMBOLS[code] || code;
}

function currencyName(lang, code, singular) {
  var n = CURRENCY_NAMES[code];
  if (!n) return code;
  if (lang === 'es') return singular ? n[2] : n[3];
  return singular ? n[0] : n[1];
}

function unitLabel(lang, unit, display, singular) {
  var u = UNITS[unit];
  if (!u) return String(unit);
  if (display === 'long') {
    var names = lang === 'es' ? u.es : u.en;
    return singular ? names[0] : names[1];
  }
  return u.sym;
}

function compactFor(lang, abs) {
  if (lang === 'es') {
    if (abs >= 1e6) return { div: 1e6, suffix: 'M' };
    if (abs >= 1e3) return { div: 1e3, suffix: 'mil' };
    return null;
  }
  if (abs >= 1e12) return { div: 1e12, suffix: 'T' };
  if (abs >= 1e9) return { div: 1e9, suffix: 'B' };
  if (abs >= 1e6) return { div: 1e6, suffix: 'M' };
  if (abs >= 1e3) return { div: 1e3, suffix: 'K' };
  return null;
}

function groupParts(intStr, sep) {
  var out = [];
  var len = intStr.length;
  var first = len % 3;
  if (first === 0) first = 3;
  out.push({ type: 'integer', value: intStr.slice(0, first) });
  for (var i = first; i < len; i += 3) {
    out.push({ type: 'group', value: sep });
    out.push({ type: 'integer', value: intStr.slice(i, i + 3) });
  }
  return out;
}

function initNF(self, locales, options) {
  options = options || {};
  var resolved = matchLocale(locales);
  var lang = languageOf(resolved);
  var style = options.style;
  if (style !== 'decimal' && style !== 'percent' && style !== 'currency' && style !== 'unit') {
    style = 'decimal';
  }
  var o = {
    locale: resolved, lang: lang, style: style,
    currency: 'USD', currencyDisplay: 'symbol',
    unit: 'meter', unitDisplay: 'short',
    notation: options.notation === 'compact' ? 'compact' : 'standard',
    useGrouping: options.useGrouping !== false && options.useGrouping !== 'false',
    signDisplay: 'auto',
    minInt: 1, minFrac: 0, maxFrac: 3
  };
  if (style === 'currency') {
    if (options.currency !== undefined) o.currency = String(options.currency).toUpperCase();
    var cd = options.currencyDisplay;
    if (cd === 'code' || cd === 'name' || cd === 'narrowSymbol' || cd === 'symbol') {
      o.currencyDisplay = cd;
    }
  }
  if (style === 'unit') {
    if (options.unit !== undefined) o.unit = String(options.unit);
    if (options.unitDisplay === 'long' || options.unitDisplay === 'short' ||
        options.unitDisplay === 'narrow') {
      o.unitDisplay = options.unitDisplay;
    }
  }
  var sd = options.signDisplay;
  if (sd === 'never' || sd === 'always' || sd === 'exceptZero' || sd === 'negative-zero') {
    o.signDisplay = sd;
  }
  if (options.minimumIntegerDigits !== undefined) {
    o.minInt = clampInt(options.minimumIntegerDigits, 1, 21);
  }
  var minSet = typeof options.minimumFractionDigits === 'number';
  var maxSet = typeof options.maximumFractionDigits === 'number';
  var zeroDec = ZERO_DECIMAL[o.currency] === 1;
  var defMin = style === 'currency' ? (zeroDec ? 0 : 2) : 0;
  if (minSet) o.minFrac = clampInt(options.minimumFractionDigits, 0, 20);
  else o.minFrac = defMin;
  if (maxSet) o.maxFrac = clampInt(options.maximumFractionDigits, 0, 20);
  else if (style === 'currency') o.maxFrac = Math.max(o.minFrac, zeroDec ? 0 : 2);
  else if (style === 'percent') o.maxFrac = Math.max(o.minFrac, 0);
  else o.maxFrac = Math.max(o.minFrac, 3);
  if (o.maxFrac < o.minFrac) o.maxFrac = o.minFrac;
  self._o = o;
}

function nfParts(o, value) {
  var n = typeof value === 'number' ? value : Number(value);
  var lang = o.lang;
  if (n !== n) return [{ type: 'nan', value: 'NaN' }];
  var neg = n < 0 || (n === 0 && 1 / n === -Infinity);
  var abs = Math.abs(n);
  if (abs === Infinity) {
    var inf = [];
    if (neg) inf.push({ type: 'minusSign', value: '-' });
    inf.push({ type: 'infinity', value: '∞' });
    return inf;
  }
  var sign = '';
  if (o.signDisplay !== 'never') {
    if (neg) sign = '-';
    else if (o.signDisplay === 'always') sign = '+';
    else if (o.signDisplay === 'exceptZero' && n > 0) sign = '+';
  }
  if (o.style === 'percent') abs = abs * 100;

  var maxF = o.maxFrac, minF = o.minFrac, suffix = null;
  if (o.notation === 'compact') {
    var comp = compactFor(lang, abs);
    if (comp) {
      abs = abs / comp.div;
      suffix = comp.suffix;
      minF = 0;
      maxF = abs < 10 ? 1 : 0;
    } else {
      minF = 0;
      maxF = 0;
    }
  }

  var fixed;
  if (abs >= 1e21) fixed = String(abs);
  else fixed = abs.toFixed(maxF);
  var dot = fixed.indexOf('.');
  var intStr, fracStr;
  if (dot < 0) { intStr = fixed; fracStr = ''; }
  else { intStr = fixed.slice(0, dot); fracStr = fixed.slice(dot + 1); }
  if (intStr.indexOf('e') >= 0) {
    intStr = String(Math.floor(abs));
    fracStr = '';
  }
  while (fracStr.length > minF && fracStr.charAt(fracStr.length - 1) === '0') {
    fracStr = fracStr.slice(0, -1);
  }
  while (intStr.length < o.minInt) intStr = '0' + intStr;

  var groupSep = lang === 'es' ? '.' : ',';
  var decSep = lang === 'es' ? ',' : '.';
  var numParts = [];
  if (o.useGrouping === false) numParts.push({ type: 'integer', value: intStr });
  else numParts = numParts.concat(groupParts(intStr, groupSep));
  if (fracStr.length) {
    numParts.push({ type: 'decimal', value: decSep });
    numParts.push({ type: 'fraction', value: fracStr });
  }
  if (suffix) {
    if (lang === 'es') numParts.push(lit(' '));
    numParts.push({ type: 'compact', value: suffix });
  }

  var out = [];
  if (sign === '-') out.push({ type: 'minusSign', value: '-' });
  else if (sign === '+') out.push({ type: 'plusSign', value: '+' });

  if (o.style === 'currency') {
    var code = o.currency;
    if (o.currencyDisplay === 'code') {
      out.push({ type: 'currency', value: code });
      out.push(lit(' '));
      out = out.concat(numParts);
    } else if (o.currencyDisplay === 'name') {
      out = out.concat(numParts);
      out.push(lit(' '));
      out.push({ type: 'currency', value: currencyName(lang, code, abs === 1) });
    } else if (lang === 'es') {
      out = out.concat(numParts);
      out.push(lit(' '));
      out.push({ type: 'currency', value: currencySymbol(code) });
    } else {
      out.push({ type: 'currency', value: currencySymbol(code) });
      out = out.concat(numParts);
    }
  } else if (o.style === 'percent') {
    out = out.concat(numParts);
    if (lang === 'es') out.push(lit(' '));
    out.push({ type: 'percentSign', value: '%' });
  } else if (o.style === 'unit') {
    out = out.concat(numParts);
    out.push(lit(' '));
    out.push({ type: 'unit', value: unitLabel(lang, o.unit, o.unitDisplay, abs === 1) });
  } else {
    out = out.concat(numParts);
  }
  return out;
}

function NumberFormat(locales, options) {
  if (!(this instanceof NumberFormat)) return new NumberFormat(locales, options);
  initNF(this, locales, options);
  var self = this;
  this.format = function (v) { return partsToString(nfParts(self._o, v)); };
  this.formatToParts = function (v) { return nfParts(self._o, v); };
}

NumberFormat.prototype.format = function (value) {
  return partsToString(nfParts(this._o, value));
};
NumberFormat.prototype.formatToParts = function (value) {
  return nfParts(this._o, value);
};
NumberFormat.prototype.resolvedOptions = function () {
  var o = this._o;
  var r = {
    locale: o.locale,
    numberingSystem: 'latn',
    style: o.style,
    minimumIntegerDigits: o.minInt,
    minimumFractionDigits: o.minFrac,
    maximumFractionDigits: o.maxFrac,
    useGrouping: o.useGrouping,
    notation: o.notation,
    signDisplay: o.signDisplay
  };
  if (o.style === 'currency') {
    r.currency = o.currency;
    r.currencyDisplay = o.currencyDisplay;
  }
  if (o.style === 'unit') {
    r.unit = o.unit;
    r.unitDisplay = o.unitDisplay;
  }
  return r;
};

/* ----------------------------------------------------------------- Collator */

function stripMarks(s) {
  if (typeof s.normalize === 'function') {
    try {
      s = s.normalize('NFD').replace(/[\u0300-\u036f]/g, '');
    } catch (_) {}
  }
  return s;
}

function normalizeCollation(s, sensitivity) {
  if (sensitivity === 'base') return stripMarks(s).toLowerCase();
  if (sensitivity === 'accent') return s.toLowerCase();
  if (sensitivity === 'case') return stripMarks(s);
  return s;
}

function numericCompare(a, b) {
  var ra = a.match(/\d+|\D+/g) || [];
  var rb = b.match(/\d+|\D+/g) || [];
  var n = Math.min(ra.length, rb.length);
  for (var i = 0; i < n; i++) {
    var x = ra[i], y = rb[i];
    var dx = /^\d+$/.test(x), dy = /^\d+$/.test(y);
    if (dx && dy) {
      var nx = Number(x), ny = Number(y);
      if (nx < ny) return -1;
      if (nx > ny) return 1;
    } else {
      if (x < y) return -1;
      if (x > y) return 1;
    }
  }
  if (ra.length < rb.length) return -1;
  if (ra.length > rb.length) return 1;
  return 0;
}

function collatorCompare(o, a, b) {
  a = String(a);
  b = String(b);
  if (o.ignorePunctuation) {
    a = a.replace(/[.,;:!?'"()\[\]{}]/g, '');
    b = b.replace(/[.,;:!?'"()\[\]{}]/g, '');
  }
  a = normalizeCollation(a, o.sensitivity);
  b = normalizeCollation(b, o.sensitivity);
  if (o.numeric) return numericCompare(a, b);
  if (a < b) return -1;
  if (a > b) return 1;
  return 0;
}

function Collator(locales, options) {
  if (!(this instanceof Collator)) return new Collator(locales, options);
  options = options || {};
  var sensitivity = options.sensitivity;
  if (sensitivity !== 'base' && sensitivity !== 'accent' &&
      sensitivity !== 'case' && sensitivity !== 'variant') {
    sensitivity = 'variant';
  }
  this._o = {
    locale: matchLocale(locales),
    lang: languageOf(matchLocale(locales)),
    sensitivity: sensitivity,
    numeric: !!options.numeric,
    ignorePunctuation: !!options.ignorePunctuation,
    caseFirst: options.caseFirst || 'false'
  };
  var self = this;
  this.compare = function (a, b) { return collatorCompare(self._o, a, b); };
}

Collator.prototype.compare = function (a, b) { return collatorCompare(this._o, a, b); };
Collator.prototype.resolvedOptions = function () {
  var o = this._o;
  return {
    locale: o.locale,
    usage: 'sort',
    sensitivity: o.sensitivity,
    ignorePunctuation: o.ignorePunctuation,
    collation: 'default',
    numeric: o.numeric,
    caseFirst: o.caseFirst
  };
};

/* -------------------------------------------------------------- PluralRules */

function pluralSelect(o, n) {
  n = Number(n);
  if (n !== n || !isFinite(n)) return 'other';
  n = Math.abs(n);
  if (o.lang === 'en' && o.type === 'ordinal') {
    var i = Math.floor(n);
    if (i === n) {
      var mod10 = i % 10, mod100 = i % 100;
      if (mod10 === 1 && mod100 !== 11) return 'one';
      if (mod10 === 2 && mod100 !== 12) return 'two';
      if (mod10 === 3 && mod100 !== 13) return 'few';
    }
    return 'other';
  }
  return n === 1 ? 'one' : 'other';
}

function PluralRules(locales, options) {
  if (!(this instanceof PluralRules)) return new PluralRules(locales, options);
  options = options || {};
  var resolved = matchLocale(locales);
  this._o = {
    locale: resolved,
    lang: languageOf(resolved),
    type: options.type === 'ordinal' ? 'ordinal' : 'cardinal'
  };
  var self = this;
  this.select = function (n) { return pluralSelect(self._o, n); };
}

PluralRules.prototype.select = function (n) { return pluralSelect(this._o, n); };
PluralRules.prototype.resolvedOptions = function () {
  var o = this._o;
  var cats;
  if (o.lang === 'en' && o.type === 'ordinal') cats = ['few', 'one', 'other', 'two'];
  else cats = ['one', 'other'];
  return { locale: o.locale, type: o.type, pluralCategories: cats };
};

/* ------------------------------------------------------- RelativeTimeFormat */

var RTF_UNITS = {
  second: { one: 'second', other: 'seconds', esOne: 'segundo', esOther: 'segundos' },
  minute: { one: 'minute', other: 'minutes', esOne: 'minuto', esOther: 'minutos' },
  hour: { one: 'hour', other: 'hours', esOne: 'hora', esOther: 'horas' },
  day: { one: 'day', other: 'days', esOne: 'día', esOther: 'días' },
  week: { one: 'week', other: 'weeks', esOne: 'semana', esOther: 'semanas' },
  month: { one: 'month', other: 'months', esOne: 'mes', esOther: 'meses' },
  quarter: { one: 'quarter', other: 'quarters', esOne: 'trimestre', esOther: 'trimestres' },
  year: { one: 'year', other: 'years', esOne: 'año', esOther: 'años' }
};

function normalizeRTFUnit(unit) {
  var u = String(unit).toLowerCase();
  var map = {
    seconds: 'second', minutes: 'minute', hours: 'hour', days: 'day',
    weeks: 'week', months: 'month', quarters: 'quarter', years: 'year'
  };
  return map[u] || u;
}

function relativeNumber(lang, abs) {
  var s = String(abs);
  if (lang === 'es') s = s.replace('.', ',');
  return s;
}

function relativeAuto(lang, n, unit) {
  var es = lang === 'es';
  if (n === 0) {
    if (unit === 'day') return es ? 'hoy' : 'today';
    if (unit === 'week') return es ? 'esta semana' : 'this week';
    if (unit === 'month') return es ? 'este mes' : 'this month';
    if (unit === 'year') return es ? 'este año' : 'this year';
    if (unit === 'hour') return es ? 'esta hora' : 'this hour';
    if (unit === 'minute') return es ? 'este minuto' : 'this minute';
    if (unit === 'second') return es ? 'este segundo' : 'this second';
    return null;
  }
  if (n === -1) {
    if (unit === 'day') return es ? 'ayer' : 'yesterday';
    if (unit === 'week') return es ? 'la semana pasada' : 'last week';
    if (unit === 'month') return es ? 'el mes pasado' : 'last month';
    if (unit === 'year') return es ? 'el año pasado' : 'last year';
    return null;
  }
  if (n === 1) {
    if (unit === 'day') return es ? 'mañana' : 'tomorrow';
    if (unit === 'week') return es ? 'la próxima semana' : 'next week';
    if (unit === 'month') return es ? 'el próximo mes' : 'next month';
    if (unit === 'year') return es ? 'el próximo año' : 'next year';
    return null;
  }
  return null;
}

function relativeFormat(o, value, unit) {
  var n = Number(value);
  if (n !== n) n = 0;
  unit = normalizeRTFUnit(unit);
  var lang = o.lang;
  if (o.numeric === 'auto') {
    var auto = relativeAuto(lang, n, unit);
    if (auto !== null) return auto;
  }
  var names = RTF_UNITS[unit] || RTF_UNITS.day;
  var abs = Math.abs(n);
  var num = relativeNumber(lang, abs);
  var label = lang === 'es'
    ? (abs === 1 ? names.esOne : names.esOther)
    : (abs === 1 ? names.one : names.other);
  if (n < 0) {
    if (lang === 'es') return 'hace ' + num + ' ' + label;
    return num + ' ' + label + ' ago';
  }
  if (lang === 'es') return 'dentro de ' + num + ' ' + label;
  return 'in ' + num + ' ' + label;
}

function RelativeTimeFormat(locales, options) {
  if (!(this instanceof RelativeTimeFormat)) return new RelativeTimeFormat(locales, options);
  options = options || {};
  var resolved = matchLocale(locales);
  this._o = {
    locale: resolved,
    lang: languageOf(resolved),
    numeric: options.numeric === 'auto' ? 'auto' : 'always',
    style: 'long'
  };
  var self = this;
  this.format = function (value, unit) { return relativeFormat(self._o, value, unit); };
}

RelativeTimeFormat.prototype.format = function (value, unit) {
  return relativeFormat(this._o, value, unit);
};
RelativeTimeFormat.prototype.resolvedOptions = function () {
  var o = this._o;
  return {
    locale: o.locale,
    style: o.style,
    numeric: o.numeric,
    numberingSystem: 'latn'
  };
};

/* --------------------------------------------------------------- ListFormat */

function listToArray(list) {
  if (list === undefined || list === null) throw new TypeError('List required');
  var out = [];
  var len = Number(list.length);
  if (len !== len) len = 0;
  for (var i = 0; i < len; i++) out.push(String(list[i]));
  return out;
}

function listFormatParts(o, list) {
  var arr = listToArray(list);
  if (arr.length === 0) return [];
  if (arr.length === 1) return [{ type: 'element', value: arr[0] }];
  var lang = o.lang;
  var parts = [];
  if (o.type === 'unit') {
    for (var j = 0; j < arr.length; j++) {
      if (j) parts.push(lit(', '));
      parts.push({ type: 'element', value: arr[j] });
    }
    return parts;
  }
  var word = o.type === 'disjunction'
    ? (lang === 'es' ? 'o' : 'or')
    : (lang === 'es' ? 'y' : 'and');
  if (arr.length === 2) {
    parts.push({ type: 'element', value: arr[0] });
    parts.push(lit(' ' + word + ' '));
    parts.push({ type: 'element', value: arr[1] });
    return parts;
  }
  for (var i = 0; i < arr.length - 1; i++) {
    if (i) parts.push(lit(', '));
    parts.push({ type: 'element', value: arr[i] });
  }
  parts.push(lit(lang === 'es' ? ' ' + word + ' ' : ', ' + word + ' '));
  parts.push({ type: 'element', value: arr[arr.length - 1] });
  return parts;
}

function ListFormat(locales, options) {
  if (!(this instanceof ListFormat)) return new ListFormat(locales, options);
  options = options || {};
  var type = options.type;
  if (type !== 'disjunction' && type !== 'unit' && type !== 'conjunction') type = 'conjunction';
  var style = options.style;
  if (style !== 'long' && style !== 'short' && style !== 'narrow') style = 'long';
  var resolved = matchLocale(locales);
  this._o = { locale: resolved, lang: languageOf(resolved), type: type, style: style };
  var self = this;
  this.format = function (list) { return partsToString(listFormatParts(self._o, list)); };
  this.formatToParts = function (list) { return listFormatParts(self._o, list); };
}

ListFormat.prototype.format = function (list) {
  return partsToString(listFormatParts(this._o, list));
};
ListFormat.prototype.formatToParts = function (list) {
  return listFormatParts(this._o, list);
};
ListFormat.prototype.resolvedOptions = function () {
  var o = this._o;
  return { locale: o.locale, type: o.type, style: o.style };
};

/* ---------------------------------------------------------------- Segmenter */

var WORD_RE = /[A-Za-z0-9]/;
try {
  WORD_RE = new RegExp('[\\p{L}\\p{N}\\p{M}]', 'u');
} catch (_) {}

function readCp(str, i) {
  var code;
  if (typeof str.codePointAt === 'function') code = str.codePointAt(i);
  else code = str.charCodeAt(i);
  var ch = typeof String.fromCodePoint === 'function'
    ? String.fromCodePoint(code) : str.charAt(i);
  return { ch: ch, len: ch.length };
}

function graphemeSegments(str, items) {
  var i = 0, idx = 0;
  while (i < str.length) {
    var r = readCp(str, i);
    items.push({ segment: r.ch, index: idx, input: str, isWordLike: false });
    i += r.len;
    idx += r.len;
  }
}

function isWordChar(ch) {
  return WORD_RE.test(ch);
}

function wordSegments(str, items) {
  var i = 0, idx = 0;
  while (i < str.length) {
    var r = readCp(str, i);
    if (isWordChar(r.ch)) {
      var start = i, startIdx = idx, seg = '';
      while (i < str.length) {
        r = readCp(str, i);
        if (!isWordChar(r.ch)) break;
        seg += r.ch;
        i += r.len;
        idx += r.len;
      }
      items.push({ segment: seg, index: startIdx, input: str, isWordLike: true });
    } else if (/\s/.test(r.ch)) {
      var i2 = idx, sp = '';
      while (i < str.length) {
        r = readCp(str, i);
        if (!/\s/.test(r.ch)) break;
        sp += r.ch;
        i += r.len;
        idx += r.len;
      }
      items.push({ segment: sp, index: i2, input: str, isWordLike: false });
    } else {
      items.push({ segment: r.ch, index: idx, input: str, isWordLike: false });
      i += r.len;
      idx += r.len;
    }
  }
}

function sentenceSegments(str, items) {
  var start = 0, i = 0;
  while (i < str.length) {
    var r = readCp(str, i);
    i += r.len;
    if (r.ch === '.' || r.ch === '!' || r.ch === '?') {
      var end = i >= str.length || /\s/.test(readCp(str, i).ch);
      if (end) {
        items.push({ segment: str.slice(start, i), index: start, input: str, isWordLike: false });
        start = i;
      }
    }
  }
  if (start < str.length) {
    items.push({ segment: str.slice(start), index: start, input: str, isWordLike: false });
  }
}

function makeSegments(str, granularity) {
  var items = [];
  if (granularity === 'word') wordSegments(str, items);
  else if (granularity === 'sentence') sentenceSegments(str, items);
  else graphemeSegments(str, items);
  var obj = {};
  obj[Symbol.iterator] = function () {
    var i = 0;
    return {
      next: function () {
        if (i < items.length) return { value: items[i++], done: false };
        return { value: undefined, done: true };
      }
    };
  };
  obj.containing = function (index) {
    index = Number(index) || 0;
    for (var i = 0; i < items.length; i++) {
      var it = items[i];
      if (index >= it.index && index < it.index + it.segment.length) return it;
    }
    return undefined;
  };
  return obj;
}

function Segmenter(locales, options) {
  if (!(this instanceof Segmenter)) return new Segmenter(locales, options);
  options = options || {};
  var g = options.granularity;
  if (g !== 'word' && g !== 'sentence') g = 'grapheme';
  var resolved = matchLocale(locales);
  this._o = { locale: resolved, lang: languageOf(resolved), granularity: g };
  var self = this;
  this.segment = function (input) {
    return makeSegments(String(input), self._o.granularity);
  };
}

Segmenter.prototype.segment = function (input) {
  return makeSegments(String(input), this._o.granularity);
};
Segmenter.prototype.resolvedOptions = function () {
  var o = this._o;
  return { locale: o.locale, granularity: o.granularity };
};

/* ------------------------------------------------------------------- Locale */

var LIKELY_SCRIPT = {
  en: 'Latn', es: 'Latn', fr: 'Latn', de: 'Latn', it: 'Latn', pt: 'Latn',
  nl: 'Latn', sv: 'Latn', pl: 'Latn', tr: 'Latn', ru: 'Cyrl', uk: 'Cyrl',
  ar: 'Arab', he: 'Hebr', hi: 'Deva', th: 'Thai', ja: 'Jpan', ko: 'Kore',
  zh: 'Hans', el: 'Grek'
};

var LIKELY_REGION = {
  en: 'US', es: 'ES', fr: 'FR', de: 'DE', it: 'IT', pt: 'BR', nl: 'NL',
  sv: 'SE', pl: 'PL', tr: 'TR', ru: 'RU', uk: 'UA', ar: 'EG', he: 'IL',
  hi: 'IN', th: 'TH', ja: 'JP', ko: 'KR', zh: 'CN', el: 'GR'
};

function titleCase(s) {
  s = String(s);
  return s.charAt(0).toUpperCase() + s.slice(1).toLowerCase();
}

function Locale(tag, options) {
  if (!(this instanceof Locale)) return new Locale(tag, options);
  options = options || {};
  var raw = tag;
  if (raw === undefined || raw === null) raw = options.language || 'en';
  var c = canonicalTag(raw);
  if (!c) throw new RangeError('Invalid language tag: ' + tag);
  var p = parseTag(c);
  this.language = options.language ? String(options.language).toLowerCase() : p.language;
  this.script = options.script ? titleCase(options.script) : p.script;
  this.region = options.region ? String(options.region).toUpperCase() : p.region;
  this.variants = p.variants.slice(0);
  this.calendar = options.calendar || 'gregory';
  this.numberingSystem = options.numberingSystem || 'latn';
  var base = this.language;
  if (this.script) base += '-' + this.script;
  if (this.region) base += '-' + this.region;
  for (var i = 0; i < this.variants.length; i++) base += '-' + this.variants[i];
  this.baseName = base;
  this.locale = base;
}

Locale.prototype.toString = function () { return this.baseName; };
Locale.prototype.maximize = function () {
  var script = this.script || LIKELY_SCRIPT[this.language] || '';
  var region = this.region || LIKELY_REGION[this.language] || '';
  var tag = this.language;
  if (script) tag += '-' + script;
  if (region) tag += '-' + region;
  return new Locale(tag);
};
Locale.prototype.minimize = function () {
  var script = this.script;
  var region = this.region;
  if (script && LIKELY_SCRIPT[this.language] === script) script = '';
  if (region && LIKELY_REGION[this.language] === region) region = '';
  var tag = this.language;
  if (script) tag += '-' + script;
  if (region) tag += '-' + region;
  return new Locale(tag);
};

/* ------------------------------------------------------------- DisplayNames */

var LANGUAGE_NAMES = {
  en: ['English', 'inglés'], es: ['Spanish', 'español'], fr: ['French', 'francés'],
  de: ['German', 'alemán'], it: ['Italian', 'italiano'], pt: ['Portuguese', 'portugués'],
  ja: ['Japanese', 'japonés'], zh: ['Chinese', 'chino'], ru: ['Russian', 'ruso'],
  ar: ['Arabic', 'árabe'], hi: ['Hindi', 'hindi'], ko: ['Korean', 'coreano'],
  nl: ['Dutch', 'neerlandés'], sv: ['Swedish', 'sueco'], pl: ['Polish', 'polaco'],
  tr: ['Turkish', 'turco'], el: ['Greek', 'griego'], he: ['Hebrew', 'hebreo'],
  th: ['Thai', 'tailandés'], uk: ['Ukrainian', 'ucraniano'], ca: ['Catalan', 'catalán'],
  da: ['Danish', 'danés'], fi: ['Finnish', 'finés'], no: ['Norwegian', 'noruego'],
  ro: ['Romanian', 'rumano'], cs: ['Czech', 'checo'], hu: ['Hungarian', 'húngaro'],
  id: ['Indonesian', 'indonesio'], vi: ['Vietnamese', 'vietnamita']
};

var REGION_NAMES = {
  US: ['United States', 'Estados Unidos'], AR: ['Argentina', 'Argentina'],
  ES: ['Spain', 'España'], MX: ['Mexico', 'México'], BR: ['Brazil', 'Brasil'],
  FR: ['France', 'Francia'], DE: ['Germany', 'Alemania'], GB: ['United Kingdom', 'Reino Unido'],
  IT: ['Italy', 'Italia'], JP: ['Japan', 'Japón'], CN: ['China', 'China'],
  CA: ['Canada', 'Canadá'], CL: ['Chile', 'Chile'], CO: ['Colombia', 'Colombia'],
  PE: ['Peru', 'Perú'], UY: ['Uruguay', 'Uruguay'], RU: ['Russia', 'Rusia'],
  IN: ['India', 'India'], AU: ['Australia', 'Australia'], PT: ['Portugal', 'Portugal'],
  CH: ['Switzerland', 'Suiza'], NL: ['Netherlands', 'Países Bajos'],
  KR: ['South Korea', 'Corea del Sur'], NZ: ['New Zealand', 'Nueva Zelanda'],
  SE: ['Sweden', 'Suecia'], PL: ['Poland', 'Polonia'], TR: ['Turkey', 'Turquía'],
  '419': ['Latin America', 'Latinoamérica']
};

var CALENDAR_NAMES = {
  gregory: ['Gregorian calendar', 'calendario gregoriano'],
  buddhist: ['Buddhist calendar', 'calendario budista'],
  chinese: ['Chinese calendar', 'calendario chino'],
  hebrew: ['Hebrew calendar', 'calendario hebreo'],
  islamic: ['Islamic calendar', 'calendario islámico'],
  japanese: ['Japanese calendar', 'calendario japonés'],
  persian: ['Persian calendar', 'calendario persa'],
  roc: ['Minguo calendar', 'calendario Minguo']
};

function DisplayNames(locales, options) {
  if (!(this instanceof DisplayNames)) return new DisplayNames(locales, options);
  options = options || {};
  var type = options.type;
  if (type !== 'language' && type !== 'region' && type !== 'currency' &&
      type !== 'calendar' && type !== 'script') {
    type = 'language';
  }
  var style = options.style;
  if (style !== 'long' && style !== 'short' && style !== 'narrow') style = 'long';
  var resolved = matchLocale(locales);
  this._o = {
    locale: resolved,
    lang: languageOf(resolved),
    type: type,
    style: style,
    fallback: options.fallback === 'none' ? 'none' : 'code'
  };
  var self = this;
  this.of = function (code) { return displayNameOf(self._o, code); };
}

function displayNameOf(o, code) {
  var c = String(code);
  var name = null;
  if (o.type === 'language') {
    var key = c.split('-')[0].toLowerCase();
    name = LANGUAGE_NAMES[key] || null;
  } else if (o.type === 'region') {
    name = REGION_NAMES[c.toUpperCase()] || null;
  } else if (o.type === 'currency') {
    var cn = CURRENCY_NAMES[c.toUpperCase()];
    if (cn) name = [cn[0], cn[2]];
    else name = null;
  } else if (o.type === 'calendar') {
    name = CALENDAR_NAMES[c.toLowerCase()] || null;
  } else if (o.type === 'script') {
    name = null;
  }
  if (name) return o.lang === 'es' ? name[1] : name[0];
  if (o.fallback === 'none') return undefined;
  return c;
}

DisplayNames.prototype.of = function (code) { return displayNameOf(this._o, code); };
DisplayNames.prototype.resolvedOptions = function () {
  var o = this._o;
  return {
    locale: o.locale,
    style: o.style,
    type: o.type,
    fallback: o.fallback
  };
};

/* --------------------------------------------------------------- objeto Intl */

var I = {
  DateTimeFormat: DateTimeFormat,
  NumberFormat: NumberFormat,
  Collator: Collator,
  PluralRules: PluralRules,
  RelativeTimeFormat: RelativeTimeFormat,
  ListFormat: ListFormat,
  Segmenter: Segmenter,
  Locale: Locale,
  DisplayNames: DisplayNames
};

I.getCanonicalLocales = function (locales) {
  var list = toLocaleArray(locales);
  var out = [];
  for (var i = 0; i < list.length; i++) {
    var c = canonicalTag(list[i]);
    if (!c) throw new RangeError('Invalid language tag: ' + list[i]);
    if (out.indexOf(c) < 0) out.push(c);
  }
  return out;
};

var SUPPORTED_VALUES = {
  calendar: ['gregory', 'buddhist', 'chinese', 'coptic', 'ethiopic', 'hebrew',
    'indian', 'islamic', 'japanese', 'persian', 'roc'],
  collation: ['compat', 'dict', 'emoji', 'eor', 'phonebk', 'phonetic', 'pinyin',
    'searchjl', 'stroke', 'trad', 'unihan', 'zhuyin'],
  currency: ['ARS', 'BRL', 'CLP', 'CNY', 'EUR', 'GBP', 'INR', 'JPY', 'MXN',
    'RUB', 'USD'],
  numberingSystem: ['arab', 'beng', 'deva', 'hebr', 'jpan', 'latn', 'thai'],
  timeZone: ['UTC', 'America/Argentina/Buenos_Aires', 'America/New_York',
    'America/Mexico_City', 'America/Sao_Paulo', 'Europe/Madrid', 'Europe/London',
    'Europe/Paris', 'Asia/Tokyo', 'Asia/Shanghai', 'Australia/Sydney'],
  unit: ['acre', 'byte', 'celsius', 'centimeter', 'day', 'fahrenheit', 'foot',
    'gram', 'hectare', 'hour', 'inch', 'kilogram', 'kilometer', 'liter', 'meter',
    'mile', 'millimeter', 'minute', 'month', 'percent', 'second', 'week', 'yard',
    'year']
};

I.supportedValuesOf = function (key) {
  var k = String(key);
  if (!hasOwn.call(SUPPORTED_VALUES, k)) {
    throw new RangeError('Invalid key: ' + k);
  }
  return SUPPORTED_VALUES[k].slice(0);
};

attachSupported(DateTimeFormat);
attachSupported(NumberFormat);
attachSupported(Collator);
attachSupported(PluralRules);
attachSupported(RelativeTimeFormat);
attachSupported(ListFormat);
attachSupported(Segmenter);
attachSupported(DisplayNames);

function tagProto(proto, name) {
  try {
    Object.defineProperty(proto, Symbol.toStringTag, { value: name, configurable: true });
  } catch (_) {}
}
tagProto(DateTimeFormat.prototype, 'Intl.DateTimeFormat');
tagProto(NumberFormat.prototype, 'Intl.NumberFormat');
tagProto(Collator.prototype, 'Intl.Collator');
tagProto(PluralRules.prototype, 'Intl.PluralRules');
tagProto(RelativeTimeFormat.prototype, 'Intl.RelativeTimeFormat');
tagProto(ListFormat.prototype, 'Intl.ListFormat');
tagProto(Segmenter.prototype, 'Intl.Segmenter');
tagProto(Locale.prototype, 'Intl.Locale');
tagProto(DisplayNames.prototype, 'Intl.DisplayNames');

/* --------------------------------------------------- parches a los prototipos */

function copyOptions(options) {
  var out = {};
  if (options && typeof options === 'object') {
    for (var k in options) if (hasOwn.call(options, k)) out[k] = options[k];
  }
  return out;
}

function hasAnyKey(o, keys) {
  for (var i = 0; i < keys.length; i++) {
    if (hasOwn.call(o, keys[i]) && o[keys[i]] !== undefined) return true;
  }
  return false;
}

Date.prototype.toLocaleDateString = function (locales, options) {
  var opts = copyOptions(options);
  var any = opts.dateStyle !== undefined || opts.timeStyle !== undefined ||
    hasAnyKey(opts, ['year', 'month', 'day', 'weekday', 'era']) ||
    hasAnyKey(opts, ['hour', 'minute', 'second', 'dayPeriod']);
  if (!any) {
    opts.year = 'numeric';
    opts.month = 'numeric';
    opts.day = 'numeric';
  }
  return new DateTimeFormat(locales, opts).format(this);
};

Date.prototype.toLocaleTimeString = function (locales, options) {
  var opts = copyOptions(options);
  var any = opts.dateStyle !== undefined || opts.timeStyle !== undefined ||
    hasAnyKey(opts, ['hour', 'minute', 'second', 'dayPeriod', 'fractionalSecondDigits']) ||
    hasAnyKey(opts, ['year', 'month', 'day', 'weekday']);
  if (!any) {
    opts.hour = 'numeric';
    opts.minute = 'numeric';
    opts.second = 'numeric';
  }
  return new DateTimeFormat(locales, opts).format(this);
};

Date.prototype.toLocaleString = function (locales, options) {
  var opts = copyOptions(options);
  var any = opts.dateStyle !== undefined || opts.timeStyle !== undefined ||
    hasAnyKey(opts, ['year', 'month', 'day', 'weekday', 'era']) ||
    hasAnyKey(opts, ['hour', 'minute', 'second', 'dayPeriod', 'fractionalSecondDigits']);
  if (!any) {
    opts.year = 'numeric';
    opts.month = 'numeric';
    opts.day = 'numeric';
    opts.hour = 'numeric';
    opts.minute = 'numeric';
    opts.second = 'numeric';
  }
  return new DateTimeFormat(locales, opts).format(this);
};

Number.prototype.toLocaleString = function (locales, options) {
  return new NumberFormat(locales, options).format(Number(this));
};

String.prototype.localeCompare = function (that, locales, options) {
  return new Collator(locales, options).compare(String(this), String(that));
};

try {
  Object.defineProperty(G, 'Intl', {
    value: I, writable: true, configurable: true, enumerable: false
  });
} catch (_) {
  G.Intl = I;
}
})();
