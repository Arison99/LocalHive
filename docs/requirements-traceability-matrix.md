# LocalHive Requirements Traceability & Verification Matrix
**Document ID:** `DOC-LH-RTM-001`  
**Task Reference:** `TASK LH-ARCH-002` (Requirements-to-Test Matrix)  
**Date:** 2026-10-12  
**Status:** Verification Baseline Approved

---

## 1. Overview & Verification Method Taxonomy

This traceability matrix maps every architectural requirement, storage invariant, security control, and operational guarantee defined in `DOC-LH-ARCH-002-REV3` to its future automated verification method.

### Verification Methods:
* **`UT` (Unit Test):** Deterministic, sub-millisecond execution testing isolated components.
* **`IT` (Integration Test):** Multi-component execution exercising live SQLite instances, filesystem hierarchies, and TCP sockets.
* **`FI` (Fault Injection):** Simulated network resets, synthetic crash injections, and disk full conditions.
* **`ST` (Stress & Concurrency):** High-concurrency load testing executed under ThreadSanitizer (TSan).
* **`SEC` (Security & Fuzzing):** Adversarial payload injection, LibFuzzer runs, timing-attack analysis, and ACL permission checks.
* **`INT` (Interoperability):** End-to-end execution utilizing official AWS CLI, Python Boto3, and database driver clients.

---

## 2. Requirements-to-Test Traceability Matrix

### 2.1. S3 Storage & Durability Requirements

| Req ID | Requirement Statement | Source Document | Test Method | Target Test Suite | Concrete Acceptance Criteria |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **REQ-STOR-01** | Incomplete/aborted uploads are never committed or visible (Invariant 1). | `DOC-LH-STORAGE-004-REV3` | `FI` | `test_storage_fault_injection` | Disconnect client at 50% payload; assert staging file aborted, `active_gen_id` untouched, `GetObject` returns 404. |
| **REQ-STOR-02** | Replacement failure preserves prior object completely (Invariant 2). | `DOC-LH-STORAGE-004-REV3` | `FI` | `test_storage_fault_injection` | Inject crash before SQLite commit; restart server; assert `GetObject` returns byte-for-byte identical prior object. |
| **REQ-STOR-03** | Readers holding active leases see immutable generations during overwrite (Invariant 3). | `DOC-LH-STORAGE-004-REV3` | `ST` | `test_storage_concurrency` | 50 concurrent readers stream object while 5 writers continuously overwrite; assert 0 readers encounter corrupt or torn payload. |
| **REQ-STOR-04** | Committed metadata never references missing physical files (Invariant 4). | `DOC-LH-STORAGE-004-REV3` | `FI` | `test_storage_recovery` | Inject crash during `PREPARED` (file in data/, not committed); restart; verify scanner removes uncommitted file without data loss. |
| **REQ-STOR-05** | Garbage collection never prunes files under active reading (Invariant 5). | `DOC-LH-STORAGE-004-REV3` | `IT` | `test_storage_gc` | Trigger GC sweep while reader lease is held; verify physical file remains intact until lease expires. |
| **REQ-STOR-06** | `Strict` durability guarantees power-loss resilience. | `DOC-LH-STORAGE-004-REV3` | `IT` | `test_durability_profiles` | Validate `FlushFileBuffers` / `fdatasync` and `fsync(dir_fd)` called for every upload; SQLite WAL sync is `FULL`. |
| **REQ-STOR-07** | `Standard` durability guarantees process-crash resilience. | `DOC-LH-STORAGE-004-REV3` | `FI` | `test_durability_profiles` | Execute `kill -9` immediately following HTTP 200; restart daemon; assert uploaded object is present and valid. |
| **REQ-STOR-08** | Serialized SQLite write loop avoids `SQLITE_BUSY` errors. | `DOC-LH-STORAGE-004-REV3` | `ST` | `test_metadata_sqlite_stress` | 100 concurrent threads execute metadata writes; assert 0 queries fail with `SQLITE_BUSY` or locked error. |
| **REQ-STOR-09** | WAL checkpoints execute without blocking concurrent readers. | `DOC-LH-STORAGE-004-REV3` | `IT` | `test_metadata_sqlite_wal` | Trigger `SQLITE_CHECKPOINT_PASSIVE` during continuous reader queries; assert readers experience zero timeouts or deadlocks. |

