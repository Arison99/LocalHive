# LocalHive S3 Compatibility Matrix & Implementation Roadmap
**Document ID:** `DOC-LH-S3MAP-005-REV2`  
**Task Reference:** `TASK LH-ARCH-001` (Phase 5 S3 Roadmap Revision)  
**Date:** 2026-10-11  
**Status:** Canonical S3 Compatibility Specification (Post-Review Revision)

---

## 1. Compliance Tiers & Scope Qualification

> [!IMPORTANT]
> **S3 API Compatibility vs. Universal AWS S3 Equivalence:**  
> LocalHive is an embedded native daemon targeting **S3 API Compatibility** (wire-format REST protocols, AWS SigV4, XML envelopes, and SDK/CLI tooling interoperability). It **does not claim universal AWS S3 equivalence**. Distributed features such as global multi-region replication, erasure-coded distributed consensus, Object Lock compliance mode, and AWS IAM role federation are outside the scope of this local infrastructure service.

### Classification Tiers:
* **Implemented (Legacy):** Exists in prototype codebase under `s3-Bucket/`, but relies on non-standard REST query strings (`/upload?bucket=...`), lacks XML envelopes, or exhibits buffer/concurrency defects.
* **Planned (Phase 1 Target):** Targeted for primary native C++17 modular release. Conforms to S3 REST specifications and passes official AWS CLI and Boto3 validation suites.
* **Planned (Phase 2 Extended):** Advanced features scheduled post-v1.0 (e.g. Multipart Uploads, Object Tagging).
* **Unsupported:** Features out of scope for a local development daemon (e.g. Cross-Region Replication, S3 Glacier storage classes, Object Lock WORM compliance, S3 Select).

---

## 2. Comprehensive S3 Operations Matrix

| S3 Operation | HTTP Method & URI | Status | Request Headers / Params | Expected Success Response | S3 Error Mappings |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`ListBuckets`** | `GET /` | **Planned (P1)** *(Legacy stdout only)* | Host | `200 OK`<br/>XML: `<ListAllMyBucketsResult>` | `AccessDenied` |
| **`CreateBucket`** | `PUT /{bucket}` | **Planned (P1)** *(Legacy in-memory only)* | `LocationConstraint` (Optional XML) | `200 OK`<br/>`Location: /{bucket}` | `BucketAlreadyExists`, `BucketAlreadyOwnedByYou`, `InvalidBucketName` |
| **`DeleteBucket`** | `DELETE /{bucket}` | **Planned (P1)** *(Legacy in-memory only)* | None | `204 No Content` | `NoSuchBucket`, `BucketNotEmpty` |
| **`HeadBucket`** | `HEAD /{bucket}` | **Planned (P1)** | None | `200 OK` | `404 NoSuchBucket`, `403 AccessDenied` |
| **`PutObject`** | `PUT /{bucket}/{key}` | **Planned (P1)** *(Legacy as POST /upload)* | `Content-Length`, `Content-Type`, `x-amz-meta-*`, `Content-MD5` | `200 OK`<br/>`ETag: "{etag}"` | `NoSuchBucket`, `InvalidDigest`, `MissingContentLength`, `EntityTooLarge`, `SlowDown` (503) |
| **`GetObject`** | `GET /{bucket}/{key}` | **Planned (P1)** *(Legacy as GET /download)* | `Range`, `If-Match`, `If-None-Match`, `If-Modified-Since` | `200 OK` or `206 Partial Content`<br/>`ETag`, `Content-Length`, `Content-Type` | `NoSuchKey`, `NoSuchBucket`, `InvalidRange`, `304 Not Modified`, `412 PreconditionFailed`, `SlowDown` (503) |
| **`HeadObject`** | `HEAD /{bucket}/{key}` | **Planned (P1)** | `If-Match`, `If-None-Match`, `If-Modified-Since` | `200 OK`<br/>`ETag`, `Content-Length`, `x-amz-meta-*` | `404 NoSuchKey`, `304 Not Modified`, `412 PreconditionFailed` |
| **`DeleteObject`** | `DELETE /{bucket}/{key}` | **Planned (P1)** *(Legacy as DELETE /delete)* | None | `204 No Content` (S3 always returns 204 even if key did not exist) | `NoSuchBucket` |
| **`DeleteObjects`** | `POST /{bucket}?delete` | **Planned (P1)** | XML: `<Delete><Object><Key>...` | `200 OK`<br/>XML: `<DeleteResult>` | `NoSuchBucket`, `MalformedXML` |
| **`ListObjectsV2`** | `GET /{bucket}?list-type=2` | **Planned (P1)** | `prefix`, `delimiter`, `max-keys`, `continuation-token`, `start-after` | `200 OK`<br/>XML: `<ListBucketResult>` | `NoSuchBucket`, `InvalidArgument` |
| **`CreateMultipartUpload`** | `POST /{bucket}/{key}?uploads` | **Planned (P2)** | `Content-Type`, `x-amz-meta-*` | `200 OK`<br/>XML: `<InitiateMultipartUploadResult>` | `NoSuchBucket`, `AccessDenied` |
| **`UploadPart`** | `PUT /{bucket}/{key}?uploadId=..&partNumber=..` | **Planned (P2)** | `Content-Length`, `partNumber` (1..10000) | `200 OK`<br/>`ETag: "{part_etag}"` | `NoSuchUpload`, `InvalidPart`, `InvalidPartOrder` |
| **`CompleteMultipartUpload`** | `POST /{bucket}/{key}?uploadId=..` | **Planned (P2)** | XML: `<CompleteMultipartUpload>` | `200 OK`<br/>XML: `<CompleteMultipartUploadResult>` | `NoSuchUpload`, `InvalidPart`, `EntityTooSmall` |
| **`AbortMultipartUpload`** | `DELETE /{bucket}/{key}?uploadId=..` | **Planned (P2)** | None | `204 No Content` | `NoSuchUpload` |
| **`ListParts`** | `GET /{bucket}/{key}?uploadId=..` | **Planned (P2)** | `max-parts`, `part-number-marker` | `200 OK`<br/>XML: `<ListPartsResult>` | `NoSuchUpload` |
| **`ObjectTagging`** | `GET/PUT/DELETE /{bucket}/{key}?tagging` | **Planned (P2)** | XML TagSet | `200 OK` | `NoSuchKey`, `InvalidTag` |
| **`Presigned URLs`** | Any with `X-Amz-Signature` query param | **Planned (P1)** | `X-Amz-Expires`, `X-Amz-Credential`, `X-Amz-Signature` | Standard operation response | `RequestExpired`, `SignatureDoesNotMatch` |
| **`S3 Glacier / Storage Classes`** | Headers e.g. `x-amz-storage-class` | **Unsupported** | N/A | Ignored / Returns `STANDARD` | N/A |
| **`Replication / Object Lock`** | `?replication`, `?object-lock` | **Unsupported** | N/A | N/A | `NotImplemented` (501) |

