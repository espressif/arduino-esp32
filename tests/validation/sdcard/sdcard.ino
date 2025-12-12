#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <vfs_api.h>  // Needed for custom SD instance
#include <unity.h>
#include <vector>
#include <functional>
#include <string>
#include <memory>

#if defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
#define SPI_COUNT_MAX 2
#elif defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
#define SPI_COUNT_MAX 1
#else
#define SPI_COUNT_MAX 4  // ESP32
#endif

#define MAX_FILES 5

class SPITestConfig {
public:
  std::string name;
  const char *mountpoint;
  uint8_t spi_num;
  int8_t sck;
  int8_t miso;
  int8_t mosi;
  int8_t ss;
  std::unique_ptr<SPIClass> spi;
  std::unique_ptr<SDFS> sd;
  bool mounted = false;

  SPITestConfig(std::string name, const char *mountpoint, uint8_t spi_num, int8_t sck, int8_t miso, int8_t mosi, int8_t ss)
    : name(name), mountpoint(mountpoint), spi_num(spi_num), sck(sck), miso(miso), mosi(mosi), ss(ss) {
    Serial.printf("Creating SPITestConfig [%s] on SPI bus %d: SCK=%d, MISO=%d, MOSI=%d, SS=%d\n", name.c_str(), spi_num, sck, miso, mosi, ss);
  }

  void begin(uint8_t max_files = MAX_FILES, bool format_if_empty = false) {
    spi = std::make_unique<SPIClass>(spi_num);
    sd = std::make_unique<SDFS>(FSImplPtr(new VFSImpl()));

    TEST_ASSERT_TRUE_MESSAGE(spi->begin(sck, miso, mosi, ss), "Failed to begin SPI");
    TEST_ASSERT_TRUE_MESSAGE(sd->begin(ss, *spi, 4000000, mountpoint, max_files, format_if_empty), "Failed to mount SD card");
    mounted = true;
  }

  void end() {
    sd->end();
    mounted = false;
    spi->end();
    spi.reset();
    sd.reset();
    Serial.printf("SPI and SD card for [%s] cleaned up\n", name.c_str());
  }
};

std::vector<std::unique_ptr<SPITestConfig>> spiTestConfigs;
using SpiTestFunction = std::function<void(SPITestConfig &config)>;

void run_init_continuous(SpiTestFunction test_function, uint8_t max_files, bool format_if_empty = false) {
  for (auto &ref : spiTestConfigs) {
    SPITestConfig &config = *ref;
    Serial.printf("Running test with SPI configuration: SCK=%d, MISO=%d, MOSI=%d, SS=%d\n", config.sck, config.miso, config.mosi, config.ss);
    config.begin(max_files, format_if_empty);
    test_function(config);
    config.end();
  }
}

void run_first_init(SpiTestFunction test_function, uint8_t max_files, bool format_if_empty = false) {
  for (auto &ref : spiTestConfigs) {
    SPITestConfig &config = *ref;
    config.begin(max_files, format_if_empty);
  }
  for (auto &ref : spiTestConfigs) {
    SPITestConfig &config = *ref;
    test_function(config);
  }
  for (auto &ref : spiTestConfigs) {
    SPITestConfig &config = *ref;
    config.end();
  }
}

void test_nonexistent_spi_interface(void) {
  // Attempt to create a SPIClass with an invalid SPI number
  SPIClass spiNotExist(SPI_COUNT_MAX);
  TEST_ASSERT_FALSE_MESSAGE(spiNotExist.begin(), "SPIClass should not be initialized with an invalid SPI number");
  spiNotExist.end();
}

void run_multiple_ways(SpiTestFunction test_function, uint8_t max_files = MAX_FILES, bool format_if_empty = false) {
  run_first_init(test_function, max_files, format_if_empty);
  run_init_continuous(test_function, max_files, format_if_empty);
}

void test_sd_basic(void) {
  Serial.println("Running test_sd_basic");
  for (auto &ref : spiTestConfigs) {
    SPITestConfig &config = *ref;
    config.begin();
    TEST_ASSERT_TRUE_MESSAGE(config.sd->exists("/"), "Root directory should exist");
    config.end();
  }
}

