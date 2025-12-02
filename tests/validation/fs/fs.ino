#include <Arduino.h>
#include <unity.h>
#include <cstring>

#include <FS.h>
#include <SPIFFS.h>
#include <FFat.h>
#include <LittleFS.h>

const uint8_t MAX_TEST_OPEN_FILES = 3;  // Limit for testing

class IFileSystem {
public:
  virtual const char *name() const = 0;
  virtual bool begin(bool formatOnFail) = 0;
  virtual void end() = 0;
  virtual bool format() = 0;
  virtual size_t totalBytes() const = 0;
  virtual size_t usedBytes() const = 0;
  virtual fs::FS &vfs() = 0;
  virtual ~IFileSystem() {}
};

/**
 * Generic wrapper for any FS implementation
 *  it is not possible to just get the parent FS from the real implementation
 *  since the VFSImpl does not have all the necessary methods for testing (usedBytes, format, etc)
 */
template<typename Impl> class WrappedFS : public IFileSystem {
public:
  WrappedFS(Impl &impl, const char *basePath, const char *label, uint8_t maxOpen) : impl_(&impl), basePath_(basePath), label_(label), maxOpen_(maxOpen) {}

  const char *name() const override {
    return label_;
  }

  bool begin(bool formatOnFail) override {
    return impl_->begin(formatOnFail, basePath_, maxOpen_, label_);
  }
  void end() override {
    impl_->end();
  }
  bool format() override {
    return impl_->format();
  }
  size_t totalBytes() const override {
    return impl_->totalBytes();
  }
  size_t usedBytes() const override {
    return impl_->usedBytes();
  }
  fs::FS &vfs() override {
    return *impl_;
  }
  uint8_t maxOpenFiles() const {
    return maxOpen_;
  }

private:
  Impl *impl_;
  const char *basePath_;
  const char *label_;
  uint8_t maxOpen_;
};

// Concrete wrappers (labels must match the CSV)
static WrappedFS<decltype(SPIFFS)> FS_SPIFFS(SPIFFS, "/spiffs", "spiffs", MAX_TEST_OPEN_FILES);
static WrappedFS<decltype(FFat)> FS_FFAT(FFat, "/ffat", "fat", MAX_TEST_OPEN_FILES);
static WrappedFS<decltype(LittleFS)> FS_LFS(LittleFS, "/littlefs", "littlefs", MAX_TEST_OPEN_FILES);
static IFileSystem *gFS = nullptr;

void setUp() {
  TEST_ASSERT_NOT_NULL_MESSAGE(gFS, "Internal: gFS not set");
  // Try to mount with format on fail - this handles both fresh mount and remount cases
  bool mounted = gFS->begin(true);
  TEST_ASSERT_TRUE_MESSAGE(mounted, "Mount failed");
}

void tearDown() {
  gFS->end();
}

void test_info_sanity() {
  size_t tot = gFS->totalBytes();
  size_t used = gFS->usedBytes();
  TEST_ASSERT_TRUE_MESSAGE(tot > 0, "totalBytes() is zero");
  TEST_ASSERT_TRUE_MESSAGE(tot >= used, "usedBytes() > totalBytes()");
}

void test_basic_write_and_read() {
  auto &V = gFS->vfs();
  {
    // write and overwrite
    for (int i = 0; i < 3; ++i) {
      File f = V.open("/t.txt", FILE_WRITE);
      TEST_ASSERT_EQUAL_INT(0, (int)f.size());
      TEST_ASSERT_TRUE_MESSAGE(f, "open WRITE failed");
      TEST_ASSERT_EQUAL_INT(5, (int)f.print("hello"));
      f.close();
    }
  }
  {
    // read back
    File f = V.open("/t.txt", FILE_READ);
    TEST_ASSERT_GREATER_THAN(0, (int)f.size());
    TEST_ASSERT_TRUE_MESSAGE(f, "open READ failed");
    String s = f.readString();
    f.close();
    TEST_ASSERT_EQUAL_STRING("hello", s.c_str());
  }
}

