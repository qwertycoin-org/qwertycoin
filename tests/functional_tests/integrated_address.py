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

"""Test integrated address RPC calls

Test the following RPCs:
    - make_integrated_address
    - split_integrated_address

"""

from __future__ import print_function
from framework.wallet import Wallet

class IntegratedAddressTest():
    def run_test(self):
      self.create()
      self.check()

    def create(self):
        print('Creating wallet')
        wallet = Wallet()
        # close the wallet if any, will throw if none is loaded
        try: wallet.close_wallet()
        except: pass
        seed = 'velvet lymph giddy number token physics poetry unquoted nibs useful sabotage limits benches lifestyle eden nitrogen anvil fewest avoid batch vials washing fences goat unquoted'
        res = wallet.restore_deterministic_wallet(seed = seed)
        assert res.address == 'QWC1GKRV4Wd3DCnubWiv1CBDBJWTCUcvTAZnE9ioE7pUMiqwXpce7GX5kfyaW4X8V523Rkyoa9NQ6LWj2DrEX7cE2PDLobNPxh'
        assert res.seed == seed

    def check(self):
        wallet = Wallet()

        print('Checking local address')
        res = wallet.make_integrated_address(payment_id = '0123456789abcdef')
        assert res.integrated_address == 'QftgH8Eyfn93DCnubWiv1CBDBJWTCUcvTAZnE9ioE7pUMiqwXpce7GX5kfyaW4X8V523Rkyoa9NQ6LWj2DrEX7cE773UEqxXKr87WcikEqozv'
        assert res.payment_id == '0123456789abcdef'
        res = wallet.split_integrated_address(res.integrated_address)
        assert res.standard_address == 'QWC1GKRV4Wd3DCnubWiv1CBDBJWTCUcvTAZnE9ioE7pUMiqwXpce7GX5kfyaW4X8V523Rkyoa9NQ6LWj2DrEX7cE2PDLobNPxh'
        assert res.payment_id == '0123456789abcdef'

        print('Checking different address')
        res = wallet.make_integrated_address(standard_address = 'QWC1UoAxCgu7VYGeuu4Npp86qCjkBYPf1g6dQj1ovJwBZxDRnPxKSyQjhVTL22p5bACTEMjEepu8eaUrtk6P5VEh6cwoFfKXhD', payment_id = '1122334455667788')
        assert res.integrated_address == 'QftgVbzSoxR7VYGeuu4Npp86qCjkBYPf1g6dQj1ovJwBZxDRnPxKSyQjhVTL22p5bACTEMjEepu8eaUrtk6P5VEhRoernfpsFcp4tDY8hEnJ8'
        assert res.payment_id == '1122334455667788'
        res = wallet.split_integrated_address(res.integrated_address)
        assert res.standard_address == 'QWC1UoAxCgu7VYGeuu4Npp86qCjkBYPf1g6dQj1ovJwBZxDRnPxKSyQjhVTL22p5bACTEMjEepu8eaUrtk6P5VEh6cwoFfKXhD'
        assert res.payment_id == '1122334455667788'

        print('Checking bad payment id')
        fails = 0
        try: wallet.make_integrated_address(standard_address = 'QWC1UoAxCgu7VYGeuu4Npp86qCjkBYPf1g6dQj1ovJwBZxDRnPxKSyQjhVTL22p5bACTEMjEepu8eaUrtk6P5VEh6cwoFfKXhD', payment_id = '11223344556677880')
        except: fails += 1
        try: wallet.make_integrated_address(standard_address = 'QWC1UoAxCgu7VYGeuu4Npp86qCjkBYPf1g6dQj1ovJwBZxDRnPxKSyQjhVTL22p5bACTEMjEepu8eaUrtk6P5VEh6cwoFfKXhD', payment_id = '112233445566778')
        except: fails += 1
        try: wallet.make_integrated_address(standard_address = 'QWC1UoAxCgu7VYGeuu4Npp86qCjkBYPf1g6dQj1ovJwBZxDRnPxKSyQjhVTL22p5bACTEMjEepu8eaUrtk6P5VEh6cwoFfKXhD', payment_id = '112233445566778g')
        except: fails += 1
        try: wallet.make_integrated_address(standard_address = 'QWC1UoAxCgu7VYGeuu4Npp86qCjkBYPf1g6dQj1ovJwBZxDRnPxKSyQjhVTL22p5bACTEMjEepu8eaUrtk6P5VEh6cwoFfKXhD', payment_id = '1122334455667788112233445566778811223344556677881122334455667788')
        except: fails += 1
        assert fails == 4

        print('Checking bad standard address')
        fails = 0
        try: wallet.make_integrated_address(standard_address = '46r4nYSevkfBUMhuykdK3gQ98XDqDTYW1hNLaXNvjpsJaSbNtdXh1sKMsdVgqkaihChAzEy29zEDPMR3NHQvGoZCLGwTerr', payment_id = '1122334455667788')
        except: fails += 1
        try: wallet.make_integrated_address(standard_address = 'QftgVbzSoxR7VYGeuu4Npp86qCjkBYPf1g6dQj1ovJwBZxDRnPxKSyQjhVTL22p5bACTEMjEepu8eaUrtk6P5VEhRoernfpsFcp4tDY8hEnJ8', payment_id = '1122334455667788')
        except: fails += 1
        assert fails == 2

if __name__ == '__main__':
    IntegratedAddressTest().run_test()
