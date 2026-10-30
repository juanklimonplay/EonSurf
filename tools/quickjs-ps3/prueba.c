/* Prueba de QuickJS-ng en PS3: corre JavaScript moderno y escribe en TTY.log */
#include <stdio.h>
#include <string.h>
#include <sys/process.h>
#include <time.h>
#include <sys/systime.h>
#include "quickjs.h"

/* newlib de PSL1GHT declara clock_gettime pero no la trae: reloj del sistema en microsegundos */
int clock_gettime(clockid_t id, struct timespec *tp)
{
	u64 us = sysGetSystemTime();
	(void)id;
	tp->tv_sec = (time_t)(us / 1000000);
	tp->tv_nsec = (long)(us % 1000000) * 1000;
	return 0;
}

SYS_PROCESS_PARAM(1001, 0x100000);

static const char *script =
"const r = [];\n"
"const ok = (n, f) => { try { r.push((f() ? 'OK    ' : 'FALLA ') + n); } catch (e) { r.push('ERROR ' + n + ': ' + e); } };\n"
"ok('flechas/let/const', () => { let a = 2; const b = x => x * a; return b(3) === 6; });\n"
"ok('clases', () => { class A { #p = 1; get p() { return this.#p; } static s() { return 2; } } return new A().p + A.s() === 3; });\n"
"ok('plantillas', () => `${1 + 1}x` === '2x');\n"
"ok('desestructurar/spread', () => { const { a, ...o } = { a: 1, b: 2 }; const [x, , y] = [1, 2, 3]; return a + o.b + x + y === 7; });\n"
"ok('Map/Set', () => new Map([[1, 2]]).get(1) === 2 && new Set([1, 1, 2]).size === 2);\n"
"ok('async/await', () => { (async () => { await null; })(); return true; });\n"
"ok('generadores', () => { function* g() { yield 1; yield 2; } return [...g()].join() === '1,2'; });\n"
"ok('?. y ??', () => ({ a: null }).a?.b ?? 5 === 5);\n"
"ok('BigInt', () => (2n ** 64n).toString() === '18446744073709551616');\n"
"ok('Float64 big-endian', () => { const d = new DataView(new ArrayBuffer(8)); d.setFloat64(0, Math.PI); return d.getUint8(0) === 0x40 && d.getFloat64(0) === Math.PI; });\n"
"ok('TypedArray', () => { const a = new Uint32Array([1, 0x01020304]); return new Uint8Array(a.buffer)[4] === 0x01 || new Uint8Array(a.buffer)[4] === 0x04; });\n"
"ok('JSON', () => JSON.stringify(JSON.parse('{\"a\":[1,2.5,\"\u00f1\"]}')) === '{\"a\":[1,2.5,\"\u00f1\"]}');\n"
"ok('RegExp simple', () => 'a-b_c'.replace(/[-_]/g, '') === 'abc' && /^(\\d+)-(\\w+)$/.exec('12-ab')[2] === 'ab');\n"
"ok('RegExp unicode', () => new RegExp('\\\\p{L}+', 'u').test('canci\u00f3n'));\n"
"ok('RegExp {n,m}', () => new RegExp('^a{2,3}$').test('aaa'));\n"
"ok('Date', () => new Date(Date.UTC(2026, 9, 30)).toISOString() === '2026-10-30T00:00:00.000Z');\n"
"ok('numeros', () => (0.1 + 0.2).toFixed(2) === '0.30' && parseFloat('3.14') === 3.14 && (1e21).toString() === '1e+21');\n"
"ok('Array moderno', () => [3, 1, 2].toSorted().at(-1) === 3 && [1, [2, [3]]].flat(Infinity).length === 3);\n"
"ok('Object.groupBy', () => Object.groupBy([1, 2, 3], x => x % 2 ? 'i' : 'p').i.length === 2);\n"
"ok('Proxy/Reflect', () => new Proxy({}, { get: () => 7 }).x === 7 && Reflect.has({ a: 1 }, 'a'));\n"
"ok('Symbol/iteradores', () => typeof Symbol.iterator === 'symbol');\n"
"let t = Date.now(), s = 0; for (let i = 0; i < 2e6; i++) s += i % 7; r.push('Velocidad: 2 millones de vueltas en ' + (Date.now() - t) + ' ms');\n"
"ok('texto UTF-8 en el codigo', () => eval(\"'\xc3\xb1'\").charCodeAt(0) === 0xf1);\n"
"r.join('\\n');\n";

int main(void)
{
	JSRuntime *rt = JS_NewRuntime();
	JSContext *ctx = JS_NewContext(rt);
	JSValue v = JS_Eval(ctx, script, strlen(script), "<prueba>", JS_EVAL_TYPE_GLOBAL);
	if (JS_IsException(v)) {
		JSValue e = JS_GetException(ctx);
		const char *s = JS_ToCString(ctx, e);
		printf("EonQJS: EXCEPCION %s\n", s ? s : "?");
		JS_FreeCString(ctx, s);
		JS_FreeValue(ctx, e);
	} else {
		const char *s = JS_ToCString(ctx, v);
		printf("EonQJS: %s\n", JS_GetVersion());
		printf("EonQJS:\n%s\nEonQJS: FIN\n", s ? s : "?");
		JS_FreeCString(ctx, s);
	}
	JS_FreeValue(ctx, v);
	JS_FreeContext(ctx);
	JS_FreeRuntime(rt);
	return 0;
}