void test_append_behavior() {
  auto &V = gFS->vfs();
  {
    File f = V.open("/append.txt", FILE_APPEND);
    TEST_ASSERT_TRUE(f);
    TEST_ASSERT_GREATER_OR_EQUAL(0, (int)f.size());
    f.println("line1");
    f.close();
  }
  {
    File f = V.open("/append.txt", FILE_APPEND);
    TEST_ASSERT_TRUE(f);
    TEST_ASSERT_GREATER_THAN(0, (int)f.size());
    f.println("line2");
    f.close();
  }
  {
    File f = V.open("/append.txt", FILE_READ);
    TEST_ASSERT_TRUE(f);
    String s = f.readString();
    f.close();
    TEST_ASSERT_NOT_EQUAL(-1, s.indexOf("line1"));
    TEST_ASSERT_NOT_EQUAL(-1, s.indexOf("line2"));
  }

  TEST_ASSERT_TRUE(V.remove("/append.txt"));
}

void test_dir_ops_and_list() {
  auto &V = gFS->vfs();
  const char *fileBasePath = "/dir/a/b";
  const int numFiles = 5;
  {
    // create nested directories
    TEST_ASSERT_TRUE_MESSAGE(V.mkdir("/dir"), "mkdir /dir failed");
    TEST_ASSERT_TRUE_MESSAGE(V.mkdir("/dir/a"), "mkdir /dir/a failed");
    TEST_ASSERT_TRUE_MESSAGE(V.mkdir(fileBasePath), "mkdir /dir/a/b failed");
  }

  {
    for (int i = 0; i < numFiles; ++i) {
      String path = String(fileBasePath) + String("/file") + String(i) + String(".txt");
      File f = V.open(path.c_str(), FILE_WRITE);
      TEST_ASSERT_TRUE_MESSAGE(f, ("open " + path + " failed").c_str());
      f.print("data:" + String(i));
      f.close();
    }
  }

  {
    File d = V.open(fileBasePath);
    TEST_ASSERT_TRUE_MESSAGE(d && d.isDirectory(), "open(/dir/a/b) not a directory");
    int count = 0;
    while (true) {
      File e = d.openNextFile();
      if (!e) {
        break;
      }

      String expectedPath = String(fileBasePath) + String("/file") + String(count) + String(".txt");
      TEST_ASSERT_EQUAL_STRING_MESSAGE(expectedPath.c_str(), e.path(), "File path mismatch");
      String content = e.readString();
      String expectedContent = "data:" + String(count);
      TEST_ASSERT_EQUAL_STRING_MESSAGE(expectedContent.c_str(), content.c_str(), "File content mismatch");
      count++;
      e.close();
    }
    d.close();
    TEST_ASSERT_EQUAL_INT_MESSAGE(numFiles, count, "File count mismatch in directory listing");
  }
}

void test_rename_and_remove() {
  auto &V = gFS->vfs();
  {
    File f = V.open("/t.txt", FILE_WRITE);
    TEST_ASSERT_TRUE(f);
    f.print("x");
    f.close();
  }
  TEST_ASSERT_TRUE(V.rename("/t.txt", "/t2.txt"));
  TEST_ASSERT_TRUE(V.exists("/t2.txt"));
  TEST_ASSERT_TRUE(!V.exists("/t.txt"));
  TEST_ASSERT_TRUE(V.remove("/t2.txt"));
  TEST_ASSERT_TRUE(!V.exists("/t2.txt"));
}

void test_binary_write_and_seek() {
  auto &V = gFS->vfs();
  {
    File f = V.open("/bin.dat", FILE_WRITE);
    TEST_ASSERT_TRUE(f);
    for (int i = 0; i < 256; ++i) {
      f.write((uint8_t)i);
    }
    f.close();
  }
  {
    File f = V.open("/bin.dat", FILE_READ);
    TEST_ASSERT_TRUE(f);
    TEST_ASSERT_EQUAL_UINT32(256, (uint32_t)f.size());
    TEST_ASSERT_TRUE(f.seek(123));
    int b = f.read();
    f.close();
    TEST_ASSERT_EQUAL_INT(123, b);
  }
}