void test_sd_dir(void) {
  Serial.println("Running test_sd_dir");
  run_multiple_ways([](SPITestConfig &config) {
    TEST_ASSERT_TRUE_MESSAGE(config.sd->mkdir("/testdir"), "Failed to create directory /testdir");
    TEST_ASSERT_TRUE_MESSAGE(config.sd->exists("/testdir"), "Directory /testdir should exist after creation");
    TEST_ASSERT_TRUE_MESSAGE(config.sd->rmdir("/testdir"), "Failed to remove directory /testdir");
    TEST_ASSERT_FALSE_MESSAGE(config.sd->exists("/testdir"), "Directory /testdir should not exist after removal");
  });
}

void test_sd_file_operations(void) {
  Serial.println("Running test_sd_file_operations");
  run_multiple_ways([](SPITestConfig &config) {
    // Write a test file
    File testFile = config.sd->open("/testfile.txt", FILE_WRITE);
    TEST_ASSERT_TRUE_MESSAGE(testFile, "Failed to open file for writing");
    TEST_ASSERT_TRUE_MESSAGE(testFile.print("Hello, SD Card!"), "Failed to write to file");
    testFile.close();

    // Read the test file
    testFile = config.sd->open("/testfile.txt", FILE_READ);
    TEST_ASSERT_TRUE_MESSAGE(testFile, "Failed to open file for reading");
    String content = testFile.readString();
    TEST_ASSERT_EQUAL_STRING_MESSAGE("Hello, SD Card!", content.c_str(), "File content does not match expected value");
    testFile.close();

    // Clean up
    TEST_ASSERT_TRUE_MESSAGE(config.sd->remove("/testfile.txt"), "Failed to remove test file");
  });
}

void test_sd_open_limit(void) {
  Serial.println("Running test_sd_open_limit");

  run_init_continuous(
    [](SPITestConfig &config) {
      Serial.printf("Testing file open limit with SPI configuration: SCK=%d, MISO=%d, MOSI=%d, SS=%d\n", config.sck, config.miso, config.mosi, config.ss);

      // Open multiple files to test the limit
      File file1 = config.sd->open("/file1.txt", FILE_WRITE);
      TEST_ASSERT_TRUE_MESSAGE(file1, "Failed to open file1 for writing");
      TEST_ASSERT_TRUE_MESSAGE(file1.print("File 1 content"), "Failed to write to file1");

      File file2 = config.sd->open("/file2.txt", FILE_WRITE);
      TEST_ASSERT_TRUE_MESSAGE(file2, "Failed to open file2 for writing");
      TEST_ASSERT_TRUE_MESSAGE(file2.print("File 2 content"), "Failed to write to file2");

      // Attempt to open a third file, which should fail due to the limit
      File file3 = config.sd->open("/file3.txt", FILE_WRITE);
      TEST_ASSERT_FALSE_MESSAGE(file3, "Third file should not be opened due to max_files=2 limit");

      // Clean up files
      file1.close();
      file2.close();
      TEST_ASSERT_TRUE_MESSAGE(config.sd->remove("/file1.txt"), "Failed to remove file1");
      TEST_ASSERT_TRUE_MESSAGE(config.sd->remove("/file2.txt"), "Failed to remove file2");
      if (file3) {
        file3.close();
        TEST_ASSERT_TRUE_MESSAGE(config.sd->remove("/file3.txt"), "Failed to remove file3");
      }
    },
    2  // max_files set to 2 for this test
  );
}

