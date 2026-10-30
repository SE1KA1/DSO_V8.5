import unittest

from tools.rack_test.integrity import IntegrityTracker, SEQ_MOD, TS_MOD


def frame(seq, ts):
    return {"seq": seq, "ts_ms": ts}


class IntegrityTests(unittest.TestCase):
    def test_clean_sequence(self):
        tracker = IntegrityTracker()
        for i in range(1, 11):
            events = tracker.feed(frame(i, i * 1000), i * 1000)
            self.assertEqual(events, [])
        snap = tracker.snapshot()
        self.assertEqual(snap["seq_gaps"], 0)
        self.assertEqual(snap["reboots"], 0)
        self.assertEqual(snap["frames"], 10)

    def test_gap_detected(self):
        tracker = IntegrityTracker()
        tracker.feed(frame(1, 1000), 1000)
        tracker.feed(frame(2, 2000), 2000)
        events = tracker.feed(frame(5, 3000), 3000)
        self.assertEqual([e["kind"] for e in events], ["seq_gap"])
        snap = tracker.snapshot()
        self.assertEqual(snap["seq_gaps"], 1)
        self.assertEqual(snap["max_seq_gap"], 2)
        self.assertEqual(snap["gaps"][0]["got"], 5)

    def test_reboot_detected_not_gap(self):
        tracker = IntegrityTracker()
        for i in range(1, 11):
            tracker.feed(frame(i, i * 1000), i * 1000)
        events = tracker.feed(frame(1, 1000), 20000)
        kinds = [e["kind"] for e in events]
        self.assertIn("reboot", kinds)
        self.assertNotIn("seq_gap", kinds)
        snap = tracker.snapshot()
        self.assertEqual(snap["reboots"], 1)
        self.assertEqual(snap["epoch"], 1)
        self.assertEqual(snap["last_seq"], 1)

    def test_seq_wrap_is_not_gap(self):
        tracker = IntegrityTracker()
        tracker.feed(frame(SEQ_MOD - 1, 1000), 1000)
        events = tracker.feed(frame(0, 2000), 2000)
        self.assertEqual([e["kind"] for e in events], ["seq_wrap"])
        snap = tracker.snapshot()
        self.assertEqual(snap["seq_gaps"], 0)
        self.assertEqual(snap["seq_wraps"], 1)

    def test_ts_wrap_is_not_reboot(self):
        tracker = IntegrityTracker()
        tracker.feed(frame(1, TS_MOD - 1000), 1000)
        events = tracker.feed(frame(2, 500), 2000)
        self.assertEqual([e["kind"] for e in events], ["ts_wrap"])
        snap = tracker.snapshot()
        self.assertEqual(snap["reboots"], 0)
        self.assertEqual(snap["ts_wraps"], 1)

    def test_duplicate(self):
        tracker = IntegrityTracker()
        tracker.feed(frame(1, 1000), 1000)
        events = tracker.feed(frame(1, 2000), 2000)
        self.assertEqual([e["kind"] for e in events], ["duplicate"])
        self.assertEqual(tracker.snapshot()["duplicates"], 1)


if __name__ == "__main__":
    unittest.main()