---

### 2.2. Concurrency, Execution & Resource Limits

| Req ID | Requirement Statement | Source Document | Test Method | Target Test Suite | Concrete Acceptance Criteria |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **REQ-CONC-01** | Thread-per-core event loop model isolates socket I/O without lock contention. | `DOC-LH-CONTRACT-003-REV3` | `ST` | `test_core_net_event_loop` | Run 10,000 HTTP requests across N cores under ThreadSanitizer; assert zero data races or thread contention deadlocks. |
| **REQ-CONC-02** | Work queue saturation returns HTTP 503 `SlowDown`. | `DOC-LH-CONTRACT-003-REV3` | `ST` | `test_core_backpressure` | Saturate work queue past 1024 tasks; assert excess requests receive `HTTP/1.1 503 Service Unavailable` with XML `SlowDown`. |
| **REQ-CONC-03** | Global queued payload memory is bounded to 64 MB. | `DOC-LH-CONTRACT-003-REV3` | `ST` | `test_core_memory_bounds` | Flood server with 100 concurrent slow-consuming uploads; assert memory consumption remains capped at configured limit. |
| **REQ-CONC-04** | Staging disk quota saturation triggers HTTP 503 rejection. | `DOC-LH-CONTRACT-003-REV3` | `IT` | `test_staging_disk_quota` | Fill `.staging/` beyond 85% watermark; assert subsequent PUT requests rejected with HTTP 503 `SlowDown`. |
| **REQ-CONC-05** | Graceful shutdown drains active requests within timeout. | `DOC-LH-CONTRACT-003-REV3` | `IT` | `test_lifecycle_drain` | Send `SIGTERM` during active 5 MB upload; verify upload completes successfully before daemon exits. |

---

### 2.3. DB Proxy (Transparent L4) Requirements

| Req ID | Requirement Statement | Source Document | Test Method | Target Test Suite | Concrete Acceptance Criteria |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **REQ-PROX-01** | Exclusive 1:1 client-to-upstream socket binding without sharing. | `DOC-LH-CONTRACT-003-REV3` | `IT` | `test_proxy_l4_forwarder` | Establish connection; assert leased socket is dedicated; verify socket closes on client disconnect and is not reused. |
| **REQ-PROX-02** | Raw TCP pre-warmed pool replenishes idle sockets asynchronously. | `DOC-LH-CONTRACT-003-REV3` | `IT` | `test_proxy_raw_tcp_pool` | Lease 5 pre-warmed sockets; assert pool replenishes to target size; assert dead sockets discarded on peek validation. |
| **REQ-PROX-03** | Bidirectional TCP backpressure pauses reads when writes stall. | `DOC-LH-CONTRACT-003-REV3` | `IT` | `test_proxy_backpressure` | Throttle upstream receiver; assert proxy pauses reading from downstream client; assert proxy memory does not grow unbounded. |
| **REQ-PROX-04** | TCP half-close (`FIN`) propagates correctly in both directions. | `DOC-LH-CONTRACT-003-REV3` | `IT` | `test_proxy_half_close` | Client sends `SHUT_WR`; assert upstream receives `SHUT_WR`; assert client can still receive remaining upstream response. |
| **REQ-PROX-05** | Upstream connection loss tears down client connection immediately. | `DOC-LH-CONTRACT-003-REV3` | `FI` | `test_proxy_upstream_reset` | Kill upstream server process; assert client socket receives immediate RST; assert proxy does NOT auto-retry transaction. |
| **REQ-PROX-06** | Transparent TLS pass-through functions without payload inspection. | `DOC-LH-CONTRACT-003-REV3` | `INT` | `test_proxy_tls_passthrough` | Connect PostgreSQL client via SSL through proxy; assert successful SSL handshake and query execution. |

---

### 2.4. Cryptography & Security Requirements

