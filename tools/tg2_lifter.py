#!/usr/bin/env python3
"""
tools/tg2_lifter.py - Static Recompiler (Lifter) for Top Gear 2 (SNES).
Translates 65816 machine code from a verified Top Gear 2 (USA) ROM into sharded C switch-case functions,
or generates a ROM-free interpreter dispatch stub (--stub) for clean CI builds.
"""

import argparse
from collections import defaultdict, deque
import hashlib
import pathlib
import sys

_DIR = pathlib.Path(__file__).resolve().parent
_PROJ_ROOT = _DIR.parent
sys.path.insert(0, str(_PROJ_ROOT / 'snesrecomp_repo' / 'recompiler'))
import snes65816

EXPECTED_ROM_SIZE = 1048576
EXPECTED_ROM_SHA256 = '76b2702c4be8b668c1017f2817c280283c275eaa41535bf6ffa2b8d2220b68c6'

# Declarative bank $8B widescreen 16:9 sprite/bounds hooks (e=0, m=0, x=0)
WIDESCREEN_PREPEND = {
    (0x8B, 0x858F): [
        'instance->ws_staging_count = 0u;',
        'instance->ws_staging_dirty = 0u;',
    ],
    (0x8B, 0xA7C1): [
        'if (instance->aspect_mode == TG2_ASPECT_16_9 && tg2_is_active_race(instance)) {',
        '    uint16_t d34 = 0u;',
        '    if (!tg2_bus_read16(instance, (uint32_t)((instance->cpu.d + 0x34u) & 0xFFFFu), &d34)) return 0;',
        '    if (d34 == 0u) {',
        '        int16_t sx = (int16_t)tg2_acc16(instance);',
        '        instance->cpu.pc = (sx < -71) ? 0xA7D8u : 0xA7C3u;',
        '        instance->instruction_count++;',
        '        return 1;',
        '    }',
        '}',
    ],
    (0x8B, 0xA873): [
        'if (tg2_ws_try_capture_tile(instance, 16u)) {',
        '    instance->cpu.pc = 0xA8B9u;',
        '    instance->instruction_count++;',
        '    return 1;',
        '}',
    ],
    (0x8B, 0xA8F2): [
        'if (tg2_ws_try_capture_tile(instance, 8u)) {',
        '    instance->cpu.pc = 0xA938u;',
        '    instance->instruction_count++;',
        '    return 1;',
        '}',
    ],
    (0x8B, 0xABB6): [
        'if (tg2_ws_try_capture_tile(instance, 16u)) {',
        '    instance->cpu.pc = 0xAC30u;',
        '    instance->instruction_count++;',
        '    return 1;',
        '}',
    ],
    (0x8B, 0xAC0E): [
        'if (tg2_ws_try_capture_tile(instance, 16u)) {',
        '    instance->cpu.pc = 0xAC30u;',
        '    instance->instruction_count++;',
        '    return 1;',
        '}',
    ],
    (0x8B, 0xAC77): [
        'if (tg2_ws_try_capture_tile(instance, 8u)) {',
        '    instance->cpu.pc = 0xACF1u;',
        '    instance->instruction_count++;',
        '    return 1;',
        '}',
    ],
    (0x8B, 0xACCF): [
        'if (tg2_ws_try_capture_tile(instance, 8u)) {',
        '    instance->cpu.pc = 0xACF1u;',
        '    instance->instruction_count++;',
        '    return 1;',
        '}',
    ],
    (0x8B, 0xD7E7): [
        'instance->ws_staging_dirty = 1u;',
    ],
}

WIDESCREEN_REPLACE = {
    (0x8B, 0xA7C9): [
        'if (!tg2_bus_read16(instance, (uint32_t)((instance->cpu.d + 0x36u) & 0xFFFFu), &word)) return 0;',
        'if (instance->aspect_mode == TG2_ASPECT_16_9 && tg2_is_active_race(instance) && word == 0x0100u) word = 0x0147u;',
        'tg2_compare16(instance, tg2_acc16(instance), word);',
    ],
    (0x8B, 0xA80C): [
        'if (!tg2_bus_read16(instance, (uint32_t)((instance->cpu.d + 0x34u) & 0xFFFFu), &word)) return 0;',
        'if (instance->aspect_mode == TG2_ASPECT_16_9 && tg2_is_active_race(instance) && word == 0u) word = 0xFFB9u;',
        'tg2_sbc16(instance, word);',
    ],
    (0x8B, 0xA827): [
        'if (!tg2_bus_read16(instance, (uint32_t)((instance->cpu.d + 0x36u) & 0xFFFFu), &word)) return 0;',
        'if (instance->aspect_mode == TG2_ASPECT_16_9 && tg2_is_active_race(instance) && word == 0x0100u) word = 0x0147u;',
        'tg2_set_acc16(instance, word); tg2_set_nz16(instance, word);',
    ],
    (0x8B, 0xA9C9): [
        'tg2_compare16(instance, tg2_acc16(instance),',
        '              (instance->aspect_mode == TG2_ASPECT_16_9 && tg2_is_active_race(instance)) ? 0x0147u : 0x0100u);',
    ],
    (0x8B, 0xA9D4): [
        'if (instance->aspect_mode == TG2_ASPECT_16_9 && tg2_is_active_race(instance)) {',
        '    instance->cpu.pc = ((int16_t)tg2_acc16(instance) < -71) ? 0xAA30u : 0xA9D6u;',
        '} else {',
        '    instance->cpu.pc = tg2_flag(instance, TG2_P_N) ? 0xAA30u : 0xA9D6u;',
        '}',
    ],
    (0x8B, 0xA9F5): [
        'if (instance->aspect_mode == TG2_ASPECT_16_9 && tg2_is_active_race(instance)) {',
        '    instance->cpu.pc = ((int16_t)tg2_acc16(instance) >= -71) ? 0xAA06u : 0xA9F7u;',
        '} else {',
        '    instance->cpu.pc = !tg2_flag(instance, TG2_P_N) ? 0xAA06u : 0xA9F7u;',
        '}',
    ],
    (0x8B, 0xAA0E): [
        'word = (instance->aspect_mode == TG2_ASPECT_16_9 && tg2_is_active_race(instance)) ? 0x0147u : 0x0100u;',
        'tg2_set_acc16(instance, word); tg2_set_nz16(instance, word);',
    ],
    (0x8B, 0xAA5F): [
        'tg2_compare16(instance, tg2_acc16(instance),',
        '              (instance->aspect_mode == TG2_ASPECT_16_9 && tg2_is_active_race(instance)) ? 0x0147u : 0x0100u);',
    ],
    (0x8B, 0xAA6A): [
        'if (instance->aspect_mode == TG2_ASPECT_16_9 && tg2_is_active_race(instance)) {',
        '    instance->cpu.pc = ((int16_t)tg2_acc16(instance) < -71) ? 0xAAC6u : 0xAA6Cu;',
        '} else {',
        '    instance->cpu.pc = tg2_flag(instance, TG2_P_N) ? 0xAAC6u : 0xAA6Cu;',
        '}',
    ],
    (0x8B, 0xAA8B): [
        'if (instance->aspect_mode == TG2_ASPECT_16_9 && tg2_is_active_race(instance)) {',
        '    instance->cpu.pc = ((int16_t)tg2_acc16(instance) >= -71) ? 0xAA9Cu : 0xAA8Du;',
        '} else {',
        '    instance->cpu.pc = !tg2_flag(instance, TG2_P_N) ? 0xAA9Cu : 0xAA8Du;',
        '}',
    ],
    (0x8B, 0xAAA4): [
        'word = (instance->aspect_mode == TG2_ASPECT_16_9 && tg2_is_active_race(instance)) ? 0x0147u : 0x0100u;',
        'tg2_set_acc16(instance, word); tg2_set_nz16(instance, word);',
    ],
    (0x8B, 0xAAF5): [
        'if (!tg2_bus_read16(instance, (uint32_t)((instance->cpu.d + 0x36u) & 0xFFFFu), &word)) return 0;',
        'if (instance->aspect_mode == TG2_ASPECT_16_9 && tg2_is_active_race(instance) && word == 0x0100u) word = 0x0147u;',
        'tg2_compare16(instance, tg2_acc16(instance), word);',
    ],
    (0x8B, 0xAAFF): [
        'if (!tg2_bus_read16(instance, (uint32_t)((instance->cpu.d + 0x34u) & 0xFFFFu), &word)) return 0;',
        'if (instance->aspect_mode == TG2_ASPECT_16_9 && tg2_is_active_race(instance) && word == 0u) word = 0xFFB9u;',
        'tg2_compare16(instance, tg2_acc16(instance), word);',
    ],
    (0x8B, 0xAB23): [
        'if (!tg2_bus_read16(instance, (uint32_t)((instance->cpu.d + 0x34u) & 0xFFFFu), &word)) return 0;',
        'if (instance->aspect_mode == TG2_ASPECT_16_9 && tg2_is_active_race(instance) && word == 0u) word = 0xFFB9u;',
        'tg2_sbc16(instance, word);',
    ],
    (0x8B, 0xAB3E): [
        'if (!tg2_bus_read16(instance, (uint32_t)((instance->cpu.d + 0x36u) & 0xFFFFu), &word)) return 0;',
        'if (instance->aspect_mode == TG2_ASPECT_16_9 && tg2_is_active_race(instance) && word == 0x0100u) word = 0x0147u;',
        'tg2_set_acc16(instance, word); tg2_set_nz16(instance, word);',
    ],
}


