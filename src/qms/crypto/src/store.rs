use std::collections::BTreeMap;

use async_trait::async_trait;
use libsignal_protocol::{
    CiphertextMessageType, Direction, GenericSignedPreKey, IdentityChange, IdentityKey,
    IdentityKeyPair, IdentityKeyStore, KyberPreKeyId, KyberPreKeyRecord, KyberPreKeyStore,
    PreKeyId, PreKeyRecord, PreKeyStore, ProtocolAddress, PublicKey, SessionRecord, SessionStore,
    SignalProtocolError, SignedPreKeyId, SignedPreKeyRecord, SignedPreKeyStore,
};
use serde::{Deserialize, Serialize};

#[derive(Clone, Serialize, Deserialize)]
pub(crate) struct StoreSnapshot {
    pub(crate) identity: IdentityStoreSnapshot,
    pub(crate) pre_keys: PreKeyStoreSnapshot,
    pub(crate) signed_pre_keys: SignedPreKeyStoreSnapshot,
    pub(crate) kyber_pre_keys: KyberPreKeyStoreSnapshot,
    pub(crate) sessions: SessionStoreSnapshot,
    pub(crate) next_pre_key_id: u32,
}

impl StoreSnapshot {
    pub(crate) fn new(identity: IdentityKeyPair, registration_id: u32) -> Self {
        Self {
            identity: IdentityStoreSnapshot {
                identity_key_pair: identity.serialize().into_vec(),
                registration_id,
                known_identities: BTreeMap::new(),
            },
            pre_keys: PreKeyStoreSnapshot::default(),
            signed_pre_keys: SignedPreKeyStoreSnapshot::default(),
            kyber_pre_keys: KyberPreKeyStoreSnapshot::default(),
            sessions: SessionStoreSnapshot::default(),
            next_pre_key_id: 1,
        }
    }

    pub(crate) fn identity(&self) -> Result<IdentityKeyPair, SignalProtocolError> {
        self.identity.identity()
    }

    pub(crate) fn registration_id(&self) -> u32 {
        self.identity.registration_id
    }
}

fn address_key(address: &ProtocolAddress) -> String {
    format!("{}#{}", address.name(), u8::from(address.device_id()))
}

#[derive(Clone, Serialize, Deserialize)]
pub(crate) struct IdentityStoreSnapshot {
    identity_key_pair: Vec<u8>,
    registration_id: u32,
    known_identities: BTreeMap<String, Vec<u8>>,
}

impl IdentityStoreSnapshot {
    fn identity(&self) -> Result<IdentityKeyPair, SignalProtocolError> {
        IdentityKeyPair::try_from(self.identity_key_pair.as_slice())
    }
}

#[async_trait(?Send)]
impl IdentityKeyStore for IdentityStoreSnapshot {
    async fn get_identity_key_pair(&self) -> Result<IdentityKeyPair, SignalProtocolError> {
        self.identity()
    }

    async fn get_local_registration_id(&self) -> Result<u32, SignalProtocolError> {
        Ok(self.registration_id)
    }

    async fn save_identity(
        &mut self,
        address: &ProtocolAddress,
        identity: &IdentityKey,
    ) -> Result<IdentityChange, SignalProtocolError> {
        let key = address_key(address);
        let encoded = identity.serialize().into_vec();
        match self.known_identities.get(&key) {
            None => {
                self.known_identities.insert(key, encoded);
                Ok(IdentityChange::NewOrUnchanged)
            }
            Some(existing) if existing == &encoded => Ok(IdentityChange::NewOrUnchanged),
            Some(_) => {
                self.known_identities.insert(key, encoded);
                Ok(IdentityChange::ReplacedExisting)
            }
        }
    }

    async fn is_trusted_identity(
        &self,
        address: &ProtocolAddress,
        identity: &IdentityKey,
        _direction: Direction,
    ) -> Result<bool, SignalProtocolError> {
        let encoded = identity.serialize();
        Ok(self
            .known_identities
            .get(&address_key(address))
            .is_none_or(|stored| stored.as_slice() == encoded.as_ref()))
    }

    async fn get_identity(
        &self,
        address: &ProtocolAddress,
    ) -> Result<Option<IdentityKey>, SignalProtocolError> {
        self.known_identities
            .get(&address_key(address))
            .map(|encoded| IdentityKey::try_from(encoded.as_slice()))
            .transpose()
    }
}