void test_binary_incremental_with_size_tracking() {
  auto &V = gFS->vfs();
  const char *path = "/bin_inc.dat";
  const int chunkSize = 64;
  const int numChunks = 8;
  uint8_t writeBuffer[chunkSize];
  uint8_t readBuffer[chunkSize];

  // Initialize write buffer with pattern
  for (int i = 0; i < chunkSize; ++i) {
    writeBuffer[i] = (uint8_t)(i & 0xFF);
  }

  {
    File f = V.open(path, FILE_WRITE);
    TEST_ASSERT_TRUE(f);
    size_t expectedSize = 0;

    // Write chunks incrementally and verify position after each write
    // Note: size() may not update until file is closed/flushed on some filesystems
    for (int chunk = 0; chunk < numChunks; ++chunk) {
      size_t posBefore = f.position();
      TEST_ASSERT_EQUAL_UINT32(expectedSize, (uint32_t)posBefore);

      size_t written = f.write(writeBuffer, chunkSize);
      TEST_ASSERT_EQUAL_UINT32(chunkSize, (uint32_t)written);

      expectedSize += chunkSize;

      // Verify position advances correctly (more reliable than size during write)
      size_t posAfter = f.position();
      TEST_ASSERT_EQUAL_UINT32(expectedSize, (uint32_t)posAfter);

      // Flush to ensure data is written (some filesystems need this for accurate size)
      f.flush();
    }

    f.close();

    // Verify final file size
    File check = V.open(path, FILE_READ);
    TEST_ASSERT_TRUE(check);
    TEST_ASSERT_EQUAL_UINT32(expectedSize, (uint32_t)check.size());
    check.close();
  }

  {
    // Read back and verify content
    File f = V.open(path, FILE_READ);
    TEST_ASSERT_TRUE(f);
    TEST_ASSERT_EQUAL_UINT32(numChunks * chunkSize, (uint32_t)f.size());

    for (int chunk = 0; chunk < numChunks; ++chunk) {
      size_t sizeBefore = f.size();
      size_t posBefore = f.position();

      size_t read = f.read(readBuffer, chunkSize);
      TEST_ASSERT_EQUAL_UINT32(chunkSize, (uint32_t)read);

      // Size should not change during read
      size_t sizeAfter = f.size();
      TEST_ASSERT_EQUAL_UINT32(sizeBefore, (uint32_t)sizeAfter);

      // Position should advance
      size_t posAfter = f.position();
      TEST_ASSERT_EQUAL_UINT32(posBefore + chunkSize, (uint32_t)posAfter);

      // Verify content
      for (int i = 0; i < chunkSize; ++i) {
        TEST_ASSERT_EQUAL_UINT8(writeBuffer[i], readBuffer[i]);
      }
    }

    f.close();
  }

  V.remove(path);
}

void test_empty_file_operations() {
  auto &V = gFS->vfs();
  const char *path = "/empty.txt";

  {
    // Create empty file
    File f = V.open(path, FILE_WRITE);
    TEST_ASSERT_TRUE(f);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)f.size());
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)f.position());
    f.close();
  }

  {
    // Verify empty file exists and has zero size
    TEST_ASSERT_TRUE(V.exists(path));
    File f = V.open(path, FILE_READ);
    TEST_ASSERT_TRUE(f);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)f.size());
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)f.position());
    TEST_ASSERT_FALSE(f.available());
    f.close();
  }

  {
    // Try to read from empty file
    File f = V.open(path, FILE_READ);
    TEST_ASSERT_TRUE(f);
    int b = f.read();
    TEST_ASSERT_EQUAL_INT(-1, b);  // EOF
    String s = f.readString();
    TEST_ASSERT_EQUAL_STRING("", s.c_str());
    f.close();
  }

  V.remove(path);
}

void test_seek_edge_cases() {
  auto &V = gFS->vfs();
  const char *path = "/seek_test.dat";
  const size_t fileSize = 1024;

  {
    // Create file with known pattern
    File f = V.open(path, FILE_WRITE);
    TEST_ASSERT_TRUE(f);
    for (size_t i = 0; i < fileSize; ++i) {
      f.write((uint8_t)(i & 0xFF));
    }
    f.close();
  }

  {
    File f = V.open(path, FILE_READ);
    TEST_ASSERT_TRUE(f);
    TEST_ASSERT_EQUAL_UINT32(fileSize, (uint32_t)f.size());

    // Seek to beginning
    TEST_ASSERT_TRUE(f.seek(0));
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)f.position());
    TEST_ASSERT_EQUAL_INT(0, f.read());

    // Seek to middle
    size_t mid = fileSize / 2;
    TEST_ASSERT_TRUE(f.seek(mid));
    TEST_ASSERT_EQUAL_UINT32(mid, (uint32_t)f.position());
    TEST_ASSERT_EQUAL_INT(mid & 0xFF, f.read());

    // Seek to end
    TEST_ASSERT_TRUE(f.seek(fileSize));
    TEST_ASSERT_EQUAL_UINT32(fileSize, (uint32_t)f.position());
    TEST_ASSERT_FALSE(f.available());
    TEST_ASSERT_EQUAL_INT(-1, f.read());  // EOF

    // Seek back to beginning
    TEST_ASSERT_TRUE(f.seek(0));
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)f.position());
    f.close();
  }

  V.remove(path);
}

