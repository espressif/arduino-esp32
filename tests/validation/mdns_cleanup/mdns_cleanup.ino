/*
 * ESPmDNS query result cleanup validation.
 * build_opt.h substitutes the query and cleanup functions to avoid network dependencies.
 */

#include <Arduino.h>
#include <ESPmDNS.h>
#include <unity.h>
#include <algorithm>
#include <vector>

static std::vector<mdns_result_t *> liveResults;
static unsigned resultFreeCalls;
static unsigned invalidFreeCalls;
static bool returnResults;

// These definitions and the calls in ESPmDNS.cpp are renamed by build_opt.h.
extern "C" esp_err_t mdns_query_ptr(const char *, const char *, uint32_t, size_t, mdns_result_t **results) {
  *results = NULL;
  if (!returnResults) {
    return ESP_OK;
  }

  mdns_result_t *first = (mdns_result_t *)calloc(1, sizeof(mdns_result_t));
  mdns_result_t *second = (mdns_result_t *)calloc(1, sizeof(mdns_result_t));
  if (!first || !second) {
    free(first);
    free(second);
    return ESP_ERR_NO_MEM;
  }
  first->port = 80;
  second->port = 443;
  first->next = second;
  liveResults.push_back(first);
  liveResults.push_back(second);
  *results = first;
  return ESP_OK;
}

extern "C" void mdns_query_results_free(mdns_result_t *results) {
  resultFreeCalls++;
  while (results) {
    auto entry = std::find(liveResults.begin(), liveResults.end(), results);
    if (entry == liveResults.end()) {
      invalidFreeCalls++;
      return;
    }
    mdns_result_t *next = results->next;
    liveResults.erase(entry);
    free(results);
    results = next;
  }
}

extern "C" void mdns_free(void) {}

void setUp(void) {
  resultFreeCalls = 0;
  invalidFreeCalls = 0;
  returnResults = true;
}

void tearDown(void) {
  // Also clean up allocations if a Unity assertion aborts a regression case.
  for (auto result : liveResults) {
    free(result);
  }
  liveResults.clear();
}

void test_mdns_end_without_query(void) {
  {
    MDNSResponder responder;
    responder.end();
    responder.end();
  }
  TEST_ASSERT_EQUAL(0, resultFreeCalls);
  TEST_ASSERT_EQUAL(0, invalidFreeCalls);
  TEST_ASSERT_TRUE(liveResults.empty());
}

void test_mdns_end_releases_query_results(void) {
  MDNSResponder responder;
  TEST_ASSERT_EQUAL(2, responder.queryService("http", "tcp"));
  TEST_ASSERT_EQUAL(80, responder.port(0));
  TEST_ASSERT_EQUAL(443, responder.port(1));
  TEST_ASSERT_EQUAL(2, liveResults.size());

  responder.end();

  TEST_ASSERT_EQUAL(1, resultFreeCalls);
  TEST_ASSERT_TRUE(liveResults.empty());
  TEST_ASSERT_EQUAL(0, responder.port(0));
}

void test_mdns_repeated_end_and_destruction(void) {
  {
    MDNSResponder responder;
    TEST_ASSERT_EQUAL(2, responder.queryService("http", "tcp"));
    responder.end();
    responder.end();
  }
  TEST_ASSERT_EQUAL(1, resultFreeCalls);
  TEST_ASSERT_EQUAL(0, invalidFreeCalls);
  TEST_ASSERT_TRUE(liveResults.empty());
}

void test_mdns_repeated_responder_lifetimes(void) {
  for (unsigned i = 0; i < 16; i++) {
    {
      MDNSResponder responder;
      TEST_ASSERT_EQUAL(2, responder.queryService("http", "tcp"));
      TEST_ASSERT_EQUAL(2, liveResults.size());
    }
    TEST_ASSERT_EQUAL(i + 1, resultFreeCalls);
    TEST_ASSERT_TRUE(liveResults.empty());
  }
  TEST_ASSERT_EQUAL(0, invalidFreeCalls);
}

void test_mdns_query_replaces_previous_results(void) {
  {
    MDNSResponder responder;
    TEST_ASSERT_EQUAL(2, responder.queryService("http", "tcp"));
    TEST_ASSERT_EQUAL(2, responder.queryService("https", "tcp"));
    TEST_ASSERT_EQUAL(1, resultFreeCalls);
    TEST_ASSERT_EQUAL(2, liveResults.size());
  }
  TEST_ASSERT_EQUAL(2, resultFreeCalls);
  TEST_ASSERT_EQUAL(0, invalidFreeCalls);
  TEST_ASSERT_TRUE(liveResults.empty());
}

void test_mdns_empty_query_clears_previous_results(void) {
  {
    MDNSResponder responder;
    TEST_ASSERT_EQUAL(2, responder.queryService("http", "tcp"));
    returnResults = false;
    TEST_ASSERT_EQUAL(0, responder.queryService("missing", "tcp"));
    TEST_ASSERT_EQUAL(0, responder.port(0));
    responder.end();
  }
  TEST_ASSERT_EQUAL(1, resultFreeCalls);
  TEST_ASSERT_EQUAL(0, invalidFreeCalls);
  TEST_ASSERT_TRUE(liveResults.empty());
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  UNITY_BEGIN();
  RUN_TEST(test_mdns_end_without_query);
  RUN_TEST(test_mdns_end_releases_query_results);
  RUN_TEST(test_mdns_repeated_end_and_destruction);
  RUN_TEST(test_mdns_repeated_responder_lifetimes);
  RUN_TEST(test_mdns_query_replaces_previous_results);
  RUN_TEST(test_mdns_empty_query_clears_previous_results);
  UNITY_END();
}

void loop() {}
