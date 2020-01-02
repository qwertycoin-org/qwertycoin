// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2020, The Qwertycoin Group.
//
// This file is part of Qwertycoin.
//
// Qwertycoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Qwertycoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Qwertycoin.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <crypto/chacha8.h>
#include <crypto/crypto.h>
#include <CryptoNoteCore/CryptoNoteBasic.h>
#include <Serialization/ISerializer.h>

namespace CN = CryptoNote;

namespace Crypto {

bool serialize(PublicKey &pubKey, Common::StringView name, CN::ISerializer &serializer);
bool serialize(SecretKey &secKey, Common::StringView name, CN::ISerializer &serializer);
bool serialize(Hash &h, Common::StringView name, CN::ISerializer &serializer);
bool serialize(chacha8_iv &chacha, Common::StringView name, CN::ISerializer &serializer);
bool serialize(KeyImage& keyImage, Common::StringView name, CN::ISerializer &serializer);
bool serialize(Signature &sig, Common::StringView name, CN::ISerializer &serializer);
bool serialize(EllipticCurveScalar &ecScalar, Common::StringView name, CN::ISerializer &serializer);
bool serialize(EllipticCurvePoint &ecPoint, Common::StringView name, CN::ISerializer &serializer);

} // namespace Crypto

namespace CryptoNote {

struct AccountKeys;
struct TransactionExtraMergeMiningTag;

void serialize(TransactionPrefix &txP, ISerializer &serializer);
void serialize(Transaction &tx, ISerializer &serializer);
void serialize(TransactionInput &in, ISerializer &serializer);
void serialize(TransactionOutput &in, ISerializer &serializer);

void serialize(BaseInput &gen, ISerializer &serializer);
void serialize(KeyInput &key, ISerializer &serializer);
void serialize(MultisignatureInput &multisignature, ISerializer &serializer);

void serialize(TransactionOutput &output, ISerializer &serializer);
void serialize(TransactionOutputTarget &output, ISerializer &serializer);
void serialize(KeyOutput &key, ISerializer &serializer);
void serialize(MultisignatureOutput &multisignature, ISerializer &serializer);

void serialize(BlockHeader &header, ISerializer &serializer);
void serialize(Block &block, ISerializer &serializer);
void serialize(ParentBlockSerializer &pbs, ISerializer &serializer);
void serialize(TransactionExtraMergeMiningTag& tag, ISerializer& serializer);

void serialize(AccountPublicAddress &address, ISerializer &serializer);
void serialize(AccountKeys &keys, ISerializer &s);

void serialize(KeyPair &keyPair, ISerializer &serializer);

} // namespace CryptoNote
