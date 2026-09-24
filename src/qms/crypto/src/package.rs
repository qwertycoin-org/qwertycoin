use libsignal_protocol::{
    DeviceId, GenericSignedPreKey, IdentityKey, KeyPair, KyberPreKeyId, KyberPreKeyRecord,
    KyberPreKeyStore, PreKeyBundle, PreKeyId, PreKeyRecord, PreKeyStore, PublicKey,
    SignalProtocolError, SignedPreKeyId, SignedPreKeyRecord, SignedPreKeyStore, Timestamp, kem,
};
use rand::{CryptoRng, Rng};
use sha2::{Digest, Sha256};

use crate::store::StoreSnapshot;
use crate::{Error, GENESIS_BYTES, INVITATION_ID_BYTES, OUTER_SECRET_BYTES, PROFILE, WIRE_VERSION};

const MAGIC: &[u8; 4] = b"QCP2";
const SIGNING_DOMAIN: &[u8] = b"QWC-QMS2-CONTACT-PACKAGE";

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ContactPackage {
    pub genesis: [u8; GENESIS_BYTES],
    pub invitation_id: [u8; INVITATION_ID_BYTES],
    pub registration_id: u32,
    pub device_id: u8,
    pub identity_key: Vec<u8>,
    pub pre_key_id: u32,
    pub pre_key_public: Vec<u8>,
    pub signed_pre_key_id: u32,
    pub signed_pre_key_public: Vec<u8>,
    pub signed_pre_key_signature: Vec<u8>,
    pub kyber_pre_key_id: u32,
    pub kyber_pre_key_public: Vec<u8>,
    pub kyber_pre_key_signature: Vec<u8>,
    pub outer_root_secret: [u8; OUTER_SECRET_BYTES],
    pub signature: Vec<u8>,
}

impl ContactPackage {
    pub fn encode(&self) -> Result<Vec<u8>, Error> {
        let mut out = self.canonical_unsigned()?;
        put_bytes_u16(&mut out, &self.signature)?;
        Ok(out)
    }

    pub fn decode(input: &[u8]) -> Result<Self, Error> {
        let mut reader = Reader::new(input);
        if reader.take(4)? != MAGIC {
            return Err(Error::InvalidPackage("contact package magic"));
        }
        if reader.u8()? != WIRE_VERSION || reader.u8()? != PROFILE {
            return Err(Error::InvalidPackage("contact package version/profile"));
        }
        let genesis = reader.array()?;
        let invitation_id = reader.array()?;
        let registration_id = reader.u32()?;
        let device_id = reader.u8()?;
        DeviceId::new(device_id).map_err(|_| Error::InvalidPackage("device id"))?;
        let identity_key = reader.bytes_u16()?;
        let pre_key_id = reader.u32()?;
        let pre_key_public = reader.bytes_u16()?;
        let signed_pre_key_id = reader.u32()?;
        let signed_pre_key_public = reader.bytes_u16()?;
        let signed_pre_key_signature = reader.bytes_u16()?;
        let kyber_pre_key_id = reader.u32()?;
        let kyber_pre_key_public = reader.bytes_u16()?;
        let kyber_pre_key_signature = reader.bytes_u16()?;
        let outer_root_secret = reader.array()?;
        let signature = reader.bytes_u16()?;
        if !reader.done() {
            return Err(Error::InvalidPackage("trailing contact package bytes"));
        }
        let result = Self {
            genesis,
            invitation_id,
            registration_id,
            device_id,
            identity_key,
            pre_key_id,
            pre_key_public,
            signed_pre_key_id,
            signed_pre_key_public,
            signed_pre_key_signature,
            kyber_pre_key_id,
            kyber_pre_key_public,
            kyber_pre_key_signature,
            outer_root_secret,
            signature,
        };
        result.verify()?;
        Ok(result)
    }

    pub fn fingerprint(&self) -> [u8; 32] {
        Sha256::digest(&self.identity_key).into()
    }

