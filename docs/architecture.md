# LocalHive Platform Architecture Specification
**Document ID:** `DOC-LH-ARCH-002-REV3`  
**Task Reference:** `TASK LH-ARCH-002` (Architecture Contract Closure)  
**Date:** 2026-10-12  
**Status:** Authoritative Engineering Architecture

---

## 1. System Vision & Architectural Scope

LocalHive is designed as a modular native C++17 infrastructure platform providing local developers and CI/CD pipelines with production-grade data primitives:
1. **LocalHive S3 (`localhive-s3`):** A lightweight, highly resilient local object store targeting **S3 API Compatibility** (standard HTTP REST methods, SigV4 authentication, XML envelopes, and AWS SDK/CLI interoperability).
2. **LocalHive DB Proxy (`localhive-dbproxy`):** An independent, low-overhead L4 TCP connection forwarder and session governor for relational and document databases.
3. **LocalHive Admin / CLI (`localhive-admin`):** An out-of-band operational interface for configuration, telemetry inspection, and health verification.

> [!IMPORTANT]
> **API Compatibility vs. Universal Equivalence:**  
> LocalHive targets wire-level **S3 API Compatibility** for developer workflows. It makes no claims of universal AWS S3 operational equivalence (such as global replication, erasure-coded distributed clusters, or petabyte-scale consistency models).

### Core Architectural Principles
* **Strict Decoupling:** S3 object storage logic and database proxy logic must never depend on each other. Neither service shall import headers or link libraries belonging to the other's domain.
* **Separation of Control and Data Paths:** Administrative operations, metrics scraping, and configuration reloads occur out-of-band without acquiring locks or injecting latency into the hot I/O data path.
* **Native Portability Without Mandatory Containers:** Windows is the primary development environment (MSVC / MinGW); Linux is the production/CI environment (GCC / Clang / systemd).
* **Layered & Bounded Concurrency:** Dedicated asynchronous event loops (`io_context`-per-thread) with bounded memory and disk quotas manage network I/O, while bounded background worker pools execute blocking filesystem and SQLite transactions.

---

## 2. Component Topology & Dependency Direction

The LocalHive platform adopts a strictly acyclic, layered architecture. Lower layers have zero knowledge of higher layers.

```mermaid
graph TD
    subgraph Executables ["Executables (Applications)"]
        S3Server["localhive-s3<br/>(S3 Storage Daemon)"]
        DBProxy["localhive-dbproxy<br/>(L4 TCP Proxy Daemon)"]
        AdminCLI["localhive-admin<br/>(Admin & Telemetry Tool / localhive-cli)"]
    end

    subgraph DomainLibraries ["Domain Logic Libraries"]
        S3Core["liblocalhive_s3_core<br/>(S3 Protocol, Auth, XML)"]
        StorageEngine["liblocalhive_storage<br/>(Generational FS, SQLite WAL)"]
        ProxyCore["liblocalhive_proxy_core<br/>(L4 TCP Forwarder, Raw Pool)"]
    end

    subgraph PlatformLibraries ["Shared Core Libraries (liblocalhive_core)"]
        CoreNet["localhive::core::net<br/>(Boost.Asio Event Loops, Sockets)"]
        CoreCrypto["localhive::core::crypto<br/>(Zero-Dep SHA256, HMAC, Constant-Time)"]
        CoreConfig["localhive::core::config<br/>(Resource Limits, CLI/Env/JSON)"]
        CoreLogging["localhive::core::logging<br/>(Structured, Leveled, Async Sink)"]
        CoreMetrics["localhive::core::metrics<br/>(Prometheus Counters, Gauges)"]
        CoreLifecycle["localhive::core::lifecycle<br/>(Signals, Graceful Drain, Runner)"]
        CorePlatform["localhive::core::platform<br/>(Win32/POSIX FS, Sync, Locking)"]
    end

    subgraph ThirdParty ["Third-Party & OS Primitives"]
        BoostAsio["Boost.Asio (Header-Only / Standalone)"]
        SQLite3["SQLite3 (Vendored Amalgamation)"]
        Win32OS["Win32 API (WS2_32, Kernel32)"]
        POSIXOS["POSIX API (sys/socket, unistd, fcntl)"]
    end

    %% Dependency Connections
    S3Server --> S3Core
    S3Server --> StorageEngine
    S3Server --> CoreLifecycle
    S3Server --> CoreLogging
    S3Server --> CoreMetrics

    DBProxy --> ProxyCore
    DBProxy --> CoreLifecycle
    DBProxy --> CoreLogging
    DBProxy --> CoreMetrics

    AdminCLI --> CoreConfig
    AdminCLI --> CoreNet
    AdminCLI --> CoreLogging

    S3Core --> CoreNet
    S3Core --> CoreCrypto
    S3Core --> CoreLogging
    
    StorageEngine --> CorePlatform
    StorageEngine --> CoreLogging
    StorageEngine --> SQLite3

    ProxyCore --> CoreNet
    ProxyCore --> CoreLogging
    ProxyCore --> CoreMetrics

    CoreNet --> BoostAsio
    CoreNet --> Win32OS
    CoreNet --> POSIXOS
    CorePlatform --> Win32OS
    CorePlatform --> POSIXOS
```