def context_key(e, m, x, pbr, pc):
    return ((e & 1) << 27) | ((m & 1) << 26) | ((x & 1) << 25) | ((pbr & 0xFF) << 16) | (pc & 0xFFFF)


def clean_generated_dir(gen_dir: pathlib.Path):
    gen_dir.mkdir(parents=True, exist_ok=True)
    for pattern in ('tg2_group_*.c', 'tg2_group_*.o', 'tg2_dispatch_*.c', 'tg2_dispatch_*.o'):
        for p in gen_dir.glob(pattern):
            p.unlink()


def emit_stub(gen_dir: pathlib.Path):
    clean_generated_dir(gen_dir)
    stub_path = gen_dir / 'tg2_dispatch_stub.c'
    stub_path.write_text(
        '/* Clean CI / ROM-less interpreter dispatch stub (zero ROM-derived code, full 16:9 hooks) */\n'
        '#include "topgear2_internal.h"\n\n'
        'int tg2_generated_static_step(struct TopGear2Recomp *instance) {\n'
        '    return tg2_interp_step_with_hooks(instance);\n'
        '}\n',
        encoding='utf-8'
    )
    print(f"Generated clean interpreter stub at {stub_path}")


def main():
    parser = argparse.ArgumentParser(description='Static W65C816 recompiler for Top Gear 2 (USA).')
    parser.add_argument('rom_path', nargs='?', default=str(_PROJ_ROOT / 'Top Gear 2 (USA).sfc'),
                        help='Path to Top Gear 2 (USA) ROM (.sfc/.smc)')
    parser.add_argument('--out-dir', default=str(_PROJ_ROOT / 'generated'),
                        help='Output directory for generated C files')
    parser.add_argument('--stub', action='store_true',
                        help='Generate ROM-free fallback stub for CI/interpreter builds')
    parser.add_argument('--skip-hash-check', action='store_true',
                        help='Skip SHA-256 verification (for advanced experimentation only)')
    args = parser.parse_args()

    gen_dir = pathlib.Path(args.out_dir)
    if args.stub:
        emit_stub(gen_dir)
        return 0

    rom_path = pathlib.Path(args.rom_path)
    if not rom_path.exists():
        print(f"Error: ROM not found at {rom_path}", file=sys.stderr)
        return 1

    rom = rom_path.read_bytes()
    if (len(rom) % 1024) == 512:
        print("Stripping 512-byte SMC copier header...")
        rom = rom[512:]

    if len(rom) != EXPECTED_ROM_SIZE:
        print(f"Error: Invalid ROM size {len(rom)} bytes (expected {EXPECTED_ROM_SIZE} bytes).", file=sys.stderr)
        return 1

    rom_sha = hashlib.sha256(rom).hexdigest()
    if not args.skip_hash_check and rom_sha != EXPECTED_ROM_SHA256:
        print(f"Error: ROM SHA-256 mismatch.\n  Got:      {rom_sha}\n  Expected: {EXPECTED_ROM_SHA256}", file=sys.stderr)
        return 1

    snes65816.set_rom_mapping(snes65816.ROM_MAP_LOROM)
    print(f"Loaded ROM: {len(rom)} bytes (SHA-256 {rom_sha[:12]}...)")

    # Entry seeds: reset vector + native boot & proved jump tables
    queue = deque()
    visited = set()

    queue.append((0x00, 0x8A38, 1, 1, 1))
    queue.append((0x81, 0x8003, 0, 1, 1))
    queue.append((0x81, 0x8008, 0, 1, 1))
    queue.append((0x81, 0xF7FD, 0, 1, 1))
    queue.append((0x1F, 0x8000, 0, 1, 1))
    queue.append((0x9F, 0x8000, 0, 1, 1))
    queue.append((0x9F, 0x8014, 0, 1, 1))
    queue.append((0x9F, 0x9194, 0, 1, 1))  # NMI
    queue.append((0x9F, 0x92B7, 0, 1, 1))  # IRQ
    queue.append((0x9F, 0x9274, 0, 1, 1))  # Direct Page vector

    # Race engine entrypoint (target of JML [$0100] at 9F:818D)
    queue.append((0x81, 0x8045, 0, 0, 0))
    queue.append((0x81, 0x8045, 0, 1, 1))

    # Proved Jump Tables
    for t in [0xDD91, 0xDDAB]:
        queue.append((0x81, t, 0, 1, 1))
    for t in [0xDCC7, 0xDCD8, 0xDCE3, 0xDCFA, 0xDD2A, 0xDD31, 0xDD38]:
        queue.append((0x9F, t, 0, 0, 0))
    for t in [0xDE11, 0xDE22, 0xDE2D, 0xDE44, 0xDE74, 0xDE7B, 0xDE82]:
        queue.append((0x9F, t, 0, 0, 0))
    for t in [0xC641, 0xC66A, 0xC69F, 0xC676, 0xC6B8, 0xC687]:
        queue.append((0x9F, t, 0, 0, 0))

    insn_map = {}

    while queue:
        bank, pc, e, m, x = queue.popleft()
        state = (bank, pc, e, m, x)
        if state in visited:
            continue
        visited.add(state)

        if not (0x8000 <= pc <= 0xFFFF) or bank in (0x7E, 0x7F):
            continue

        try:
            off = snes65816.lorom_offset(bank, pc)
            if off >= len(rom):
                continue
            insn = snes65816.decode_insn(rom, off, pc, bank, m, x)
        except Exception:
            continue

        k = context_key(e, m, x, bank, pc)
        insn_map[k] = (insn, e, m, x, bank, pc)

        next_e, next_m, next_x = e, m, x
        if insn.mnem == 'XCE':
            next_e = 0
        elif insn.mnem == 'REP':
            if insn.operand & 0x20:
                next_m = 0
            if insn.operand & 0x10:
                next_x = 0
        elif insn.mnem == 'SEP':
            if insn.operand & 0x20:
                next_m = 1
            if insn.operand & 0x10:
                next_x = 1

        if insn.mnem in ('BRA', 'BRL'):
            queue.append((bank, insn.operand, next_e, next_m, next_x))
        elif insn.mnem in ('BEQ', 'BNE', 'BCS', 'BCC', 'BMI', 'BPL', 'BVS', 'BVC'):
            queue.append((bank, insn.operand, next_e, next_m, next_x))
            queue.append((bank, pc + insn.length, next_e, next_m, next_x))
        elif insn.mnem == 'JSR':
            if insn.mode == snes65816.ABS:
                queue.append((bank, insn.operand, next_e, next_m, next_x))
            queue.append((bank, pc + insn.length, next_e, next_m, next_x))
        elif insn.mnem == 'JSL':
            tgt_b = (insn.operand >> 16) & 0xFF
            tgt_pc = insn.operand & 0xFFFF
            queue.append((tgt_b, tgt_pc, next_e, next_m, next_x))
            queue.append((bank, pc + insn.length, next_e, next_m, next_x))
        elif insn.mnem in ('JMP', 'JML'):
            if insn.mode == snes65816.ABS:
                queue.append((bank, insn.operand, next_e, next_m, next_x))
            elif insn.mode == snes65816.LONG:
                tgt_b = (insn.operand >> 16) & 0xFF
                tgt_pc = insn.operand & 0xFFFF
                queue.append((tgt_b, tgt_pc, next_e, next_m, next_x))
        elif insn.mnem in ('RTS', 'RTL', 'RTI', 'STP'):
            pass
        else:
            queue.append((bank, pc + insn.length, next_e, next_m, next_x))

    print(f"CFG Discovery complete: {len(insn_map)} reachable instruction contexts.")

    groups = defaultdict(list)
    for k, (insn, e, m, x, bank, pc) in insn_map.items():
        addr24 = ((bank & 0xFF) << 16) | (pc & 0xFFFF)
        grp = addr24 >> 10
        groups[grp].append((k, insn, e, m, x, bank, pc))

    print(f"Sharding into {len(groups)} C group files (with 16:9 widescreen hooks)...")
    clean_generated_dir(gen_dir)

    group_decls = []
    for grp in sorted(groups.keys()):
        grp_hex = f"{grp:04X}"
        c_path = gen_dir / f"tg2_group_{grp_hex}.c"
        group_decls.append(f"int tg2_group_{grp_hex}(struct TopGear2Recomp *instance);")

        lines = [
            '/* Generated static W65C816 shard for Top Gear 2 */',
            '#include "topgear2_internal.h"',
            '',
            f"int tg2_group_{grp_hex}(struct TopGear2Recomp *instance) {{",
            '    uint32_t address = 0u;',
            '    uint16_t word = 0u;',
            '    uint8_t byte = 0u;',
            '    (void)address; (void)word; (void)byte;',
            '    if (!instance) return 0;',
            '    uint32_t key = tg2_generated_context_key(instance);',
            '    switch (key) {',
        ]

        for k, insn, e, m, x, bank, pc in sorted(groups[grp], key=lambda item: item[0]):
            code = generate_insn_c(insn, e, m, x, bank, pc)
            lines.append(f"    case 0x{k:08X}u: /* {bank:02X}:{pc:04X} {insn.mnem} {insn._fmt()} */")
            for c in code:
                lines.append(f"        {c}")
            lines.append("        return 1;")

        lines.extend([
            "    default:",
            "        return tg2_fallback_step(instance);",
            "    }",
            "}",
            "",
        ])

        c_path.write_text('\n'.join(lines), encoding='utf-8')

    idx_path = gen_dir / "tg2_dispatch_index.c"
    idx_lines = [
        '/* Generated ROM-wide static dispatch index for Top Gear 2 */',
        '#include "topgear2_internal.h"',
        '',
        *group_decls,
        '',
        'int tg2_generated_static_step(struct TopGear2Recomp *instance) {',
        '    if (!instance) return 0;',
        '    uint32_t address = ((uint32_t)instance->cpu.pbr << 16) | instance->cpu.pc;',
        '    uint32_t group = address >> 10;',
        '    switch (group) {',
    ]
    for grp in sorted(groups.keys()):
        grp_hex = f"{grp:04X}"
        idx_lines.append(f"    case 0x{grp_hex}u: return tg2_group_{grp_hex}(instance);")
    idx_lines.extend([
        '    default: return tg2_fallback_step(instance);',
        '    }',
        '}',
        '',
    ])

    idx_path.write_text('\n'.join(idx_lines), encoding='utf-8')
    print(f"Generated {len(groups)} shards + tg2_dispatch_index.c successfully!")
    return 0


