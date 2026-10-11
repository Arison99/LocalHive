# LocalHive Architecture Issue Register
**Document ID:** `DOC-LH-ISSUE-001`  
**Task Reference:** `TASK LH-ARCH-002` (Issue Register)  
**Date:** 2026-10-12  
**Status:** Closed / Resolved Baseline

---

## 1. Overview & Classification Schema

This register tracks all architectural contradictions, missing contracts, unsupported guarantees, and operational risks identified across the LocalHive codebase and documentation.

* **Status Categories:**
  * `RESOLVED`: Addressed with an authoritative decision in `DOC-LH-ARCH-002-REV3` and corresponding ADR.
  * `DEFERRED`: Formally scoped to a future milestone (e.g., Post-v1.0).
  * `OPEN`: Awaiting external dependency or operational decision.

---

## 2. Comprehensive Issue Register

| Issue ID | Category | Summary of Issue | Architectural Decision | Rationale | Target Milestone | Unresolved Dependencies |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **ISS-001** | Build / CI | Missing root `CMakeLists.txt` causes root CI workflows to fail. | Introduce top-level CMakeLists.txt orchestrating modular targets. | Allows unified build across platforms without breaking subdirectory builds. | Milestone 0 | None. Standard CMake 3.16+. |
| **ISS-002** | Test Link | `s3_bucket_tests` bundles 3 files each with `int main()`, failing link. | Split into 3 independent test executables and register with CTest. | Preserves existing test code; enables automated CTest execution. | Milestone 0 | None. |
| **ISS-003** | Storage / Invariant | Invariant 4 race: SQLite committed before physical file moved from staging. | Re-order lifecycle: Physical move to unique path occurs in `PREPARED` *before* SQLite commit. | Guarantees committed metadata never references a missing physical file. | Milestone 2 (ADR-006) | None. |
| **ISS-004** | Durability | `synchronous = NORMAL` in SQLite WAL allows rollback across power loss. | Introduce 3 explicit profiles: `Strict`, `Standard`, and `Development`. | Balances power-loss resilience against ephemeral test throughput. | Milestone 2 (ADR-007) | None. |
| **ISS-005** | DB Proxy | Conflation of transparent L4 TCP forwarding with L7 session pooling. | Restrict Phase 1 to transparent 1:1 L4 stream forwarding; defer L7 pooling. | Prevents transaction state interleaving and session corruption. | Milestone 4 (ADR-008) | Wire protocol parsers (Phase 2). |
| **ISS-006** | Cryptography | External OpenSSL dependency causes build friction on Windows MSVC. | Retain zero-dependency C++17 RFC-compliant crypto with strict test gates. | Frictionless Windows builds; verified via RFC vectors and fuzzing. | Milestone 1 (ADR-009) | Cryptographic audit review. |
| **ISS-007** | Concurrency | Task count alone used for queue bounds; memory bounds unconstrained. | Enforce dual bounds: task count (1024) and global payload memory (64 MB); HTTP 503 SlowDown. | Prevents process OOM under high payload ingestion volume. | Milestone 1 (ADR-010) | None. |
| **ISS-008** | Security | Administrative control plane lacked OS access control and token auth. | Windows Named Pipe DACL (Admins/SYSTEM), Linux socket (0600), Bearer token on HTTP. | Prevents local unprivileged user privilege escalation. | Milestone 5 (ADR-011) | None. |
| **ISS-009** | Compliance | Unqualified claims of universal AWS S3 operational equivalence. | Define scope as S3 API Compatibility; explicitly disclaim distributed equivalence. | Establishes realistic engineering boundaries and testable scope. | Milestone 3 (ADR-012) | AWS CLI / SDK interop tests. |
| **ISS-010** | High Availability | Unsafe automatic database failover risking split-brain data corruption. | Prohibit automatic proxy primary failover; drop client socket on upstream reset. | Only external consensus coordinators can safely execute primary failover. | Milestone 4 | Consensus coordinator (Out of scope). |
| **ISS-011** | Portability | Heavy Boost library dependencies on Windows. | Use header-only / standalone Boost.Asio; vendor SQLite amalgamation. | Guarantees zero-friction developer setup on clean Windows machines. | Milestone 1 (ADR-001, ADR-002) | None. |
| **ISS-012** | Database Scope | Ambiguity regarding whether SQLite is an upstream network database target. | Explicitly document SQLite as an internal embedded library, never a network upstream. | SQLite has no wire protocol; proxying over TCP is technically invalid. | Milestone 4 | None. |

---

## 3. Unresolved Architectural Dependencies & Open Questions

1. **DEP-001: Cryptographic Correctness Sign-off:**  
   * *Status:* Awaiting Milestone 1 Implementation.  
   * *Dependency:* The zero-dependency SHA-256 and HMAC-SHA256 modules require passing RFC 4231 and RFC 6234 test vector suites and fuzz testing prior to Milestone 3 (S3 SigV4) integration.
2. **DEP-002: Windows NTFS POSIX Delete Semantics Compatibility:**  
   * *Status:* Verified in specification; pending Win32 runtime validation on Windows Server / Windows 10 versions.  
   * *Dependency:* Uses `FILE_DISPOSITION_FLAG_POSIX_SEMANTICS` supported on Windows 10 version 1709+. Fallback path for older Windows versions requires moving open obsolete files to a `.recycle/` quarantine folder.
3. **DEP-003: Linux systemd Credential Storage:**  
   * *Status:* Deferred to Milestone 5.  
   * *Dependency:* Administrative secret token provisioning can leverage systemd `LoadCredential=` on modern Linux distributions (systemd v247+), falling back to mode 0400 file storage.
