#include <catch2/catch_test_macros.hpp>
#include "multiDownload.hpp"
#include <fstream>
#include <filesystem>

TEST_CASE("MultiDownload error handling", "[multiDownload]")
{
  SECTION("Invalid URL handling")
  {
    std::vector<DownloadTarget> targets = {
        {.url = "invalid://not-a-real-url",
         .dest = "nonexistent_file.txt"}};

    MultiDownload downloader(targets);
    CHECK_FALSE(downloader.execute());

    auto errors = downloader.get_errors();
    REQUIRE(!errors.empty());
    CHECK(errors[0].url == "invalid://not-a-real-url");
  }

  SECTION("File creation error")
  {
    // Create a test path that definitely won't have write permissions or can't be created
    std::string testPath = "/path/that/doesnt/exist/file.txt";

    std::vector<DownloadTarget> targets = {
        {// Using a non-existent local file protocol to minimize network dependency
         .url = "file:///nonexistent-file-for-testing",
         .dest = testPath}};

    MultiDownload downloader(targets);
    bool result = downloader.execute();
    CHECK_FALSE(result);

    auto errors = downloader.get_errors();
    CHECK(!errors.empty());
  }
}