def get_addr_expr(mode, val, bank):
    if mode == snes65816.DP:
        return f"(uint32_t)((instance->cpu.d + 0x{val:02X}u) & 0xFFFFu)"
    elif mode == snes65816.DP_X:
        return f"(uint32_t)((instance->cpu.d + 0x{val:02X}u + instance->cpu.x) & 0xFFFFu)"
    elif mode == snes65816.DP_Y:
        return f"(uint32_t)((instance->cpu.d + 0x{val:02X}u + instance->cpu.y) & 0xFFFFu)"
    elif mode == snes65816.ABS:
        return f"(((uint32_t)instance->cpu.dbr << 16) | 0x{val:04X}u)"
    elif mode == snes65816.ABS_X:
        return f"((((uint32_t)instance->cpu.dbr << 16) + 0x{val:04X}u + instance->cpu.x) & 0xFFFFFFu)"
    elif mode == snes65816.ABS_Y:
        return f"((((uint32_t)instance->cpu.dbr << 16) + 0x{val:04X}u + instance->cpu.y) & 0xFFFFFFu)"
    elif mode == snes65816.LONG:
        return f"0x{val:06X}u"
    elif mode == snes65816.LONG_X:
        return f"((0x{val:06X}u + instance->cpu.x) & 0xFFFFFFu)"
    elif mode == snes65816.STK:
        return f"(uint32_t)((instance->cpu.s + 0x{val:02X}u) & 0xFFFFu)"
    return None


