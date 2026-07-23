// Host tests for FanControl hysteresis (trustworthy readings).
// Setpoints mirror the project defaults: ON 88 C, OFF 80 C.
#include <unity.h>
#include "FanControl.h"

using namespace fan;

static Thresholds kT{/*onC=*/88.0f, /*offC=*/80.0f};
static TempReading valid(float c) { return TempReading{c, TempStatus::Valid}; }
static TempReading disconnected() { return TempReading{-127.0f, TempStatus::Disconnected}; }
static TempReading powerOnDefault() { return TempReading{85.0f, TempStatus::PowerOnDefault}; }

void test_starts_off(void) {
    Controller c(kT);
    TEST_ASSERT_FALSE(c.fanOn());
}

void test_stays_off_below_on_threshold(void) {
    Controller c(kT);
    TEST_ASSERT_FALSE(c.update(valid(85.0f))); // in dead band, was off -> stay off
}

void test_turns_on_at_on_threshold(void) {
    Controller c(kT);
    TEST_ASSERT_TRUE(c.update(valid(88.0f))); // exactly onC -> ON
}

void test_turns_on_above_on_threshold(void) {
    Controller c(kT);
    TEST_ASSERT_TRUE(c.update(valid(95.0f)));
}

void test_holds_on_through_dead_band(void) {
    Controller c(kT);
    c.update(valid(90.0f));                    // ON
    TEST_ASSERT_TRUE(c.update(valid(85.0f)));  // dead band, was on -> stay on
    TEST_ASSERT_TRUE(c.update(valid(81.0f)));  // just above offC -> stay on
}

void test_turns_off_at_off_threshold(void) {
    Controller c(kT);
    c.update(valid(90.0f));                     // ON
    TEST_ASSERT_FALSE(c.update(valid(80.0f)));  // exactly offC -> OFF
}

void test_turns_off_below_off_threshold(void) {
    Controller c(kT);
    c.update(valid(90.0f));                     // ON
    TEST_ASSERT_FALSE(c.update(valid(70.0f)));
}

void test_full_cycle(void) {
    Controller c(kT);
    TEST_ASSERT_FALSE(c.update(valid(75.0f))); // cold
    TEST_ASSERT_FALSE(c.update(valid(87.9f))); // approaching, not yet
    TEST_ASSERT_TRUE(c.update(valid(88.1f)));  // trips ON
    TEST_ASSERT_TRUE(c.update(valid(82.0f)));  // cooling, still in band -> ON
    TEST_ASSERT_FALSE(c.update(valid(79.9f))); // drops below offC -> OFF
}

// --- fault policy: B (defer to OEM). Any untrusted reading -> abstain (OFF). ---

void test_fault_disconnected_off_from_off(void) {
    Controller c(kT);
    TEST_ASSERT_FALSE(c.update(disconnected()));
}

void test_fault_disconnected_turns_off_running_fan(void) {
    Controller c(kT);
    c.update(valid(95.0f));                          // ON
    TEST_ASSERT_TRUE(c.fanOn());
    TEST_ASSERT_FALSE(c.update(disconnected()));     // fault -> abstain -> OFF
}

// The critical one: 85.0 C sits INSIDE the ON band, but as a PowerOnDefault it
// must be ignored, not acted on as a real "hot" reading.
void test_fault_power_on_default_not_treated_as_hot(void) {
    Controller c(kT);
    TEST_ASSERT_FALSE(c.update(powerOnDefault()));   // would trip ON if trusted
}

void test_recovers_after_fault(void) {
    Controller c(kT);
    c.update(valid(95.0f));                          // ON
    c.update(disconnected());                        // fault -> OFF
    TEST_ASSERT_TRUE(c.update(valid(90.0f)));        // good hot reading -> back ON
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_starts_off);
    RUN_TEST(test_stays_off_below_on_threshold);
    RUN_TEST(test_turns_on_at_on_threshold);
    RUN_TEST(test_turns_on_above_on_threshold);
    RUN_TEST(test_holds_on_through_dead_band);
    RUN_TEST(test_turns_off_at_off_threshold);
    RUN_TEST(test_turns_off_below_off_threshold);
    RUN_TEST(test_full_cycle);
    RUN_TEST(test_fault_disconnected_off_from_off);
    RUN_TEST(test_fault_disconnected_turns_off_running_fan);
    RUN_TEST(test_fault_power_on_default_not_treated_as_hot);
    RUN_TEST(test_recovers_after_fault);
    return UNITY_END();
}
