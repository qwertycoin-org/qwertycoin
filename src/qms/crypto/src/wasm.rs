//! Browser-facing wasm-bindgen ABI.
//!
//! Binary responses intentionally match the native C ABI payloads so Web and
//! Qt clients can share parsers and conformance vectors.

use wasm_bindgen::prelude::*;

use crate::{Engine, Error, GENESIS_BYTES, INVITATION_ID_BYTES, RatchetCiphertext};

fn js_error(error: Error) -> JsError {
    JsError::new(&error.to_string())
}

fn append_u32(out: &mut Vec<u8>, value: usize) -> Result<(), JsError> {
    let value = u32::try_from(value).map_err(|_| JsError::new("QMS field exceeds u32"))?;
    out.extend_from_slice(&value.to_le_bytes());
    Ok(())
}

#[wasm_bindgen]
pub fn qwc_qms_wasm_abi_version() -> u32 {
    1
}

#[wasm_bindgen]
pub fn qwc_qms_wasm_engine_new() -> Result<Vec<u8>, JsError> {
    Engine::new()
        .and_then(|engine| engine.state())
        .map_err(js_error)
}

#[wasm_bindgen]
pub fn qwc_qms_wasm_prepare_contact_package(
    state: &[u8],
    genesis: &[u8],
) -> Result<Vec<u8>, JsError> {
    let genesis: [u8; GENESIS_BYTES] = genesis
        .try_into()
        .map_err(|_| JsError::new("genesis must be exactly 32 bytes"))?;
    let prepared = Engine::from_state(state)
        .and_then(|engine| engine.prepare_contact_package(genesis))
        .map_err(js_error)?;
    let package = crate::ContactPackage::decode(&prepared.package).map_err(js_error)?;
    let mut result = Vec::new();
    result.extend_from_slice(&package.invitation_id);
    append_u32(&mut result, prepared.package.len())?;
    result.extend_from_slice(&prepared.package);
    result.extend_from_slice(&prepared.next_state);
    Ok(result)
}

#[wasm_bindgen]
pub fn qwc_qms_wasm_prepare_import_contact(
    state: &[u8],
    local_invitation_id: &[u8],
    remote_package: &[u8],
    now_unix_seconds: u32,
) -> Result<Vec<u8>, JsError> {
    let invitation: [u8; INVITATION_ID_BYTES] = local_invitation_id
        .try_into()
        .map_err(|_| JsError::new("invitation id must be exactly 16 bytes"))?;
    let prepared = Engine::from_state(state)
        .and_then(|engine| {
            engine.prepare_import_contact(invitation, remote_package, u64::from(now_unix_seconds))
        })
        .map_err(js_error)?;
    let mut result = Vec::with_capacity(32 + prepared.next_state.len());
    result.extend_from_slice(&prepared.fingerprint);
    result.extend_from_slice(&prepared.next_state);
    Ok(result)
}

#[wasm_bindgen]
pub fn qwc_qms_wasm_prepare_send_text(
    state: &[u8],
    contact_id: &str,
    text: &str,
    now_unix_seconds: u32,
) -> Result<Vec<u8>, JsError> {
    let prepared = Engine::from_state(state)
        .and_then(|engine| engine.prepare_send_text(contact_id, text, u64::from(now_unix_seconds)))
        .map_err(js_error)?;
    let mut result = Vec::new();
    result.push(prepared.ciphertext.message_type);
    result.extend_from_slice(&prepared.message_id);
    append_u32(&mut result, prepared.ciphertext.bytes.len())?;
    result.extend_from_slice(&prepared.ciphertext.bytes);
    result.extend_from_slice(&prepared.next_state);
    Ok(result)
}

#[wasm_bindgen]
pub fn qwc_qms_wasm_prepare_receive_text(
    state: &[u8],
    contact_id: &str,
    message_type: u8,
    ciphertext: &[u8],
) -> Result<Vec<u8>, JsError> {
    let prepared = Engine::from_state(state)
        .and_then(|engine| {
            engine.prepare_receive_text(
                contact_id,
                &RatchetCiphertext {
                    message_type,
                    bytes: ciphertext.to_vec(),
                },
            )
        })
        .map_err(js_error)?;
    let mut result = Vec::new();
    result.extend_from_slice(&prepared.message_id);
    append_u32(&mut result, prepared.text.len())?;
    result.extend_from_slice(prepared.text.as_bytes());
    result.extend_from_slice(&prepared.next_state);
    Ok(result)
}

#[wasm_bindgen]
pub fn qwc_qms_wasm_transport_context(
    state: &[u8],
    contact_id: &str,
    outgoing: bool,
) -> Result<Vec<u8>, JsError> {
    let context = Engine::from_state(state)
        .and_then(|engine| engine.transport_context(contact_id, outgoing))
        .map_err(js_error)?;
    let mut result = Vec::with_capacity(97);
    result.extend_from_slice(&context.genesis);
    result.extend_from_slice(&context.invitation_id);
    result.extend_from_slice(&context.session_id);
    result.extend_from_slice(&context.root_secret);
    result.push(context.direction);
    Ok(result)
}