void test_sd_directory_listing(void) {
  Serial.println("Running test_sd_directory_listing");

  run_multiple_ways([](SPITestConfig &config) {
    // Create test directory structure
    TEST_ASSERT_TRUE_MESSAGE(config.sd->mkdir("/testdir"), "Failed to create /testdir");
    TEST_ASSERT_TRUE_MESSAGE(config.sd->mkdir("/testdir/subdir"), "Failed to create /testdir/subdir");

    // Create test files
    File file1 = config.sd->open("/testdir/file1.txt", FILE_WRITE);
    TEST_ASSERT_TRUE_MESSAGE(file1, "Failed to create file1.txt");
    TEST_ASSERT_TRUE_MESSAGE(file1.print("Content of file 1"), "Failed to write to file1.txt");
    file1.close();

    File file2 = config.sd->open("/testdir/file2.dat", FILE_WRITE);
    TEST_ASSERT_TRUE_MESSAGE(file2, "Failed to create file2.dat");
    TEST_ASSERT_TRUE_MESSAGE(file2.print("Content of file 2"), "Failed to write to file2.dat");
    file2.close();

    File subfile = config.sd->open("/testdir/subdir/subfile.txt", FILE_WRITE);
    TEST_ASSERT_TRUE_MESSAGE(subfile, "Failed to create subfile.txt");
    TEST_ASSERT_TRUE_MESSAGE(subfile.print("Content of subfile"), "Failed to write to subfile.txt");
    subfile.close();

    // Test directory listing
    File dir = config.sd->open("/testdir");
    TEST_ASSERT_TRUE_MESSAGE(dir, "Failed to open /testdir for listing");
    TEST_ASSERT_TRUE_MESSAGE(dir.isDirectory(), "/testdir should be a directory");

    int fileCount = 0;
    int dirCount = 0;
    File entry = dir.openNextFile();
    while (entry) {
      if (entry.isDirectory()) {
        dirCount++;
        Serial.printf("Found directory: %s\n", entry.name());
      } else {
        fileCount++;
        Serial.printf("Found file: %s (size: %d bytes)\n", entry.name(), entry.size());
      }
      entry.close();
      entry = dir.openNextFile();
    }
    dir.close();

    TEST_ASSERT_EQUAL_MESSAGE(2, fileCount, "Should find 2 files in /testdir");
    TEST_ASSERT_EQUAL_MESSAGE(1, dirCount, "Should find 1 subdirectory in /testdir");

    // Clean up
    TEST_ASSERT_TRUE_MESSAGE(config.sd->remove("/testdir/subdir/subfile.txt"), "Failed to remove subfile.txt");
    TEST_ASSERT_TRUE_MESSAGE(config.sd->rmdir("/testdir/subdir"), "Failed to remove subdir");
    TEST_ASSERT_TRUE_MESSAGE(config.sd->remove("/testdir/file1.txt"), "Failed to remove file1.txt");
    TEST_ASSERT_TRUE_MESSAGE(config.sd->remove("/testdir/file2.dat"), "Failed to remove file2.dat");
    TEST_ASSERT_TRUE_MESSAGE(config.sd->rmdir("/testdir"), "Failed to remove testdir");
  });
}

void test_sd_file_size_operations(void) {
  Serial.println("Running test_sd_file_size_operations");

  run_multiple_ways([](SPITestConfig &config) {
    const char *testData = "This is a test file with specific content for size testing.";
    size_t expectedSize = strlen(testData);

    // Create file with known content
    File file = config.sd->open("/sizefile.txt", FILE_WRITE);
    TEST_ASSERT_TRUE_MESSAGE(file, "Failed to create sizefile.txt");
    TEST_ASSERT_EQUAL_MESSAGE(expectedSize, file.print(testData), "Failed to write expected amount of data");
    file.close();

    // Test file size
    file = config.sd->open("/sizefile.txt", FILE_READ);
    TEST_ASSERT_TRUE_MESSAGE(file, "Failed to open sizefile.txt for reading");
    TEST_ASSERT_EQUAL_MESSAGE(expectedSize, file.size(), "File size doesn't match expected size");

    // Test available() method
    TEST_ASSERT_EQUAL_MESSAGE(expectedSize, file.available(), "Available bytes don't match file size");

    // Test position tracking
    TEST_ASSERT_EQUAL_MESSAGE(0, file.position(), "Initial position should be 0");

    char buffer[20];
    file.readBytes(buffer, 10);
    TEST_ASSERT_EQUAL_MESSAGE(10, file.position(), "Position should be 10 after reading 10 bytes");
    TEST_ASSERT_EQUAL_MESSAGE(expectedSize - 10, file.available(), "Available should decrease after reading");

    // Test seek
    TEST_ASSERT_TRUE_MESSAGE(file.seek(0), "Failed to seek to beginning");
    TEST_ASSERT_EQUAL_MESSAGE(0, file.position(), "Position should be 0 after seek");
    TEST_ASSERT_EQUAL_MESSAGE(expectedSize, file.available(), "Available should be full size after seek to start");

    file.close();

    // Clean up
    TEST_ASSERT_TRUE_MESSAGE(config.sd->remove("/sizefile.txt"), "Failed to remove sizefile.txt");
  });
}

