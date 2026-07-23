* Minimise token usage

* Don't poll or re-read: For background tasks, wait for completion once rather than repeatedly reading output files.

* Skip redundant verification: After a tool succeeds without error, don't re-read the result to confirm.

* Match verbosity to task complexity: Routine ops (merge, deploy, simple file edits) need minimal commentary. Save detailed explanations for complex logic, architectural decisions, or when asked.

* Do test driven development, when unit tests are appropriate, first write the test that defines the expected behavior, then implement the code to pass the test.

* Do not write superfluous tests, write meaningful unit tests and prefer more end-to-end tests.

* Start new chat sessions with "AGENTS.md read!"

* refer to readme.md for build instructions

* IMPORTANT: always use a DEBUG build for all development, compilations, and testing. Run `meson setup build --reconfigure --buildtype=debug` initially, then `meson compile -C build -j 6 $TARGET` for targeted builds.

* IMPORTANT: limit number of concurrent build jobs to 6

* IMPORTANT: never commit changes without the user's explicit approval.

* Only the `krisp` application must be maintained. Changes may break the other applications.

* When commiting, refer to `.codex/agents/commit.toml` for spawning a subagent
