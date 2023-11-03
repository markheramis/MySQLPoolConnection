#include <locale>
#include <ctime>
#include <chrono>
#include <mutex>
#include <map>
#include <csignal>
#include <sstream>
#include <fstream>
#include <thread>
#include <iostream>
#include <memory>
#include "./src/ConnectionPool.h"

bool running = true;

std::string GetProgramDir(char **argv)
{
    std::string programPath(argv[0]);
    size_t pos = programPath.rfind('/');
    if (pos == programPath.npos)
    {
        std::cerr << "Could not get program directory." << std::endl;
        exit(EXIT_FAILURE);
    }
    return programPath.substr(0, pos);
}

std::map<std::string, std::string> ReadConfigFile(std::string filename)
{
    std::map<std::string, std::string> params;
    std::ifstream stream(filename);
    if (stream.is_open())
    {
        std::string line;
        while (std::getline(stream, line))
        {
            if (line.length() == 0 || line.find('#') != line.npos)
                continue;
            std::stringstream ss(line);
            std::string name;
            std::string value;
            ss >> name;
            ss >> value;
            params[name] = value;
        }
        stream.close();
    }
    else
    {
        std::cerr << filename << " does not exist." << std::endl;
    }
    return params;
}

class DatabaseManager
{
private:
    std::shared_ptr<ConnectionPool> connPool;

public:
    /**
     * @brief The Constructor for DatabaseManager class.
     * 
     * Used to initialize the connection pool.
     * 
     * @param host mysql server name or ip address.
     * @param port mysql server port.
     * @param user mysql user name.
     * @param password mysql user password.
     * @param database mysql database name.
     * 
     */
    DatabaseManager(std::string host, int port, std::string user, std::string password, std::string database)
    {
        size_t NUM_CONNS = 3;
        connPool.reset(new ConnectionPool(host, port, user, password, database, NUM_CONNS));
        if (!connPool->HasActiveConnections())
        {
            std::cerr << "Error Initializing connection pool!" << std::endl;
            exit(EXIT_FAILURE);
        }
        std::cout << "Connection Initialized!" << std::endl;
    }
    /**
     * @brief Executes all database operation after getting a connection from the pool.
     * 
     * @param table The table name to query.
     * 
     * @returns void
     */
    void doDatabaseOperation(std::string table)
    {
        auto sqlPtr = connPool->GetConnecion();
        std::thread([this, sqlPtr, table]() {
            std::string error;
            std::stringstream ssquery;
            ssquery << "select * from " << sqlPtr->getDatabase() << ".`" << table << "`";
            auto results = sqlPtr->selectQuery(ssquery.str(), error);
            if (error.length() > 0) {
                std::cout << error << std::endl;
            } else {
                std::cout << "Results Count " << results.size() << std::endl;
                for (const auto &row : results) {
                    for (const auto &field : row) {
                        std::cout << field << " ";
                    }
                    std::cout << std::endl;
                }
            }
            connPool->ReleaseConnecion(sqlPtr); 
        }).detach();
    }
};

/**
 * @brief The main function.
 */
int main(int argc, char **argv)
{
    std::signal(SIGINT, [](int signal) {
        running = false;
        std::cout << "Program Interrupted." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        exit(EXIT_SUCCESS);
    });
    std::cout << "Starting...\n";

    std::string programDir = GetProgramDir(argv);
    std::stringstream ssdbfile;
    ssdbfile << programDir << "/config.txt";
    auto dbconfigs = ReadConfigFile(ssdbfile.str());

    std::string host = dbconfigs.at("dbhost");
    int port = std::stoi(dbconfigs.at("port"));
    std::string database = dbconfigs.at("database");
    std::string user = dbconfigs.at("user");
    std::string password = dbconfigs.at("password");
    std::string table = dbconfigs.at("table");

    DatabaseManager dbManager(host, port, user, password, database);
    while (running)
    {
        dbManager.doDatabaseOperation(table);
    }

    return 0;
}
