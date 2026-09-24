mod ffi;
mod package;
mod store;
#[cfg(target_arch = "wasm32")]
mod wasm;

use std::collections::{BTreeMap, BTreeSet};
use std::time::{Duration, SystemTime};

use futures_util::FutureExt;
use libsignal_protocol::{
    CiphertextMessage, CiphertextMessageType, DeviceId, IdentityKeyPair, PreKeySignalMessage,
    ProtocolAddress, SignalMessage, SignalProtocolError, message_decrypt, message_encrypt,
    process_prekey_bundle,
};
use rand::TryRngCore as _;
use rand::rngs::OsRng;
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use thiserror::Error;

pub use package::ContactPackage;
use package::generate_contact_package;
use store::StoreSnapshot;

pub const WIRE_VERSION: u8 = 2;
pub const PROFILE: u8 = 2;
pub const GENESIS_BYTES: usize = 32;
pub const INVITATION_ID_BYTES: usize = 16;
pub const SESSION_ID_BYTES: usize = 16;
pub const MESSAGE_ID_BYTES: usize = 16;
pub const OUTER_SECRET_BYTES: usize = 32;
pub const MAX_TEXT_BYTES: usize = 4096;
pub const INNER_FIXED_BYTES: usize = 104;

const INNER_DOMAIN: &[u8] = b"QWC-QMS-INNER-V2";
const SESSION_DOMAIN: &[u8] = b"QWC-QMS2-SESSION-ID";

