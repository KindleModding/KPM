#include <catch2/catch_test_macros.hpp>
#include "utils.hpp"
#include <filesystem>
#include <multiDownload.hpp>

TEST_CASE("Package target parsing", "[utils]")
{
  SECTION("Simple package ID")
  {
    ParsedPackageTarget target = parsePackageTarget("com.test.package");
    CHECK(target.package_name == "com.test.package");
    CHECK(target.repository_id.empty());
    CHECK(target.version_name.empty());
    CHECK(target.version_comparison_type == VersionComparisonType::NONE);
  }

  SECTION("Package with repository")
  {
    ParsedPackageTarget target = parsePackageTarget("repo/com.test.package");
    CHECK(target.package_name == "com.test.package");
    CHECK(target.repository_id == "repo");
    CHECK(target.version_name.empty());
    CHECK(target.version_comparison_type == VersionComparisonType::NONE);
  }

  SECTION("Package with version")
  {
    ParsedPackageTarget target = parsePackageTarget("com.test.package@1.0");
    CHECK(target.package_name == "com.test.package");
    CHECK(target.repository_id.empty());
    CHECK(target.version_name == "1.0");
    CHECK(target.version_comparison_type == VersionComparisonType::EQ);
  }

  SECTION("Package with repository and version")
  {
    ParsedPackageTarget target = parsePackageTarget("repo/com.test.package@1.0");
    CHECK(target.package_name == "com.test.package");
    CHECK(target.repository_id == "repo");
    CHECK(target.version_name == "1.0");
    CHECK(target.version_comparison_type == VersionComparisonType::EQ);
  }

  SECTION("Package with version comparison >=")
  {
    ParsedPackageTarget target = parsePackageTarget("com.test.package@>=1.0");
    CHECK(target.package_name == "com.test.package");
    CHECK(target.version_name == "1.0");
    CHECK(target.version_comparison_type == VersionComparisonType::GTEQ);
  }

  SECTION("Package with version comparison <=")
  {
    ParsedPackageTarget target = parsePackageTarget("com.test.package@<=1.0");
    CHECK(target.package_name == "com.test.package");
    CHECK(target.version_name == "1.0");
    CHECK(target.version_comparison_type == VersionComparisonType::LTEQ);
  }
}

TEST_CASE("Package installation utilities", "[utils][installation]")
{
  // These tests would need to mock filesystem operations
  // or use a test fixture with a temporary directory

  SECTION("isPackageCached function")
  {
    // Test logic for package caching
    DownloadTarget mockTarget = {
        .url = "http://example.com/package",
        .dest = "nonexistent_file.kpkg"};

    CHECK_FALSE(isPackageCached(mockTarget));

    // Would need to create a real file to test the positive case
  }
}