# LocalHive S3 Bucket

A lightweight, local S3-compatible storage server built in C++ using Boost.Asio and SQLite.

## Features

- **S3-Compatible API**: Upload, download, and delete files with HTTP REST API
- **Bucket Management**: Create, list, and delete storage buckets
- **Metadata Storage**: SQLite-based metadata management
- **Multi-threaded**: Concurrent request handling
- **Cross-platform**: Works on Windows, Linux, and macOS

## Prerequisites

### Required Dependencies
- **C++ Compiler**: GCC 7+ or Clang 6+ or MSVC 2019+
- **CMake**: 3.10 or later
- **Boost Libraries**: System component (1.65+)
- **SQLite3**: Development libraries

### Installing Dependencies

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libboost-all-dev libsqlite3-dev
```

#### Windows (with vcpkg)
```powershell
vcpkg install boost-system:x64-windows sqlite3:x64-windows
```

#### macOS (with Homebrew)
```bash
brew install cmake boost sqlite3
```

## Building

### Option 1: Using CMake (Recommended)
```bash
# Create build directory
mkdir build && cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release
```

### Option 2: Using PowerShell Script (Windows)
```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
```

### Option 3: Using Makefile (Linux/macOS)
```bash
make all
```

## Usage

### Starting the Server
```bash
./build/s3_bucket
```

The server will start on port 8080 and display available endpoints.

### API Endpoints

#### 1. Health Check
```bash
curl http://localhost:8080/health
```

#### 2. Upload File
```bash
curl -X POST "http://localhost:8080/upload?bucket=my-bucket&key=my-file.txt" \
     --data-binary @local-file.txt
```

#### 3. Download File
```bash
curl "http://localhost:8080/download?bucket=my-bucket&key=my-file.txt" \
     -o downloaded-file.txt
```

#### 4. Delete File
```bash
curl -X DELETE "http://localhost:8080/delete?bucket=my-bucket&key=my-file.txt"
```

#### 5. List Buckets
```bash
curl http://localhost:8080/buckets
```

### Testing

Run the comprehensive test suite:
```bash
./build/s3_bucket_tests
```

Or with make:
```bash
make test
```

## Project Structure

```
s3-Bucket/
├── include/                 # Header files
│   ├── bucket_manager.h     # Bucket operations
│   ├── file_operations.h    # File I/O operations
│   ├── metadata_manager.h   # SQLite metadata management
│   └── routes/              # HTTP route handlers
│       ├── upload.h
│       ├── download.h
│       └── delete.h
├── lib/                     # Implementation files
│   ├── bucket_manager.cpp
│   ├── file_operations.cpp
│   └── metadata_manager.cpp
├── server/                  # Server implementation
│   ├── server.cpp           # Main server logic
│   └── routes/              # Route implementations
│       ├── upload.cpp
│       ├── download.cpp
│       └── delete.cpp
├── tests/                   # Test files
│   ├── bucket_test.cpp
│   ├── file_test.cpp
│   └── metadata_test.cpp
├── buckets/                 # Storage directory (created at runtime)
├── CMakeLists.txt          # CMake configuration
├── Makefile                # Alternative build system
├── build.ps1              # Windows build script
└── README.md              # This file
```

## Storage Layout

Files are stored in the following structure:
```
buckets/
├── bucket-name-1/
│   ├── config.json          # Bucket configuration
│   └── objects/             # Stored files
│       ├── file1.txt
│       └── file2.jpg
├── bucket-name-2/
│   └── objects/
└── metadata.db             # SQLite database for metadata
```

## Configuration

The server can be configured by modifying `server/server.cpp`:
- Port number (default: 8080)
- Storage directory (default: "buckets")
- Thread pool size

## Development

### Adding New Features

1. **New Endpoints**: Add route handlers in `server/routes/`
2. **Storage Backends**: Extend `FileOperations` class
3. **Authentication**: Implement in server request handling
4. **Logging**: Add logging framework integration

### Code Style
- Use C++17 features
- Follow RAII principles
- Handle exceptions appropriately
- Use const correctness

## Troubleshooting

### Common Issues

1. **Build Errors**: Ensure all dependencies are installed
2. **Permission Errors**: Check file system permissions for bucket directory
3. **Port Conflicts**: Change port in server.cpp if 8080 is occupied
4. **Database Errors**: Ensure SQLite3 development libraries are installed

### Debug Build
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

### Verbose Logging
Enable debug output by modifying the server code to include more logging.

## Performance Considerations

- Uses multi-threading for concurrent requests
- SQLite WAL mode for better concurrent access
- Memory-mapped I/O for large files (future enhancement)
- Connection pooling (future enhancement)

## Security Notes

⚠️ **Development Use Only**: This server is designed for local development and testing. Do not expose it to public networks without proper security measures:

- No authentication/authorization
- No input validation/sanitization
- No rate limiting
- No HTTPS/TLS encryption

## Contributing

1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality
4. Ensure all tests pass
5. Submit a pull request

## License

This project is part of LocalHive and follows the same license terms.

## Roadmap

- [ ] Authentication and authorization
- [ ] HTTPS/TLS support
- [ ] Compression support
- [ ] Multipart uploads
- [ ] Web UI for management
- [ ] Docker containerization
- [ ] AWS S3 SDK compatibility
- [ ] Replication and backup
- [ ] Performance monitoring