#define DROGON_TEST_MAIN
#include <drogon/drogon_test.h>
#include <drogon/drogon.h>
#include <filesystem>

DROGON_TEST(BasicTest)
{
    // Add your tests here
}

DROGON_TEST(StaticFileServingTest)
{
    auto client = drogon::HttpClient::newHttpClient("http://127.0.0.1:8080");
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setPath("/config.json");
    client->sendRequest(req, [TEST_CTX](drogon::ReqResult result, const drogon::HttpResponsePtr &resp) {
        REQUIRE(result == drogon::ReqResult::Ok);
        REQUIRE(resp != nullptr);
        // Since we disabled static file serving, it should be 404 Not Found
        // or whatever Drogon returns for unhandled paths.
        // Usually it's 404.
        CHECK(resp->statusCode() == drogon::k404NotFound);
    });
}

int main(int argc, char** argv) 
{
    using namespace drogon;

    // Load config to know the port and settings
    // We need to load it here because the app runs in this process
    std::string configPath = "config.json";
    if (!std::filesystem::exists(configPath)) {
        configPath = "../config.json";
    }
    app().loadConfigFile(configPath);

    std::promise<void> p1;
    std::future<void> f1 = p1.get_future();

    // Start the main loop on another thread
    std::thread thr([&]() {
        // Queues the promise to be fulfilled after starting the loop
        app().getLoop()->queueInLoop([&p1]() { p1.set_value(); });
        app().run();
    });

    // The future is only satisfied after the event loop started
    f1.get();
    int status = test::run(argc, argv);

    // Ask the event loop to shutdown and wait
    app().getLoop()->queueInLoop([]() { app().quit(); });
    thr.join();
    return status;
}