| Req ID | Requirement Statement | Source Document | Test Method | Target Test Suite | Concrete Acceptance Criteria |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **REQ-SEC-01** | Zero-dependency SHA-256 complies with RFC 6234 standard vectors. | `DOC-LH-CONTRACT-003-REV3` | `UT` | `test_crypto_sha256` | Execute against all official NIST / RFC 6234 test vectors; assert 100% byte-for-byte match. |
| **REQ-SEC-02** | Zero-dependency HMAC-SHA256 complies with RFC 4231 vectors. | `DOC-LH-CONTRACT-003-REV3` | `UT` | `test_crypto_hmac` | Execute against all 7 test vectors in RFC 4231; assert 100% match. |
| **REQ-SEC-03** | Constant-time equality comparison prevents timing side-channels. | `DOC-LH-CONTRACT-003-REV3` | `SEC` | `test_crypto_timing` | Audit with Valgrind / CTGRIND; assert execution time has zero correlation with differing byte positions. |
| **REQ-SEC-04** | Windows Named Pipe DACL rejects unprivileged user processes. | `DOC-LH-CONTRACT-003-REV3` | `SEC` | `test_admin_ipc_security_win` | Connect to pipe as standard non-admin user; assert `CreateFileW` fails with `ERROR_ACCESS_DENIED`. |
| **REQ-SEC-05** | Linux Unix Domain Socket rejects non-owner processes. | `DOC-LH-CONTRACT-003-REV3` | `SEC` | `test_admin_ipc_security_posix` | Attempt connection from different UID; assert `connect()` fails with `EACCES`. |
| **REQ-SEC-06** | Loopback HTTP endpoint rejects requests missing valid Bearer token. | `DOC-LH-CONTRACT-003-REV3` | `SEC` | `test_admin_http_auth` | Request `/metrics` without token or with invalid token; assert `HTTP 401 Unauthorized`. |
| **REQ-SEC-07** | Sensitive data redaction in logging and telemetry. | `DOC-LH-CONTRACT-003-REV3` | `UT` | `test_logging_redaction` | Log requests containing AWS Secret Keys and DB passwords; assert values replaced with `[REDACTED]`. |

---

### 2.5. S3 API Compatibility & Interoperability

| Req ID | Requirement Statement | Source Document | Test Method | Target Test Suite | Concrete Acceptance Criteria |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **REQ-S3-01** | S3 Path-Style and Virtual-Hosted routing resolve correctly. | `DOC-LH-S3MAP-005-REV2` | `UT` | `test_s3_router` | Test `/b/k`, `Host: b.localhost/k`, trailing slashes, URL encodings; assert accurate extraction. |
| **REQ-S3-02** | AWS Signature Version 4 validates authentic requests. | `DOC-LH-S3MAP-005-REV2` | `UT` | `test_s3_sigv4` | Validate canonicalization and HMAC against AWS official SigV4 test suite. |
| **REQ-S3-03** | S3 XML error envelopes match AWS specification. | `DOC-LH-S3MAP-005-REV2` | `UT` | `test_s3_xml` | Validate XML response structure for `NoSuchKey`, `NoSuchBucket`, `SlowDown`. |
| **REQ-S3-04** | ListObjectsV2 supports pagination, prefixes, and delimiters. | `DOC-LH-S3MAP-005-REV2` | `IT` | `test_s3_list_v2` | List 2,500 keys with `max-keys=1000` and `delimiter='/'`; assert correct `NextContinuationToken` and `CommonPrefixes`. |
| **REQ-S3-05** | Single byte-range requests return HTTP 206 Partial Content. | `DOC-LH-S3MAP-005-REV2` | `IT` | `test_s3_ranges` | Issue `Range: bytes=100-199`; assert HTTP 206, `Content-Range: bytes 100-199/size`, exact 100 bytes returned. |
| **REQ-S3-06** | Official AWS CLI executes bucket and object commands. | `DOC-LH-S3MAP-005-REV2` | `INT` | `test_aws_cli_harness` | Execute `aws s3 mb`, `aws s3 cp`, `aws s3 ls`, `aws s3 rm` against custom endpoint; assert zero errors. |
| **REQ-S3-07** | Python Boto3 client executes object workflows. | `DOC-LH-S3MAP-005-REV2` | `INT` | `test_boto3_harness` | Upload and download 50 MB file via Boto3; assert MD5 checksum matches. |
