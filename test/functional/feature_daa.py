#!/usr/bin/env python3
# Copyright (c) 2026 The eCash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Exercise this patch's compiled DAA in isolated regtest, including reorgs."""

from test_framework.blocktools import create_block, create_coinbase
from test_framework.daa import H, INTERVAL, next_bits
from test_framework.messages import CBlockHeader, MAGIC_BYTES
from test_framework.p2p import P2PInterface
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error

# Existing eCash identity, scoped to this test; no production identity change.
MAGIC_BYTES['regtest'] = bytes.fromhex('eca5b134')


class DAATest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 3
        # Upstream's framework writes bitcoin.conf; eCash defaults to ecash.conf.
        self.extra_args = [
            ['-conf=bitcoin.conf', '-test=daa'],
            ['-conf=bitcoin.conf', '-test=daa', '-minimumchainwork=0x400'],
            ['-conf=bitcoin.conf'],
        ]

    def setup_network(self):
        self.setup_nodes()

    def mine(self, end, *, alternate=False):
        node = self.nodes[0]
        while len(self.bits) <= end:
            height = len(self.bits)
            if alternate:
                delta = 1300 if height == H-1 else 300
            elif height < 180:
                delta = 600
            elif height < H+INTERVAL:
                delta = 300
            elif height < H+2*INTERVAL:
                delta = 150
            elif height < H+3*INTERVAL:
                delta = 3600
            else:
                delta = 60 if height % 2 else 1140
                if height == H+3*INTERVAL+40:
                    delta = -100  # Valid raw time reversal, above previous MTP.
            timestamp = max(self.times[-1]+delta, 1_600_000_000)
            node.setmocktime(timestamp+10)
            required = next_bits(self.bits, self.times, height)
            if height in {H, H+1, 288} or height >= H and (height-H) % INTERVAL == 0:
                template = node.getblocktemplate({'rules': ['segwit', 'bip300301']})
                assert_equal(int(template['bits'],16), required)
            block = create_block(int(node.getbestblockhash(),16), create_coinbase(height), timestamp)
            block.nBits = required
            if height in {H, H+1, 288, H+INTERVAL, H+2*INTERVAL}:
                wrong = CBlockHeader(block)
                wrong.nBits = required-1
                # Header and full-block validation must reject the wrong bits.
                # Solve with a temporary block so header PoW is genuinely valid.
                block.nBits = wrong.nBits
                block.solve()
                wrong = CBlockHeader(block)
                assert_raises_rpc_error(-25, 'bad-diffbits', node.submitheader, wrong.serialize().hex())
                assert_equal(node.submitblock(block.serialize().hex()), 'bad-diffbits')
                block.nBits = required
            block.solve()
            assert_equal(node.submitblock(block.serialize().hex()), None)
            self.bits.append(required)
            self.times.append(timestamp)
            self.hashes.append(node.getbestblockhash())

    def run_test(self):
        node, peer, standard = self.nodes
        node.add_p2p_connection(P2PInterface())
        genesis = node.getblock(node.getbestblockhash())
        self.bits = [int(genesis['bits'],16)]
        self.times = [genesis['time']]
        self.hashes = [node.getbestblockhash()]
        self.log.info('Constant, changing, and oscillating timings across activation and four retargets')
        self.mine(H+4*INTERVAL)
        for n in range(H,H+INTERVAL):
            assert_equal(self.bits[n],self.bits[H-1])
        assert self.bits[H+INTERVAL] != self.bits[H-1]
        peer.setmocktime(self.times[-1]+10)
        self.log.info('Download headers through presync and redownload, then validate blocks')
        self.connect_nodes(0,1)
        self.sync_blocks(self.nodes[:2])
        self.log.info('Standard regtest still has no retargeting')
        self.generatetoaddress(standard, 145, standard.get_deterministic_priv_key().address, sync_fun=self.no_op)
        assert_equal(standard.getblock(standard.getbestblockhash())['bits'],'207fffff')
        self.log.info('Reorganize across activation and replace the fixed anchor')
        self.disconnect_nodes(0,1)
        node.invalidateblock(self.hashes[H-1])
        self.bits = self.bits[:H-1]
        self.times = self.times[:H-1]
        self.hashes = self.hashes[:H-1]
        self.mine(H+5*INTERVAL, alternate=True)
        self.restart_node(0, extra_args=self.extra_args[0])
        assert_equal(node.getblock(node.getbestblockhash())['bits'],f'{self.bits[-1]:08x}')
        peer.setmocktime(self.times[-1]+10)
        self.connect_nodes(0,1)
        self.sync_blocks(self.nodes[:2])


if __name__ == '__main__':
    DAATest(__file__).main()
