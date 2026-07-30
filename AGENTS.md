* Minimise token usage without omitting material findings, risks, or verification.

* Keep tool output high-signal:
  - Search narrowly (`rg -l` first, then inspect only relevant ranges).
  - Use small output limits for searches and successful builds; expand output only after a failure.
  - Prefer structured parsing over dumping binary or generated files.
  - Run successful test suites with concise output (for GoogleTest, use `--gtest_brief=1`); rerun only failures with full diagnostics.

* Do not busy-poll or repeatedly re-read output. For background tasks, use the available wait mechanism and wait for completion.

* Do not repeat an identical successful check. Still inspect diffs and run tests proportionate to the change.

* Match verbosity to task complexity: Routine ops (merge, deploy, simple file edits) need minimal commentary. Save detailed explanations for complex logic, architectural decisions, or when asked.

* Use test-driven development when unit tests are appropriate: first write the test that defines the expected behaviour, then implement the code to pass it.

* Do not write superfluous tests. Prefer the lowest-level test that meaningfully verifies externally observable behaviour; use end-to-end tests for cross-component behaviour.

* Start new chat sessions with "AGENTS.md read!" but only do this once.

* Refer to README.md for build instructions.

* IMPORTANT: For a new, missing, or stale build directory, or after dependency or build-configuration changes, always use the following sequence to set up dependencies and configure a DEBUG Clang build:
  1. `conan install . -pr=conan_clang_profile --build=missing`
  2. `meson setup build --reconfigure --native-file build/conan/conan_meson_native.ini --buildtype=debug`
  3. `meson compile -C build -j 6 krisp`

* For subsequent targeted builds, run `meson compile -C build -j 6 $TARGET`.

* IMPORTANT: limit number of concurrent build jobs to 6

* IMPORTANT: never commit changes without the user's explicit approval.

* Preserve unrelated staged and unstaged changes. Never revert or overwrite user changes.

* Only the `krisp` application must be maintained. Other applications are out of scope; do not spend additional effort maintaining them unless required to build `krisp`.

* Ray tracing is currently unsupported. Keep its C++ and shader build paths disabled unless the user explicitly asks to restore and repair it.

* When making materially performance-relevant changes, add or update the relevant notes in `docs/PERFORMANCE.md`. Ignore negligible constant-time bookkeeping and similarly insignificant performance effects.

* When the user explicitly approves a commit, use the subagent configured in `.codex/agents/commit.toml`.
