"use strict";

const assert = require("node:assert/strict");
const path = require("node:path");

if (process.argv.length !== 3) {
  throw new Error("usage: node wasm_roundtrip.cjs <wasm-bindgen-js>");
}
const qms = require(path.resolve(process.argv[2]));

function u32(bytes, offset) {
  return bytes[offset] | (bytes[offset + 1] << 8) |
    (bytes[offset + 2] << 16) | (bytes[offset + 3] << 24);
}
function hex(bytes) {
  return Buffer.from(bytes).toString("hex");
}
function preparedPackage(state, genesis) {
  const encoded = qms.qwc_qms_wasm_prepare_contact_package(state, genesis);
  const invitation = encoded.slice(0, 16);
  const packageLength = u32(encoded, 16) >>> 0;
  return {
    invitation,
    package: encoded.slice(20, 20 + packageLength),
    state: encoded.slice(20 + packageLength),
  };
}
function imported(state, invitation, remote) {
  const encoded = qms.qwc_qms_wasm_prepare_import_contact(
    state, invitation, remote, 1700000000);
  return { contactId: hex(encoded.slice(0, 32)), state: encoded.slice(32) };
}
function sent(state, contactId, text, timestamp) {
  const encoded = qms.qwc_qms_wasm_prepare_send_text(state, contactId, text, timestamp);
  const ciphertextLength = u32(encoded, 17) >>> 0;
  return {
    type: encoded[0],
    messageId: encoded.slice(1, 17),
    ciphertext: encoded.slice(21, 21 + ciphertextLength),
    state: encoded.slice(21 + ciphertextLength),
  };
}
function received(state, contactId, message) {
  const encoded = qms.qwc_qms_wasm_prepare_receive_text(
    state, contactId, message.type, message.ciphertext);
  const textLength = u32(encoded, 16) >>> 0;
  return {
    messageId: encoded.slice(0, 16),
    text: Buffer.from(encoded.slice(20, 20 + textLength)).toString("utf8"),
    state: encoded.slice(20 + textLength),
  };
}

assert.equal(qms.qwc_qms_wasm_abi_version(), 1);
const genesis = new Uint8Array(32).fill(0x62);
const alicePackage = preparedPackage(qms.qwc_qms_wasm_engine_new(), genesis);
const bobPackage = preparedPackage(qms.qwc_qms_wasm_engine_new(), genesis);
const aliceImport = imported(alicePackage.state, alicePackage.invitation, bobPackage.package);
const bobImport = imported(bobPackage.state, bobPackage.invitation, alicePackage.package);
const outbound = sent(aliceImport.state, aliceImport.contactId, "x".repeat(4096), 1700000001);
assert.equal(outbound.type, 1, "first message must carry PQXDH pre-key data");
assert.ok(outbound.ciphertext.length + 1 <= 9556, "first message must fit 9600-byte envelope");
const inbound = received(bobImport.state, bobImport.contactId, outbound);
assert.equal(inbound.text, "x".repeat(4096));
assert.deepEqual(Buffer.from(inbound.messageId), Buffer.from(outbound.messageId));
const reply = sent(inbound.state, bobImport.contactId, "reply", 1700000002);
assert.equal(reply.type, 2, "reply must use the ongoing Triple Ratchet message type");
const openedReply = received(outbound.state, aliceImport.contactId, reply);
assert.equal(openedReply.text, "reply");
console.log(JSON.stringify({
  abi: 1,
  pqxdhBytes: outbound.ciphertext.length,
  pqxdhFits9600Envelope: true,
  ongoingTripleRatchet: true,
  textBytes: 4096,
}));