void test_sd_nested_directories(void) {
  Serial.println("Running test_sd_nested_directories");

  run_multiple_ways([](SPITestConfig &config) {
    // Create nested directory structure
    TEST_ASSERT_TRUE_MESSAGE(config.sd->mkdir("/level1"), "Failed to create /level1");
    TEST_ASSERT_TRUE_MESSAGE(config.sd->mkdir("/level1/level2"), "Failed to create /level1/level2");
    TEST_ASSERT_TRUE_MESSAGE(config.sd->mkdir("/level1/level2/level3"), "Failed to create /level1/level2/level3");

    // Create files at different levels
    File file1 = config.sd->open("/level1/file_level1.txt", FILE_WRITE);
    TEST_ASSERT_TRUE_MESSAGE(file1, "Failed to create file at level1");
    TEST_ASSERT_TRUE_MESSAGE(file1.print("Level 1 content"), "Failed to write to level1 file");
    file1.close();

    File file2 = config.sd->open("/level1/level2/file_level2.txt", FILE_WRITE);
    TEST_ASSERT_TRUE_MESSAGE(file2, "Failed to create file at level2");
    TEST_ASSERT_TRUE_MESSAGE(file2.print("Level 2 content"), "Failed to write to level2 file");
    file2.close();

    File file3 = config.sd->open("/level1/level2/level3/file_level3.txt", FILE_WRITE);
    TEST_ASSERT_TRUE_MESSAGE(file3, "Failed to create file at level3");
    TEST_ASSERT_TRUE_MESSAGE(file3.print("Level 3 content"), "Failed to write to level3 file");
    file3.close();

    // Verify files exist at all levels
    TEST_ASSERT_TRUE_MESSAGE(config.sd->exists("/level1/file_level1.txt"), "Level 1 file should exist");
    TEST_ASSERT_TRUE_MESSAGE(config.sd->exists("/level1/level2/file_level2.txt"), "Level 2 file should exist");
    TEST_ASSERT_TRUE_MESSAGE(config.sd->exists("/level1/level2/level3/file_level3.txt"), "Level 3 file should exist");

    // Test reading from nested files
    File readFile = config.sd->open("/level1/level2/level3/file_level3.txt", FILE_READ);
    TEST_ASSERT_TRUE_MESSAGE(readFile, "Failed to open level3 file for reading");
    String content = readFile.readString();
    TEST_ASSERT_EQUAL_STRING_MESSAGE("Level 3 content", content.c_str(), "Level 3 file content mismatch");
    readFile.close();

    // Clean up from deepest to shallowest
    TEST_ASSERT_TRUE_MESSAGE(config.sd->remove("/level1/level2/level3/file_level3.txt"), "Failed to remove level3 file");
    TEST_ASSERT_TRUE_MESSAGE(config.sd->rmdir("/level1/level2/level3"), "Failed to remove level3 dir");
    TEST_ASSERT_TRUE_MESSAGE(config.sd->remove("/level1/level2/file_level2.txt"), "Failed to remove level2 file");
    TEST_ASSERT_TRUE_MESSAGE(config.sd->rmdir("/level1/level2"), "Failed to remove level2 dir");
    TEST_ASSERT_TRUE_MESSAGE(config.sd->remove("/level1/file_level1.txt"), "Failed to remove level1 file");
    TEST_ASSERT_TRUE_MESSAGE(config.sd->rmdir("/level1"), "Failed to remove level1 dir");
  });
}

