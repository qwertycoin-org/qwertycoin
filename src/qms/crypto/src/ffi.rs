//! Narrow, versioned C ABI for the native Qt/CLI integration.
//!
//! Every successful operation returns one owned byte buffer. Ownership moves to
//! the caller and must be released with `qwc_qms_crypto_buffer_free`. Inputs are
//! borrowed only for the duration of the call. No Rust reference crosses the ABI.
//! All entry points validate pointer/length pairs and catch Rust panics.

use std::panic::{AssertUnwindSafe, catch_unwind};
use std::ptr;
use std::slice;

use crate::{Engine, Error, GENESIS_BYTES, INVITATION_ID_BYTES, RatchetCiphertext};

pub const ABI_VERSION: u32 = 3;

#[repr(C)]
pub struct Buffer {
    pub data: *mut u8,
    pub len: usize,
}

impl Buffer {
    const fn empty() -> Self {
        Self {
            data: ptr::null_mut(),
            len: 0,
        }
    }

    fn from_vec(value: Vec<u8>) -> Self {
        let boxed = value.into_boxed_slice();
        let len = boxed.len();
        let data = Box::into_raw(boxed) as *mut u8;
        Self { data, len }
    }
}

fn append_u32(out: &mut Vec<u8>, value: usize) -> Result<(), Error> {
    let value = u32::try_from(value).map_err(|_| Error::State("FFI field exceeds u32"))?;
    out.extend_from_slice(&value.to_le_bytes());
    Ok(())
}

unsafe fn borrowed<'a>(data: *const u8, len: usize) -> Result<&'a [u8], Error> {
    if len == 0 {
        return Ok(&[]);
    }
    if data.is_null() {
        return Err(Error::State("null FFI input with non-zero length"));
    }
    // SAFETY: the public ABI requires `data` to point at `len` readable bytes
    // for the duration of the call; every exported function copies or consumes
    // the slice before returning.
    Ok(unsafe { slice::from_raw_parts(data, len) })
}