#[derive(Clone, Default, Serialize, Deserialize)]
pub(crate) struct PreKeyStoreSnapshot {
    values: BTreeMap<u32, Vec<u8>>,
}

#[async_trait(?Send)]
impl PreKeyStore for PreKeyStoreSnapshot {
    async fn get_pre_key(&self, id: PreKeyId) -> Result<PreKeyRecord, SignalProtocolError> {
        let id: u32 = id.into();
        PreKeyRecord::deserialize(
            self.values
                .get(&id)
                .ok_or(SignalProtocolError::InvalidPreKeyId)?,
        )
    }

    async fn save_pre_key(
        &mut self,
        id: PreKeyId,
        record: &PreKeyRecord,
    ) -> Result<(), SignalProtocolError> {
        self.values.insert(id.into(), record.serialize()?);
        Ok(())
    }

    async fn remove_pre_key(&mut self, id: PreKeyId) -> Result<(), SignalProtocolError> {
        self.values.remove(&id.into());
        Ok(())
    }
}

#[derive(Clone, Default, Serialize, Deserialize)]
pub(crate) struct SignedPreKeyStoreSnapshot {
    values: BTreeMap<u32, Vec<u8>>,
}

#[async_trait(?Send)]
impl SignedPreKeyStore for SignedPreKeyStoreSnapshot {
    async fn get_signed_pre_key(
        &self,
        id: SignedPreKeyId,
    ) -> Result<SignedPreKeyRecord, SignalProtocolError> {
        let id: u32 = id.into();
        SignedPreKeyRecord::deserialize(
            self.values
                .get(&id)
                .ok_or(SignalProtocolError::InvalidSignedPreKeyId)?,
        )
    }

    async fn save_signed_pre_key(
        &mut self,
        id: SignedPreKeyId,
        record: &SignedPreKeyRecord,
    ) -> Result<(), SignalProtocolError> {
        self.values.insert(id.into(), record.serialize()?);
        Ok(())
    }
}

#[derive(Clone, Default, Serialize, Deserialize)]
pub(crate) struct KyberPreKeyStoreSnapshot {
    values: BTreeMap<u32, Vec<u8>>,
    base_keys_seen: BTreeMap<(u32, u32), Vec<Vec<u8>>>,
}

#[async_trait(?Send)]
impl KyberPreKeyStore for KyberPreKeyStoreSnapshot {
    async fn get_kyber_pre_key(
        &self,
        id: KyberPreKeyId,
    ) -> Result<KyberPreKeyRecord, SignalProtocolError> {
        let id: u32 = id.into();
        KyberPreKeyRecord::deserialize(
            self.values
                .get(&id)
                .ok_or(SignalProtocolError::InvalidKyberPreKeyId)?,
        )
    }

    async fn save_kyber_pre_key(
        &mut self,
        id: KyberPreKeyId,
        record: &KyberPreKeyRecord,
    ) -> Result<(), SignalProtocolError> {
        self.values.insert(id.into(), record.serialize()?);
        Ok(())
    }

    async fn mark_kyber_pre_key_used(
        &mut self,
        kyber_id: KyberPreKeyId,
        ec_id: SignedPreKeyId,
        base_key: &PublicKey,
    ) -> Result<(), SignalProtocolError> {
        let key = (kyber_id.into(), ec_id.into());
        let encoded = base_key.serialize().into_vec();
        let seen = self.base_keys_seen.entry(key).or_default();
        if seen.contains(&encoded) {
            return Err(SignalProtocolError::InvalidMessage(
                CiphertextMessageType::PreKey,
                "reused base key".to_owned(),
            ));
        }
        seen.push(encoded);
        Ok(())
    }
}

#[derive(Clone, Default, Serialize, Deserialize)]
pub(crate) struct SessionStoreSnapshot {
    values: BTreeMap<String, Vec<u8>>,
}

#[async_trait(?Send)]
impl SessionStore for SessionStoreSnapshot {
    async fn load_session(
        &self,
        address: &ProtocolAddress,
    ) -> Result<Option<SessionRecord>, SignalProtocolError> {
        self.values
            .get(&address_key(address))
            .map(|record| SessionRecord::deserialize(record))
            .transpose()
    }

    async fn store_session(
        &mut self,
        address: &ProtocolAddress,
        record: &SessionRecord,
    ) -> Result<(), SignalProtocolError> {
        self.values
            .insert(address_key(address), record.serialize()?);
        Ok(())
    }
}
