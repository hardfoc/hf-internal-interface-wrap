# Testing and CI

How the library is checked: host unit tests, compiled documentation, ESP32 builds and static analysis, on every
pull request.

## Host unit tests

The host tests build the MCU-independent code with GCC or Clang, warnings as errors, and run it without
hardware.

```bash
cmake -S tests/host -B build/host -G Ninja
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

| File | Covers |
| --- | --- |
| [`tests/host/test_can_frame.cpp`](../tests/host/test_can_frame.cpp) | `hf_can_message_t` (BaseCan v2): lengths, DLC codes, flags, validation, conversion to the transport-neutral frame layout |
| [`tests/host/test_spsc_ring.cpp`](../tests/host/test_spsc_ring.cpp) | `HfSpscRing`: capacity rounding, full and empty, overruns, counter wrap, a producer and a consumer thread |
| [`tests/host/HostTest.h`](../tests/host/HostTest.h) | The minimal test framework |

`-DHF_HOST_SANITIZE=address` adds AddressSanitizer and UBSan; `-DHF_HOST_SANITIZE=thread` adds
ThreadSanitizer, which the ring's two-thread test is written for.

## Documentation snippets

Every multi-line C++ block on this site is included from [`tests/docs/`](../tests/docs/quickstart.cpp), which the
host build compiles (target `hf_iiw_doc_snippets`) with the same warnings. A change to an interface that breaks a
page breaks the build first. The README's example is a copy of the Quickstart snippet that
`hardfoc-docs gen --check` keeps in step.

## CI workflows

| Workflow | What it checks |
| --- | --- |
| Host tests | GCC and Clang, plus AddressSanitizer and ThreadSanitizer builds: unit tests and doc snippets |
| ESP32 Examples Build CI | Every app in `examples/esp32/app_config.yml` for each listed ESP-IDF version and build type |
| C++ Lint CI, C++ Analysis CI | clang-format, clang-tidy and cppcheck |
| Docs | Strict site build, figure and generated-table drift, Doxygen, link check, Lighthouse budget |
| Markdown Lint CI, YAML Lint CI, Docs Link Check CI | Repository text files |

The workflows call the shared, versioned workflows of
[hf-general-ci-tools](https://github.com/hardfoc/hf-general-ci-tools) and
[hf-espidf-ci-tools](https://github.com/hardfoc/hf-espidf-ci-tools). Nothing builds the STM32 sources yet (see
[STM32](stm32.md)).

## On hardware

The [ESP32 examples](examples.md) are the hardware test suite: each prints a pass / fail summary over the serial
monitor. Run `can_test` after a CAN change, with a transceiver and a second node or loopback mode.
