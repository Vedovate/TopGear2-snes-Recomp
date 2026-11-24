# Top Gear 2 (SNES) - Native Static Recompilation Project

Este documento descreve a arquitetura, o ambiente de execução e as convenções para agentes de IA no projeto **Top Gear 2 (SNES) — Definitive Edition** (port nativo em C por recompilação estática).

---

## 1. Visão Geral

* **Alvo:** ROM `Top Gear 2 (USA)` (1 MB LoROM/FastROM — 1.048.576 bytes sem header, SHA-256 `76b2702c4be8b668c1017f2817c280283c275eaa41535bf6ffa2b8d2220b68c6`; headers SMC de 512 bytes são removidos automaticamente).
* **Direitos Autorais e Escopo:** Nenhuma ROM, asset protegido ou saída gerada em `generated/` é versionada no Git. O código gerado é produzido localmente pelo usuário via `tools/tg2_lifter.py`.
* **Abordagem:** Recompilação Estática W65C816 (`generated/` + `tools/tg2_lifter.py` com os 24 hooks de widescreen 16:9 no banco `$8B`) com fallback interpretado (`src/topgear2_interp.c`), renderizador PPU/HD/Widescreen (`src/topgear2_video.c`), APU SPC700+DSP em C puro (`snesrecomp_repo/runner/src/snes/`) e interface SDL2 carregada dinamicamente (`src/topgear2_runner.c`).
* **Estrutura Principal:**
  * `include/topgear2.h`, `include/topgear2_internal.h`, `include/topgear2_menu.h`, `include/topgear2_sha256.h`, `include/topgear2_platform.h`
  * `src/topgear2_sha256.c`, `src/topgear2_platform.c`, `src/topgear2_machine.c`, `src/topgear2_ppu_dma.c`, `src/topgear2_video.c`, `src/topgear2_scheduler.c`, `src/topgear2_audio.c`, `src/topgear2_runtime_api.c`, `src/topgear2_menu.c`, `src/topgear2_runner.c`
  * `tools/tg2_lifter.py` (lifting completo com CLI `argparse`, verificação SHA-256, hooks widescreen e modo `--stub` para CI sem ROM)
  * `tests/test_regressions.c`, `tests/test_tg2_boot.c`, `tests/test_definitive_edition.c`, `tests/test_race_and_money.c`, `tests/diag_frames.c`

---

## 2. Ambiente de Execução e Build

* **Sistema / Toolchain:** Windows x64 (MinGW-w64 `gcc`, `mingw32-make.exe`, `windres`), Linux (`gcc`, `make`) e macOS (`gcc`, `make`).
* **Lifting Local:** `mingw32-make.exe lift ROM="caminho/para/Top Gear 2 (USA).sfc"` (ou `python tools/tg2_lifter.py ...`).
* **Build Completo:** `mingw32-make.exe all` gera `topgear2.exe` (com ícone `topgear2.ico` via `topgear2.rc`) e `topgear2.dll` (no Unix: `topgear2` e `libtopgear2.so` / `libtopgear2.dylib`).
* **Build Sem ROM (CI / Testes Unitários / Release ROM-less):** `mingw32-make.exe NO_LIFT=1 all test_regressions.exe` compila com `generated/tg2_dispatch_stub.c` (`tg2_interp_step_with_hooks`, incluindo os 24 hooks de widescreen 16:9 sem código da ROM). Use `mingw32-make.exe release-romless` para montar `dist_topgear2/` pronta para distribuição limpa.
* **Execução e Seleção de ROM:** Requer `SDL2` no runtime (`SDL2.dll` no Windows). Sem `--rom` ou `TG2_ROM_PATH`, o runner busca nos diretórios de dados/executável ou abre o seletor nativo + Drag & Drop na janela.
* **Diretórios de Dados:** Usa `%APPDATA%\TopGear2Recomp` (Windows) ou `$XDG_DATA_HOME/topgear2-recomp` / `~/.local/share/topgear2-recomp` (Linux/macOS); use `--portable` ou crie `portable.txt` ao lado do executável para modo portátil.
* **Testes:** `.\test_regressions.exe` (unitários), `.\test_regressions.exe "Top Gear 2 (USA).sfc"`, `.\test_tg2_boot.exe`, `.\test_definitive_edition.exe`, `.\test_race_and_money.exe` e `.\topgear2.exe --rom "Top Gear 2 (USA).sfc" --headless --frames 120 --no-autoload`.

---

## 3. Subsistemas Críticos (Definitive Edition & Save States)

* **Save State v2 (`src/topgear2_runtime_api.c`):** Serializa CPU/WRAM/PPU/DMA/Scheduler, latches `ophct`/`opvct`, relógios da APU e SPC700/DSP/Timers/Portas via `apu_saveload` + `apu_clearPortQueue`. Usa backup em memória para rollback atômico em caso de erro de leitura/corrupção e valida versão `2`. Resumo JSON é exportado separadamente via `tg2_save_state_json_summary`.
* **Career Auto-Save (`src/topgear2_runner.c`):** Disparado exclusivamente durante corrida ativa (`ppu[0x07] == 0x78 && ppu[0x08] == 0x71 && (ppu[0x00] & 0x80) == 0`).
* **Menu `ESC` (`src/topgear2_menu.c`):** Configura controles (P1 + gamepad P2), vídeo (toggle instantâneo 4:3/16:9), pintura RGB dos carros (com cache em `src/topgear2_video.c`), áudio/rumble (rumble limitado à corrida ativa), seleção de ROM em runtime, nome do jogador (`1CF3`/`FE1C`/`01E4`) e unidade de velocidade (`KM/H` vs `MPH` em `1CFF`/`FE28`/`019A` e tiles HUD `CA9E`/`CA58`), persistindo em JSON com escaping seguro em `tg2_config.json`.

---

## 4. Licença, Memória e Registro de Decisões

* **Licença:** **PolyForm Noncommercial License 1.0.0** (`LICENSE`), com atribuições de terceiros em `THIRD_PARTY_LICENSES.md`.
* Registre decisões arquiteturais, invariantes de WRAM/PPU/APU e causas raiz de bugs no `ai-memory` (`memory_write_page`) sob `decisions/`, `architecture/` ou `gotchas/`.
* Mantenha `CLAUDE.md` apontando para `@AGENTS.md`.
* Mantenha `generated/`, `*.sfc`/`*.smc` e binários fora do repositório Git conforme `.gitignore` e verifique com `git ls-files`.
* **Envio ao GitHub (Push Manual Obrigatório):** Nunca execute `git push` automaticamente para o GitHub; envie alterações ao repositório remoto somente quando o usuário solicitar explicitamente.