### Dependency Rules & Constraints
1. **No Lateral Dependencies:** `liblocalhive_s3_core` and `liblocalhive_proxy_core` are completely isolated. Neither depends on the other.
2. **Strict Downward Dependency:** Domain libraries depend solely on `liblocalhive_core` and designated third-party libraries (e.g. SQLite for `liblocalhive_storage`).
3. **Core Independence:** `liblocalhive_core` has no dependencies on any domain logic. It encapsulates only reusable OS abstractions, networking primitives, cryptographic functions, and diagnostic tools.

---

## 3. Subsystem Architecture

### 3.1. LocalHive Core (`liblocalhive_core`)

| Module | Namespace | Responsibilities & Design |
| :--- | :--- | :--- |
| **Lifecycle** | `localhive::core::lifecycle` | Service execution lifecycle (`ServiceRunner`), signal trapping (`SIGINT`, `SIGTERM`, Windows Console Ctrl handlers), cancellation tokens (`std::atomic_bool`), and orderly phased shutdown. |
| **Configuration** | `localhive::core::config` | Strongly-typed configuration structures, command-line parsing, environment variable overrides (`LOCALHIVE_*`), and JSON file deserialization with semantic validation. |
| **Logging** | `localhive::core::logging` | Asynchronous, lock-free ring-buffered logging. Supports structured JSON and human-readable console sinks with compile-time level filtering (`TRACE`, `DEBUG`, `INFO`, `WARN`, `ERROR`, `FATAL`). Sensitive-data redaction. |
| **Metrics** | `localhive::core::metrics` | Lock-free counters, atomic gauges, and histogram buckets. Prometheus text format serialization via an out-of-band HTTP or pipe endpoint. |
| **Networking** | `localhive::core::net` | Boost.Asio Thread-per-Core Event Loop Pool (`io_context`-per-thread), TCP socket wrappers, buffered stream adapters, and socket option helpers (TCP_NODELAY, SO_KEEPALIVE, SO_REUSEADDR). |
| **Security / Crypto** | `localhive::core::crypto` | **Zero external dependencies.** Self-contained C++17 implementation of SHA-256 (RFC 6234) and HMAC-SHA256 (RFC 4231), timing-safe byte equality, and hex/base64 encoders. Optional compiler intrinsics for hardware SHA-NI. |
| **Platform Abstraction** | `localhive::core::platform` | Windows vs. Linux unified abstractions for file locking (`LockFileEx` vs `fcntl`), atomic file replacement (`ReplaceFileW` vs `renameat2`), directory sync, and high-resolution monotonic clocks. |

---

### 3.2. LocalHive S3 Subsystem (`localhive-s3`)

The S3 daemon implements S3 REST API compliance backed by an atomic filesystem and SQLite metadata engine:

