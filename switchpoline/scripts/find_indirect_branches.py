#!/usr/bin/env python3
import os
import sys
from typing import List, Optional

os.environ['PWNLIB_NOTERM'] = '1'
from pwn import ELF, disasm, asm, context

context.arch = 'aarch64'

# br - https://developer.arm.com/documentation/ddi0596/2020-12/Base-Instructions/BR--Branch-to-Register-?lang=en
br_pattern = 0b11010110000111110000000000000000
br_mask = 0b11111111111111111111110000011111
# blr - https://developer.arm.com/documentation/ddi0596/2020-12/Base-Instructions/BLR--Branch-with-Link-to-Register-?lang=en
blr_pattern = 0b11010110001111110000000000000000
blr_mask = 0b11111111111111111111110000011111


def test_patterns():
    SAMPLE = '''
    BR x0
    BLR x0
    '''
    # BLRAA, BLRAAZ, BLRAB, BLRABZ: Branch with Link to Register, with pointer authentication.
    # BRAA, BRAAZ, BRAB, BRABZ: Branch to Register, with pointer authentication.
    print(disasm(asm(SAMPLE)))
    br_sample = int.from_bytes(asm('BR x12'), byteorder='little', signed=False)
    print(hex(br_sample & br_mask), (br_sample & br_mask) == br_pattern)
    blr_sample = int.from_bytes(asm('BLR x0'), byteorder='little', signed=False)
    print(hex(blr_sample & blr_mask), (blr_sample & blr_mask) == blr_pattern)


class IndirectBranch:
    """
    Represent any indirect branch found
    """

    def __init__(self, address: int):
        self.address = address
        self.disassembly: Optional[str] = None
        self.symbol: Optional[str] = None
        self.symbol_offset: Optional[int] = None

    def report(self):
        print(f'{self.disassembly:48s} |  location: {self.symbol} +{hex(self.symbol_offset)}')

    def is_critical(self) -> bool:
        """
        We can accept some branches, because they are executed before attacker input or sensitive data input
        :return:
        """
        if self.symbol == 'libc_start_init' and self.symbol_offset <= 0x100:
            return False
        return True


def find_indirect_branch_addresses(content: bytes) -> List[int]:
    indirect_branches = []
    for address in range(0, len(content), 4):
        ins = int.from_bytes(content[address:address + 4], byteorder='little', signed=False)
        if (ins & br_mask) == br_pattern or (ins & blr_mask) == blr_pattern:
            indirect_branches.append(address)
    return indirect_branches


def find_indirect_branches(elf: ELF, content: bytes, content_offset: int) -> List[IndirectBranch]:
    addresses = find_indirect_branch_addresses(content)
    print(f'Found {len(addresses)} indirect branches:')
    symbols = list(elf.symbols.items())
    symbols.sort(key=lambda x: x[1])
    symbol_index = 0
    result = []
    for address in addresses:
        while symbol_index < len(symbols) - 1 and symbols[symbol_index + 1][1] < address + content_offset:
            symbol_index += 1
        branch = IndirectBranch(address + content_offset)
        branch.disassembly = disasm(content[address:address + 4], address + content_offset)
        branch.symbol = symbols[symbol_index][0]
        branch.symbol_offset = address + content_offset - symbols[symbol_index][1]
        result.append(branch)
    return result


def report_indirect_branches(fname: str) -> bool:
    print(fname)
    elf = ELF(fname)

    has_too_much_branches = False

    for segment in elf.executable_segments:
        print('Executable segment:', segment.__dict__)
        for section in elf.sections:
            if section.name and segment.header.p_vaddr <= section.header.sh_addr <= segment.header.p_vaddr + segment.header.p_memsz:
                print('  - contains section', section.name)
        content = segment.data()
        print(f'  - length: {len(content)} bytes')
        branches = find_indirect_branches(elf, content, segment.header.p_vaddr)
        for branch in branches:
            branch.report()
            if branch.is_critical():
                has_too_much_branches = True
    print('')
    return has_too_much_branches


def assert_no_indirect_branches(fname: str):
    assert not report_indirect_branches(fname)


if __name__ == '__main__':
    if len(sys.argv) == 1:
        # report_indirect_branches('../tests/test3-aarch64')
        report_indirect_branches('../temp/sample1-aarch64')
        print('USAGE: ', sys.argv[0], '<files>...')
        sys.exit(0)

    has_too_much_branches = 0
    for arg in sys.argv[1:]:
        if report_indirect_branches(arg):
            has_too_much_branches += 1
    print(f'{has_too_much_branches} / {len(sys.argv)-1} files have indirect branches remaining')
    if has_too_much_branches > 0:
        sys.exit(1)
