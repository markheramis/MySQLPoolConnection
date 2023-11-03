#ifndef CONNECTION_POOL_H__ // #include guards
#define CONNECTION_POOL_H__

#include <vector>
#include <map>
#include <chrono>
#include <unordered_set>

#include "SQLConnection.h"
#include "concurrentqueue.h"

class ConnectionPool
{
public:
    ConnectionPool(
        std::string server, int port, std::string user,
        std::string password, std::string database, int numConnection);

    ~ConnectionPool();

    SQLConnection *GetConnecion(unsigned int timeout = 0);
    bool ReleaseConnecion(SQLConnection *sqlPtr);

    bool OpenPoolConnections();
    void ResetPoolConnections();
    void ClosePoolConnections();

    bool HasActiveConnections();

private:
    std::atomic_flag _pool_mutex;
    bool hasActiveConnections;
    std::unordered_set<int> Indexes;
    moodycamel::ConcurrentQueue<int> connectionQueue;
    std::vector<std::unique_ptr<SQLConnection>> mySqlPtrList;
};

/**
 * @brief Construct a new Connection Pool:: Connection Pool object
 *
 * @param server mysql server name or ip address.
 * @param port mysql server port.
 * @param user mysql user name.
 * @param password mysql user password.
 * @param database mysql database name.
 * @param numConnection number of connection to create.
 *
 * @returns ConnectionPool object that got created.
 */
ConnectionPool::ConnectionPool(std::string server, int port, std::string user, std::string password, std::string database, int numConnection)
{
    if (server.empty() || user.empty())
    {
        throw std::invalid_argument("Server or user name is empty.");
    }

    std::cout << "Creating connection pool server=" << server << " database=" << database << std::endl;

    hasActiveConnections = false;
    bool success = false;
    try
    {
        _pool_mutex.test_and_set(std::memory_order_acquire);
        for (int i = 0; i < numConnection; i++)
        {
            mySqlPtrList.emplace_back(
                new SQLConnection(server, port, user, password, database, i));

            success = mySqlPtrList[i]->connect();

            if (success)
            {
                connectionQueue.enqueue(i);
                Indexes.insert(i);
            }
            else
            {
                std::cerr << "Connection pool failed. Cannot connect to server." << std::endl;
                ClosePoolConnections();
                throw std::runtime_error("Failed to connect to server.");
            }
        }

        size_t count = mySqlPtrList.size();
        if (success && count > 0 && count == connectionQueue.size_approx())
        {
            hasActiveConnections = true;
            std::cout << "Pool created successfully." << std::endl;
        }
        _pool_mutex.clear(std::memory_order_release);
    }
    catch (const std::exception &e)
    {
        ClosePoolConnections();
        std::cerr << "Exception caught while creating connection pool: " << e.what() << std::endl;
        throw; // rethrow the exception to be handled by the caller
    }
}

ConnectionPool::~ConnectionPool()
{
    // ClosePoolConnections();
}

bool ConnectionPool::HasActiveConnections()
{
    return hasActiveConnections;
}

SQLConnection *ConnectionPool::GetConnecion(unsigned int timeout)
{
    if (!hasActiveConnections)
    {
        std::cerr << "No active sql connection." << std::endl;
        return nullptr;
    }

    if (timeout < 0)
        std::cerr << "Error: Get connection Timeout value less then 0." << std::endl;

    int ind;
    bool success = false;
    auto begin = std::chrono::system_clock::now();

    do
    {
        success = connectionQueue.try_dequeue(ind);
        if (success && ind < mySqlPtrList.size())
        {
            _pool_mutex.test_and_set(std::memory_order_acquire);
            auto it = Indexes.find(ind);
            if (it != Indexes.end())
                Indexes.erase(ind);
            _pool_mutex.clear(std::memory_order_relaxed);
            return mySqlPtrList[ind].get();
        }

        // set max waiting time to get connection
        // return nullptr on time out
        if (timeout > 0)
        {
            auto end = std::chrono::system_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(end - begin).count();
            if (elapsed >= timeout)
                return nullptr;
        }

    } while (!success);

    return nullptr;
}

bool ConnectionPool::ReleaseConnecion(SQLConnection *sqlPtr)
{
    if (sqlPtr->getPoolId() > -1)
    {
        _pool_mutex.test_and_set(std::memory_order_acquire);
        auto it = Indexes.find(sqlPtr->getPoolId());
        if (it == Indexes.end())
        {
            connectionQueue.enqueue(sqlPtr->getPoolId());
            Indexes.insert(sqlPtr->getPoolId());
        }
        _pool_mutex.clear(std::memory_order_release);
        return true;
    }
    return false;
}

bool ConnectionPool::OpenPoolConnections()
{
    try
    {
        ClosePoolConnections();
        // return true if no exception is thrown
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        // return false if an exception is thrown
        return false;
    }
}

void ConnectionPool::ClosePoolConnections()
{
    hasActiveConnections = false;
    for (auto &sqlPtr : mySqlPtrList)
    {
        if (sqlPtr != nullptr && sqlPtr->isValide())
            sqlPtr->close();
    }

    _pool_mutex.test_and_set(std::memory_order_acquire);
    connectionQueue = moodycamel::ConcurrentQueue<int>();
    Indexes = std::unordered_set<int>();
    _pool_mutex.clear(std::memory_order_release);
}

void ConnectionPool::ResetPoolConnections()
{
    bool success = false;
    ClosePoolConnections();
    for (auto &sqlPtr : mySqlPtrList)
    {
        success = sqlPtr->connect();
        if (success)
        {
            _pool_mutex.test_and_set(std::memory_order_acquire);
            Indexes.insert(sqlPtr->getPoolId());
            connectionQueue.enqueue(sqlPtr->getPoolId());
            _pool_mutex.clear(std::memory_order_release);
        }
        else
        {
            std::cerr << "Connection pool failed. Cannot connect to server." << std::endl;
            for (auto &sqlPtr : mySqlPtrList)
            {
                if (sqlPtr != nullptr && sqlPtr->isValide())
                    sqlPtr->close();
            }
        }
    }

    size_t count = mySqlPtrList.size();
    if (success && count > 0 && count == connectionQueue.size_approx())
        hasActiveConnections = true;
}

#endif