    pub(crate) fn verify(&self) -> Result<(), Error> {
        let identity = IdentityKey::try_from(self.identity_key.as_slice())?;
        let signed = PublicKey::deserialize(&self.signed_pre_key_public)
            .map_err(SignalProtocolError::from)?;
        let kyber = kem::PublicKey::deserialize(&self.kyber_pre_key_public)?;
        if !identity
            .public_key()
            .verify_signature(&signed.serialize(), &self.signed_pre_key_signature)
        {
            return Err(Error::InvalidPackage("signed EC prekey signature"));
        }
        if !identity
            .public_key()
            .verify_signature(&kyber.serialize(), &self.kyber_pre_key_signature)
        {
            return Err(Error::InvalidPackage("ML-KEM prekey signature"));
        }
        if !identity
            .public_key()
            .verify_signature(&self.signing_input()?, &self.signature)
        {
            return Err(Error::InvalidPackage("contact package signature"));
        }
        Ok(())
    }

    pub(crate) fn pre_key_bundle(&self) -> Result<PreKeyBundle, Error> {
        self.verify()?;
        Ok(PreKeyBundle::new(
            self.registration_id,
            DeviceId::new(self.device_id).map_err(|_| Error::InvalidPackage("device id"))?,
            Some((
                PreKeyId::from(self.pre_key_id),
                PublicKey::deserialize(&self.pre_key_public).map_err(SignalProtocolError::from)?,
            )),
            SignedPreKeyId::from(self.signed_pre_key_id),
            PublicKey::deserialize(&self.signed_pre_key_public)
                .map_err(SignalProtocolError::from)?,
            self.signed_pre_key_signature.clone(),
            KyberPreKeyId::from(self.kyber_pre_key_id),
            kem::PublicKey::deserialize(&self.kyber_pre_key_public)?,
            self.kyber_pre_key_signature.clone(),
            IdentityKey::try_from(self.identity_key.as_slice())?,
        )?)
    }

    fn canonical_unsigned(&self) -> Result<Vec<u8>, Error> {
        let mut out = Vec::new();
        out.extend_from_slice(MAGIC);
        out.push(WIRE_VERSION);
        out.push(PROFILE);
        out.extend_from_slice(&self.genesis);
        out.extend_from_slice(&self.invitation_id);
        out.extend_from_slice(&self.registration_id.to_le_bytes());
        out.push(self.device_id);
        put_bytes_u16(&mut out, &self.identity_key)?;
        out.extend_from_slice(&self.pre_key_id.to_le_bytes());
        put_bytes_u16(&mut out, &self.pre_key_public)?;
        out.extend_from_slice(&self.signed_pre_key_id.to_le_bytes());
        put_bytes_u16(&mut out, &self.signed_pre_key_public)?;
        put_bytes_u16(&mut out, &self.signed_pre_key_signature)?;
        out.extend_from_slice(&self.kyber_pre_key_id.to_le_bytes());
        put_bytes_u16(&mut out, &self.kyber_pre_key_public)?;
        put_bytes_u16(&mut out, &self.kyber_pre_key_signature)?;
        out.extend_from_slice(&self.outer_root_secret);
        Ok(out)
    }

    fn signing_input(&self) -> Result<Vec<u8>, Error> {
        let canonical = self.canonical_unsigned()?;
        let mut domain_bound = Vec::with_capacity(SIGNING_DOMAIN.len() + canonical.len());
        domain_bound.extend_from_slice(SIGNING_DOMAIN);
        domain_bound.extend_from_slice(&canonical);
        Ok(domain_bound)
    }
}

