import test from 'node:test';
import assert from 'node:assert/strict';
import {pbkdf2Sync, createCipheriv} from 'node:crypto';
import {existsSync, readFileSync} from 'node:fs';
import {decodeVault, VaultFormatError} from '../src/codec.js';
import {openVault} from '../src/crypto.js';

// Independent Java DataOutputStream.writeUTF encoder (UTF-16 code units).
function utf(text) {
  const bytes = [];
  for (let i = 0; i < text.length; ++i) {
    const c = text.charCodeAt(i);
    if (c >= 1 && c <= 127) bytes.push(c);
    else if (c <= 2047) bytes.push(0xc0 | (c >> 6), 0x80 | (c & 63));
    else bytes.push(0xe0 | (c >> 12), 0x80 | ((c >> 6) & 63), 0x80 | (c & 63));
  }
  const size = Buffer.alloc(2); size.writeUInt16BE(bytes.length);
  return Buffer.concat([size, Buffer.from(bytes)]);
}
const i32 = n => { const b = Buffer.alloc(4); b.writeInt32BE(n); return b; };
const full = {
  tuya: {clientId: 'id', clientSecret: 'secret', deviceId: 'device'},
  hub: {ipv4: '192.168.2.200', apiKey: 'A'.repeat(64)},
  https: {host: 'hub.example', port: 443, fingerprintHex: 'B'.repeat(64)},
  wol: {mac: '00:11:22:33:44:55', ip: '192.168.2.255'},
  labels: {'emoji': 'Quạt 🌬️', 'nul': 'A\0B'}
};
const empty = {tuya: null, hub: null, https: null, wol: null, labels: null};
function encode(v) {
  const chunks = [Buffer.from([1])];
  function group(value, body) {
    chunks.push(Buffer.from([value ? 1 : 0]));
    if (value) body(value);
  }
  const strings = (...s) => chunks.push(...s.map(utf));
  group(v.tuya, x => strings(x.clientId, x.clientSecret, x.deviceId));
  group(v.hub, x => strings(x.ipv4, x.apiKey));
  group(v.https, x => { strings(x.host); chunks.push(i32(x.port)); strings(x.fingerprintHex); });
  group(v.wol, x => strings(x.mac, x.ip));
  group(v.labels, x => {
    chunks.push(i32(Object.keys(x).length));
    for (const [key, value] of Object.entries(x)) strings(key, value);
  });
  return Buffer.concat(chunks);
}
function seal(bytes, password = 'master', uid = 'test-uid') {
  const salt = Buffer.from(Array.from({length: 16}, (_, i) => i));
  const iv = Buffer.from(Array.from({length: 12}, (_, i) => 0xa0 + i));
  const key = pbkdf2Sync(Buffer.from(password, 'utf8'), salt, 1000, 32, 'sha256');
  const cipher = createCipheriv('aes-256-gcm', key, iv);
  cipher.setAAD(Buffer.from('khairc-vault-v1|' + uid, 'utf8'));
  const ct = Buffer.concat([cipher.update(bytes), cipher.final(), cipher.getAuthTag()]);
  return {schemaVersion: 1, kdfAlg: 'PBKDF2-SHA256', iterations: 1000,
    salt: salt.toString('base64'), iv: iv.toString('base64'), ct: ct.toString('base64')};
}
for (const [name, value] of [['đủ nhóm', full], ['thiếu nhóm', empty],
  ['chỉ HUB', {...empty, hub: full.hub}]]) {
  test('roundtrip ' + name, async () => {
    const plaintext = await openVault('master', 'test-uid', seal(encode(value)));
    try { assert.deepEqual(decodeVault(plaintext), value); } finally { plaintext.fill(0); }
  });
}
test('LABELS giữ NUL và emoji', () => assert.deepEqual(decodeVault(encode(full)).labels, full.labels));
test('từ chối thừa byte', () => assert.throws(() => decodeVault(Buffer.concat([encode(full), Buffer.from([0])])), VaultFormatError));
test('từ chối version 2', () => {
  const b = encode(empty); b[0] = 2; assert.throws(() => decodeVault(b), VaultFormatError);
});
test('từ chối cờ 2 ở mọi nhóm', () => {
  for (let i = 1; i <= 5; ++i) {
    const b = encode(empty); b[i] = 2; assert.throws(() => decodeVault(b), VaultFormatError);
  }
});
test('từ chối mọi plaintext cụt', () => {
  const b = encode(full);
  for (let i = 0; i < b.length; ++i) assert.throws(() => decodeVault(b.subarray(0, i)), VaultFormatError);
});
test('từ chối modified UTF-8 lỗi', () => {
  for (const bytes of [[0], [0xf0, 0x9f, 0x98, 0x80], [0xc2], [0xc1, 0x81], [0xe0, 0x80, 0x80], [0xc2, 0x41]]) {
    const length = Buffer.alloc(2); length.writeUInt16BE(bytes.length);
    const b = Buffer.concat([Buffer.from([1, 1]), length, Buffer.from(bytes), utf('x'), utf('y'), Buffer.from([0, 0, 0, 0])]);
    assert.throws(() => decodeVault(b), VaultFormatError);
  }
});
test('từ chối LABELS trùng key hoặc count sai', () => {
  for (const count of [-1, 7]) assert.throws(() => decodeVault(Buffer.concat([Buffer.from([1, 0, 0, 0, 0, 1]), i32(count)])), VaultFormatError);
  assert.throws(() => decodeVault(Buffer.concat([Buffer.from([1, 0, 0, 0, 0, 1]), i32(2), utf('k'), utf('a'), utf('k'), utf('b')])), VaultFormatError);
});
test('sai mật khẩu', async () => {
  await assert.rejects(openVault('wrong', 'test-uid', seal(encode(full))), {name: 'OperationError'});
});
test('sửa một byte ciphertext', async () => {
  const doc = seal(encode(full)), ct = Buffer.from(doc.ct, 'base64'); ct[0] ^= 1;
  await assert.rejects(openVault('master', 'test-uid', {...doc, ct: ct.toString('base64')}), {name: 'OperationError'});
});
test('uid khác', async () => {
  await assert.rejects(openVault('master', 'another-uid', seal(encode(full))), {name: 'OperationError'});
});
test('doc thiếu field hoặc thuật toán lạ', async () => {
  const doc = seal(encode(full));
  for (const field of ['schemaVersion', 'kdfAlg', 'iterations', 'salt', 'iv', 'ct']) {
    const missing = {...doc}; delete missing[field];
    await assert.rejects(openVault('master', 'test-uid', missing), VaultFormatError);
  }
  await assert.rejects(openVault('master', 'test-uid', {...doc, kdfAlg: 'other'}), VaultFormatError);
});
test('giới hạn SealedDoc', async () => {
  const doc = seal(encode(empty));
  for (const change of [{schemaVersion: 2}, {iterations: 0}, {iterations: 2000001}, {iterations: 1.5},
    {salt: 'AA=='}, {iv: 'AA=='}, {ct: 'AA=='}, {salt: '!'.repeat(24)}]) {
    await assert.rejects(openVault('master', 'test-uid', {...doc, ...change}), VaultFormatError);
  }
});
test('Firestore REST integerValue là chuỗi', async () => {
  const doc = seal(encode(full));
  const fields = Object.fromEntries(Object.entries(doc).map(([k, v]) => [k,
    typeof v === 'number' ? {integerValue: String(v)} : {stringValue: v}]));
  assert.deepEqual(decodeVault(await openVault('master', 'test-uid', {fields})), full);
  fields.iterations = {integerValue: '1e3'};
  await assert.rejects(openVault('master', 'test-uid', {fields}), VaultFormatError);
});
test('apiKey 63 ký tự', () => {
  assert.throws(() => decodeVault(encode({...empty, hub: {...full.hub, apiKey: 'A'.repeat(63)}})), VaultFormatError);
});
test('mật khẩu UTF-8 có dấu', async () => {
  const password = 'Mật khẩu chủ 123';
  assert.deepEqual(decodeVault(await openVault(password, 'test-uid', seal(encode(full), password))), full);
});
const katPath = new URL('./kat-app.json', import.meta.url);
test('KAT từ app', {skip: !existsSync(katPath) && 'chưa có KAT từ app'}, async () => {
  const kat = JSON.parse(readFileSync(katPath, 'utf8'));
  const plaintext = await openVault(kat.password, kat.uid,
    {...kat, schemaVersion: 1, kdfAlg: 'PBKDF2-SHA256'});
  try { assert.deepEqual(decodeVault(plaintext), {...empty, ...kat.expected}); }
  finally { plaintext.fill(0); }
});