def generate_insn_c(insn, e, m, x, bank, pc):
    out = []
    mn = insn.mnem
    mode = insn.mode
    v = insn.operand
    next_pc = (pc + insn.length) & 0xFFFF
    is_native_16 = (e == 0 and m == 0 and x == 0)

    if is_native_16 and (bank, pc) in WIDESCREEN_PREPEND:
        out.extend(WIDESCREEN_PREPEND[(bank, pc)])

    if is_native_16 and (bank, pc) in WIDESCREEN_REPLACE:
        out.extend(WIDESCREEN_REPLACE[(bank, pc)])
        if mn not in ('BRA', 'BRL', 'BEQ', 'BNE', 'BCS', 'BCC', 'BMI', 'BPL', 'BVS', 'BVC', 'JMP', 'JML', 'JSR', 'JSL', 'RTS', 'RTL', 'RTI', 'STP'):
            out.append(f"instance->cpu.pc = 0x{next_pc:04X}u;")
        out.append("instance->instruction_count++;")
        return out

    # Flag instructions
    if mn == 'CLC':
        out.append("tg2_set_flag(instance, TG2_P_C, 0);")
    elif mn == 'SEC':
        out.append("tg2_set_flag(instance, TG2_P_C, 1);")
    elif mn == 'CLD':
        out.append("tg2_set_flag(instance, TG2_P_D, 0);")
    elif mn == 'SED':
        out.append("tg2_set_flag(instance, TG2_P_D, 1);")
    elif mn == 'CLI':
        out.append("tg2_set_flag(instance, TG2_P_I, 0);")
    elif mn == 'SEI':
        out.append("tg2_set_flag(instance, TG2_P_I, 1);")
    elif mn == 'CLV':
        out.append("tg2_set_flag(instance, TG2_P_V, 0);")
    elif mn == 'XCE':
        out.append("{")
        out.append("    uint8_t old_e = instance->cpu.e;")
        out.append("    uint8_t old_c = (uint8_t)tg2_flag(instance, TG2_P_C);")
        out.append("    instance->cpu.e = old_c;")
        out.append("    tg2_set_flag(instance, TG2_P_C, old_e != 0u);")
        out.append("    if (instance->cpu.e) {")
        out.append("        instance->cpu.p |= (TG2_P_M | TG2_P_X);")
        out.append("        instance->cpu.s = (uint16_t)(0x0100u | (instance->cpu.s & 0x00FFu));")
        out.append("    }")
        out.append("}")
    elif mn == 'REP':
        out.append(f"instance->cpu.p &= (uint8_t)~0x{v:02X}u;")
        out.append("if (instance->cpu.e) instance->cpu.p |= (TG2_P_M | TG2_P_X);")
        out.append("if (instance->cpu.e || (instance->cpu.p & TG2_P_X) != 0u) { instance->cpu.x &= 0x00FFu; instance->cpu.y &= 0x00FFu; }")
    elif mn == 'SEP':
        out.append(f"instance->cpu.p |= 0x{v:02X}u;")
        out.append("if (instance->cpu.e) instance->cpu.p |= (TG2_P_M | TG2_P_X);")
        out.append("if (instance->cpu.e || (instance->cpu.p & TG2_P_X) != 0u) { instance->cpu.x &= 0x00FFu; instance->cpu.y &= 0x00FFu; }")

    # Load / Store
    elif mn == 'LDA':
        if mode == snes65816.IMM:
            if m:
                out.append(f"byte = 0x{v:02X}u; tg2_set_acc8(instance, byte); tg2_set_nz8(instance, byte);")
            else:
                out.append(f"word = 0x{v:04X}u; tg2_set_acc16(instance, word); tg2_set_nz16(instance, word);")
        else:
            addr = get_addr_expr(mode, v, bank)
            if addr:
                if m:
                    out.append(f"if (!tg2_bus_read8(instance, {addr}, &byte)) return 0; tg2_set_acc8(instance, byte); tg2_set_nz8(instance, byte);")
                else:
                    out.append(f"if (!tg2_bus_read16(instance, {addr}, &word)) return 0; tg2_set_acc16(instance, word); tg2_set_nz16(instance, word);")
            else:
                out.append("return tg2_fallback_step(instance);")

    elif mn == 'STA':
        addr = get_addr_expr(mode, v, bank)
        if addr:
            if m:
                out.append(f"if (!tg2_bus_write8(instance, {addr}, tg2_acc8(instance))) return 0;")
            else:
                out.append(f"if (!tg2_bus_store16(instance, {addr}, tg2_acc16(instance))) return 0;")
        else:
            out.append("return tg2_fallback_step(instance);")

    elif mn == 'STZ':
        addr = get_addr_expr(mode, v, bank)
        if addr:
            if m:
                out.append(f"if (!tg2_bus_write8(instance, {addr}, 0u)) return 0;")
            else:
                out.append(f"if (!tg2_bus_store16(instance, {addr}, 0u)) return 0;")
        else:
            out.append("return tg2_fallback_step(instance);")

    elif mn == 'LDX':
        if mode == snes65816.IMM:
            if x:
                out.append(f"byte = 0x{v:02X}u; tg2_set_index8(instance, &instance->cpu.x, byte); tg2_set_nz8(instance, byte);")
            else:
                out.append(f"word = 0x{v:04X}u; tg2_set_index16(instance, &instance->cpu.x, word); tg2_set_nz16(instance, word);")
        else:
            addr = get_addr_expr(mode, v, bank)
            if addr:
                if x:
                    out.append(f"if (!tg2_bus_read8(instance, {addr}, &byte)) return 0; tg2_set_index8(instance, &instance->cpu.x, byte); tg2_set_nz8(instance, byte);")
                else:
                    out.append(f"if (!tg2_bus_read16(instance, {addr}, &word)) return 0; tg2_set_index16(instance, &instance->cpu.x, word); tg2_set_nz16(instance, word);")
            else:
                out.append("return tg2_fallback_step(instance);")

    elif mn == 'STX':
        addr = get_addr_expr(mode, v, bank)
        if addr:
            if x:
                out.append(f"if (!tg2_bus_write8(instance, {addr}, (uint8_t)instance->cpu.x)) return 0;")
            else:
                out.append(f"if (!tg2_bus_store16(instance, {addr}, instance->cpu.x)) return 0;")
        else:
            out.append("return tg2_fallback_step(instance);")

    elif mn == 'LDY':
        if mode == snes65816.IMM:
            if x:
                out.append(f"byte = 0x{v:02X}u; tg2_set_index8(instance, &instance->cpu.y, byte); tg2_set_nz8(instance, byte);")
            else:
                out.append(f"word = 0x{v:04X}u; tg2_set_index16(instance, &instance->cpu.y, word); tg2_set_nz16(instance, word);")
        else:
            addr = get_addr_expr(mode, v, bank)
            if addr:
                if x:
                    out.append(f"if (!tg2_bus_read8(instance, {addr}, &byte)) return 0; tg2_set_index8(instance, &instance->cpu.y, byte); tg2_set_nz8(instance, byte);")
                else:
                    out.append(f"if (!tg2_bus_read16(instance, {addr}, &word)) return 0; tg2_set_index16(instance, &instance->cpu.y, word); tg2_set_nz16(instance, word);")
            else:
                out.append("return tg2_fallback_step(instance);")

    elif mn == 'STY':
        addr = get_addr_expr(mode, v, bank)
        if addr:
            if x:
                out.append(f"if (!tg2_bus_write8(instance, {addr}, (uint8_t)instance->cpu.y)) return 0;")
            else:
                out.append(f"if (!tg2_bus_store16(instance, {addr}, instance->cpu.y)) return 0;")
        else:
            out.append("return tg2_fallback_step(instance);")

    # Arithmetic & Logic
    elif mn == 'ADC':
        if mode == snes65816.IMM:
            if m:
                out.append(f"tg2_adc8(instance, 0x{v:02X}u);")
            else:
                out.append(f"tg2_adc16(instance, 0x{v:04X}u);")
        else:
            addr = get_addr_expr(mode, v, bank)
            if addr:
                if m:
                    out.append(f"if (!tg2_bus_read8(instance, {addr}, &byte)) return 0; tg2_adc8(instance, byte);")
                else:
                    out.append(f"if (!tg2_bus_read16(instance, {addr}, &word)) return 0; tg2_adc16(instance, word);")
            else:
                out.append("return tg2_fallback_step(instance);")

    elif mn == 'SBC':
        if mode == snes65816.IMM:
            if m:
                out.append(f"tg2_sbc8(instance, 0x{v:02X}u);")
            else:
                out.append(f"tg2_sbc16(instance, 0x{v:04X}u);")
        else:
            addr = get_addr_expr(mode, v, bank)
            if addr:
                if m:
                    out.append(f"if (!tg2_bus_read8(instance, {addr}, &byte)) return 0; tg2_sbc8(instance, byte);")
                else:
                    out.append(f"if (!tg2_bus_read16(instance, {addr}, &word)) return 0; tg2_sbc16(instance, word);")
            else:
                out.append("return tg2_fallback_step(instance);")

    elif mn == 'CMP':
        if mode == snes65816.IMM:
            if m:
                out.append(f"tg2_compare8(instance, tg2_acc8(instance), 0x{v:02X}u);")
            else:
                out.append(f"tg2_compare16(instance, tg2_acc16(instance), 0x{v:04X}u);")
        else:
            addr = get_addr_expr(mode, v, bank)
            if addr:
                if m:
                    out.append(f"if (!tg2_bus_read8(instance, {addr}, &byte)) return 0; tg2_compare8(instance, tg2_acc8(instance), byte);")
                else:
                    out.append(f"if (!tg2_bus_read16(instance, {addr}, &word)) return 0; tg2_compare16(instance, tg2_acc16(instance), word);")
            else:
                out.append("return tg2_fallback_step(instance);")

    elif mn == 'CPX':
        if mode == snes65816.IMM:
            if x:
                out.append(f"tg2_compare8(instance, (uint8_t)instance->cpu.x, 0x{v:02X}u);")
            else:
                out.append(f"tg2_compare16(instance, instance->cpu.x, 0x{v:04X}u);")
        else:
            addr = get_addr_expr(mode, v, bank)
            if addr:
                if x:
                    out.append(f"if (!tg2_bus_read8(instance, {addr}, &byte)) return 0; tg2_compare8(instance, (uint8_t)instance->cpu.x, byte);")
                else:
                    out.append(f"if (!tg2_bus_read16(instance, {addr}, &word)) return 0; tg2_compare16(instance, instance->cpu.x, word);")
            else:
                out.append("return tg2_fallback_step(instance);")

    elif mn == 'CPY':
        if mode == snes65816.IMM:
            if x:
                out.append(f"tg2_compare8(instance, (uint8_t)instance->cpu.y, 0x{v:02X}u);")
            else:
                out.append(f"tg2_compare16(instance, instance->cpu.y, 0x{v:04X}u);")
        else:
            addr = get_addr_expr(mode, v, bank)
            if addr:
                if x:
                    out.append(f"if (!tg2_bus_read8(instance, {addr}, &byte)) return 0; tg2_compare8(instance, (uint8_t)instance->cpu.y, byte);")
                else:
                    out.append(f"if (!tg2_bus_read16(instance, {addr}, &word)) return 0; tg2_compare16(instance, instance->cpu.y, word);")
            else:
                out.append("return tg2_fallback_step(instance);")

    elif mn == 'AND':
        if mode == snes65816.IMM:
            if m:
                out.append(f"tg2_set_acc8(instance, (uint8_t)(tg2_acc8(instance) & 0x{v:02X}u)); tg2_set_nz8(instance, tg2_acc8(instance));")
            else:
                out.append(f"tg2_set_acc16(instance, (uint16_t)(tg2_acc16(instance) & 0x{v:04X}u)); tg2_set_nz16(instance, tg2_acc16(instance));")
        else:
            addr = get_addr_expr(mode, v, bank)
            if addr:
                if m:
                    out.append(f"if (!tg2_bus_read8(instance, {addr}, &byte)) return 0; tg2_set_acc8(instance, (uint8_t)(tg2_acc8(instance) & byte)); tg2_set_nz8(instance, tg2_acc8(instance));")
                else:
                    out.append(f"if (!tg2_bus_read16(instance, {addr}, &word)) return 0; tg2_set_acc16(instance, (uint16_t)(tg2_acc16(instance) & word)); tg2_set_nz16(instance, tg2_acc16(instance));")
            else:
                out.append("return tg2_fallback_step(instance);")

    elif mn == 'ORA':
        if mode == snes65816.IMM:
            if m:
                out.append(f"tg2_set_acc8(instance, (uint8_t)(tg2_acc8(instance) | 0x{v:02X}u)); tg2_set_nz8(instance, tg2_acc8(instance));")
            else:
                out.append(f"tg2_set_acc16(instance, (uint16_t)(tg2_acc16(instance) | 0x{v:04X}u)); tg2_set_nz16(instance, tg2_acc16(instance));")
        else:
            addr = get_addr_expr(mode, v, bank)
            if addr:
                if m:
                    out.append(f"if (!tg2_bus_read8(instance, {addr}, &byte)) return 0; tg2_set_acc8(instance, (uint8_t)(tg2_acc8(instance) | byte)); tg2_set_nz8(instance, tg2_acc8(instance));")
                else:
                    out.append(f"if (!tg2_bus_read16(instance, {addr}, &word)) return 0; tg2_set_acc16(instance, (uint16_t)(tg2_acc16(instance) | word)); tg2_set_nz16(instance, tg2_acc16(instance));")
            else:
                out.append("return tg2_fallback_step(instance);")

    elif mn == 'EOR':
        if mode == snes65816.IMM:
            if m:
                out.append(f"tg2_set_acc8(instance, (uint8_t)(tg2_acc8(instance) ^ 0x{v:02X}u)); tg2_set_nz8(instance, tg2_acc8(instance));")
            else:
                out.append(f"tg2_set_acc16(instance, (uint16_t)(tg2_acc16(instance) ^ 0x{v:04X}u)); tg2_set_nz16(instance, tg2_acc16(instance));")
        else:
            addr = get_addr_expr(mode, v, bank)
            if addr:
                if m:
                    out.append(f"if (!tg2_bus_read8(instance, {addr}, &byte)) return 0; tg2_set_acc8(instance, (uint8_t)(tg2_acc8(instance) ^ byte)); tg2_set_nz8(instance, tg2_acc8(instance));")
                else:
                    out.append(f"if (!tg2_bus_read16(instance, {addr}, &word)) return 0; tg2_set_acc16(instance, (uint16_t)(tg2_acc16(instance) ^ word)); tg2_set_nz16(instance, tg2_acc16(instance));")
            else:
                out.append("return tg2_fallback_step(instance);")

    elif mn == 'BIT':
        if mode == snes65816.IMM:
            if m:
                out.append(f"tg2_set_flag(instance, TG2_P_Z, (tg2_acc8(instance) & 0x{v:02X}u) == 0);")
            else:
                out.append(f"tg2_set_flag(instance, TG2_P_Z, (tg2_acc16(instance) & 0x{v:04X}u) == 0);")
        else:
            addr = get_addr_expr(mode, v, bank)
            if addr:
                if m:
                    out.append(f"if (!tg2_bus_read8(instance, {addr}, &byte)) return 0;")
                    out.append("tg2_set_flag(instance, TG2_P_Z, (tg2_acc8(instance) & byte) == 0);")
                    out.append("tg2_set_flag(instance, TG2_P_N, (byte & 0x80u) != 0);")
                    out.append("tg2_set_flag(instance, TG2_P_V, (byte & 0x40u) != 0);")
                else:
                    out.append(f"if (!tg2_bus_read16(instance, {addr}, &word)) return 0;")
                    out.append("tg2_set_flag(instance, TG2_P_Z, (tg2_acc16(instance) & word) == 0);")
                    out.append("tg2_set_flag(instance, TG2_P_N, (word & 0x8000u) != 0);")
                    out.append("tg2_set_flag(instance, TG2_P_V, (word & 0x4000u) != 0);")
            else:
                out.append("return tg2_fallback_step(instance);")

    # Increments & Decrements
    elif mn == 'INC':
        if mode == snes65816.ACC:
            if m:
                out.append("byte = (uint8_t)(tg2_acc8(instance) + 1u); tg2_set_acc8(instance, byte); tg2_set_nz8(instance, byte);")
            else:
                out.append("word = (uint16_t)(tg2_acc16(instance) + 1u); tg2_set_acc16(instance, word); tg2_set_nz16(instance, word);")
        else:
            addr = get_addr_expr(mode, v, bank)
            if addr:
                if m:
                    out.append(f"address = {addr}; if (!tg2_bus_read8(instance, address, &byte)) return 0; byte = (uint8_t)(byte + 1u); tg2_set_nz8(instance, byte); if (!tg2_bus_write8(instance, address, byte)) return 0;")
                else:
                    out.append(f"address = {addr}; if (!tg2_bus_read16(instance, address, &word)) return 0; word = (uint16_t)(word + 1u); tg2_set_nz16(instance, word); if (!tg2_bus_write16(instance, address, word)) return 0;")
            else:
                out.append("return tg2_fallback_step(instance);")

    elif mn == 'DEC':
        if mode == snes65816.ACC:
            if m:
                out.append("byte = (uint8_t)(tg2_acc8(instance) - 1u); tg2_set_acc8(instance, byte); tg2_set_nz8(instance, byte);")
            else:
                out.append("word = (uint16_t)(tg2_acc16(instance) - 1u); tg2_set_acc16(instance, word); tg2_set_nz16(instance, word);")
        else:
            addr = get_addr_expr(mode, v, bank)
            if addr:
                if m:
                    out.append(f"address = {addr}; if (!tg2_bus_read8(instance, address, &byte)) return 0; byte = (uint8_t)(byte - 1u); tg2_set_nz8(instance, byte); if (!tg2_bus_write8(instance, address, byte)) return 0;")
                else:
                    out.append(f"address = {addr}; if (!tg2_bus_read16(instance, address, &word)) return 0; word = (uint16_t)(word - 1u); tg2_set_nz16(instance, word); if (!tg2_bus_write16(instance, address, word)) return 0;")
            else:
                out.append("return tg2_fallback_step(instance);")

    elif mn == 'INX':
        if x:
            out.append("instance->cpu.x = (uint8_t)(instance->cpu.x + 1u); tg2_set_nz8(instance, (uint8_t)instance->cpu.x);")
        else:
            out.append("instance->cpu.x = (uint16_t)(instance->cpu.x + 1u); tg2_set_nz16(instance, instance->cpu.x);")
    elif mn == 'DEX':
        if x:
            out.append("instance->cpu.x = (uint8_t)(instance->cpu.x - 1u); tg2_set_nz8(instance, (uint8_t)instance->cpu.x);")
        else:
            out.append("instance->cpu.x = (uint16_t)(instance->cpu.x - 1u); tg2_set_nz16(instance, instance->cpu.x);")
    elif mn == 'INY':
        if x:
            out.append("instance->cpu.y = (uint8_t)(instance->cpu.y + 1u); tg2_set_nz8(instance, (uint8_t)instance->cpu.y);")
        else:
            out.append("instance->cpu.y = (uint16_t)(instance->cpu.y + 1u); tg2_set_nz16(instance, instance->cpu.y);")
    elif mn == 'DEY':
        if x:
            out.append("instance->cpu.y = (uint8_t)(instance->cpu.y - 1u); tg2_set_nz8(instance, (uint8_t)instance->cpu.y);")
        else:
            out.append("instance->cpu.y = (uint16_t)(instance->cpu.y - 1u); tg2_set_nz16(instance, instance->cpu.y);")

    # Register transfers
    elif mn == 'TAX':
        if x:
            out.append("tg2_set_index8(instance, &instance->cpu.x, tg2_acc8(instance)); tg2_set_nz8(instance, (uint8_t)instance->cpu.x);")
        else:
            out.append("tg2_set_index16(instance, &instance->cpu.x, tg2_acc16(instance)); tg2_set_nz16(instance, instance->cpu.x);")
    elif mn == 'TXA':
        if m:
            out.append("tg2_set_acc8(instance, (uint8_t)instance->cpu.x); tg2_set_nz8(instance, tg2_acc8(instance));")
        else:
            out.append("tg2_set_acc16(instance, instance->cpu.x); tg2_set_nz16(instance, tg2_acc16(instance));")
    elif mn == 'TAY':
        if x:
            out.append("tg2_set_index8(instance, &instance->cpu.y, tg2_acc8(instance)); tg2_set_nz8(instance, (uint8_t)instance->cpu.y);")
        else:
            out.append("tg2_set_index16(instance, &instance->cpu.y, tg2_acc16(instance)); tg2_set_nz16(instance, instance->cpu.y);")
    elif mn == 'TYA':
        if m:
            out.append("tg2_set_acc8(instance, (uint8_t)instance->cpu.y); tg2_set_nz8(instance, tg2_acc8(instance));")
        else:
            out.append("tg2_set_acc16(instance, instance->cpu.y); tg2_set_nz16(instance, tg2_acc16(instance));")
    elif mn == 'TSX':
        if x:
            out.append("tg2_set_index8(instance, &instance->cpu.x, (uint8_t)instance->cpu.s); tg2_set_nz8(instance, (uint8_t)instance->cpu.x);")
        else:
            out.append("tg2_set_index16(instance, &instance->cpu.x, instance->cpu.s); tg2_set_nz16(instance, instance->cpu.x);")
    elif mn == 'TXS':
        out.append("instance->cpu.s = instance->cpu.e ? (uint16_t)(0x0100u | (instance->cpu.x & 0x00FFu)) : instance->cpu.x;")
    elif mn == 'TXY':
        if x:
            out.append("tg2_set_index8(instance, &instance->cpu.y, (uint8_t)instance->cpu.x); tg2_set_nz8(instance, (uint8_t)instance->cpu.y);")
        else:
            out.append("tg2_set_index16(instance, &instance->cpu.y, instance->cpu.x); tg2_set_nz16(instance, instance->cpu.y);")
    elif mn == 'TYX':
        if x:
            out.append("tg2_set_index8(instance, &instance->cpu.x, (uint8_t)instance->cpu.y); tg2_set_nz8(instance, (uint8_t)instance->cpu.x);")
        else:
            out.append("tg2_set_index16(instance, &instance->cpu.x, instance->cpu.y); tg2_set_nz16(instance, instance->cpu.x);")
    elif mn == 'TCD':
        out.append("instance->cpu.d = instance->cpu.a; tg2_set_nz16(instance, instance->cpu.d);")
    elif mn == 'TDC':
        out.append("tg2_set_acc16(instance, instance->cpu.d); tg2_set_nz16(instance, instance->cpu.a);")
    elif mn == 'TCS':
        out.append("instance->cpu.s = instance->cpu.e ? (uint16_t)(0x0100u | (instance->cpu.a & 0x00FFu)) : instance->cpu.a;")
    elif mn == 'TSC':
        out.append("tg2_set_acc16(instance, instance->cpu.s); tg2_set_nz16(instance, instance->cpu.a);")
    elif mn == 'XBA':
        out.append("instance->cpu.a = (uint16_t)((instance->cpu.a >> 8) | (instance->cpu.a << 8)); tg2_set_nz8(instance, (uint8_t)instance->cpu.a);")

    # Stack operations
    elif mn == 'PHA':
        if m:
            out.append("if (!tg2_push8(instance, tg2_acc8(instance))) return 0;")
        else:
            out.append("if (!tg2_push16(instance, tg2_acc16(instance))) return 0;")
    elif mn == 'PLA':
        if m:
            out.append("if (!tg2_pull8(instance, &byte)) return 0; tg2_set_acc8(instance, byte); tg2_set_nz8(instance, byte);")
        else:
            out.append("if (!tg2_pull16(instance, &word)) return 0; tg2_set_acc16(instance, word); tg2_set_nz16(instance, word);")
    elif mn == 'PHX':
        if x:
            out.append("if (!tg2_push8(instance, (uint8_t)instance->cpu.x)) return 0;")
        else:
            out.append("if (!tg2_push16(instance, instance->cpu.x)) return 0;")
    elif mn == 'PLX':
        if x:
            out.append("if (!tg2_pull8(instance, &byte)) return 0; tg2_set_index8(instance, &instance->cpu.x, byte); tg2_set_nz8(instance, byte);")
        else:
            out.append("if (!tg2_pull16(instance, &word)) return 0; tg2_set_index16(instance, &instance->cpu.x, word); tg2_set_nz16(instance, word);")
    elif mn == 'PHY':
        if x:
            out.append("if (!tg2_push8(instance, (uint8_t)instance->cpu.y)) return 0;")
        else:
            out.append("if (!tg2_push16(instance, instance->cpu.y)) return 0;")
    elif mn == 'PLY':
        if x:
            out.append("if (!tg2_pull8(instance, &byte)) return 0; tg2_set_index8(instance, &instance->cpu.y, byte); tg2_set_nz8(instance, byte);")
        else:
            out.append("if (!tg2_pull16(instance, &word)) return 0; tg2_set_index16(instance, &instance->cpu.y, word); tg2_set_nz16(instance, word);")
    elif mn == 'PHP':
        out.append("if (!tg2_push8(instance, instance->cpu.p)) return 0;")
    elif mn == 'PLP':
        out.append("if (!tg2_pull8(instance, &byte)) return 0; instance->cpu.p = byte;")
        out.append("if (instance->cpu.e) { instance->cpu.p |= (TG2_P_M | TG2_P_X); instance->cpu.x &= 0x00FFu; instance->cpu.y &= 0x00FFu; }")
        out.append("else if (instance->cpu.p & TG2_P_X) { instance->cpu.x &= 0x00FFu; instance->cpu.y &= 0x00FFu; }")
    elif mn == 'PHB':
        out.append("if (!tg2_push8(instance, instance->cpu.dbr)) return 0;")
    elif mn == 'PLB':
        out.append("if (!tg2_pull8(instance, &byte)) return 0; instance->cpu.dbr = byte; tg2_set_nz8(instance, byte);")
    elif mn == 'PHD':
        out.append("if (!tg2_push16(instance, instance->cpu.d)) return 0;")
    elif mn == 'PLD':
        out.append("if (!tg2_pull16(instance, &word)) return 0; instance->cpu.d = word; tg2_set_nz16(instance, word);")
    elif mn == 'PHK':
        out.append("if (!tg2_push8(instance, instance->cpu.pbr)) return 0;")
    elif mn == 'PEA':
        out.append(f"if (!tg2_push16(instance, 0x{v:04X}u)) return 0;")

    # Branches & Jumps
    elif mn in ('BRA', 'BRL'):
        out.append(f"instance->cpu.pc = 0x{v:04X}u;")
    elif mn == 'BNE':
        out.append(f"instance->cpu.pc = !tg2_flag(instance, TG2_P_Z) ? 0x{v:04X}u : 0x{next_pc:04X}u;")
    elif mn == 'BEQ':
        out.append(f"instance->cpu.pc = tg2_flag(instance, TG2_P_Z) ? 0x{v:04X}u : 0x{next_pc:04X}u;")
    elif mn == 'BCC':
        out.append(f"instance->cpu.pc = !tg2_flag(instance, TG2_P_C) ? 0x{v:04X}u : 0x{next_pc:04X}u;")
    elif mn == 'BCS':
        out.append(f"instance->cpu.pc = tg2_flag(instance, TG2_P_C) ? 0x{v:04X}u : 0x{next_pc:04X}u;")
    elif mn == 'BPL':
        out.append(f"instance->cpu.pc = !tg2_flag(instance, TG2_P_N) ? 0x{v:04X}u : 0x{next_pc:04X}u;")
    elif mn == 'BMI':
        out.append(f"instance->cpu.pc = tg2_flag(instance, TG2_P_N) ? 0x{v:04X}u : 0x{next_pc:04X}u;")
    elif mn == 'BVC':
        out.append(f"instance->cpu.pc = !tg2_flag(instance, TG2_P_V) ? 0x{v:04X}u : 0x{next_pc:04X}u;")
    elif mn == 'BVS':
        out.append(f"instance->cpu.pc = tg2_flag(instance, TG2_P_V) ? 0x{v:04X}u : 0x{next_pc:04X}u;")

    elif mn in ('JMP', 'JML'):
        if insn.opcode == 0xDC:
            out.append(f"if (!tg2_bus_read16(instance, 0x{v:04X}u, &word)) return 0; instance->cpu.pc = word;")
            out.append(f"if (!tg2_bus_read8(instance, 0x{(v + 2) & 0xFFFF:04X}u, &byte)) return 0; instance->cpu.pbr = byte;")
        elif mode == snes65816.ABS:
            out.append(f"instance->cpu.pc = 0x{v:04X}u;")
        elif mode == snes65816.LONG:
            tgt_b = (v >> 16) & 0xFF
            tgt_pc = v & 0xFFFF
            out.append(f"instance->cpu.pbr = 0x{tgt_b:02X}u; instance->cpu.pc = 0x{tgt_pc:04X}u;")
        elif mode == snes65816.INDIR:
            out.append(f"if (!tg2_bus_read16(instance, 0x{v:04X}u, &word)) return 0; instance->cpu.pc = word;")
        elif mode == snes65816.INDIR_X:
            out.append(f"if (!tg2_bus_read16(instance, (((uint32_t)instance->cpu.pbr << 16) | (uint16_t)(0x{v:04X}u + instance->cpu.x)), &word)) return 0; instance->cpu.pc = word;")
        else:
            out.append("return tg2_fallback_step(instance);")

    elif mn == 'JSR':
        ret_pc = (next_pc - 1) & 0xFFFF
        if mode == snes65816.ABS:
            out.append(f"if (!tg2_push16(instance, 0x{ret_pc:04X}u)) return 0; instance->cpu.pc = 0x{v:04X}u;")
        elif mode == snes65816.INDIR_X:
            out.append(f"if (!tg2_push16(instance, 0x{ret_pc:04X}u)) return 0;")
            out.append(f"if (!tg2_bus_read16(instance, (((uint32_t)instance->cpu.pbr << 16) | (uint16_t)(0x{v:04X}u + instance->cpu.x)), &word)) return 0; instance->cpu.pc = word;")
        else:
            out.append("return tg2_fallback_step(instance);")

    elif mn == 'JSL':
        ret_pc = (next_pc - 1) & 0xFFFF
        tgt_b = (v >> 16) & 0xFF
        tgt_pc = v & 0xFFFF
        out.append("if (!tg2_push8(instance, instance->cpu.pbr)) return 0;")
        out.append(f"if (!tg2_push16(instance, 0x{ret_pc:04X}u)) return 0;")
        out.append(f"instance->cpu.pbr = 0x{tgt_b:02X}u; instance->cpu.pc = 0x{tgt_pc:04X}u;")

    elif mn == 'RTS':
        out.append("if (!tg2_pull16(instance, &word)) return 0; instance->cpu.pc = (uint16_t)(word + 1u);")
    elif mn == 'RTL':
        out.append("if (!tg2_pull16(instance, &word)) return 0; if (!tg2_pull8(instance, &byte)) return 0;")
        out.append("instance->cpu.pbr = byte; instance->cpu.pc = (uint16_t)(word + 1u);")
    elif mn == 'RTI':
        out.append("if (!tg2_pull8(instance, &byte)) return 0; instance->cpu.p = byte;")
        out.append("if (instance->cpu.e) { instance->cpu.p |= (TG2_P_M | TG2_P_X); instance->cpu.x &= 0x00FFu; instance->cpu.y &= 0x00FFu; }")
        out.append("else if (instance->cpu.p & TG2_P_X) { instance->cpu.x &= 0x00FFu; instance->cpu.y &= 0x00FFu; }")
        out.append("if (!tg2_pull16(instance, &word)) return 0; instance->cpu.pc = word;")
        out.append("if (!instance->cpu.e) { if (!tg2_pull8(instance, &byte)) return 0; instance->cpu.pbr = byte; } else instance->cpu.pbr = 0u;")

    elif mn == 'NOP':
        pass
    else:
        out.append("return tg2_fallback_step(instance);")

    if mn not in ('BRA', 'BRL', 'BEQ', 'BNE', 'BCS', 'BCC', 'BMI', 'BPL', 'BVS', 'BVC', 'JMP', 'JML', 'JSR', 'JSL', 'RTS', 'RTL', 'RTI', 'STP'):
        out.append(f"instance->cpu.pc = 0x{next_pc:04X}u;")

    out.append("instance->instruction_count++;")
    return out


if __name__ == '__main__':
    sys.exit(main())
