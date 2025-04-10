#!/bin/bash
BASEDIR=$(dirname "$(realpath $0)")
TEST_DIR="$BASEDIR/builddir/kpm_test"
CACHE_DIR="$BASEDIR/builddir/kpm_test_cache"
TEST_REPO="$BASEDIR/test_repo"
TEST_PKG="com.test.helloworld"

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counters
TESTS_TOTAL=0
TESTS_PASSED=0
TESTS_FAILED=0

# Clean up previous test environment
cleanup() {
    echo -e "${BLUE}Cleaning up test environment...${NC}"
    rm -rf "$TEST_DIR" "$CACHE_DIR"
    mkdir -p "$TEST_DIR" "$CACHE_DIR"
}

# Run a test with proper output formatting
run_test() {
    TEST_NAME="$1"
    shift
    COMMAND="$@"
    
    ((TESTS_TOTAL++))
    echo -e "\n${YELLOW}[$TESTS_TOTAL] Testing: ${TEST_NAME}${NC}"
    echo -e "${BLUE}$ $COMMAND${NC}"
    
    if eval "$COMMAND"; then
        echo -e "${GREEN}✓ Test passed: $TEST_NAME${NC}"
        ((TESTS_PASSED++))
        return 0
    else
        echo -e "${RED}✗ Test failed: $TEST_NAME${NC}"
        ((TESTS_FAILED++))
        return 1
    fi
}

# Test if a package is installed
is_package_installed() {
    PKG_ID="$1"
    echo -e "${BLUE}Checking if package $PKG_ID is installed...${NC}"
    $BASEDIR/builddir/src/kpm --kpm_dir "$TEST_DIR" --cache_dir "$CACHE_DIR" list-installed | grep -q "$PKG_ID"
    return $?
}

# Print summary at the end
print_summary() {
    echo -e "\n${YELLOW}=== Test Summary ===${NC}"
    echo -e "Total tests: $TESTS_TOTAL"
    echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"
    echo -e "${RED}Failed: $TESTS_FAILED${NC}"
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "${GREEN}All tests passed!${NC}"
        exit 0
    else
        echo -e "${RED}Some tests failed!${NC}"
        exit 1
    fi
}

# Make sure the test repo exists
check_test_repo() {
    if [ ! -d "$TEST_REPO" ]; then
        echo -e "${RED}Test repository not found at $TEST_REPO. Please set up a test repository first.${NC}"
        exit 1
    fi
    
    if [ ! -f "$TEST_REPO/manifest.json" ]; then
        echo -e "${RED}Test repository missing manifest.json at $TEST_REPO/manifest.json${NC}"
        exit 1
    fi
}

# Begin tests
echo -e "${YELLOW}=== Starting KPM Test Suite ===${NC}"
echo -e "${BLUE}Test directory: $TEST_DIR${NC}"
echo -e "${BLUE}Cache directory: $CACHE_DIR${NC}"

# Setup
cleanup
check_test_repo

# Test 1: Basic repository operations
run_test "Adding repository" \
    "$BASEDIR/builddir/src/kpm -v --kpm_dir \"$TEST_DIR\" --cache_dir \"$CACHE_DIR\" add-repo file://$TEST_REPO"

run_test "Updating repositories" \
    "$BASEDIR/builddir/src/kpm -v --kpm_dir \"$TEST_DIR\" --cache_dir \"$CACHE_DIR\" update"

run_test "Listing repositories" \
    "$BASEDIR/builddir/src/kpm -v --kpm_dir \"$TEST_DIR\" --cache_dir \"$CACHE_DIR\" list-repos"

# Test 2: Package listing and searching
run_test "Listing all packages" \
    "$BASEDIR/builddir/src/kpm -v --kpm_dir \"$TEST_DIR\" --cache_dir \"$CACHE_DIR\" list"

run_test "Searching for packages" \
    "$BASEDIR/builddir/src/kpm -v --kpm_dir \"$TEST_DIR\" --cache_dir \"$CACHE_DIR\" search hello"

run_test "Listing installed packages (empty)" \
    "$BASEDIR/builddir/src/kpm -v --kpm_dir \"$TEST_DIR\" --cache_dir \"$CACHE_DIR\" list-installed"

# Test 3: Package installation
run_test "Installing a package" \
    "$BASEDIR/builddir/src/kpm -v --kpm_dir \"$TEST_DIR\" --cache_dir \"$CACHE_DIR\" install $TEST_PKG"

run_test "Verifying package installation" \
    "is_package_installed $TEST_PKG"

run_test "Listing installed packages (non-empty)" \
    "$BASEDIR/builddir/src/kpm -v --kpm_dir \"$TEST_DIR\" --cache_dir \"$CACHE_DIR\" list-installed | grep -q \"$TEST_PKG\""

# Test 4: Package caching
run_test "Re-installing package (should use cache)" \
    "$BASEDIR/builddir/src/kpm -v --kpm_dir \"$TEST_DIR\" --cache_dir \"$CACHE_DIR\" install $TEST_PKG | grep -q \"already cached\""

# Test 5: Package removal
run_test "Uninstalling a package" \
    "$BASEDIR/builddir/src/kpm -v --kpm_dir \"$TEST_DIR\" --cache_dir \"$CACHE_DIR\" uninstall $TEST_PKG"

run_test "Verifying package removal" \
    "! is_package_installed $TEST_PKG"

# Test 6: Package upgrade (install, then upgrade)
run_test "Installing specific package version" \
    "$BASEDIR/builddir/src/kpm -v --kpm_dir \"$TEST_DIR\" --cache_dir \"$CACHE_DIR\" install $TEST_PKG"

# If there's a newer version in the repo, this would test the upgrade
run_test "Upgrading packages" \
    "$BASEDIR/builddir/src/kpm -v --kpm_dir \"$TEST_DIR\" --cache_dir \"$CACHE_DIR\" upgrade"

# Test 7: Error handling
run_test "Installing non-existent package (should fail)" \
    "! $BASEDIR/builddir/src/kpm -v --kpm_dir \"$TEST_DIR\" --cache_dir \"$CACHE_DIR\" install non.existent.package 2>&1 | grep -q \"No installation candidate\""

run_test "Removing non-existent package (should fail)" \
    "! $BASEDIR/builddir/src/kpm -v --kpm_dir \"$TEST_DIR\" --cache_dir \"$CACHE_DIR\" uninstall non.existent.package 2>&1 | grep -q \"not installed\""

# Test 8: Repository removal
run_test "Removing repository" \
    "$BASEDIR/builddir/src/kpm -v --kpm_dir \"$TEST_DIR\" --cache_dir \"$CACHE_DIR\" remove-repo test_repo"

# Print test summary
print_summary