void test_file_truncation_and_overwrite() {
  auto &V = gFS->vfs();
  const char *path = "/trunc.txt";

  {
    // Write large file
    File f = V.open(path, FILE_WRITE);
    TEST_ASSERT_TRUE(f);
    for (int i = 0; i < 1000; ++i) {
      f.print("data");
    }
    f.close();

    File check = V.open(path, FILE_READ);
    TEST_ASSERT_TRUE(check);
    size_t largeSize = check.size();
    check.close();
    TEST_ASSERT_GREATER_THAN(1000, (int)largeSize);
  }

  {
    // Overwrite with smaller content (truncation)
    File f = V.open(path, FILE_WRITE);
    TEST_ASSERT_TRUE(f);
    f.print("small");
    f.close();

    // Check size after closing
    File check = V.open(path, FILE_READ);
    TEST_ASSERT_TRUE(check);
    size_t smallSize = check.size();
    check.close();
    TEST_ASSERT_LESS_THAN(100, (int)smallSize);
  }

  {
    // Verify truncated content
    File f = V.open(path, FILE_READ);
    TEST_ASSERT_TRUE(f);
    String content = f.readString();
    TEST_ASSERT_EQUAL_STRING("small", content.c_str());
    f.close();
  }

  V.remove(path);
}

void test_multiple_file_handles() {
  auto &V = gFS->vfs();
  const char *path1 = "/multi1.txt";
  const char *path2 = "/multi2.txt";

  {
    // Open multiple files for writing
    File f1 = V.open(path1, FILE_WRITE);
    File f2 = V.open(path2, FILE_WRITE);
    TEST_ASSERT_TRUE(f1);
    TEST_ASSERT_TRUE(f2);

    f1.print("file1");
    f2.print("file2");

    f1.close();
    f2.close();

    // Verify sizes after closing (more reliable)
    File check1 = V.open(path1, FILE_READ);
    File check2 = V.open(path2, FILE_READ);
    TEST_ASSERT_TRUE(check1);
    TEST_ASSERT_TRUE(check2);
    TEST_ASSERT_EQUAL_UINT32(5, (uint32_t)check1.size());
    TEST_ASSERT_EQUAL_UINT32(5, (uint32_t)check2.size());
    check1.close();
    check2.close();
  }

  {
    // Open multiple files for reading
    File f1 = V.open(path1, FILE_READ);
    File f2 = V.open(path2, FILE_READ);
    TEST_ASSERT_TRUE(f1);
    TEST_ASSERT_TRUE(f2);

    String s1 = f1.readString();
    String s2 = f2.readString();

    TEST_ASSERT_EQUAL_STRING("file1", s1.c_str());
    TEST_ASSERT_EQUAL_STRING("file2", s2.c_str());

    f1.close();
    f2.close();
  }

  V.remove(path1);
  V.remove(path2);
}

