# LocalHive Repository Assessment & Baseline Inventory
**Document ID:** `DOC-LH-ASSESS-001`  
**Task Reference:** `TASK LH-ARCH-001` (Phase 1 Discovery)  
**Date:** 2026-10-11  
**Status:** Baseline Established / Evidence-Verified

---

## 1. Executive Summary

LocalHive is envisioned in its root documentation as a modular local development suite combining a genuinely S3-compatible object store with multi-database connector proxies (MongoDB, SQLite, Redis, MySQL, PostgreSQL, Cassandra, MariaDB).

A thorough code audit of the current repository reveals a significant divergence between the high-level project vision and the actual committed codebase:
1. **Implemented S3 Subsystem:** Located entirely under [`s3-Bucket/`](file:///e:/arison99/LocalHive/s3-Bucket). It provides an experimental, non-standard HTTP REST service using Boost.Asio, raw filesystem operations via `std::filesystem`, and embedded metadata tracking via `sqlite3`.
2. **Database Connectors / Proxies:** **Zero lines of code exist** for database connectors, wire protocol decoders, connection pooling, or proxy forwarding. SQLite is present strictly as an embedded local database file for S3 metadata.
3. **Build System & CI Inconsistencies:** The root repository lacks a top-level `CMakeLists.txt`. Both root GitHub Actions workflows ([`build.yml`](file:///e:/arison99/LocalHive/build.yml) and [`.github/workflows/cmake-multi-platform.yml`](file:///e:/arison99/LocalHive/.github/workflows/cmake-multi-platform.yml)) attempt to build at the root and inevitably fail. Inside [`s3-Bucket/CmakeLists.txt`](file:///e:/arison99/LocalHive/s3-Bucket/CmakeLists.txt), the test suite cannot be linked because three separate test translation units each define an `int main()` function.
4. **Protocol & Concurrency Violations:** The existing HTTP server uses custom URL query parameters (`/upload?bucket=...&key=...`) rather than S3 REST URI paths (`PUT /bucket/key`), lacks AWS SigV4 authentication, parses headers incompletely, drops TCP buffer fragments, spawns unconstrained detached threads (`std::thread::detach`), and writes uploaded files directly in place without atomic staging or rollback.

---

## 2. Directory Structure & Inventory

```
e:\arison99\LocalHive/
├── .github/
│   └── workflows/
│       └── cmake-multi-platform.yml # Multi-platform CI starter; points to root -S (Broken)
├── .vscode/
│   ├── c_cpp_properties.json        # References Intel oneAPI & Win32 paths
│   ├── settings.json
│   └── tasks.json                   # Hardcoded E:\Mingw\bin\gcc.exe build task
├── build.yml                        # Legacy GitHub action running root cmake . && make (Broken)
├── LICENSE                          # GNU General Public License v3.0
├── LocalHive.webp                   # Project branding image
├── README.md                        # Conceptual overview; fictitious LocalHive.h sample
├── S3-Bucket.md                     # Points to s3Bucket.png
├── s3Bucket.png                     # Architectural mockup diagram
└── s3-Bucket/
    ├── CmakeLists.txt               # CMake build file (target: s3_bucket, s3_bucket_tests)
    ├── Makefile                     # Linux GNU make build file
    ├── build.ps1                    # Windows PowerShell build script with direct MinGW fallback
    ├── README.md                    # S3 service documentation & manual curl examples
    ├── simple_test.cpp              # Standalone sanity test for BucketManager & FileOperations
    ├── test_client.py               # Python-based HTTP client test using requests
    ├── buckets/                     # Default storage root for buckets
    │   ├── bucket1/
    │   └── bucket2/
    ├── build/
    │   └── s3_bucket.exe            # Pre-compiled Windows binary artifact (5,945,751 bytes)
    ├── include/
    │   ├── bucket_manager.h         # BucketManager class declaration
    │   ├── file_operations.h        # FileOperations static helper declaration
    │   ├── metadata_manager.h       # MetadataManager SQLite wrapper declaration
    │   └── routes/
    │       ├── delete.h             # handleDelete declaration
    │       ├── download.h           # handleDownload declaration
    │       └── upload.h             # handleUpload declaration
    ├── lib/
    │   ├── bucket_manager.cpp       # BucketManager implementation
    │   ├── file_operations.cpp      # FileOperations implementation
    │   └── metadata_manager.cpp     # MetadataManager SQLite implementation
    ├── logs/                        # Empty runtime directory
    ├── server/
    │   ├── config.json              # Empty configuration stub
    │   ├── server.cpp               # Boost.Asio TCP server & dispatcher
    │   └── routes/
    │       ├── delete.cpp           # DELETE /delete route handler
    │       ├── download.cpp         # GET /download route handler
    │       └── upload.cpp           # POST /upload route handler
    └── tests/
        ├── bucket_test.cpp          # BucketManager unit test (has main())
        ├── file_test.cpp            # FileOperations unit test (has main())
        └── metadata_test.cpp        # MetadataManager unit test (has main())
```

---

## 3. Build Systems & Target Inventory

### 3.1. Root Workspace Level
* **Top-Level `CMakeLists.txt`:** **Missing**. No CMake build exists at workspace root `e:\arison99\LocalHive`.
* **CI Workflows:**
  * [`build.yml:L28-33`](file:///e:/arison99/LocalHive/build.yml#L28-L33): Executes `cmake .` and `make`, then runs `./LocalHiveTests`. Fails immediately because no CMakeLists.txt exists at root and no target named `LocalHiveTests` is defined anywhere in the repository.
  * [`.github/workflows/cmake-multi-platform.yml:L60-65`](file:///e:/arison99/LocalHive/.github/workflows/cmake-multi-platform.yml#L60-L65): Executes `cmake -B ${{ github.workspace }}/build -S ${{ github.workspace }}`. Fails on both Ubuntu and Windows for the same reason.

### 3.2. Subsystem Level (`s3-Bucket/`)
* **`s3-Bucket/CmakeLists.txt`** (Capitalized `C`):
  * **Target `s3_bucket` (Executable):**
    * Sources: [`lib/bucket_manager.cpp`](file:///e:/arison99/LocalHive/s3-Bucket/lib/bucket_manager.cpp), [`lib/file_operations.cpp`](file:///e:/arison99/LocalHive/s3-Bucket/lib/file_operations.cpp), [`lib/metadata_manager.cpp`](file:///e:/arison99/LocalHive/s3-Bucket/lib/metadata_manager.cpp), [`server/server.cpp`](file:///e:/arison99/LocalHive/s3-Bucket/server/server.cpp), [`server/routes/upload.cpp`](file:///e:/arison99/LocalHive/s3-Bucket/server/routes/upload.cpp), [`server/routes/download.cpp`](file:///e:/arison99/LocalHive/s3-Bucket/server/routes/download.cpp), [`server/routes/delete.cpp`](file:///e:/arison99/LocalHive/s3-Bucket/server/routes/delete.cpp).
    * Linked libraries: `Boost::system`, `${SQLITE3_LIBRARIES}`, `pthread`.
    * **Defect 1:** Hardcoded `pthread` ([line 30](file:///e:/arison99/LocalHive/s3-Bucket/CmakeLists.txt#L30)) prevents compilation under MSVC toolchains on Windows.
    * **Defect 2:** Requires `PkgConfig` ([lines 23-24](file:///e:/arison99/LocalHive/s3-Bucket/CmakeLists.txt#L23-L24)) to discover SQLite3 (`pkg_check_modules(SQLITE3 REQUIRED sqlite3)`). This fails on standard Windows developer environments where pkg-config is not configured.
  * **Target `s3_bucket_tests` (Executable):**
    * Sources: [`tests/bucket_test.cpp`](file:///e:/arison99/LocalHive/s3-Bucket/tests/bucket_test.cpp), [`tests/file_test.cpp`](file:///e:/arison99/LocalHive/s3-Bucket/tests/file_test.cpp), [`tests/metadata_test.cpp`](file:///e:/arison99/LocalHive/s3-Bucket/tests/metadata_test.cpp), plus library sources.
    * **Critical Link Defect:** Each of the three test files declares its own `int main()` function ([`bucket_test.cpp:L75`](file:///e:/arison99/LocalHive/s3-Bucket/tests/bucket_test.cpp#L75), [`file_test.cpp:L88`](file:///e:/arison99/LocalHive/s3-Bucket/tests/file_test.cpp#L88), [`metadata_test.cpp:L85`](file:///e:/arison99/LocalHive/s3-Bucket/tests/metadata_test.cpp#L85)). Compiling them into a single target triggers duplicate symbol definitions (`multiple definition of 'main'`).
    * **Defect 3:** Tests are not registered with `enable_testing()` or `add_test()`, making `ctest` inoperative.
* **`s3-Bucket/Makefile`:**
  * Configured specifically for GCC on Linux (`CXX = g++`, `LDFLAGS = -lboost_system -lsqlite3 -lpthread`).
  * Contains the same link defect: `$(TEST_TARGET)` aggregates all `tests/*.cpp` files simultaneously.
* **`s3-Bucket/build.ps1`:**
  * PowerShell helper script. Checks for MSYS2 MinGW64 g++ compiler at `C:\msys64\mingw64\`.
  * Provides a fallback Direct Compilation mode with `g++` linking `-lws2_32 -lpthread -lsqlite3`.
  * Repeats the multiple-main aggregation when attempting to build `s3_bucket_tests.exe`.

---

## 4. Codebase Components & Behavioral Analysis

### 4.1. Network & HTTP Layer
* **Implementation:** Located in [`s3-Bucket/server/server.cpp`](file:///e:/arison99/LocalHive/s3-Bucket/server/server.cpp).
* **Socket Engine:** Uses Boost.Asio synchronous TCP acceptor listening on `0.0.0.0:8080` ([`server.cpp:L75`](file:///e:/arison99/LocalHive/s3-Bucket/server/server.cpp#L75)).
* **Concurrency Model:** Uses unmanaged thread creation:
  ```cpp
  // server.cpp:92-94
  std::thread clientThread(handleClient, std::move(socket));
  clientThread.detach();
  ```
  *Risk:* Detached threads cannot be joined during graceful shutdown. Unbounded client connections will trigger OS thread exhaustion.
* **HTTP Framing & Buffer Corruption Bug:**
  * In `server.cpp:20`, `boost::asio::read_until(socket, buffer, "\r\n")` consumes data into an `asio::streambuf`.
  * When dispatching to `handleUpload(socket)` or `handleDownload(socket)`:
    * The route handlers declare a **separate** local `boost::asio::streambuf buffer;` and call `read_until(socket, buffer, "\r\n\r\n")` directly on the socket ([`upload.cpp:L19-20`](file:///e:/arison99/LocalHive/s3-Bucket/server/routes/upload.cpp#L19-L20)).
    * Any bytes beyond the initial `\r\n` that were prefetched into the first buffer in `server.cpp` are completely discarded from the stack frame!
    * In `upload.cpp:73`, after reading headers into `buffer`, it reads the body directly from the socket:
      ```cpp
      boost::asio::read(socket, boost::asio::buffer(file_content));
      ```
      If any portion of the body was already pulled into `buffer` by `read_until("\r\n\r\n")`, it is ignored, leading to truncated or desynchronized payloads.
* **Route Dispatching:** Substring searches on the raw request line:
  * `POST /upload?bucket=...&key=...`
  * `GET /download?bucket=...&key=...`
  * `DELETE /delete?bucket=...&key=...`
  * `GET /buckets`
  * `GET /health`
* **Defect:** Non-standard REST API. AWS S3 requires `PUT /{bucket}/{key}` or host-header virtual host routing `PUT /{key}` with `Host: {bucket}.localhost:8080`.

### 4.2. Storage & Filesystem Layer
* **Implementation:** Located in [`s3-Bucket/lib/bucket_manager.cpp`](file:///e:/arison99/LocalHive/s3-Bucket/lib/bucket_manager.cpp) and [`s3-Bucket/lib/file_operations.cpp`](file:///e:/arison99/LocalHive/s3-Bucket/lib/file_operations.cpp).
* **Storage Layout:**
  * Buckets are physical subdirectories under a relative directory: `buckets/<bucket_name>/`.
  * Objects are saved to: `buckets/<bucket_name>/objects/<object_key>`.
  * An unparsed `config.json` stub is written to `buckets/<bucket_name>/config.json`.
* **Path Traversal Vulnerability:**
  * In [`upload.cpp:L82`](file:///e:/arison99/LocalHive/s3-Bucket/server/routes/upload.cpp#L82), [`download.cpp:L61`](file:///e:/arison99/LocalHive/s3-Bucket/server/routes/download.cpp#L61), and [`delete.cpp:L61`](file:///e:/arison99/LocalHive/s3-Bucket/server/routes/delete.cpp#L61):
    ```cpp
    std::string file_path = "buckets/" + bucket_name + "/objects/" + object_key;
    ```
    No sanitization is performed against `..`, `/`, `\`, or null bytes. A key containing `../../Windows/System32/evil` escapes the sandbox.
* **Atomicity & Durability Violations:**
  * Uploads open a direct `std::ofstream` to the target destination without staging.
  * Incomplete network transfers leave partial files permanently on disk.
  * Overwrites truncate existing files directly. A crash during upload destroys the previous version.
  * No `fsync()` or platform commit (`FlushFileBuffers` on Windows, `fdatasync` on Linux) is called.

### 4.3. Metadata Storage (SQLite)
* **Implementation:** Located in [`s3-Bucket/lib/metadata_manager.cpp`](file:///e:/arison99/LocalHive/s3-Bucket/lib/metadata_manager.cpp).
* **Database Location:** Relative path `buckets/metadata.db`.
* **Schema:**
  ```sql
  CREATE TABLE IF NOT EXISTS Metadata (
      Key TEXT PRIMARY KEY,
      Size INTEGER,
      CreationDate TEXT,
      LastModifiedDate TEXT,
      ContentType TEXT
  );
  ```
* **Deficiencies & Bugs:**
  1. **Primary Key Collisions:** `Key` stores `"bucket/key"`. The SQL insert is `INSERT INTO Metadata ... VALUES (?, ?, ?, ?, ?)`. Overwriting an existing object fails with a SQLite primary key constraint error because `INSERT OR REPLACE` or `ON CONFLICT` is not used.
  2. **Hardcoded Metadata:** Uploads insert a static hardcoded date: `creation_date = "2025-11-18"` ([`upload.cpp:L98`](file:///e:/arison99/LocalHive/s3-Bucket/server/routes/upload.cpp#L98)) and static `"application/octet-stream"`.
  3. **Dangling Metadata on Deletion:** In [`delete.cpp:L73-75`](file:///e:/arison99/LocalHive/s3-Bucket/server/routes/delete.cpp#L73-L75), database metadata deletion is commented out (`// TODO: Remove metadata from database`). Deleting an object leaves orphaned metadata.
  4. **No Connection Caching or Concurrency Control:** `MetadataManager` instances are opened and closed per request without WAL mode configuration, busy timeouts, or mutex protection around the SQLite handle.
  5. **Null Pointer Dereference:** In `getMetadata()`, `reinterpret_cast<const char*>(sqlite3_column_text(stmt, col))` dereferences `nullptr` if any column contains `NULL`.

### 4.4. Database Connector / Proxy Implementations
* **Status:** **Completely absent**.
* **Analysis:** The repository contains no sockets, headers, abstractions, or forwarders for database network protocols (MySQL wire protocol, Postgres v3 frontend/backend, Redis RESP, Mongo wire protocol). The claim in `README.md:L80-89` showing `LocalHive hive; hive.connectToDatabase(...)` is conceptual documentation only.

---

## 5. Verified Build and Test Baselines

### 5.1. Artifact State
* An executable [`s3-Bucket/build/s3_bucket.exe`](file:///e:/arison99/LocalHive/s3-Bucket/build/s3_bucket.exe) (5.67 MB) is present in the repository from a prior MinGW/GCC build.
* Runtime folders `s3-Bucket/buckets/bucket1` and `s3-Bucket/buckets/bucket2` exist.

### 5.2. Compilation Feasibility Baseline

| Target | Build System | Status | Root Cause / Evidence |
| :--- | :--- | :--- | :--- |
| **Workspace Root** | `cmake .` | **Fails** | No `CMakeLists.txt` at repository root. |
| **Workspace CI (Linux)** | `build.yml` | **Fails** | Runs `cmake . && make` at root; targets missing. |
| **Workspace CI (Matrix)** | `cmake-multi-platform.yml` | **Fails** | Invokes `-S ${{ github.workspace }}`; no root CMake. |
| **`s3_bucket` (Linux)** | `s3-Bucket/Makefile` | **Runnable** | Builds if `libboost-all-dev` and `libsqlite3-dev` are installed. |
| **`s3_bucket` (Windows MinGW)** | `s3-Bucket/build.ps1` | **Runnable** | Compiles with direct MinGW `g++` fallback if tools exist in PATH or MSYS2. |
| **`s3_bucket` (Windows MSVC)** | `s3-Bucket/CmakeLists.txt` | **Fails** | Hardcoded `-lpthread` and required `PkgConfig` fail on MSVC. |
| **`s3_bucket_tests`** | CMake / Make / PS1 | **Fails Link** | Multiple definitions of `main()` across `bucket_test.cpp`, `file_test.cpp`, `metadata_test.cpp`. |
| **`simple_test.cpp`** | Direct CLI compile | **Passes** | Independent translation unit; verifies basic `createBucket` and `FileOperations::uploadFile`. |

---

## 6. Gap Analysis & Architecture Mandate

To transition LocalHive from this prototype into a production-grade infrastructure platform, the following architectural boundaries must be established:

1. **Root-Level Multi-Project CMake Structure:** Establish an umbrella CMake orchestration supporting modular static/shared libraries, independent server executables, and separate test targets.
2. **Decoupled Architecture:**
   * `localhive_core`: Common infrastructure (logging, metrics, configuration, OS abstractions, networking primitives).
   * `localhive_s3`: S3-compliant HTTP server, SigV4 authenticator, canonical XML serializer, and resilient storage engine.
   * `localhive_dbproxy`: Independent TCP proxy service providing connection pooling, load balancing, upstream health checks, and protocol framing.
3. **Storage Correctness Engine:** Replace naive `std::ofstream` writes with atomic staging, immutable generation tracking, SQLite WAL transactions, and crash recovery.
4. **Wire Protocol Integrity:** Transition from ad-hoc URL parameter parsing to strict HTTP/1.1 S3 REST routing.
