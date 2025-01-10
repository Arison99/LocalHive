# LocalHive

![License](https://img.shields.io/github/license/Arison99/LocalHive)
![Build](https://img.shields.io/github/actions/workflow/status/Arison99/LocalHive/build.yml)
![Version](https://img.shields.io/github/v/release/Arison99/LocalHive)
![Commits](https://img.shields.io/github/commit-activity/m/Arison99/LocalHive)
![Languages](https://img.shields.io/github/languages/count/Arison99/LocalHive)
![Tools](https://img.shields.io/badge/tools-C++%20%7C%20MongoDB%20%7C%20SQLite%20%7C%20Redis-blue)
![Issues](https://img.shields.io/github/issues/Arison99/LocalHive)
![PRs](https://img.shields.io/github/issues-pr/Arison99/LocalHive)

## Overview

**LocalHive** is a powerful development tool designed to streamline local application development. It provides a fully functional local S3 bucket and versatile database connectors, empowering developers to manage and analyze their data with ease. Written in C++, LocalHive is robust, efficient, and future-focused, enabling seamless integration with your projects.

## Features

- **Local S3 Bucket**: Access and manage a fully functional local S3 bucket for your development needs.
- **Database Connectors**: Seamlessly connect to multiple databases including:
  - MongoDB
  - SQLite
  - Redis
  - MySQL
  - PostgreSQL
  - Cassandra
  - MariaDB
- **Data Operations**:
  - Store images and Videos in Buckets
  - Search, create, and edit database tables.
  - Visualize database processes through interactive graphical analytics.
- **Future Plans**: UI-based frontend-backend integration without requiring manual Models, Views, and Controllers.

## Installation

### Prerequisites
- **Operating System**: Windows, macOS, or Linux
- **Dependencies**:
  - C++ Compiler (e.g., GCC, Clang, or MSVC)
  - MongoDB, SQLite, Redis (optional: other database clients)

### Steps
1. Clone the repository:
   ```bash
   git clone https://github.com/Arison99/LocalHive.git
   ```
2. Navigate to the project directory:
   ```bash
   cd LocalHive
   ```
3. Build the project:
   ```bash
   cmake .
   make
   ```
4. Run the application:
   ```bash
   ./LocalHive
   ```

## Usage

### Local S3 Bucket
- Start LocalHive to create a local S3 bucket.
- Access the bucket for storing and retrieving files during development.

### Database Connectors
- Use LocalHive to connect to supported databases.
- Execute operations like table creation, search, and edits.

### Graphical Analytics
- Monitor database processes via built-in graphical analytics.
- Gain insights into queries, transactions, and more.

## Example
Here’s a quick example to connect to MongoDB and create a table:
```cpp
#include "LocalHive.h"

int main() {
    LocalHive hive;
    hive.connectToDatabase("MongoDB", "localhost", 27017);
    hive.createTable("users", {"id", "name", "email"});
    return 0;
}
```

## Contributing
We welcome contributions! Please:
1. Fork the repository.
2. Create a feature branch.
3. Submit a pull request.

## Roadmap
- Design an S3 Bucket
- Allow creation of newBuckets
- Add & implement User Authentication with Administration Rights
- Create & add DB_Connectors for different Databases
- Design Backend APIs for the S3 Bucket & DB_Connectors
- Design UIs ( frontends )
- Handle Data_Requests from the frontend (Webpages or Desktop App)
- Build components for data display 
- Fetch and handle backend processes for analysis
- Build APIs for Data_Analytics
- Enhance graphical analytics with real-time monitoring.
- Introduce frontend-backend integration via a no-code UI.
- Support additional databases.
- Provide Docker-based deployment for simplified setup.

## License
This project is licensed under the [MIT License](LICENSE).

## Contact
- **Author**: [Arison99](mailto:harrisondaviinci@gmail.com)
- **GitHub**: [Arison99](https://github.com/Arison99/LocalHive)

---
This documentation is still under progress, so that implies more information about this product will be added during the development process 

Let LocalHive simplify your development process and make local development as seamless as possible!