void test_free_space_tracking() {
  size_t initialUsed = gFS->usedBytes();
  size_t total = gFS->totalBytes();
  TEST_ASSERT_GREATER_THAN(0, (int)total);

  auto &V = gFS->vfs();
  const char *path = "/space_test.dat";
  const size_t testSize = 4096;

  {
    // Write file and check space usage
    size_t usedBefore = gFS->usedBytes();
    File f = V.open(path, FILE_WRITE);
    TEST_ASSERT_TRUE(f);

    for (size_t i = 0; i < testSize; ++i) {
      f.write((uint8_t)(i & 0xFF));
    }
    f.close();

    size_t usedAfter = gFS->usedBytes();
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(usedBefore, (uint32_t)usedAfter);
    // Note: usedBytes may not increase immediately due to caching/buffering
  }

  {
    // Remove file and verify space is freed
    size_t usedBefore = gFS->usedBytes();
    TEST_ASSERT_TRUE(V.remove(path));
    size_t usedAfter = gFS->usedBytes();
    // Space should be freed (or at least not increase)
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(usedBefore, (uint32_t)usedAfter);
  }

  // Final used should be close to initial (allowing for filesystem overhead)
  size_t finalUsed = gFS->usedBytes();
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(initialUsed + 10000, (uint32_t)finalUsed);  // Allow some overhead
}

void test_error_cases() {
  if (strcmp(gFS->name(), "spiffs") == 0) {
    TEST_MESSAGE("Skipping error case tests for SPIFFS due to different error handling");
    return;
  }

  auto &V = gFS->vfs();

  // Try to open non-existent file for reading
  TEST_ASSERT_FALSE(V.open("/nonexistent.txt", FILE_READ));
  TEST_ASSERT_FALSE(V.remove("/nonexistent.txt"));
  TEST_ASSERT_FALSE(V.rename("/nonexistent.txt", "/newname.txt"));
  TEST_ASSERT_FALSE(V.rmdir("/nonexistent_dir"));
}

void test_large_file_operations() {
  auto &V = gFS->vfs();
  const char *path = "/large.dat";
  const size_t largeSize = 10 * 1024;  // 10KB
  uint8_t buffer[256];

  // Initialize buffer
  for (int i = 0; i < 256; ++i) {
    buffer[i] = (uint8_t)i;
  }

  {
    // Write large file in chunks
    File f = V.open(path, FILE_WRITE);
    TEST_ASSERT_TRUE(f);

    size_t totalWritten = 0;
    for (size_t i = 0; i < largeSize; i += 256) {
      size_t toWrite = (largeSize - i < 256) ? (largeSize - i) : 256;
      size_t written = f.write(buffer, toWrite);
      TEST_ASSERT_EQUAL_UINT32(toWrite, (uint32_t)written);
      totalWritten += written;

      // Verify position grows correctly (more reliable than size during write)
      TEST_ASSERT_EQUAL_UINT32(totalWritten, (uint32_t)f.position());
    }

    f.close();

    // Verify final size
    File check = V.open(path, FILE_READ);
    TEST_ASSERT_TRUE(check);
    TEST_ASSERT_EQUAL_UINT32(largeSize, (uint32_t)check.size());
    check.close();
  }

  {
    // Read back large file
    File f = V.open(path, FILE_READ);
    TEST_ASSERT_TRUE(f);
    TEST_ASSERT_EQUAL_UINT32(largeSize, (uint32_t)f.size());

    size_t totalRead = 0;
    uint8_t readBuffer[256];
    while (totalRead < largeSize) {
      size_t toRead = (largeSize - totalRead < 256) ? (largeSize - totalRead) : 256;
      size_t read = f.read(readBuffer, toRead);
      TEST_ASSERT_GREATER_THAN(0, (int)read);
      totalRead += read;
    }

    TEST_ASSERT_EQUAL_UINT32(largeSize, (uint32_t)totalRead);
    f.close();
  }

  V.remove(path);
}

void test_write_read_patterns() {
  auto &V = gFS->vfs();
  const char *path = "/pattern.dat";

  {
    // Sequential write pattern
    File f = V.open(path, FILE_WRITE);
    TEST_ASSERT_TRUE(f);

    for (int i = 0; i < 100; ++i) {
      size_t posBefore = f.position();
      f.write((uint8_t)i);
      size_t posAfter = f.position();
      TEST_ASSERT_EQUAL_UINT32(posBefore + 1, (uint32_t)posAfter);
    }

    f.close();

    // Verify final size after closing
    File check = V.open(path, FILE_READ);
    TEST_ASSERT_TRUE(check);
    TEST_ASSERT_EQUAL_UINT32(100, (uint32_t)check.size());
    check.close();
  }

  {
    // Random access read pattern
    File f = V.open(path, FILE_READ);
    TEST_ASSERT_TRUE(f);

    // Read from various positions
    int positions[] = {0, 10, 50, 99, 25, 75};
    for (int i = 0; i < 6; ++i) {
      int pos = positions[i];
      TEST_ASSERT_TRUE(f.seek(pos));
      int value = f.read();
      TEST_ASSERT_EQUAL_INT(pos, value);
    }

    f.close();
  }

  V.remove(path);
}

