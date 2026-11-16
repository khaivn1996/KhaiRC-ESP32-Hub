export class VaultFormatError extends Error {
  constructor() { super('vault_format'); this.name = 'VaultFormatError'; }
}

export function decodeVault(bytes) {
  if (!(bytes instanceof Uint8Array)) throw new VaultFormatError();
  let pos = 0;
  const fail = () => { throw new VaultFormatError(); };
  const u8 = () => pos < bytes.length ? bytes[pos++] : fail();
  const int32 = () => (u8() << 24) | (u8() << 16) | (u8() << 8) | u8();
  function utf() {
    const length = (u8() << 8) | u8();
    const end = pos + length;
    if (end > bytes.length) fail();
    let text = '';
    const continuation = () => {
      if (pos >= end) fail();
      const b = u8();
      if ((b & 0xc0) !== 0x80) fail();
      return b & 63;
    };
    while (pos < end) {
      const a = u8();
      let code;
      if (a > 0 && a < 0x80) code = a;
      else if ((a & 0xe0) === 0xc0) {
        code = ((a & 31) << 6) | continuation();
        if (code < 0x80 && !(a === 0xc0 && code === 0)) fail();
      } else if ((a & 0xf0) === 0xe0) {
        code = ((a & 15) << 12) | (continuation() << 6) | continuation();
        if (code < 0x800) fail();
      } else fail();
      // Java writeUTF encodes UTF-16 code units, including surrogate pairs.
      text += String.fromCharCode(code);
    }
    return text;
  }
  function group(read) {
    const flag = u8();
    if (flag !== 0 && flag !== 1) fail();
    return flag ? read() : null;
  }
  if (u8() !== 1) fail();
  const tuya = group(() => ({clientId: utf(), clientSecret: utf(), deviceId: utf()}));
  const hub = group(() => ({ipv4: utf(), apiKey: utf()}));
  const https = group(() => ({host: utf(), port: int32(), fingerprintHex: utf()}));
  const wol = group(() => ({mac: utf(), ip: utf()}));
  const labels = group(() => {
    const count = int32();
    if (count < 0 || count > 6) fail();
    const entries = [];
    const keys = new Set();
    for (let i = 0; i < count; ++i) {
      const key = utf(), value = utf();
      if (keys.has(key)) fail();
      keys.add(key); entries.push([key, value]);
    }
    return Object.fromEntries(entries);
  });
  if (pos !== bytes.length) fail();
  if (hub && !/^[\x20-\x7e]{64}$/.test(hub.apiKey)) fail();
  return {tuya, hub, https, wol, labels};
}