#[derive(Debug, Error)]
pub enum Error {
    #[error("libsignal protocol error: {0}")]
    Signal(#[from] SignalProtocolError),
    #[error("state encoding error: {0}")]
    StateEncoding(#[from] postcard::Error),
    #[error("invalid contact package: {0}")]
    InvalidPackage(&'static str),
    #[error("invalid QMS message: {0}")]
    InvalidMessage(&'static str),
    #[error("QMS state error: {0}")]
    State(&'static str),
}

#[derive(Clone, Serialize, Deserialize)]
struct ContactContext {
    local_package: Vec<u8>,
    remote_package: Vec<u8>,
    session_id: [u8; SESSION_ID_BYTES],
    local_send_direction: u8,
}

#[derive(Clone, Serialize, Deserialize)]
struct EngineState {
    store: StoreSnapshot,
    local_packages: BTreeMap<[u8; INVITATION_ID_BYTES], Vec<u8>>,
    contacts: BTreeMap<String, ContactContext>,
    received_message_ids: BTreeSet<[u8; MESSAGE_ID_BYTES]>,
}

#[derive(Clone)]
pub struct Engine {
    state: EngineState,
}

#[derive(Debug)]
pub struct PreparedPackage {
    pub package: Vec<u8>,
    pub next_state: Vec<u8>,
}

#[derive(Debug)]
pub struct PreparedImport {
    pub contact_id: String,
    pub fingerprint: [u8; 32],
    pub next_state: Vec<u8>,
}

#[derive(Debug)]
pub struct RatchetCiphertext {
    pub message_type: u8,
    pub bytes: Vec<u8>,
}

#[derive(Debug)]
pub struct PreparedSend {
    pub ciphertext: RatchetCiphertext,
    pub message_id: [u8; MESSAGE_ID_BYTES],
    pub next_state: Vec<u8>,
}

#[derive(Debug)]
pub struct PreparedReceive {
    pub text: String,
    pub message_id: [u8; MESSAGE_ID_BYTES],
    pub next_state: Vec<u8>,
}

#[derive(Debug, Clone, Eq, PartialEq)]
pub struct TransportContext {
    pub genesis: [u8; GENESIS_BYTES],
    pub invitation_id: [u8; INVITATION_ID_BYTES],
    pub session_id: [u8; SESSION_ID_BYTES],
    pub root_secret: [u8; OUTER_SECRET_BYTES],
    pub direction: u8,
}

impl Engine {
    pub fn new() -> Result<Self, Error> {
        let mut rng = OsRng.unwrap_err();
        let identity = IdentityKeyPair::generate(&mut rng);
        let registration_id = (u32::from(
            rng.try_next_u32()
                .map_err(|_| Error::State("operating-system randomness unavailable"))?,
        ) & 0x3fff)
            .max(1);
        Ok(Self {
            state: EngineState {
                store: StoreSnapshot::new(identity, registration_id),
                local_packages: BTreeMap::new(),
                contacts: BTreeMap::new(),
                received_message_ids: BTreeSet::new(),
            },
        })
    }

    pub fn from_state(encoded: &[u8]) -> Result<Self, Error> {
        Ok(Self {
            state: postcard::from_bytes(encoded)?,
        })
    }

    pub fn state(&self) -> Result<Vec<u8>, Error> {
        Ok(postcard::to_allocvec(&self.state)?)
    }

    pub fn prepare_contact_package(
        &self,
        genesis: [u8; GENESIS_BYTES],
    ) -> Result<PreparedPackage, Error> {
        let mut candidate = self.clone();
        let mut rng = OsRng.unwrap_err();
        let package = generate_contact_package(&mut candidate.state.store, genesis, &mut rng)
            .now_or_never()
            .expect("QMS stores are synchronous")?;
        let encoded = package.encode()?;
        candidate
            .state
            .local_packages
            .insert(package.invitation_id, encoded.clone());
        Ok(PreparedPackage {
            package: encoded,
            next_state: candidate.state()?,
        })
    }

    pub fn prepare_import_contact(
        &self,
        local_invitation_id: [u8; INVITATION_ID_BYTES],
        encoded_remote: &[u8],
        now_unix_seconds: u64,
    ) -> Result<PreparedImport, Error> {
        let mut candidate = self.clone();
        let local_encoded = candidate
            .state
            .local_packages
            .get(&local_invitation_id)
            .ok_or(Error::State("unknown local invitation"))?
            .clone();
        let local = ContactPackage::decode(&local_encoded)?;
        let remote = ContactPackage::decode(encoded_remote)?;
        if local.genesis != remote.genesis {
            return Err(Error::InvalidPackage("network genesis mismatch"));
        }
        if local.identity_key == remote.identity_key {
            return Err(Error::InvalidPackage("self contact package"));
        }
        let fingerprint = remote.fingerprint();
        let contact_id = hex::encode(fingerprint);
        if let Some(existing) = candidate.state.contacts.get(&contact_id) {
            if existing.local_package == local_encoded
                && existing.remote_package.as_slice() == encoded_remote
            {
                return Ok(PreparedImport {
                    contact_id,
                    fingerprint,
                    next_state: candidate.state()?,
                });
            }
            return Err(Error::State("conflicting contact import"));
        }

        let local_fingerprint = local.fingerprint();
        let local_send_direction = if local_fingerprint < fingerprint {
            0
        } else {
            1
        };
        let session_id = session_id(&local, &remote);
        let remote_address = address_for_package(&remote, session_id, local_send_direction ^ 1)?;
        let local_address = address_for_package(&local, session_id, local_send_direction)?;
        let mut rng = OsRng.unwrap_err();
        process_prekey_bundle(
            &remote_address,
            &local_address,
            &mut candidate.state.store.sessions,
            &mut candidate.state.store.identity,
            &remote.pre_key_bundle()?,
            unix_time(now_unix_seconds),
            &mut rng,
        )
        .now_or_never()
        .expect("QMS stores are synchronous")?;

        candidate.state.contacts.insert(
            contact_id.clone(),
            ContactContext {
                local_package: local_encoded,
                remote_package: encoded_remote.to_vec(),
                session_id,
                local_send_direction,
            },
        );
        Ok(PreparedImport {
            contact_id,
            fingerprint,
            next_state: candidate.state()?,
        })
    }

    pub fn prepare_send_text(
        &self,
        contact_id: &str,
        text: &str,
        now_unix_seconds: u64,
    ) -> Result<PreparedSend, Error> {
        if text.len() > MAX_TEXT_BYTES || std::str::from_utf8(text.as_bytes()).is_err() {
            return Err(Error::InvalidMessage(
                "message must be valid UTF-8 and at most 4096 bytes",
            ));
        }
        let mut candidate = self.clone();
        let contact = candidate
            .state
            .contacts
            .get(contact_id)
            .cloned()
            .ok_or(Error::State("unknown contact"))?;
        let local = ContactPackage::decode(&contact.local_package)?;
        let remote = ContactPackage::decode(&contact.remote_package)?;
        let mut rng = OsRng.unwrap_err();
        let mut message_id = [0u8; MESSAGE_ID_BYTES];
        rng.try_fill_bytes(&mut message_id)
            .map_err(|_| Error::State("operating-system randomness unavailable"))?;
        let inner = encode_inner(
            &local,
            &remote,
            contact.session_id,
            contact.local_send_direction,
            message_id,
            text,
        )?;
        let remote_address = address_for_package(
            &remote,
            contact.session_id,
            contact.local_send_direction ^ 1,
        )?;
        let local_address =
            address_for_package(&local, contact.session_id, contact.local_send_direction)?;
        let encrypted = message_encrypt(
            &inner,
            &remote_address,
            &local_address,
            &mut candidate.state.store.sessions,
            &mut candidate.state.store.identity,
            unix_time(now_unix_seconds),
            &mut rng,
        )
        .now_or_never()
        .expect("QMS stores are synchronous")?;
        let message_type = match encrypted.message_type() {
            CiphertextMessageType::PreKey => 1,
            CiphertextMessageType::Whisper => 2,
            _ => return Err(Error::State("unexpected libsignal ciphertext type")),
        };
        Ok(PreparedSend {
            ciphertext: RatchetCiphertext {
                message_type,
                bytes: encrypted.serialize().to_vec(),
            },
            message_id,
            next_state: candidate.state()?,
        })
    }

    pub fn prepare_receive_text(
        &self,
        contact_id: &str,
        ciphertext: &RatchetCiphertext,
    ) -> Result<PreparedReceive, Error> {
        let mut candidate = self.clone();
        let contact = candidate
            .state
            .contacts
            .get(contact_id)
            .cloned()
            .ok_or(Error::State("unknown contact"))?;
        let local = ContactPackage::decode(&contact.local_package)?;
        let remote = ContactPackage::decode(&contact.remote_package)?;
        let parsed = match ciphertext.message_type {
            1 => CiphertextMessage::PreKeySignalMessage(PreKeySignalMessage::try_from(
                ciphertext.bytes.as_slice(),
            )?),
            2 => CiphertextMessage::SignalMessage(SignalMessage::try_from(
                ciphertext.bytes.as_slice(),
            )?),
            _ => return Err(Error::InvalidMessage("unknown libsignal ciphertext type")),
        };
        let remote_address = address_for_package(
            &remote,
            contact.session_id,
            contact.local_send_direction ^ 1,
        )?;
        let local_address =
            address_for_package(&local, contact.session_id, contact.local_send_direction)?;
        let mut rng = OsRng.unwrap_err();
        let plaintext = message_decrypt(
            &parsed,
            &remote_address,
            &local_address,
            &mut candidate.state.store.sessions,
            &mut candidate.state.store.identity,
            &mut candidate.state.store.pre_keys,
            &candidate.state.store.signed_pre_keys,
            &mut candidate.state.store.kyber_pre_keys,
            &mut rng,
        )
        .now_or_never()
        .expect("QMS stores are synchronous")?;
        let incoming_direction = contact.local_send_direction ^ 1;
        let (message_id, text) =
            decode_inner(&plaintext, &local, contact.session_id, incoming_direction)?;
        if !candidate.state.received_message_ids.insert(message_id) {
            return Err(Error::InvalidMessage("replayed message id"));
        }
        Ok(PreparedReceive {
            text,
            message_id,
            next_state: candidate.state()?,
        })
    }

    pub fn transport_context(
        &self,
        contact_id: &str,
        outgoing: bool,
    ) -> Result<TransportContext, Error> {
        let contact = self
            .state
            .contacts
            .get(contact_id)
            .ok_or(Error::State("unknown contact"))?;
        let package = ContactPackage::decode(if outgoing {
            &contact.remote_package
        } else {
            &contact.local_package
        })?;
        Ok(TransportContext {
            genesis: package.genesis,
            invitation_id: package.invitation_id,
            session_id: contact.session_id,
            root_secret: package.outer_root_secret,
            direction: if outgoing {
                contact.local_send_direction
            } else {
                contact.local_send_direction ^ 1
            },
        })
    }
}

fn unix_time(seconds: u64) -> SystemTime {
    SystemTime::UNIX_EPOCH + Duration::from_secs(seconds)
}

fn address_for_package(
    package: &ContactPackage,
    session_id: [u8; SESSION_ID_BYTES],
    owner_send_direction: u8,
) -> Result<ProtocolAddress, Error> {
    if owner_send_direction > 1 {
        return Err(Error::State("invalid address direction"));
    }
    let mut hasher = Sha256::new();
    hasher.update(b"QWC-QMS2-LIBSIGNAL-ADDRESS");
    hasher.update(package.genesis);
    hasher.update([WIRE_VERSION, PROFILE]);
    hasher.update(package.invitation_id);
    hasher.update(session_id);
    hasher.update([owner_send_direction]);
    hasher.update(&package.identity_key);
    let name = format!("qms2:{}", hex::encode(hasher.finalize()));
    let device =
        DeviceId::new(package.device_id).map_err(|_| Error::InvalidPackage("device id"))?;
    Ok(ProtocolAddress::new(name, device))
}

fn session_id(local: &ContactPackage, remote: &ContactPackage) -> [u8; SESSION_ID_BYTES] {
    let local_tuple = (local.fingerprint(), local.invitation_id);
    let remote_tuple = (remote.fingerprint(), remote.invitation_id);
    let (first, second) = if local_tuple < remote_tuple {
        (local_tuple, remote_tuple)
    } else {
        (remote_tuple, local_tuple)
    };
    let mut hasher = Sha256::new();
    hasher.update(SESSION_DOMAIN);
    hasher.update(local.genesis);
    hasher.update([WIRE_VERSION, PROFILE]);
    hasher.update(first.0);
    hasher.update(first.1);
    hasher.update(second.0);
    hasher.update(second.1);
    hasher.finalize()[..SESSION_ID_BYTES]
        .try_into()
        .expect("fixed slice")
}

fn encode_inner(
    _local: &ContactPackage,
    remote: &ContactPackage,
    session_id: [u8; SESSION_ID_BYTES],
    direction: u8,
    message_id: [u8; MESSAGE_ID_BYTES],
    text: &str,
) -> Result<Vec<u8>, Error> {
    if direction > 1 {
        return Err(Error::State("invalid direction"));
    }
    let mut out = Vec::with_capacity(INNER_FIXED_BYTES + text.len());
    out.extend_from_slice(INNER_DOMAIN);
    out.push(WIRE_VERSION);
    out.push(PROFILE);
    out.extend_from_slice(&remote.genesis);
    out.extend_from_slice(&remote.invitation_id);
    out.extend_from_slice(&session_id);
    out.push(direction);
    out.extend_from_slice(&message_id);
    out.push(1);
    out.extend_from_slice(&(text.len() as u32).to_le_bytes());
    out.extend_from_slice(text.as_bytes());
    debug_assert_eq!(out.len(), INNER_FIXED_BYTES + text.len());
    Ok(out)
}

fn decode_inner(
    input: &[u8],
    local: &ContactPackage,
    expected_session_id: [u8; SESSION_ID_BYTES],
    expected_direction: u8,
) -> Result<([u8; MESSAGE_ID_BYTES], String), Error> {
    if input.len() < INNER_FIXED_BYTES || &input[..INNER_DOMAIN.len()] != INNER_DOMAIN {
        return Err(Error::InvalidMessage("inner domain or length"));
    }
    let mut position = INNER_DOMAIN.len();
    if input[position] != WIRE_VERSION || input[position + 1] != PROFILE {
        return Err(Error::InvalidMessage("inner version/profile"));
    }
    position += 2;
    if input[position..position + GENESIS_BYTES] != local.genesis {
        return Err(Error::InvalidMessage("inner genesis"));
    }
    position += GENESIS_BYTES;
    if input[position..position + INVITATION_ID_BYTES] != local.invitation_id {
        return Err(Error::InvalidMessage("inner recipient invitation"));
    }
    position += INVITATION_ID_BYTES;
    if input[position..position + SESSION_ID_BYTES] != expected_session_id {
        return Err(Error::InvalidMessage("inner session"));
    }
    position += SESSION_ID_BYTES;
    if input[position] != expected_direction {
        return Err(Error::InvalidMessage("inner direction"));
    }
    position += 1;
    let message_id = input[position..position + MESSAGE_ID_BYTES]
        .try_into()
        .expect("fixed slice");
    position += MESSAGE_ID_BYTES;
    if input[position] != 1 {
        return Err(Error::InvalidMessage("inner content type"));
    }
    position += 1;
    let text_length = u32::from_le_bytes(
        input[position..position + 4]
            .try_into()
            .expect("fixed slice"),
    ) as usize;
    position += 4;
    if text_length > MAX_TEXT_BYTES || input.len() - position != text_length {
        return Err(Error::InvalidMessage("inner text length"));
    }
    let text = std::str::from_utf8(&input[position..])
        .map_err(|_| Error::InvalidMessage("inner UTF-8"))?
        .to_owned();
    Ok((message_id, text))
}

#[cfg(test)]
mod tests;
