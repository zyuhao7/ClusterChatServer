# Scenario: Deterministic C++ database fixture
- Given: `cluster_chat_unit` includes database-backed model tests and a reachable local MySQL instance
- When: the binary starts a suite run
- Then: it creates an isolated test schema from a dedicated SQL template and points runtime config at that schema automatically

## Test Steps

- Case 1 (happy path): bootstrap creates a clean schema and `UserModal` persists/query data inside the isolated schema
- Case 2 (edge case): table reset runs before the next database case and leaves zero residual rows, including auto-increment-sensitive tables
- Case 3 (repeatability): repeated suite execution starts from a clean schema without requiring manual `mysql < ...` commands

## Status
- [x] Write scenario document
- [ ] Write solid test according to document
- [ ] Run test and watch it failing
- [ ] Implement to make test pass
- [ ] Run test and confirm it passed
- [ ] Refactor implementation without breaking test
- [ ] Run test and confirm still passing after refactor
