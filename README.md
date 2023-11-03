# MySQL Pool Connection

This project provides a simple and efficient way to manage MySQL connections in a multithreaded environment. It uses a connection pool to reuse existing connections, improving performance by avoiding the overhead of establishing a new connection every time one is needed.

# Install Dependencies (Debian/Ubuntu)

```
sudo apt update
sudo apt install -y libmysqlclient-dev mysql-client
```
# Usage

To use this in your project, include the `ConnectionPool.h` file and create a `ConnectionPool` object:


``` C++
#include "./sqlconn/ConnectionPool.h"

std::string host = "127.0.0.1";
std::string username = "root";
std::string password = "password";
std::string database = "mydb";
size_t NUM_CONNS = 3;

std::shared_ptr<ConnectionPool> connPool;
connPool.reset(new ConnectionPool(host, port, username, password, database, NUM_CONNS));
```
You can then get a connection from the pool, use it, and release it back to the pool:
```
// get connection
auto sqlPtr = connPool->GetConnecion();

// Use connection

// release connection
connPool->ReleaseConnecion(sqlPtr);
```

# Running the Example
To run the provided example:
1. Update the database credentials by editing the .env file.
2. Run the following commands:

```
cd example
./compile.sh
./example
```

Press Ctrl+C to exit the program.