void test_sd_file_count_in_directory(void) {
  Serial.println("Running test_sd_file_count_in_directory");
  run_multiple_ways([](SPITestConfig &config) {
    const char *fileBasePath = "/dir/a/b";
    const int numFiles = 5;

    auto getExpectedFile = [fileBasePath](int i) -> std::pair<String, String> {
      return {String(fileBasePath) + "/file" + String(i) + ".txt", "data:" + String(i)};
    };

    {
      // create nested directories
      TEST_ASSERT_TRUE_MESSAGE(config.sd->mkdir("/dir"), "mkdir /dir failed");
      TEST_ASSERT_TRUE_MESSAGE(config.sd->mkdir("/dir/a"), "mkdir /dir/a failed");
      TEST_ASSERT_TRUE_MESSAGE(config.sd->mkdir(fileBasePath), "mkdir /dir/a/b failed");
    }

    {
      for (int i = 0; i < numFiles; ++i) {
        String path = String(fileBasePath) + String("/file") + String(i) + String(".txt");
        File f = config.sd->open(path.c_str(), FILE_WRITE);
        TEST_ASSERT_TRUE_MESSAGE(f, ("open " + path + " failed").c_str());
        f.print("data:" + String(i));
        f.close();
      }
    }

    {
      File d = config.sd->open(fileBasePath);
      TEST_ASSERT_TRUE_MESSAGE(d && d.isDirectory(), "open(/dir/a/b) not a directory");

      bool found[numFiles] = {false};
      int count = 0;

      while (true) {
        File e = d.openNextFile();
        if (!e) {
          break;
        }

        String path = e.path();
        String content = e.readString();
        bool matched = false;

        for (int i = 0; i < numFiles; ++i) {
          if (!found[i]) {
            auto [expectedPath, expectedContent] = getExpectedFile(i);
            if (path == expectedPath) {
              TEST_ASSERT_EQUAL_STRING_MESSAGE(expectedContent.c_str(), content.c_str(), "File content mismatch");
              found[i] = true;
              matched = true;
              break;
            }
          }
        }

        TEST_ASSERT_TRUE_MESSAGE(matched, ("Unexpected file found: " + path).c_str());
        count++;
        e.close();
      }

      d.close();
      TEST_ASSERT_EQUAL_INT_MESSAGE(numFiles, count, "File count mismatch in directory listing");

      for (int i = 0; i < numFiles; ++i) {
        auto [expectedPath, _] = getExpectedFile(i);
        TEST_ASSERT_TRUE_MESSAGE(found[i], ("Expected file not found: " + expectedPath).c_str());
      }
    }

    // Cleanup: remove files and directories in reverse order (deepest first)
    for (int i = 0; i < numFiles; ++i) {
      auto [filePath, _] = getExpectedFile(i);
      TEST_ASSERT_TRUE_MESSAGE(config.sd->remove(filePath.c_str()), ("Failed to remove file: " + filePath).c_str());
    }
    // Remove directories
    TEST_ASSERT_TRUE_MESSAGE(config.sd->rmdir("/dir/a/b"), "Failed to remove directory: /dir/a/b");
    TEST_ASSERT_TRUE_MESSAGE(config.sd->rmdir("/dir/a"), "Failed to remove directory: /dir/a");
    TEST_ASSERT_TRUE_MESSAGE(config.sd->rmdir("/dir"), "Failed to remove directory: /dir");
  });
}

void test_sd_file_append_operations(void) {
  Serial.println("Running test_sd_file_append_operations");

  run_multiple_ways([](SPITestConfig &config) {
    const char *filename = "/appendtest.txt";

    // Create initial file
    File file = config.sd->open(filename, FILE_WRITE);
    TEST_ASSERT_TRUE_MESSAGE(file, "Failed to create file for append test");
    TEST_ASSERT_TRUE_MESSAGE(file.print("Line 1\n"), "Failed to write initial line");
    file.close();

    // Append to file
    file = config.sd->open(filename, FILE_APPEND);
    TEST_ASSERT_TRUE_MESSAGE(file, "Failed to open file for append");
    TEST_ASSERT_TRUE_MESSAGE(file.print("Line 2\n"), "Failed to append second line");
    TEST_ASSERT_TRUE_MESSAGE(file.print("Line 3\n"), "Failed to append third line");
    file.close();

    // Verify file contents
    file = config.sd->open(filename, FILE_READ);
    TEST_ASSERT_TRUE_MESSAGE(file, "Failed to open file for reading");

    String line1 = file.readStringUntil('\n');
    String line2 = file.readStringUntil('\n');
    String line3 = file.readStringUntil('\n');

    TEST_ASSERT_EQUAL_STRING_MESSAGE("Line 1", line1.c_str(), "First line mismatch");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("Line 2", line2.c_str(), "Second line mismatch");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("Line 3", line3.c_str(), "Third line mismatch");

    file.close();

    // Clean up
    TEST_ASSERT_TRUE_MESSAGE(config.sd->remove(filename), "Failed to remove append test file");
  });
}