---

## 3. Standard S3 XML Wire Envelopes

### 3.1. Error Envelope (`application/xml`)
All S3 error responses conform to the standard AWS XML structure:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<Error>
    <Code>NoSuchKey</Code>
    <Message>The specified key does not exist.</Message>
    <Key>photos/vacation.jpg</Key>
    <BucketName>my-bucket</BucketName>
    <Resource>/my-bucket/photos/vacation.jpg</Resource>
    <RequestId>43BF4B98-289B-4B7C-A57E-E7D3F6531980</RequestId>
    <HostId>localhive-s3-node1</HostId>
</Error>
```

### 3.2. Backpressure Error Envelope (`SlowDown`)
When worker queues exceed high watermarks (80% capacity), the daemon responds with HTTP 503:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<Error>
    <Code>SlowDown</Code>
    <Message>Please reduce your request rate.</Message>
    <Resource>/my-bucket/photos/vacation.jpg</Resource>
    <RequestId>98FA2B11-124A-4A89-B73C-E8D2F1234567</RequestId>
    <HostId>localhive-s3-node1</HostId>
</Error>
```

### 3.3. ListObjectsV2 Envelope (`application/xml`)
```xml
<?xml version="1.0" encoding="UTF-8"?>
<ListBucketResult xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
    <Name>my-bucket</Name>
    <Prefix>docs/</Prefix>
    <KeyCount>2</KeyCount>
    <MaxKeys>1000</MaxKeys>
    <Delimiter>/</Delimiter>
    <IsTruncated>false</IsTruncated>
    <Contents>
        <Key>docs/spec.pdf</Key>
        <LastModified>2026-10-11T20:15:30.000Z</LastModified>
        <ETag>&quot;b10a8db164e0754105b7a99be72e3fe5&quot;</ETag>
        <Size>1048576</Size>
        <StorageClass>STANDARD</StorageClass>
    </Contents>
    <CommonPrefixes>
        <Prefix>docs/archive/</Prefix>
    </CommonPrefixes>
</ListBucketResult>
```

---

## 4. Advanced Protocol Semantics

