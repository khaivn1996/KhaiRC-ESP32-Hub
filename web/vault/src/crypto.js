import {VaultFormatError} from './codec.js';

function base64(value, min, max) {
  if (typeof value !== 'string' || value.length > Math.ceil(max / 3) * 4 ||
      !/^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/.test(value)) {
    throw new VaultFormatError();
  }
  const bytes = Uint8Array.from(atob(value), ch => ch.charCodeAt(0));
  if (bytes.length < min || bytes.length > max) throw new VaultFormatError();
  return bytes;
}

// Accept the app document or the typed fields returned by Firestore REST.
export async function openVault(password, uid, doc) {
  const fields = doc?.fields;
  const integer = name => {
    const value = fields ? fields[name]?.integerValue : doc?.[name];
    if (fields && (typeof value !== 'string' || !/^-?\d+$/.test(value))) return NaN;
    return fields ? Number(value) : value;
  };
  const string = name => fields ? fields[name]?.stringValue : doc?.[name];
  const iterations = integer('iterations');
  if (integer('schemaVersion') !== 1 || string('kdfAlg') !== 'PBKDF2-SHA256' ||
      !Number.isInteger(iterations) || iterations < 1 || iterations > 2000000 ||
      typeof password !== 'string' || typeof uid !== 'string' || !uid) {
    throw new VaultFormatError();
  }
  const salt = base64(string('salt'), 16, 16);
  const iv = base64(string('iv'), 12, 12);
  const ct = base64(string('ct'), 16, 1048576);
  const encoded = new TextEncoder().encode(password);
  try {
    const material = await crypto.subtle.importKey('raw', encoded, 'PBKDF2', false, ['deriveKey']);
    const key = await crypto.subtle.deriveKey(
      {name: 'PBKDF2', hash: 'SHA-256', salt, iterations}, material,
      {name: 'AES-GCM', length: 256}, false, ['decrypt']);
    const plaintext = await crypto.subtle.decrypt(
      {name: 'AES-GCM', iv, tagLength: 128,
        additionalData: new TextEncoder().encode('khairc-vault-v1|' + uid)}, key, ct);
    return new Uint8Array(plaintext);
  } finally { encoded.fill(0); }
}
