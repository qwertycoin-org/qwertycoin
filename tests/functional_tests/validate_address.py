#!/usr/bin/env python3

# Copyright (c) 2019-2022, The Monero Project
# 
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without modification, are
# permitted provided that the following conditions are met:
# 
# 1. Redistributions of source code must retain the above copyright notice, this list of
#    conditions and the following disclaimer.
# 
# 2. Redistributions in binary form must reproduce the above copyright notice, this list
#    of conditions and the following disclaimer in the documentation and/or other
#    materials provided with the distribution.
# 
# 3. Neither the name of the copyright holder nor the names of its contributors may be
#    used to endorse or promote products derived from this software without specific
#    prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Test address validation RPC calls
"""

from __future__ import print_function
from framework.wallet import Wallet

class AddressValidationTest():
    def run_test(self):
      self.create()
      self.check_bad_addresses()
      self.check_good_addresses()
      self.check_openalias_addresses()

    def create(self):
        print('Creating wallet')
        seed = 'velvet lymph giddy number token physics poetry unquoted nibs useful sabotage limits benches lifestyle eden nitrogen anvil fewest avoid batch vials washing fences goat unquoted'
        address = 'QWC1GKRV4Wd3DCnubWiv1CBDBJWTCUcvTAZnE9ioE7pUMiqwXpce7GX5kfyaW4X8V523Rkyoa9NQ6LWj2DrEX7cE2PDLobNPxh'
        self.wallet = Wallet()
        # close the wallet if any, will throw if none is loaded
        try: self.wallet.close_wallet()
        except: pass
        res = self.wallet.restore_deterministic_wallet(seed = seed)
        assert res.address == address
        assert res.seed == seed

    def check_bad_addresses(self):
        print('Validating bad addresses')
        bad_addresses = ['', 'a', '42ey1afDFnn4886T7196doS9GPMzexD9gXpsZJDwVjeRVdFCSoHnv7KPbBeGpzJBzHRCAs9UxqeoyFQMYbqSWYTfJJQAWD9', ' ', '@', '42ey']
        for address in bad_addresses:
            res = self.wallet.validate_address(address, any_net_type = False)
            assert not res.valid
            res = self.wallet.validate_address(address, any_net_type = True)
            assert not res.valid

    def check_good_addresses(self):
        print('Validating good addresses')
        addresses = [
            [ 'mainnet',  '', 'QWC1GKRV4Wd3DCnubWiv1CBDBJWTCUcvTAZnE9ioE7pUMiqwXpce7GX5kfyaW4X8V523Rkyoa9NQ6LWj2DrEX7cE2PDLobNPxh' ],
            [ 'mainnet',  '', 'QWC1MH8Mit9fVkUZ8JUmeLXZoSGZ4EyTx6mrfkDUazq5361xT7VQsp7ZugqtNJamRQYBXF8jf3Sq1Z2ETh4jMpnnA4yBYckQv9' ],
            [ 'testnet',  '', 'TBQmZouTsMTZT15ndaYMYQTBcmKpLUSuHM4eLiC7NgKiQuRNhQne1qW3JoAkSNXTGG4Rzp2b5Ham35wVgCSvFKGy4cMXj6RiPL' ],
            [ 'stagenet', '', 'VrdXmDDWDmwb88LMC1i1dGdGzroNUYNsK3xor2E66gRFQmMkLk5YJLq2Q5BWjWYeH5XKJ8vLkqE86HdpgaHBBFZd5AacyShmEx' ],
            [ 'mainnet', 'i', 'QftgFwAVzfsRb5wFn2SN9H3ntHJjeoXjsGS81YC3sfCQBo2Ebw4GhiMKfYLuyFd1TrDG3PhmMxfTr81Jdw6fHSC69c1CqijeM9S9nxaamX4iv' ],
            [ 'mainnet', 's', 'QqbMfv3rXbaRMGADmVsVFLaric1VLDGmRFJZL6v7vLdnPYWkxgvnNhq8rMpziyJ9MuKgxjj1vq3p31V9Db9976Ug6CpB2iCQRH' ],
            [ 'mainnet', 's', 'QqbMTePG5Qy4S5od87BgeCQqs7pCPHx92U71ZQ3uhVVDAju2bAiy8PdNrmTds86B2fE4WnWaotPGZdMAUo3E7FVH3RGn9b2eKf' ],
            [ 'testnet', 'i', 'TM7SrdgfjU85hThbnx69zRib7krLqLEXrapcw3zEQFyJLGFdKhtY6qY7bRn89TBKA8ZbscYZuobQrheEEG9Q3m5D7oX5wCHBRtU6ERdJEfphG' ],
            [ 'testnet', 's', 'TWp7j63E5nM3GAe4w5eQYNMjf6JQdUSh9YMzay2z37BkWNeSBqw942dYLuahnq9TRYXFp1m7KAWa3TB2rxSbxKSq3ffmTz5tWk' ],
            [ 'testnet', 's', 'TWp7fvBhEbjA4biMkHkhJNiWKUaTFGEoYjej2Mpp2SFmgCfLXEGc19Jh35qxbCkEKXe9du7Qhbxru1KAmCq6pEoj9Nt2uFja78' ],
            [ 'stagenet', 'i', 'W2LD4X3hXERGgJHRXyD3zaHiEwWMu1a38L1495cWyiSWGxm7Rh6QUwu3ZYZimJ42AgHFyyoKAfpguiBZs3ZQNQ2GAwnrfskGSip5DUHEtdNWn' ],
            [ 'stagenet', 's', 'WC2sjx47EsuUECJ6fDmHCH35E5xsjD7yvQtt8u5xNUDHK9VZpsM7Kqk9uAuKimwbCSd49zQWWhdwASEVEFTW8rEQA6xbUVLgQU' ],
            [ 'stagenet', 's', 'WC2t5o1NhLU2fAsjexf9ePLZiUaMLBpb5JDHvDyXouchcZ9LUTGNFgpXwYo9iVqe2Q8PwDTq1YRRgCFgdsyQAst42PivEvdGVM' ],
        ]
        for any_net_type in [True, False]:
            for address in addresses:
                res = self.wallet.validate_address(address[2], any_net_type = any_net_type)
                if any_net_type or address[0] == 'mainnet':
                    assert res.valid
                    assert res.integrated == (address[1] == 'i')
                    assert res.subaddress == (address[1] == 's')
                    assert res.nettype == address[0]
                    assert res.openalias_address == ''
                else:
                    assert not res.valid

    def check_openalias_addresses(self):
        print('Validating openalias addresses')
        addresses = [
            ['donate@example.invalid']
        ]
        for address in addresses:
            res = self.wallet.validate_address(address[0])
            assert not res.valid
            res = self.wallet.validate_address(address[0], allow_openalias = True)
            assert not res.valid
            assert res.openalias_address == ''

if __name__ == '__main__':
    AddressValidationTest().run_test()
