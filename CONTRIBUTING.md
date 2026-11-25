# Contributing

Thanks for looking. The project is small and deliberately simple, so the best contributions are bug reports with a minimal reproduction, fixes with a test, themes, and a port of the backend to another board.

## Build and test

```sh
cmake -S . -B build/gcc -G Ninja -DCMAKE_C_COMPILER=gcc -DCYDUI_SANITIZE=ON
cmake --build build/gcc && ctest --test-dir build/gcc            # all tests, with ASan and UBSan
cmake -S . -B build/clang -G Ninja -DCMAKE_C_COMPILER=clang -DCYDUI_SANITIZE=ON
cmake --build build/clang && ctest --test-dir build/clang        # also builds the libFuzzer harnesses
cmake -S . -B build/o2 -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Release
cmake --build build/o2 && ctest --test-dir build/o2              # the goldens must not depend on optimization
python3 tools/check_docs.py                                       # every public function is documented
tools/gen_fonts.sh --check                                        # the bundled fonts match the generator (needs tools/.venv)
```

A change should pass all of these. There is no hosted CI on purpose: the checks run on your machine (the maintainer keeps a local script that chains them, adds a short fuzz run of every harness and the static checks: no `malloc`, no mutable globals in the core, `gcc -fanalyzer`, `clang-tidy`).

## Rules of the code
- C99, `-pedantic -Wall -Wextra -Wshadow -Wcast-qual -Werror`; the core uses nothing but `string.h`, `stdint.h`, `stddef.h` and `stdbool.h`.
- **No `malloc`, no mutable globals in the core** (everything lives in the structs the application owns), **no `float` in the pixel path**: the same events and clock must give the same pixels on the PC and on the board.
- Every public function has a documentation comment in its header; the public API is frozen by `tests/test_api_surface.c` (see `docs/11-api.md` before changing a signature).
- Keep it simple. A feature earns its place by what it costs on a board with 320 KB of RAM.
- Everything in the repository is in English: code, comments, docs, the texts of the examples and the tests.

## Tests
A bug fix comes with a test that fails without it. A rendering change comes with its golden: look at the PNGs in `build/golden/` before running `CYDUI_UPDATE_GOLDEN=1 ./build/gcc/tests/test_golden`. Code that takes data from outside (text, events, bitmaps, files) gets a fuzz harness or an extension of one (`tests/fuzz_*.c`, with seeds in `tests/fuzz_seeds/` made by `tools/make_fuzz_seeds.py`).

## Commits
One logical change per commit, a message that says what and why. Do not add attribution trailers for tools.
