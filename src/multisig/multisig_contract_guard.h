#pragma once

namespace cryptonote {
class transaction;
}

namespace multisig::signing {

bool accepts_contract_extra(const cryptonote::transaction& tx);

}
