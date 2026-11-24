# Third-Party Licenses & Attribution

This repository's original source code (`tools/tg2_lifter.py`, runtime subsystems in `src/` and `include/`, menu UI, tests, and build scripts) is licensed under the **PolyForm Noncommercial License 1.0.0** ([`LICENSE`](LICENSE)).

The notices below apply to third-party components used or statically linked by this project.

---

## 1. snesrecomp

Used as the static recompilation framework, W65C816 decoder foundation, and SNES core (`snesrecomp_repo/` submodule).

- **Upstream:** <https://github.com/RetroPortingToolKit/snesrecomp>
- **License:** PolyForm Noncommercial License 1.0.0 (`snesrecomp_repo/LICENSE`)
- **Attribution file:** `snesrecomp_repo/THIRD_PARTY_ATTRIBUTION.md`

---

## 2. snesrev (zelda3 / smw)

The SNES APU/SPC700/DSP runtime architecture and hardware models under `snesrecomp_repo/runner/src/snes/` derive from `snesrev`.

- **Upstream:** <https://github.com/snesrev/zelda3>, <https://github.com/snesrev/smw>
- **License:** MIT

```text
Copyright (c) 2022 snesrev
Copyright (c) 2023 snesrev
Copyright (c) 2021 elzo_d

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## 3. LakeSnes — 65816 Interpreter Fallback Core

`snesrecomp_repo/runner/src/snes/interp816.c` and `interp816.h`, used as the dynamic fallback tier alongside statically lifted functions, derive from LakeSnes.

- **Upstream:** <https://github.com/angelo-wf/lakesnes>
- **License:** MIT

```text
Copyright (c) 2021-2023 angelo_wf and contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## 4. Simple DirectMedia Layer (SDL2) — Runtime Dependency (Not Bundled)

The desktop runner dynamically loads `SDL2.dll` (Windows), `libSDL2-2.0.so.0` (Linux), or `libSDL2-2.0.0.dylib` (macOS) at runtime. No SDL2 source code or binary is redistributed in this repository.

- **Upstream:** <https://github.com/libsdl-org/SDL>
- **License:** zlib License

---

## 5. SDL_GameControllerDB (`gamecontrollerdb.txt`)

`gamecontrollerdb.txt` is bundled from `mdqinc/SDL_GameControllerDB` so the SDL2 runner supports community controller mappings out of the box.

- **Upstream:** <https://github.com/mdqinc/SDL_GameControllerDB>
- **License:** zlib License

```text
This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```
