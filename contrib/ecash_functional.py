#!/usr/bin/env python3
# Copyright (c) 2026 The eCash developers
# Distributed under the MIT software license, see COPYING.
"""Run unchanged upstream tests against existing eCash config/magic/RPC rules."""
import runpy
import sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
functional=root/'test/functional'
sys.path.insert(0,str(functional))
from test_framework.messages import MAGIC_BYTES
from test_framework.test_node import TestNode
MAGIC_BYTES.update(mainnet=bytes.fromhex('eca5dcb7'),testnet4=bytes.fromhex('eca5b124'),regtest=bytes.fromhex('eca5b134'))
original=TestNode.__init__
def init(self,*args,**kwargs):
    original(self,*args,**kwargs)
    self.args.extend(['-conf=bitcoin.conf','-deprecatedrpc=getblocktemplate'])
TestNode.__init__=init
target=functional/sys.argv.pop(1)
sys.argv[0]=str(target)
runpy.run_path(str(target),run_name='__main__')
