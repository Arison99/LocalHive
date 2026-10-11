# LocalHive S3 Bucket Build Script
# Run this with: powershell -ExecutionPolicy Bypass -File build.ps1

Param(
    [switch]$DirectCompile = $false
)

Write-Host "Building LocalHive S3 Bucket..." -ForegroundColor Green

# MSYS2 paths
$MSYS2_CMAKE = "C:\msys64\mingw64\bin\cmake.exe"
$MSYS2_INCLUDE = "C:\msys64\mingw64\include"
$MSYS2_LIB = "C:\msys64\mingw64\lib"

# Check if required tools are available
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake -and (Test-Path $MSYS2_CMAKE)) {
    $cmake = $MSYS2_CMAKE
    Write-Host "Using MSYS2 CMake: $cmake" -ForegroundColor Yellow
} elseif (-not $cmake) {
    Write-Host "CMake not found. Will try direct compilation..." -ForegroundColor Yellow
    $DirectCompile = $true
}

# Check for C++ compiler
$gcc = Get-Command g++ -ErrorAction SilentlyContinue
$clang = Get-Command clang++ -ErrorAction SilentlyContinue
$msvc = Get-Command cl -ErrorAction SilentlyContinue

if (-not ($gcc -or $clang -or $msvc)) {
    Write-Host "Error: No C++ compiler found. Please install GCC, Clang, or Visual Studio." -ForegroundColor Red
    exit 1
}

# Create build directory
if (Test-Path "build") {
    Remove-Item -Recurse -Force "build"
}
New-Item -ItemType Directory -Path "build" | Out-Null

if ($DirectCompile) {
    Write-Host "Using direct compilation with g++..." -ForegroundColor Yellow
    
    # Direct compilation flags
    $CXXFLAGS = "-std=c++17", "-Wall", "-Wextra", "-g", "-Iinclude"
    $LIBS = "-lws2_32", "-lpthread"
    
    if (Test-Path $MSYS2_INCLUDE) {
        $CXXFLAGS += "-I$MSYS2_INCLUDE"
        $LIBS += "-L$MSYS2_LIB", "-lsqlite3"
        Write-Host "Using MSYS2 libraries" -ForegroundColor Yellow
    }
    
    # Compile server
    Write-Host "Compiling server..." -ForegroundColor Yellow
    $sourceFiles = @(
        "lib/bucket_manager.cpp",
        "lib/file_operations.cpp", 
        "lib/metadata_manager.cpp",
        "server/server.cpp",
        "server/routes/upload.cpp",
        "server/routes/download.cpp",
        "server/routes/delete.cpp"
    )
    
    $compileCmd = @("g++") + $CXXFLAGS + $sourceFiles + @("-o", "build/s3_bucket.exe") + $LIBS
    & $compileCmd[0] $compileCmd[1..($compileCmd.Length-1)]
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Server compiled successfully!" -ForegroundColor Green
    } else {
        Write-Host "Server compilation failed" -ForegroundColor Red
        exit 1
    }
    
    # Compile tests
    Write-Host "Compiling tests..." -ForegroundColor Yellow
    $testFiles = @(
        "tests/bucket_test.cpp",
        "tests/file_test.cpp", 
        "tests/metadata_test.cpp",
        "lib/bucket_manager.cpp",
        "lib/file_operations.cpp",
        "lib/metadata_manager.cpp"
    )
    
    $testCmd = @("g++") + $CXXFLAGS + $testFiles + @("-o", "build/s3_bucket_tests.exe") + $LIBS
    Write-Host "Executing: g++ $($CXXFLAGS -join ' ') $($testFiles -join ' ') -o build/s3_bucket_tests.exe $($LIBS -join ' ')" -ForegroundColor Gray
    & $testCmd[0] $testCmd[1..($testCmd.Length-1)]
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Tests compiled successfully!" -ForegroundColor Green
    } else {
        Write-Host "Test compilation failed" -ForegroundColor Red
    }
    
} else {
    # Change to build directory
    Set-Location "build"
    
    try {
        # Configure with CMake
        Write-Host "Configuring with CMake..." -ForegroundColor Yellow
        if ($cmake -eq $MSYS2_CMAKE) {
            & $cmake .. -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles"
        } else {
            cmake .. -DCMAKE_BUILD_TYPE=Release
        }
        
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configuration failed"
        }
        
        # Build the project
        Write-Host "Building project..." -ForegroundColor Yellow
        if ($cmake -eq $MSYS2_CMAKE) {
            & $cmake --build . --config Release
        } else {
            cmake --build . --config Release
        }
        
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed"
        }
        
        Write-Host "Build completed successfully!" -ForegroundColor Green
        
    } catch {
        Write-Host "CMake build failed: $_" -ForegroundColor Red
        Set-Location ..
        Write-Host "Trying direct compilation..." -ForegroundColor Yellow
        & $PSCommandPath -DirectCompile
        return
    } finally {
        Set-Location ..
    }
}

Write-Host ""
if (Test-Path "build/s3_bucket.exe") {
    Write-Host "Server executable: build/s3_bucket.exe" -ForegroundColor Green
}
if (Test-Path "build/s3_bucket_tests.exe") {
    Write-Host "Test executable: build/s3_bucket_tests.exe" -ForegroundColor Green
}

Write-Host ""
Write-Host "To run the server: .\build\s3_bucket.exe" -ForegroundColor Cyan
Write-Host "To run tests: .\build\s3_bucket_tests.exe" -ForegroundColor Cyan