```mermaid
sequenceDiagram
    autonumber
    participant Client as S3 Client / SDK / CLI
    participant Router as S3 HTTP Router
    participant Auth as SigV4 Authenticator
    participant S3Service as S3 Service Coordinator
    participant ObjStore as IObjectStore (Atomic FS)
    participant MetaStore as IMetadataStore (SQLite WAL)

    Client->>Router: HTTP PUT /bucket/object.dat (Payload)
    Router->>Auth: Validate Authorization (SigV4 / Anonymous)
    Auth-->>Router: Authenticated Context (AccessKey, Role)
    Router->>S3Service: PutObject(Bucket, Key, Stream, Size, Headers)
    S3Service->>MetaStore: CheckBucket(Bucket)
    MetaStore-->>S3Service: Bucket Exists (bucket_id)
    S3Service->>ObjStore: StageData(bucket_id, gen_id, Stream)
    Note over ObjStore: RECEIVING -> STAGED:<br/>1. Stream to .staging/<br/>2. Compute SHA256/MD5<br/>3. Flush write buffer (fdatasync/FlushFileBuffers)
    ObjStore-->>S3Service: Staged(GenID, Size, Checksum)
    S3Service->>ObjStore: PromoteStagedToActive(bucket_id, object_id, gen_id)
    Note over ObjStore: STAGED -> PREPARED:<br/>4. Move to data/b_{id}/{obj}_{gen}.dat<br/>(Immutable unique filename - NEVER overwrites)<br/>5. Flush parent directory entry
    ObjStore-->>S3Service: Physical File in Active Location
    S3Service->>MetaStore: CommitObject(Record, gen_id)
    Note over MetaStore: PREPARED -> PUBLISHED:<br/>6. SQLite BEGIN IMMEDIATE<br/>7. Update active_gen_id pointer<br/>8. Retire prior generation to 'obsolete'<br/>9. SQLite COMMIT
    MetaStore-->>S3Service: Committed
    S3Service-->>Router: S3Response(200 OK, ETag, VersionID)
    Router-->>Client: HTTP/1.1 200 OK (COMMITTED)
```

#### Key Architecture Components:
* **Normative 5-State Object Lifecycle:** Strictly enforces `RECEIVING` $\to$ `STAGED` $\to$ `PREPARED` $\to$ `PUBLISHED` $\to$ `COMMITTED`. Physical files are moved to their unique destination before SQLite metadata commit, guaranteeing Invariant 4.
* **HTTP / S3 Protocol Handler:** Parses standard HTTP/1.1 requests supporting both **Path-Style** (`http://localhost:8080/bucket/key`) and **Virtual-Hosted-Style** (`http://bucket.localhost:8080/key`). Enforces AWS XML schema compliance for responses and errors.
* **Authentication Subsystem (`SigV4`):** Parses `Authorization: AWS4-HMAC-SHA256` and presigned URLs. Computes HMAC signatures using zero-dependency `localhive::core::crypto` and verifies signatures in constant time.
* **Storage Engine (`liblocalhive_storage`):**
  * `IObjectStore`: Content-addressable generational filesystem store.
  * `IMetadataStore`: SQLite 3 embedded database operating in WAL mode with configurable durability profiles (`Strict`, `Standard`, `Development`).

---

### 3.3. LocalHive DB Proxy Subsystem (`localhive-dbproxy`)

The DB Proxy daemon runs as a standalone native binary that manages client connection lifecycles, mitigates database exhaustion, and proxies TCP traffic to upstream databases.

```mermaid
graph LR
    subgraph ClientLayer ["Client Applications"]
        C1["App Conn 1"]
        C2["App Conn 2"]
        C3["App Conn N"]
    end

    subgraph DBProxy ["localhive-dbproxy (Transparent L4)"]
        Listener["TCP Front-End Listener<br/>(:3306, :5432, :6379, :27017)"]
        Governor["Connection Governor<br/>(Max Inflight: 1024, Rate Limit)"]
        Splice["Bidirectional Asynchronous<br/>TCP Splice (Half-Close Aware)"]
        PrewarmPool["Raw TCP Pre-Warmed Pool<br/>(Unallocated Sockets to reduce latency)"]
        Health["Upstream Health Prober<br/>(Active / Passive TCP Ping)"]
    end

    subgraph UpstreamDB ["Upstream Database Instances"]
        DBPrimary["Primary DB Node"]
        DBReplica["Standby / Read Node"]
    end

    C1 --> Listener
    C2 --> Listener
    C3 --> Listener
    Listener --> Governor
    Governor --> Splice
    PrewarmPool -. Pre-connects .-> DBPrimary
    PrewarmPool -->|Lease Exclusive 1:1 Socket| Splice
    Splice <-->|1:1 Dedicated Socket Binding| DBPrimary
    Health -.-> DBPrimary
    Health -.-> DBReplica
```

