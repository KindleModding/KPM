#include <catch2/catch_test_macros.hpp>
#include "database.hpp"

TEST_CASE("Database initialization", "[database]")
{
  Database db(":memory:");

  SECTION("Repository operations")
  {
    // Test adding a repository
    Repository repo;
    repo.id = "test";
    repo.url = "http://example.com";
    REQUIRE(db.AddRepository(repo));

    // Get repositories and verify
    auto repos = db.GetRepositories();
    REQUIRE(repos.size() == 1);
    CHECK(repos[0].id == "test");
    CHECK(repos[0].url == "http://example.com");

    // Test deleting a repository
    REQUIRE(db.DeleteRepository("test"));
    repos = db.GetRepositories();
    CHECK(repos.empty());
  }
}

TEST_CASE("Package operations", "[database][packages]")
{
  Database db(":memory:");

  // Prepare by adding a repository
  Repository repo;
  repo.id = "test";
  repo.url = "http://example.com";
  REQUIRE(db.AddRepository(repo));

  SECTION("Installing packages")
  {
    // Test installing a package
    PackageInstallCandidate pkg;
    pkg.package_id = "com.test.package";
    pkg.display_name = "Test Package";
    pkg.version_name = "1.0";
    pkg.version_number = 100;
    pkg.description = "A test package";
    pkg.repository_id = "test";
    pkg.architecture = "arm64";

    db.InstallPackage(pkg);

    // Verify the package is installed
    auto installed = db.GetInstalledPackages();
    REQUIRE(installed.size() == 1);
    CHECK(installed[0].package_id == pkg.package_id);
    CHECK(installed[0].version_name == pkg.version_name);
  }

  SECTION("Uninstalling packages")
  {
    // Install a package first
    PackageInstallCandidate pkg;
    pkg.package_id = "com.test.package";
    pkg.display_name = "Test Package";
    pkg.version_name = "1.0";
    pkg.version_number = 100;
    pkg.repository_id = "test";
    pkg.architecture = "arm64";

    db.InstallPackage(pkg);

    // Test package removal
    REQUIRE(db.UninstallPackage(pkg.package_id));

    // Verify it's gone
    auto installed = db.GetInstalledPackages();
    CHECK(installed.empty());
  }
}