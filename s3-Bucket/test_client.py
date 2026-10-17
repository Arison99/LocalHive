#!/usr/bin/env python3
"""
LocalHive S3 Client Example
A simple Python client for testing the LocalHive S3 server
"""

import requests
import json
import os
import sys
from typing import Optional

class LocalHiveClient:
    def __init__(self, base_url: str = "http://localhost:8080"):
        self.base_url = base_url.rstrip('/')
        
    def health_check(self) -> bool:
        """Check if the server is running"""
        try:
            response = requests.get(f"{self.base_url}/health")
            return response.status_code == 200
        except requests.RequestException:
            return False
    
    def upload_file(self, bucket: str, key: str, file_path: str) -> bool:
        """Upload a file to the server"""
        if not os.path.exists(file_path):
            print(f"Error: File {file_path} not found")
            return False
            
        try:
            with open(file_path, 'rb') as f:
                response = requests.post(
                    f"{self.base_url}/upload",
                    params={'bucket': bucket, 'key': key},
                    data=f.read()
                )
            
            if response.status_code == 200:
                print(f"✓ Uploaded {file_path} as {bucket}/{key}")
                return True
            else:
                print(f"✗ Upload failed: {response.text}")
                return False
                
        except requests.RequestException as e:
            print(f"✗ Upload error: {e}")
            return False
    
    def download_file(self, bucket: str, key: str, output_path: str) -> bool:
        """Download a file from the server"""
        try:
            response = requests.get(
                f"{self.base_url}/download",
                params={'bucket': bucket, 'key': key}
            )
            
            if response.status_code == 200:
                os.makedirs(os.path.dirname(output_path), exist_ok=True)
                with open(output_path, 'wb') as f:
                    f.write(response.content)
                print(f"✓ Downloaded {bucket}/{key} to {output_path}")
                return True
            elif response.status_code == 404:
                print(f"✗ File not found: {bucket}/{key}")
                return False
            else:
                print(f"✗ Download failed: {response.text}")
                return False
                
        except requests.RequestException as e:
            print(f"✗ Download error: {e}")
            return False
    
    def delete_file(self, bucket: str, key: str) -> bool:
        """Delete a file from the server"""
        try:
            response = requests.delete(
                f"{self.base_url}/delete",
                params={'bucket': bucket, 'key': key}
            )
            
            if response.status_code == 200:
                print(f"✓ Deleted {bucket}/{key}")
                return True
            elif response.status_code == 404:
                print(f"✗ File not found: {bucket}/{key}")
                return False
            else:
                print(f"✗ Delete failed: {response.text}")
                return False
                
        except requests.RequestException as e:
            print(f"✗ Delete error: {e}")
            return False
    
    def list_buckets(self) -> bool:
        """List all buckets on the server"""
        try:
            response = requests.get(f"{self.base_url}/buckets")
            
            if response.status_code == 200:
                print("Available buckets:")
                print(response.text)
                return True
            else:
                print(f"✗ List buckets failed: {response.text}")
                return False
                
        except requests.RequestException as e:
            print(f"✗ List buckets error: {e}")
            return False


def create_test_file(filename: str, content: str = None) -> str:
    """Create a test file for uploading"""
    if content is None:
        content = f"This is a test file: {filename}\nCreated for LocalHive S3 testing.\nTimestamp: {os.path.getmtime('.')}"
    
    os.makedirs("test_files", exist_ok=True)
    filepath = os.path.join("test_files", filename)
    
    with open(filepath, 'w') as f:
        f.write(content)
    
    print(f"✓ Created test file: {filepath}")
    return filepath


def main():
    print("LocalHive S3 Client Example")
    print("=" * 40)
    
    client = LocalHiveClient()
    
    # Check server health
    print("1. Checking server health...")
    if not client.health_check():
        print("✗ Server is not running. Please start the LocalHive S3 server.")
        sys.exit(1)
    print("✓ Server is running")
    
    # Create test files
    print("\n2. Creating test files...")
    test_file1 = create_test_file("document.txt", "Hello, LocalHive!\nThis is a test document.")
    test_file2 = create_test_file("data.json", '{"name": "LocalHive", "type": "S3 Storage", "version": "1.0"}')
    
    # Upload files
    print("\n3. Uploading files...")
    client.upload_file("my-bucket", "docs/document.txt", test_file1)
    client.upload_file("my-bucket", "data/info.json", test_file2)
    
    # List buckets
    print("\n4. Listing buckets...")
    client.list_buckets()
    
    # Download files
    print("\n5. Downloading files...")
    os.makedirs("downloads", exist_ok=True)
    client.download_file("my-bucket", "docs/document.txt", "downloads/document.txt")
    client.download_file("my-bucket", "data/info.json", "downloads/info.json")
    
    # Test file not found
    print("\n6. Testing file not found...")
    client.download_file("my-bucket", "nonexistent.txt", "downloads/nonexistent.txt")
    
    # Delete files
    print("\n7. Deleting files...")
    client.delete_file("my-bucket", "docs/document.txt")
    client.delete_file("my-bucket", "data/info.json")
    
    # Test delete non-existent file
    print("\n8. Testing delete non-existent file...")
    client.delete_file("my-bucket", "nonexistent.txt")
    
    print("\n" + "=" * 40)
    print("✓ All tests completed!")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user")
    except Exception as e:
        print(f"\n\nUnexpected error: {e}")
        sys.exit(1)