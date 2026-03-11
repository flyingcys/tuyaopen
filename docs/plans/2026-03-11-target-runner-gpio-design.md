# Target Runner GPIO Design

**Context**

The repository already has:

- stable Host unit tests
- a minimal `test_app/unit_test_app`
- a minimal `pytest` target runner skeleton

The next missing layer is not more Host coverage. It is the first real Target-side suite with a runner protocol that can work both:

- locally on `LINUX`
- later on real boards over serial

## Recommended Approach

Adopt a command-driven target test protocol with a suite registry.

The target app should support:

- listing registered suites
- running all suites
- running one named suite

The local `LINUX` path should invoke this protocol through process arguments. The real-board path should expose the same protocol through CLI commands so `pytest` can drive it over serial without changing suite semantics.

## Alternatives Considered

### 1. Hardcode a single board and serial flow

Fastest if board type, flashing tool, and wiring are already fixed.

Rejected because the current repository docs do not define one required board or one mandatory flash tool, so hardcoding would guess wrong too easily.

### 2. Keep only local process execution

Lowest risk in the short term.

Rejected because it would leave the new `Target` path unable to expand into real hardware validation, which is exactly the current gap.

### 3. Command-driven suite registry with pluggable runner

Recommended.

It keeps the first batch small enough to verify on `LINUX`, but uses the same suite names and control flow that future serial/HIL execution will need.

## Architecture

### Target app

`test_app/unit_test_app` becomes a small harness with:

- a suite registry
- per-suite run functions
- argument parsing for local `LINUX` execution
- optional CLI command registration for non-Linux targets

The default no-argument behavior remains a simple smoke run so the existing `test_smoke.py` stays valid.

### Target suites

The first real suite is `tkl_gpio`.

This suite should focus on adapter-level behavior that is valid even without external wiring:

- invalid pin rejection
- null parameter rejection
- basic deinit/read/irq API contract checks

If the environment exposes a usable GPIO device later, the suite can grow positive-path cases without changing the runner design.

### Pytest runner

`pytest` needs two invocation modes:

- local process mode: spawn the built `LINUX` executable with suite arguments
- serial mode: send `ut_run <suite>` or `ut_list` through the target CLI

The same suite names must be used in both modes.

### Flash runner

`flash_runner.py` should stop being a pure placeholder and support a generic command-template interface via environment variables. That avoids baking one board toolchain into the repository.

## Error Handling

- No local built binary: `pytest` skips with a direct reason
- No flash command configured for a real target: `pytest` skips or fails clearly before serial assertions
- Unknown suite name: target app prints a deterministic error and returns non-zero
- No GPIO device on Linux: only environment-dependent positive-path checks may skip; contract tests must still run

## Verification

This batch is complete only if all of the following are true:

1. `cd test_app/unit_test_app && tos.py check && tos.py build`
2. `python -m pytest tests/target/pytest -m target -v`
3. `test_tkl_gpio.py` is no longer a placeholder skip in local `LINUX` mode

## Expected Outcome

After this batch:

- the target app is suite-addressable instead of smoke-only
- `pytest` can choose suites by name
- `tkl_gpio` becomes the first real Target-side suite
- the repository has a reusable pattern for later `tkl_uart`, `tkl_flash`, and board-specific HIL expansion
