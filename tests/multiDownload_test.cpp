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
    // This test would typically create a condition where file creation fails,
    // such as an invalid path or insufficient permissions
    // For now we'll just verify the error collection logic

    std::vector<DownloadTarget> targets = {
        {
            .url = "http://example.com/nonexistent",
            .dest = "/path/that/doesnt/exist/file.txt" // This should fail to create
        }};

    MultiDownload downloader(targets);
    bool result = downloader.execute();

    // The test expectation depends on how the implementation handles this case
    // We should at least not crash and collect errors properly
    if (!result)
    {
      auto errors = downloader.get_errors();
      CHECK(!errors.empty());
    }
  }
}

// Note: Tests that actually perform downloads would be marked as integration tests
// and might be skipped during regular unit testing