#### Phase 1 L4 Forwarding Guarantees:
1. **Transparent L4 TCP Stream Forwarding:**
   * Strict 1:1 client-to-upstream socket binding. Zero payload inspection.
   * Asynchronous bidirectional splicing with socket backpressure (pauses reading from downstream when upstream write buffer fills).
   * Proper TCP half-close propagation (client `FIN` maps to upstream `FIN`).
   * Connection pre-warming: pre-establishes raw, unallocated TCP connections to eliminate connect handshake latency. Once allocated, connection is strictly exclusive.
   * **Failover Safety:** No automatic primary failover. Dropped upstream connections reset client sockets immediately.
2. **Protocol-Aware Pooling & Multiplexing (Future / L7):**
   * Requires parsing wire protocols (PostgreSQL, MySQL, Redis, MongoDB).
   * Deferred to post-v1.0 to prevent transaction boundary corruption.
   * **Explicit Constraint:** SQLite is an in-process C library and is **never** an upstream network proxy target.

---

### 3.4. LocalHive Admin & Control Plane (`localhive-admin`)

Administrative capabilities are strictly separated from high-throughput data paths:

1. **Security & Transport:**
   * **Windows:** Named Pipe (`\\.\pipe\localhive-s3-admin`) protected by an explicit Win32 DACL granting access solely to `BuiltinAdministrators` and `LocalSystem`.
   * **Linux:** Unix Domain Socket (`/var/run/localhive/s3-admin.sock`) with file permissions `0600` owned by `localhive:localhive`.
   * **Loopback HTTP:** Binds exclusively to `127.0.0.1:9090`. Requires `Authorization: Bearer <token>` loaded from a mode `0400` secret file.
2. **Capabilities:**
   * Query server runtime statistics (active connections, open reader leases, SQLite WAL size).
   * Dynamic log-level adjustment without restarting the process.
   * Command graceful connection draining (stop accepting new sockets, allow in-flight S3 streams or DB queries to terminate within a timeout).

---

## 4. Concurrency & Threading Model

LocalHive implements an event-loop-per-core model with bounded worker pools, payload memory limits, and explicit backpressure:

```mermaid
graph TD
    subgraph NetworkTier ["Network Tier (io_context per Core)"]
        Loop1["io_context Thread 1"]
        Loop2["io_context Thread 2"]
        LoopN["io_context Thread N"]
    end

    subgraph MemoryGovernor ["Payload Memory & Staging Quota Governor"]
        MemLimit["Global Queued Payload <= 64 MB"]
        DiskLimit["Staging Disk Usage <= 20 GB"]
        Throttle{"Usage >= 80% High-Watermark?"}
        MemLimit --> Throttle
        DiskLimit --> Throttle
        Throttle -- Yes --> Reject["HTTP 503 SlowDown / Pause Read"]
        Throttle -- No --> Accept["Dispatch to Work Queue"]
    end

    subgraph WorkerTier ["Worker Tier (Blocking Storage & DB Tasks)"]
        StorageWorker["Bounded Storage ThreadPool<br/>(Max 1024 Tasks)"]
        SQLiteWorker["Dedicated SQLite Writer Thread<br/>(Serialized Write Loop)"]
        Accept --> StorageWorker
        StorageWorker --> SQLiteWorker
    end

    Loop1 -. Assign connection .-> MemLimit
    Loop2 -. Assign connection .-> MemLimit
```

1. **Network Event Loop Pool:** `io_context` instances equal to physical CPU cores. Connections pinned to a single loop, eliminating lock overhead.
2. **Memory & Disk Governors:** Payload buffers bounded globally to 64 MB; staging disk bounded to 20 GB. High-watermark (80%) triggers HTTP 503 `SlowDown`.
3. **SQLite Serialized Worker:** Dedicated writer thread executes `BEGIN IMMEDIATE` transaction loops with bounded 5,000 ms busy-timeout handling.
