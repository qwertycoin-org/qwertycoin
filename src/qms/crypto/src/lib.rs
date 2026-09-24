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
pub const INNER_BASE_BYTES: usize = 105;
pub const OUTER_ROTATION_INTERVAL: u64 = 16;

const INNER_DOMAIN: &[u8] = b"QWC-QMS-INNER-V2";
const SESSION_DOMAIN: &[u8] = b"QWC-QMS2-SESSION-ID";
const INNER_HAS_OUTER_OFFER: u8 = 1;
const INNER_HAS_OUTER_ACK: u8 = 2;

#[derive(Clone, Debug, Eq, PartialEq, Serialize, Deserialize)]
struct VersionedOuterSecret {
    epoch: u64,
    secret: [u8; OUTER_SECRET_BYTES],
}

#[derive(Clone, Serialize, Deserialize)]
struct LocalOuterState {
    active: VersionedOuterSecret,
    offered: Option<VersionedOuterSecret>,
    retiring: Option<VersionedOuterSecret>,
    send_count: u64,
}

#[derive(Clone, Serialize, Deserialize)]
struct RemoteOuterState {
    active: VersionedOuterSecret,
    received: Option<VersionedOuterSecret>,
}

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
    local_outer: LocalOuterState,
    remote_outer: RemoteOuterState,
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

#[derive(Debug)]
struct InnerMessage {
    message_id: [u8; MESSAGE_ID_BYTES],
    text: String,
    outer_offer: Option<VersionedOuterSecret>,
    outer_ack: Option<u64>,
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
                local_outer: LocalOuterState {
                    active: VersionedOuterSecret {
                        epoch: 0,
                        secret: local.outer_root_secret,
                    },
                    offered: None,
                    retiring: None,
                    send_count: 0,
                },
                remote_outer: RemoteOuterState {
                    active: VersionedOuterSecret {
                        epoch: 0,
                        secret: remote.outer_root_secret,
                    },
                    received: None,
                },
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
        {
            let contact = candidate
                .state
                .contacts
                .get_mut(contact_id)
                .ok_or(Error::State("unknown contact"))?;
            if contact.local_outer.offered.is_none()
                && contact.local_outer.send_count != 0
                && contact.local_outer.send_count % OUTER_ROTATION_INTERVAL == 0
            {
                let epoch = contact
                    .local_outer
                    .active
                    .epoch
                    .checked_add(1)
                    .ok_or(Error::State("outer secret epoch exhausted"))?;
                let mut secret = [0u8; OUTER_SECRET_BYTES];
                let mut rng = OsRng.unwrap_err();
                rng.try_fill_bytes(&mut secret)
                    .map_err(|_| Error::State("operating-system randomness unavailable"))?;
                contact.local_outer.offered = Some(VersionedOuterSecret { epoch, secret });
            }
        }
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
            contact.local_outer.offered.as_ref(),
            contact
                .remote_outer
                .received
                .as_ref()
                .map(|secret| secret.epoch),
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
        {
            let next = candidate
                .state
                .contacts
                .get_mut(contact_id)
                .ok_or(Error::State("unknown contact"))?;
            next.local_outer.send_count = next
                .local_outer
                .send_count
                .checked_add(1)
                .ok_or(Error::State("outer rotation send counter exhausted"))?;
            if let Some(received) = next.remote_outer.received.take() {
                next.remote_outer.active = received;
            }
        }
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
        let inner = decode_inner(&plaintext, &local, contact.session_id, incoming_direction)?;
        if !candidate
            .state
            .received_message_ids
            .insert(inner.message_id)
        {
            return Err(Error::InvalidMessage("replayed message id"));
        }
        apply_outer_control(
            candidate
                .state
                .contacts
                .get_mut(contact_id)
                .ok_or(Error::State("unknown contact"))?,
            inner.outer_offer,
            inner.outer_ack,
        )?;
        Ok(PreparedReceive {
            text: inner.text,
            message_id: inner.message_id,
            next_state: candidate.state()?,
        })
    }

    pub fn transport_contexts(
        &self,
        contact_id: &str,
        outgoing: bool,
    ) -> Result<Vec<TransportContext>, Error> {
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
        let direction = if outgoing {
            contact.local_send_direction
        } else {
            contact.local_send_direction ^ 1
        };
        let secrets: Vec<&VersionedOuterSecret> = if outgoing {
            vec![
                contact
                    .remote_outer
                    .received
                    .as_ref()
                    .unwrap_or(&contact.remote_outer.active),
            ]
        } else {
            let mut values = vec![&contact.local_outer.active];
            if let Some(offered) = contact.local_outer.offered.as_ref() {
                values.push(offered);
            }
            if let Some(retiring) = contact.local_outer.retiring.as_ref() {
                if !values.iter().any(|value| value.secret == retiring.secret) {
                    values.push(retiring);
                }
            }
            values
        };
        Ok(secrets
            .into_iter()
            .map(|secret| TransportContext {
                genesis: package.genesis,
                invitation_id: package.invitation_id,
                session_id: contact.session_id,
                root_secret: secret.secret,
                direction,
            })
            .collect())
    }

    pub fn transport_context(
        &self,
        contact_id: &str,
        outgoing: bool,
    ) -> Result<TransportContext, Error> {
        self.transport_contexts(contact_id, outgoing)?
            .into_iter()
            .next()
            .ok_or(Error::State("missing transport context"))
    }
}

