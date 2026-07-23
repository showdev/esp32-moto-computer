// Host tests for the EngineHours motohours accumulator.
// Thresholds mirror the project defaults: running >= 13.2 V, stopped <= 12.8 V.
#include <unity.h>
#include "EngineHours.h"

using namespace engine;

static HoursConfig kCfg{/*runOnV=*/13.2f, /*runOffV=*/12.8f};

void test_starts_stopped_and_empty(void) {
    HourMeter m(kCfg);
    TEST_ASSERT_FALSE(m.running());
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)m.seconds());
}

void test_seeds_initial_seconds(void) {
    HourMeter m(kCfg, /*initialSeconds=*/3600);
    TEST_ASSERT_EQUAL_UINT32(3600, (uint32_t)m.seconds());
    TEST_ASSERT_FALSE(m.dirty()); // seeded value equals persisted baseline
}

void test_stays_stopped_below_on_threshold(void) {
    HourMeter m(kCfg);
    TEST_ASSERT_FALSE(m.update(12.4f, 1000)); // key-on/engine-off sag
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)m.seconds());
}

void test_starts_running_at_on_threshold(void) {
    HourMeter m(kCfg);
    TEST_ASSERT_TRUE(m.update(13.2f, 0));
}

void test_does_not_accumulate_while_stopped(void) {
    HourMeter m(kCfg);
    m.update(12.0f, 5000);
    m.update(12.0f, 5000);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)m.seconds());
}

void test_accumulates_while_running(void) {
    HourMeter m(kCfg);
    m.update(14.0f, 0);          // becomes running; no prior interval to credit
    m.update(14.0f, 1000);       // +1000 ms
    m.update(14.0f, 2000);       // +2000 ms
    TEST_ASSERT_EQUAL_UINT32(3, (uint32_t)m.seconds());
}

void test_holds_state_in_dead_band(void) {
    HourMeter m(kCfg);
    m.update(13.0f, 0);                       // band, was stopped -> stays stopped
    TEST_ASSERT_FALSE(m.running());
    m.update(14.0f, 0);                       // -> running
    m.update(13.0f, 1000);                    // band, was running -> stays running
    TEST_ASSERT_TRUE(m.running());
    TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)m.seconds());
}

// Transition attribution: the interval that ENDS a run is credited to the run
// (state evaluated before the stopping reading takes effect); the next interval
// while stopped is not.
void test_stopping_interval_is_counted_once(void) {
    HourMeter m(kCfg);
    m.update(14.0f, 0);                       // running
    m.update(12.0f, 5000);                    // credit 5 s, THEN stop
    TEST_ASSERT_FALSE(m.running());
    TEST_ASSERT_EQUAL_UINT32(5, (uint32_t)m.seconds());
    m.update(12.0f, 5000);                    // stopped throughout -> no credit
    TEST_ASSERT_EQUAL_UINT32(5, (uint32_t)m.seconds());
}

void test_dirty_tracks_unpersisted_change(void) {
    HourMeter m(kCfg);
    TEST_ASSERT_FALSE(m.dirty());             // nothing accumulated yet
    m.update(14.0f, 0);
    m.update(14.0f, 1000);                    // total advanced
    TEST_ASSERT_TRUE(m.dirty());
    m.markPersisted();
    TEST_ASSERT_FALSE(m.dirty());
    m.update(12.0f, 0);                       // stops; total unchanged
    TEST_ASSERT_FALSE(m.dirty());             // engine off -> no NVS write needed
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_starts_stopped_and_empty);
    RUN_TEST(test_seeds_initial_seconds);
    RUN_TEST(test_stays_stopped_below_on_threshold);
    RUN_TEST(test_starts_running_at_on_threshold);
    RUN_TEST(test_does_not_accumulate_while_stopped);
    RUN_TEST(test_accumulates_while_running);
    RUN_TEST(test_holds_state_in_dead_band);
    RUN_TEST(test_stopping_interval_is_counted_once);
    RUN_TEST(test_dirty_tracks_unpersisted_change);
    return UNITY_END();
}
