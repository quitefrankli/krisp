* Minimise token usage without omitting material findings, risks, or verification.

* Prioritise simple, clear designs and clean, maintainable code. Do not knowingly
  introduce avoidable complexity, duplication, weak abstractions, shortcuts, or
  technical debt. If a requested change appears to require a material reduction
  in design or code quality, stop before making that compromise, explain the
  trade-off, and ask the user for explicit approval.

* Keep tool output high-signal:
  - Search narrowly (`rg -l` first, then inspect only relevant ranges).
  - Use small output limits for searches and successful builds; expand output only after a failure.
  - Prefer structured parsing over dumping binary or generated files.
  - Run successful test suites with concise output (for GoogleTest, use `--gtest_brief=1`); rerun only failures with full diagnostics.

* Do not busy-poll or repeatedly re-read output. For background tasks, use the available wait mechanism and wait for completion.

* Do not repeat an identical successful check. Still inspect diffs and run tests proportionate to the change.

* Match verbosity to task complexity: Routine ops (merge, deploy, simple file edits) need minimal commentary. Save detailed explanations for complex logic, architectural decisions, or when asked.

* Do not write superfluous tests. Prefer the lowest-level test that meaningfully verifies externally observable behaviour; use end-to-end tests for cross-component behaviour.

* Start new chat sessions with "AGENTS.md read!" but only do this once.

* Refer to `docs/` for additional documentation and design notes.

* Krisp is in early development; do not preserve legacy APIs, save formats, or backwards compatibility unless explicitly requested.

* IMPORTANT: never commit changes without first consulting the user. Commit messages must use a descriptive imperative subject and, when the change is non-trivial, a wrapped body with summary of changes. Use one of the following appropriate commit title prefixes: [FEATURE], [BUGFIX], [REFACTOR], [OTHER]

* Preserve unrelated staged and unstaged changes. Never revert or overwrite user changes.

* Only the `krisp` application must be maintained. Other applications are out of scope; do not spend additional effort maintaining them unless required to build `krisp`.

* Ray tracing is currently unsupported. Keep its C++ and shader build paths disabled unless the user explicitly asks to restore and repair it.

* When making materially performance-relevant changes, add or update the relevant notes in `docs/PERFORMANCE.md`. Ignore negligible constant-time bookkeeping and similarly insignificant performance effects.

# Building

* build with `meson compile -C build -j 6 krisp`
* run tests with `meson test -C build -j 6`
* if there a are dependency/build-config changes or if the build directory needs to be cleanedup, use the following sequence (and always use debug build)
```
conan install . -pr=conan_clang_profile --build=missing
meson setup build --reconfigure --native-file build/conan/conan_meson_native.ini --buildtype=debug
meson compile -C build -j 6 krisp
```