void test_directory_operations_edge_cases() {
  auto &V = gFS->vfs();
  TEST_ASSERT_TRUE(V.mkdir("/test_dir"));

  if (strcmp(gFS->name(), "spiffs") != 0) {
    TEST_ASSERT_TRUE(V.mkdir("/test_dir"));
    TEST_ASSERT_FALSE(V.mkdir("/deep/nested/path"));
    TEST_ASSERT_FALSE(V.rmdir("/nonexistent_dir"));
  }
}

void test_max_open_files_limit() {
  if (strcmp(gFS->name(), "littlefs") == 0) {
    TEST_MESSAGE("Skipping: LittleFS does not have a max open files limit");
    return;
  }

  auto &V = gFS->vfs();

  // Create test files first
  {
    File f1 = V.open("/max1.txt", FILE_WRITE);
    File f2 = V.open("/max2.txt", FILE_WRITE);
    File f3 = V.open("/max3.txt", FILE_WRITE);
    TEST_ASSERT_TRUE(f1);
    TEST_ASSERT_TRUE(f2);
    TEST_ASSERT_TRUE(f3);
    f1.print("file1");
    f2.print("file2");
    f3.print("file3");
    f1.close();
    f2.close();
    f3.close();
  }

  // Open files up to the limit
  File files[MAX_TEST_OPEN_FILES + 1];
  int openedCount = 0;

  // Open maxOpen files - all should succeed
  for (int i = 0; i < MAX_TEST_OPEN_FILES; ++i) {
    char path[16];
    snprintf(path, sizeof(path), "/max%d.txt", i + 1);
    files[i] = V.open(path, FILE_READ);
    if (files[i]) {
      openedCount++;
    }
  }

  // Verify we opened exactly maxOpen files
  TEST_ASSERT_EQUAL_INT(MAX_TEST_OPEN_FILES, openedCount);

  // Try to open one more file beyond the limit
  File extraFile = V.open("/max1.txt", FILE_READ);
  TEST_ASSERT_FALSE_MESSAGE(extraFile, "Opened file beyond maxOpen limit");

  // Close one file
  files[0].close();
  openedCount--;

  // Now we should be able to open a new file
  File newFile = V.open("/max1.txt", FILE_READ);
  TEST_ASSERT_TRUE(newFile);
  newFile.close();

  // Close remaining files
  for (int i = 1; i < MAX_TEST_OPEN_FILES; ++i) {
    if (files[i]) {
      files[i].close();
    }
  }

  // Cleanup test files
  V.remove("/max1.txt");
  V.remove("/max2.txt");
  V.remove("/max3.txt");
}

// ---------------- Run the same test set over all FS ----------------

static void run_suite_for(IFileSystem &fs) {
  gFS = &fs;
  Serial.println();
  Serial.print("=== FS: ");
  Serial.println(fs.name());

  RUN_TEST(test_info_sanity);
  RUN_TEST(test_basic_write_and_read);
  RUN_TEST(test_append_behavior);
  RUN_TEST(test_dir_ops_and_list);
  RUN_TEST(test_rename_and_remove);
  RUN_TEST(test_binary_write_and_seek);
  RUN_TEST(test_binary_incremental_with_size_tracking);
  RUN_TEST(test_empty_file_operations);
  RUN_TEST(test_seek_edge_cases);
  RUN_TEST(test_file_truncation_and_overwrite);
  RUN_TEST(test_multiple_file_handles);
  RUN_TEST(test_free_space_tracking);
  RUN_TEST(test_error_cases);
  RUN_TEST(test_large_file_operations);
  RUN_TEST(test_write_read_patterns);
  RUN_TEST(test_directory_operations_edge_cases);
  RUN_TEST(test_max_open_files_limit);
  gFS = nullptr;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  UNITY_BEGIN();

  run_suite_for(FS_SPIFFS);
  run_suite_for(FS_FFAT);
  run_suite_for(FS_LFS);

  UNITY_END();
}

void loop() {}
