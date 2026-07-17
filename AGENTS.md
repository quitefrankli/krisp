* Minimise token usage

* Don't poll or re-read: For background tasks, wait for completion once rather than repeatedly reading output files.

* Skip redundant verification: After a tool succeeds without error, don't re-read the result to confirm.

* Match verbosity to task complexity: Routine ops (merge, deploy, simple file edits) need minimal commentary. Save detailed explanations for complex logic, architectural decisions, or when asked.

* Don't add any readme.md files or other documentation files without first prompting the user.

* Do test driven development, when unit tests are appropriate, first write the test that defines the expected behavior, then implement the code to pass the test.

* Do not write superfluous tests, do write unit tests but make sure they are meaningful.

* start every new session with "AGENTS.md read!"

* refer to readme.md for build instructions

* IMPORTANT: always use a DEBUG build for all development, compilations, and testing. Run `meson setup build --reconfigure --buildtype=debug` initially, then `meson compile -C build $TARGET` for targeted builds.

* IMPORTANT: never commit changes without the user's explicit approval.

* Commit messages must use a descriptive imperative subject and, when the change is non-trivial, a wrapped body explaining the main behavior and important constraints.

* limit number of concurrent build jobs to 6