void test_sd_large_file_operations(void) {
  Serial.println("Running test_sd_large_file_operations");

  run_multiple_ways([](SPITestConfig &config) {
    const char *filename = "/largefile.bin";
    const size_t chunkSize = 512;
    const size_t numChunks = 10;  // 5KB total
    const size_t totalSize = chunkSize * numChunks;

    // Create a buffer with known pattern
    uint8_t writeBuffer[chunkSize];
    for (size_t i = 0; i < chunkSize; i++) {
      writeBuffer[i] = (uint8_t)(i % 256);
    }

    // Write large file in chunks
    File file = config.sd->open(filename, FILE_WRITE);
    TEST_ASSERT_TRUE_MESSAGE(file, "Failed to create large file");

    for (size_t chunk = 0; chunk < numChunks; chunk++) {
      size_t written = file.write(writeBuffer, chunkSize);
      TEST_ASSERT_EQUAL_MESSAGE(chunkSize, written, "Failed to write complete chunk");
    }
    file.close();

    // Verify file size
    file = config.sd->open(filename, FILE_READ);
    TEST_ASSERT_TRUE_MESSAGE(file, "Failed to open large file for reading");
    TEST_ASSERT_EQUAL_MESSAGE(totalSize, file.size(), "Large file size mismatch");

    // Read and verify chunks
    uint8_t readBuffer[chunkSize];
    for (size_t chunk = 0; chunk < numChunks; chunk++) {
      size_t bytesRead = file.readBytes((char *)readBuffer, chunkSize);
      TEST_ASSERT_EQUAL_MESSAGE(chunkSize, bytesRead, "Failed to read complete chunk");

      // Verify chunk content
      for (size_t i = 0; i < chunkSize; i++) {
        if (readBuffer[i] != writeBuffer[i]) {
          char errorMsg[100];
          snprintf(errorMsg, sizeof(errorMsg), "Data mismatch at chunk %zu, byte %zu: expected %d, got %d", chunk, i, writeBuffer[i], readBuffer[i]);
          TEST_FAIL_MESSAGE(errorMsg);
        }
      }
    }
    file.close();

    // Clean up
    TEST_ASSERT_TRUE_MESSAGE(config.sd->remove(filename), "Failed to remove large test file");
  });
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);  // Wait for Serial to be ready
  }
  Serial.println("SPI test START");

// pins for SD1
#define SD1_SCK  SCK
#define SD1_MISO MISO
#define SD1_MOSI MOSI
#define SD1_SS   SS

#if defined(CONFIG_IDF_TARGET_ESP32)
// pins for SD2 - ESP32
#define SD2_SCK  25
#define SD2_MISO 26
#define SD2_MOSI 27
#define SD2_SS   14

// ESP32 uses FSPI for the flash memory (for tests we use VSPI)
#undef FSPI
#define FSPI VSPI
#else
#define SD2_SCK  1
#define SD2_MISO 2
#define SD2_MOSI 3
#define SD2_SS   8
#endif

  spiTestConfigs.push_back(std::make_unique<SPITestConfig>("FSPI", "/sd1", FSPI, SD1_SCK, SD1_MISO, SD1_MOSI, SD1_SS));
#if SPI_COUNT_MAX >= 2
  spiTestConfigs.push_back(std::make_unique<SPITestConfig>("HSPI", "/sd2", HSPI, SD2_SCK, SD2_MISO, SD2_MOSI, SD2_SS));
#endif

  UNITY_BEGIN();
  RUN_TEST(test_nonexistent_spi_interface);
  RUN_TEST(test_sd_basic);
  RUN_TEST(test_sd_dir);
  RUN_TEST(test_sd_file_operations);
  RUN_TEST(test_sd_open_limit);
  RUN_TEST(test_sd_directory_listing);
  RUN_TEST(test_sd_file_size_operations);
  RUN_TEST(test_sd_nested_directories);
  RUN_TEST(test_sd_file_count_in_directory);
  RUN_TEST(test_sd_file_append_operations);
  RUN_TEST(test_sd_large_file_operations);

  UNITY_END();

  Serial.println("SPI test END");
}

void loop() {}