fn write_result(
    output: *mut Buffer,
    error: *mut Buffer,
    operation: impl FnOnce() -> Result<Vec<u8>, Error>,
) -> i32 {
    if output.is_null() || error.is_null() {
        return 2;
    }
    // SAFETY: non-null pointers are required to reference writable Buffer
    // values owned by the caller for the duration of this call.
    unsafe {
        *output = Buffer::empty();
        *error = Buffer::empty();
    }
    match catch_unwind(AssertUnwindSafe(operation)) {
        Ok(Ok(bytes)) => {
            unsafe { *output = Buffer::from_vec(bytes) };
            0
        }
        Ok(Err(reason)) => {
            unsafe { *error = Buffer::from_vec(reason.to_string().into_bytes()) };
            1
        }
        Err(_) => {
            unsafe {
                *error = Buffer::from_vec(b"QMS crypto backend panic".to_vec());
            }
            255
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn qwc_qms_crypto_abi_version() -> u32 {
    ABI_VERSION
}

#[unsafe(no_mangle)]
pub extern "C" fn qwc_qms_crypto_buffer_free(buffer: Buffer) {
    if buffer.data.is_null() {
        return;
    }
    // SAFETY: buffers returned by this module are boxed slices whose exact
    // length is returned in the same value and which may be freed exactly once.
    unsafe {
        drop(Box::from_raw(ptr::slice_from_raw_parts_mut(
            buffer.data,
            buffer.len,
        )));
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn qwc_qms_crypto_engine_new(output: *mut Buffer, error: *mut Buffer) -> i32 {
    write_result(output, error, || Engine::new()?.state())
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qwc_qms_crypto_prepare_contact_package(
    state: *const u8,
    state_len: usize,
    genesis: *const u8,
    genesis_len: usize,
    output: *mut Buffer,
    error: *mut Buffer,
) -> i32 {
    write_result(output, error, || {
        // SAFETY: validated and copied before this entry point returns.
        let state = unsafe { borrowed(state, state_len)? };
        let genesis = unsafe { borrowed(genesis, genesis_len)? };
        let genesis: [u8; GENESIS_BYTES] = genesis
            .try_into()
            .map_err(|_| Error::State("genesis must be exactly 32 bytes"))?;
        let prepared = Engine::from_state(state)?.prepare_contact_package(genesis)?;
        let package = crate::ContactPackage::decode(&prepared.package)?;
        let mut result = Vec::new();
        result.extend_from_slice(&package.invitation_id);
        result.extend_from_slice(&package.fingerprint());
        append_u32(&mut result, prepared.package.len())?;
        result.extend_from_slice(&prepared.package);
        result.extend_from_slice(&prepared.next_state);
        Ok(result)
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qwc_qms_crypto_prepare_import_contact(
    state: *const u8,
    state_len: usize,
    local_invitation_id: *const u8,
    local_invitation_id_len: usize,
    remote_package: *const u8,
    remote_package_len: usize,
    now_unix_seconds: u64,
    output: *mut Buffer,
    error: *mut Buffer,
) -> i32 {
    write_result(output, error, || {
        let state = unsafe { borrowed(state, state_len)? };
        let local_invitation_id =
            unsafe { borrowed(local_invitation_id, local_invitation_id_len)? };
        let local_invitation_id: [u8; INVITATION_ID_BYTES] = local_invitation_id
            .try_into()
            .map_err(|_| Error::State("invitation id must be exactly 16 bytes"))?;
        let remote_package = unsafe { borrowed(remote_package, remote_package_len)? };
        let prepared = Engine::from_state(state)?.prepare_import_contact(
            local_invitation_id,
            remote_package,
            now_unix_seconds,
        )?;
        let mut result = Vec::with_capacity(32 + prepared.next_state.len());
        result.extend_from_slice(&prepared.fingerprint);
        result.extend_from_slice(&prepared.next_state);
        Ok(result)
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qwc_qms_crypto_prepare_send_text(
    state: *const u8,
    state_len: usize,
    contact_id: *const u8,
    contact_id_len: usize,
    text: *const u8,
    text_len: usize,
    now_unix_seconds: u64,
    output: *mut Buffer,
    error: *mut Buffer,
) -> i32 {
    write_result(output, error, || {
        let state = unsafe { borrowed(state, state_len)? };
        let contact_id = std::str::from_utf8(unsafe { borrowed(contact_id, contact_id_len)? })
            .map_err(|_| Error::State("contact id must be UTF-8"))?;
        let text = std::str::from_utf8(unsafe { borrowed(text, text_len)? })
            .map_err(|_| Error::InvalidMessage("message must be UTF-8"))?;
        let prepared =
            Engine::from_state(state)?.prepare_send_text(contact_id, text, now_unix_seconds)?;
        let mut result = Vec::new();
        result.push(prepared.ciphertext.message_type);
        result.extend_from_slice(&prepared.message_id);
        append_u32(&mut result, prepared.ciphertext.bytes.len())?;
        result.extend_from_slice(&prepared.ciphertext.bytes);
        result.extend_from_slice(&prepared.next_state);
        Ok(result)
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qwc_qms_crypto_prepare_receive_text(
    state: *const u8,
    state_len: usize,
    contact_id: *const u8,
    contact_id_len: usize,
    message_type: u8,
    ciphertext: *const u8,
    ciphertext_len: usize,
    output: *mut Buffer,
    error: *mut Buffer,
) -> i32 {
    write_result(output, error, || {
        let state = unsafe { borrowed(state, state_len)? };
        let contact_id = std::str::from_utf8(unsafe { borrowed(contact_id, contact_id_len)? })
            .map_err(|_| Error::State("contact id must be UTF-8"))?;
        let ciphertext = unsafe { borrowed(ciphertext, ciphertext_len)? };
        let prepared = Engine::from_state(state)?.prepare_receive_text(
            contact_id,
            &RatchetCiphertext {
                message_type,
                bytes: ciphertext.to_vec(),
            },
        )?;
        let mut result = Vec::new();
        result.extend_from_slice(&prepared.message_id);
        append_u32(&mut result, prepared.text.len())?;
        result.extend_from_slice(prepared.text.as_bytes());
        result.extend_from_slice(&prepared.next_state);
        Ok(result)
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qwc_qms_crypto_transport_context(
    state: *const u8,
    state_len: usize,
    contact_id: *const u8,
    contact_id_len: usize,
    outgoing: bool,
    output: *mut Buffer,
    error: *mut Buffer,
) -> i32 {
    write_result(output, error, || {
        let state = unsafe { borrowed(state, state_len)? };
        let contact_id = std::str::from_utf8(unsafe { borrowed(contact_id, contact_id_len)? })
            .map_err(|_| Error::State("contact id must be UTF-8"))?;
        let contexts = Engine::from_state(state)?.transport_contexts(contact_id, outgoing)?;
        let mut result = Vec::with_capacity(4 + 97 * contexts.len());
        append_u32(&mut result, contexts.len())?;
        for context in contexts {
            result.extend_from_slice(&context.genesis);
            result.extend_from_slice(&context.invitation_id);
            result.extend_from_slice(&context.session_id);
            result.extend_from_slice(&context.root_secret);
            result.push(context.direction);
        }
        Ok(result)
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn abi_rejects_invalid_pointer_length_and_owns_buffers() {
        let mut output = Buffer::empty();
        let mut error = Buffer::empty();
        assert_eq!(0, qwc_qms_crypto_engine_new(&mut output, &mut error));
        assert!(!output.data.is_null());
        assert!(output.len > 0);
        qwc_qms_crypto_buffer_free(output);
        output = Buffer::empty();

        let code = unsafe {
            qwc_qms_crypto_prepare_contact_package(
                ptr::null(),
                1,
                ptr::null(),
                0,
                &mut output,
                &mut error,
            )
        };
        assert_eq!(1, code);
        assert!(!error.data.is_null());
        qwc_qms_crypto_buffer_free(error);
    }
}