pub(crate) async fn generate_contact_package<R: Rng + CryptoRng>(
    store: &mut StoreSnapshot,
    genesis: [u8; GENESIS_BYTES],
    rng: &mut R,
) -> Result<ContactPackage, Error> {
    let identity = store.identity()?;
    let registration_id = store.registration_id();
    let pre_key_id = store.next_pre_key_id;
    let signed_pre_key_id = pre_key_id
        .checked_add(1)
        .ok_or(Error::State("prekey id exhausted"))?;
    let kyber_pre_key_id = pre_key_id
        .checked_add(2)
        .ok_or(Error::State("prekey id exhausted"))?;
    store.next_pre_key_id = pre_key_id
        .checked_add(3)
        .ok_or(Error::State("prekey id exhausted"))?;

    let pre_key = KeyPair::generate(rng);
    let signed_pre_key = KeyPair::generate(rng);
    let signed_public = signed_pre_key.public_key.serialize();
    let signed_signature = identity
        .private_key()
        .calculate_signature(&signed_public, rng)
        .map_err(SignalProtocolError::from)?;
    let kyber_pre_key = kem::KeyPair::generate(kem::KeyType::Kyber1024, rng);
    let kyber_public = kyber_pre_key.public_key.serialize();
    let kyber_signature = identity
        .private_key()
        .calculate_signature(&kyber_public, rng)
        .map_err(SignalProtocolError::from)?;

    store
        .pre_keys
        .save_pre_key(
            PreKeyId::from(pre_key_id),
            &PreKeyRecord::new(PreKeyId::from(pre_key_id), &pre_key),
        )
        .await?;
    store
        .signed_pre_keys
        .save_signed_pre_key(
            SignedPreKeyId::from(signed_pre_key_id),
            &SignedPreKeyRecord::new(
                SignedPreKeyId::from(signed_pre_key_id),
                Timestamp::from_epoch_millis(0),
                &signed_pre_key,
                &signed_signature,
            ),
        )
        .await?;
    store
        .kyber_pre_keys
        .save_kyber_pre_key(
            KyberPreKeyId::from(kyber_pre_key_id),
            &KyberPreKeyRecord::new(
                KyberPreKeyId::from(kyber_pre_key_id),
                Timestamp::from_epoch_millis(0),
                &kyber_pre_key,
                &kyber_signature,
            ),
        )
        .await?;

    let mut invitation_id = [0u8; INVITATION_ID_BYTES];
    rng.fill(&mut invitation_id);
    let mut outer_root_secret = [0u8; OUTER_SECRET_BYTES];
    rng.fill(&mut outer_root_secret);
    let mut package = ContactPackage {
        genesis,
        invitation_id,
        registration_id,
        device_id: 1,
        identity_key: identity.identity_key().serialize().into_vec(),
        pre_key_id,
        pre_key_public: pre_key.public_key.serialize().into_vec(),
        signed_pre_key_id,
        signed_pre_key_public: signed_public.into_vec(),
        signed_pre_key_signature: signed_signature.into_vec(),
        kyber_pre_key_id,
        kyber_pre_key_public: kyber_public.into_vec(),
        kyber_pre_key_signature: kyber_signature.into_vec(),
        outer_root_secret,
        signature: Vec::new(),
    };
    package.signature = identity
        .private_key()
        .calculate_signature(&package.signing_input()?, rng)
        .map_err(SignalProtocolError::from)?
        .into_vec();
    package.verify()?;
    Ok(package)
}

fn put_bytes_u16(out: &mut Vec<u8>, value: &[u8]) -> Result<(), Error> {
    let len = u16::try_from(value.len()).map_err(|_| Error::InvalidPackage("field too long"))?;
    out.extend_from_slice(&len.to_le_bytes());
    out.extend_from_slice(value);
    Ok(())
}

struct Reader<'a> {
    input: &'a [u8],
    position: usize,
}

impl<'a> Reader<'a> {
    fn new(input: &'a [u8]) -> Self {
        Self { input, position: 0 }
    }

    fn take(&mut self, count: usize) -> Result<&'a [u8], Error> {
        let end = self
            .position
            .checked_add(count)
            .filter(|end| *end <= self.input.len())
            .ok_or(Error::InvalidPackage("truncated contact package"))?;
        let result = &self.input[self.position..end];
        self.position = end;
        Ok(result)
    }

    fn u8(&mut self) -> Result<u8, Error> {
        Ok(self.take(1)?[0])
    }

    fn u16(&mut self) -> Result<u16, Error> {
        Ok(u16::from_le_bytes(self.take(2)?.try_into().expect("exact")))
    }

    fn u32(&mut self) -> Result<u32, Error> {
        Ok(u32::from_le_bytes(self.take(4)?.try_into().expect("exact")))
    }

    fn array<const N: usize>(&mut self) -> Result<[u8; N], Error> {
        Ok(self.take(N)?.try_into().expect("exact"))
    }

    fn bytes_u16(&mut self) -> Result<Vec<u8>, Error> {
        let length = usize::from(self.u16()?);
        Ok(self.take(length)?.to_vec())
    }

    fn done(&self) -> bool {
        self.position == self.input.len()
    }
}