fn apply_outer_control(
    contact: &mut ContactContext,
    offer: Option<VersionedOuterSecret>,
    ack: Option<u64>,
) -> Result<(), Error> {
    if let Some(offer) = offer {
        if offer.epoch <= contact.remote_outer.active.epoch {
            if offer.epoch == contact.remote_outer.active.epoch
                && offer.secret != contact.remote_outer.active.secret
            {
                return Err(Error::InvalidMessage("conflicting active outer secret"));
            }
        } else if let Some(received) = contact.remote_outer.received.as_ref() {
            if *received != offer {
                return Err(Error::InvalidMessage("conflicting offered outer secret"));
            }
        } else {
            let expected = contact
                .remote_outer
                .active
                .epoch
                .checked_add(1)
                .ok_or(Error::State("outer secret epoch exhausted"))?;
            if offer.epoch != expected {
                return Err(Error::InvalidMessage("non-sequential outer secret offer"));
            }
            contact.remote_outer.received = Some(offer);
        }
    }
    if let Some(ack) = ack {
        if ack <= contact.local_outer.active.epoch {
            return Ok(());
        }
        let offered = contact
            .local_outer
            .offered
            .take()
            .ok_or(Error::InvalidMessage(
                "outer secret acknowledgement without offer",
            ))?;
        if offered.epoch != ack {
            contact.local_outer.offered = Some(offered);
            return Err(Error::InvalidMessage("outer secret acknowledgement epoch"));
        }
        contact.local_outer.retiring = Some(contact.local_outer.active.clone());
        contact.local_outer.active = offered;
    }
    Ok(())
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
    outer_offer: Option<&VersionedOuterSecret>,
    outer_ack: Option<u64>,
) -> Result<Vec<u8>, Error> {
    if direction > 1 {
        return Err(Error::State("invalid direction"));
    }
    let control_bytes =
        outer_offer.map_or(0, |_| 8 + OUTER_SECRET_BYTES) + outer_ack.map_or(0, |_| 8);
    let mut out = Vec::with_capacity(INNER_BASE_BYTES + control_bytes + text.len());
    out.extend_from_slice(INNER_DOMAIN);
    out.push(WIRE_VERSION);
    out.push(PROFILE);
    out.extend_from_slice(&remote.genesis);
    out.extend_from_slice(&remote.invitation_id);
    out.extend_from_slice(&session_id);
    out.push(direction);
    out.extend_from_slice(&message_id);
    let mut flags = 0u8;
    if outer_offer.is_some() {
        flags |= INNER_HAS_OUTER_OFFER;
    }
    if outer_ack.is_some() {
        flags |= INNER_HAS_OUTER_ACK;
    }
    out.push(flags);
    if let Some(offer) = outer_offer {
        out.extend_from_slice(&offer.epoch.to_le_bytes());
        out.extend_from_slice(&offer.secret);
    }
    if let Some(ack) = outer_ack {
        out.extend_from_slice(&ack.to_le_bytes());
    }
    out.push(1);
    out.extend_from_slice(&(text.len() as u32).to_le_bytes());
    out.extend_from_slice(text.as_bytes());
    debug_assert_eq!(out.len(), INNER_BASE_BYTES + control_bytes + text.len());
    Ok(out)
}

fn decode_inner(
    input: &[u8],
    local: &ContactPackage,
    expected_session_id: [u8; SESSION_ID_BYTES],
    expected_direction: u8,
) -> Result<InnerMessage, Error> {
    if input.len() < INNER_BASE_BYTES || &input[..INNER_DOMAIN.len()] != INNER_DOMAIN {
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
    let flags = input[position];
    position += 1;
    if flags & !(INNER_HAS_OUTER_OFFER | INNER_HAS_OUTER_ACK) != 0 {
        return Err(Error::InvalidMessage("inner control flags"));
    }
    let outer_offer = if flags & INNER_HAS_OUTER_OFFER != 0 {
        if input.len() - position < 8 + OUTER_SECRET_BYTES {
            return Err(Error::InvalidMessage("truncated outer secret offer"));
        }
        let epoch = u64::from_le_bytes(
            input[position..position + 8]
                .try_into()
                .expect("fixed slice"),
        );
        position += 8;
        let secret = input[position..position + OUTER_SECRET_BYTES]
            .try_into()
            .expect("fixed slice");
        position += OUTER_SECRET_BYTES;
        Some(VersionedOuterSecret { epoch, secret })
    } else {
        None
    };
    let outer_ack = if flags & INNER_HAS_OUTER_ACK != 0 {
        if input.len() - position < 8 {
            return Err(Error::InvalidMessage(
                "truncated outer secret acknowledgement",
            ));
        }
        let epoch = u64::from_le_bytes(
            input[position..position + 8]
                .try_into()
                .expect("fixed slice"),
        );
        position += 8;
        Some(epoch)
    } else {
        None
    };
    if input.len() - position < 5 {
        return Err(Error::InvalidMessage("truncated inner payload"));
    }
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
    Ok(InnerMessage {
        message_id,
        text,
        outer_offer,
        outer_ack,
    })
}

#[cfg(test)]
mod tests;
