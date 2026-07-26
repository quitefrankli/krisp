* Minimise token usage

* Don't poll or re-read: For background tasks, wait for completion once rather than repeatedly reading output files.

* Skip redundant verification: After a tool succeeds without error, don't re-read the result to confirm.

* Match verbosity to task complexity: Routine ops (merge, deploy, simple file edits) need minimal commentary. Save detailed explanations for complex logic, architectural decisions, or when asked.

* Do test driven development, when unit tests are appropriate, first write the test that defines the expected behavior, then implement the code to pass the test.

* Do not write superfluous tests, write meaningful unit tests and prefer more end-to-end tests.

* Start new chat sessions with "AGENTS.md read!"

* Refer to README.md for build instructions.

* IMPORTANT: always use the following sequence to set up dependencies and configure a DEBUG Clang build:
  1. `conan install . -pr=conan_clang_profile --build=missing`
  2. `meson setup build --reconfigure --native-file build/conan/conan_meson_native.ini --buildtype=debug`
  3. `meson compile -C build -j 6 krisp`

* For subsequent targeted builds, run `meson compile -C build -j 6 $TARGET`.

* IMPORTANT: limit number of concurrent build jobs to 6

* IMPORTANT: never commit changes without the user's explicit approval.

* Only the `krisp` application must be maintained. Changes may break the other applications.

* When making performance-relevant changes, add or update the relevant notes in `docs/PERFORMANCE.md`.

* When commiting, refer to `.codex/agents/commit.toml` for spawning a subagent
