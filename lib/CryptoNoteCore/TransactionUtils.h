// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2023, The Qwertycoin Group.
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

#include <CryptoNoteCore/ITransaction.h>
#include <CryptoNoteCore/CryptoNoteBasic.h>

namespace CryptoNote {

bool checkInputsKeyimagesDiff(const CryptoNote::TransactionPrefix &tx);

// TransactionInput helper functions
size_t getRequiredSignaturesCount(const TransactionInput &in);
uint64_t getTransactionInputAmount(const TransactionInput &in);
TransactionTypes::InputType getTransactionInputType(const TransactionInput &in);
const TransactionInput &getInputChecked(const TransactionPrefix &transaction, size_t index);
const TransactionInput &getInputChecked(
    const TransactionPrefix &transaction,
    size_t index,
    TransactionTypes::InputType type);

bool isOutToKey(
    const Crypto::PublicKey &spendPublicKey,
    const Crypto::PublicKey &outKey,
    const Crypto::KeyDerivation &derivation,
    size_t keyIndex);

// TransactionOutput helper functions
TransactionTypes::OutputType getTransactionOutputType(const TransactionOutputTarget &out);
const TransactionOutput &getOutputChecked(const TransactionPrefix &transaction, size_t index);
const TransactionOutput &getOutputChecked(
    const TransactionPrefix &transaction,
    size_t index,
    TransactionTypes::OutputType type);

bool findOutputsToAccount(
    const CryptoNote::TransactionPrefix &transaction,
    const AccountPublicAddress &addr,
    const Crypto::SecretKey &viewSecretKey,
    std::vector<uint32_t> &out,
    uint64_t &amount);

} // namespace CryptoNote