### 4.1. Byte-Range Requests (`Range: bytes=start-end`)
* **Single Range:** Supports `bytes=0-499` (first 500 bytes), `bytes=500-` (byte 500 to EOF), `bytes=-500` (final 500 bytes).
* **Response:** `HTTP/1.1 206 Partial Content` with `Content-Range: bytes 0-499/1048576` and `Content-Length: 500`.
* **Out of Range:** Returns `HTTP/1.1 416 Range Not Satisfiable` with `InvalidRange` S3 XML error.
* **Zero Allocation Streaming:** Uses `IStreamReader::seek(start)` and streams directly into network socket buffers.

### 4.2. Conditional Requests & Cache Validation
Evaluated before streaming object content:
* `If-Match: "{etag}"`: Proceeds only if object ETag matches; otherwise `412 PreconditionFailed`.
* `If-None-Match: "{etag}"`: Returns `304 Not Modified` (with empty body) if ETag matches.
* `If-Modified-Since`: Returns `304 Not Modified` if object was not modified after timestamp.
* `If-Unmodified-Since`: Returns `412 PreconditionFailed` if object was modified after timestamp.

### 4.3. AWS Signature Version 4 (SigV4) Execution Protocol
1. **Canonical Request Construction:**
   * Canonical URI (strictly URL-encoded path).
   * Canonical Query String (lexicographically sorted by parameter name).
   * Canonical Headers (lowercased names, sorted, whitespace stripped).
   * Signed Headers list (semicolon-separated).
   * Payload Hash (`x-amz-content-sha256` or hex SHA-256 of body; handles `UNSIGNED-PAYLOAD`).
2. **String to Sign:**
   ```
   AWS4-HMAC-SHA256
   {RequestDateTime} (YYYYMMDDTHHMMSSZ)
   {Date}/{Region}/s3/aws4_request
   HashedCanonicalRequest (SHA256 hex)
   ```
3. **Signature Derivation (using zero-dep `localhive::core::crypto`):**
   ```
   kDate    = HMAC-SHA256("AWS4" + SecretAccessKey, "YYYYMMDD")
   kRegion  = HMAC-SHA256(kDate, Region)
   kService = HMAC-SHA256(kRegion, "s3")
   kSigning = HMAC-SHA256(kService, "aws4_request")
   Signature = Hex(HMAC-SHA256(kSigning, StringToSign))
   ```
4. **Validation:** Timing-safe memory comparison (`constant_time_equals`) against client signature header. Rejects requests with time drift > 900 seconds (`RequestTimeTooSkewed`).

---

## 5. Tooling & SDK Verification Test Plan

All compatibility milestones are verified using real-world client tooling targeting `http://localhost:8080`:

### 5.1. AWS CLI Verification Suite
```bash
# Configure local CLI profile
aws configure set aws_access_key_id "localhive-dev" --profile localhive
aws configure set aws_secret_access_key "localhive-secret" --profile localhive
aws configure set region "us-east-1" --profile localhive

# 1. Bucket Operations
aws --endpoint-url=http://localhost:8080 --profile localhive s3 mb s3://test-bucket
aws --endpoint-url=http://localhost:8080 --profile localhive s3 ls

# 2. Object Operations
aws --endpoint-url=http://localhost:8080 --profile localhive s3 cp ./sample.dat s3://test-bucket/sample.dat
aws --endpoint-url=http://localhost:8080 --profile localhive s3 cp s3://test-bucket/sample.dat ./downloaded.dat
diff ./sample.dat ./downloaded.dat

# 3. ListObjectsV2 with Delimiters & Prefixes
aws --endpoint-url=http://localhost:8080 --profile localhive s3api list-objects-v2 --bucket test-bucket --prefix "sample"

# 4. Deletion
aws --endpoint-url=http://localhost:8080 --profile localhive s3 rm s3://test-bucket/sample.dat
aws --endpoint-url=http://localhost:8080 --profile localhive s3 rb s3://test-bucket
```

### 5.2. Official SDK Test Matrix
* **Python (Boto3):**
  ```python
  import boto3
  s3 = boto3.client(
      's3',
      endpoint_url='http://localhost:8080',
      aws_access_key_id='localhive-dev',
      aws_secret_access_key='localhive-secret',
      region_name='us-east-1'
  )
  s3.put_object(Bucket='my-bucket', Key='test.txt', Body=b'LocalHive Boto3 verification')
  resp = s3.get_object(Bucket='my-bucket', Key='test.txt')
  assert resp['Body'].read() == b'LocalHive Boto3 verification'
  ```
* **Go (`aws-sdk-go-v2`):** Validates concurrent reader streaming, custom retry policies, and byte-range retrieval.
* **Node.js (`@aws-sdk/client-s3`):** Validates virtual-hosted style routing and streaming request body upload.
